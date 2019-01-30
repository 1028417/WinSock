
#include "WinSockServer.h"

namespace NS_WinSock
{
	static const UINT __AcceptDeliverCoefficient = 8;

	bool CWinSockServer::create(UINT uPort)
	{
		if (!__super::create(true, false))
		{
			return false;
		}

		m_lpfnAcceptEx = (LPFN_ACCEPTEX)GetExtensionFunction(WSAID_ACCEPTEX);
		if (NULL == m_lpfnAcceptEx)
		{
			return false;
		}

		int optval = 1;
		if (!setOpt(SO_REUSEADDR, &optval, sizeof optval))
		{
			return false;
		}

		if (!listen(uPort))
		{
			return false;
		}

		auto fnOnAccepted = [](CAcceptSockMgr& AcceptSockMgr, CWinSock& WinSock) {
			//GetAcceptExSockaddrs(
			//if (!WinSock.setOpt(SO_UPDATE_ACCEPT_CONTEXT, &m_sock, sizeof SOCKET))
			//{
			//	return false;
			//}

			CB_RecvCB fnRecvCB = [&](CWinSock& WinSock, char *pData
				, DWORD dwNumberOfBytesTransferred, ULONG_PTR lpCompletionKey) {
				AcceptSockMgr.addMsg(pData, dwNumberOfBytesTransferred);
				return true;
			};
			CB_PeerShutdownedCB fnPeerShutdownedCB = [&](CWinSock& WinSock) {
				AcceptSockMgr.newRecycle(&WinSock);
			};

			if (!WinSock.poolBind(fnRecvCB, fnPeerShutdownedCB))
			{
				return false;
			}

			if (E_WinSockResult::WSR_OK != WinSock.receiveEx())
			{
				return false;
			}

			return true;
		};

		auto fnShuntdown = [](CAcceptSockMgr& AcceptSockMgr, CWinSock& WinSock) {
			if (!WinSock.disconnect())
			{
				WinSock.close();
				return false;
			}

			return true;
		};

		m_AcceptSockMgr.init(fnOnAccepted, fnShuntdown);

		return true;
	}

	void CWinSockServer::accept(UINT uThreadCount)
	{
		do {
			thread thrAccept([&] {
				SOCKET socClient = INVALID_SOCKET;

				sockaddr_in addrClient;
				memset(&addrClient, 0, sizeof(addrClient));

				//char pszAddr[16] = {0};
				while (true)
				{
					int iRet = CWinSock::accept(socClient, addrClient);
					if (WSAEINTR == iRet)
					{
						break;
					}

					if (INVALID_SOCKET == socClient)
					{
						continue;
					}

					//inet_ntop(addrClient.sin_family, &addrClient.sin_addr, pszAddr, sizeof(pszAddr));
					m_AcceptSockMgr.newAccept(new CWinSock(socClient));
				}
			});
			thrAccept.detach();
		} while (0 < uThreadCount && 0 < --uThreadCount);
	}

	E_WinSockResult CWinSockServer::_acceptEx(SOCKET socAccept, tagAcceptPerIOData& perIOData)
	{
		if (!m_lpfnAcceptEx(m_sock, socAccept, perIOData.wsaBuf.buf, perIOData.dwReceiveDataLength, __ADDRLEN, __ADDRLEN, NULL, &perIOData))
		{
			int iErr = WSAGetLastError();
			if (ERROR_IO_PENDING == iErr)
			{
				return E_WinSockResult::WSR_OK;
			}

			printSockErr("AcceptEx", iErr);
			return E_WinSockResult::WSR_Error;
		}

		if (HasOverlappedIoCompleted(&perIOData))
		{
			this->handleCPCallback(perIOData, (DWORD)perIOData.InternalHigh, m_sock);
		}

		return E_WinSockResult::WSR_OK;
	}

	bool CWinSockServer::_acceptEx(UINT uClientCount)
	{
		return m_AcceptPerIOArray.asign(__AcceptDeliverCoefficient, *this
			, [&](tagAcceptPerIOData& perIOData) {
			UINT uNum = uClientCount / __AcceptDeliverCoefficient;
			CWinSock *pAcceptSock = perIOData.createNodes(uNum);
			if (NULL == pAcceptSock)
			{
				return false;
			}

			if (E_WinSockResult::WSR_OK != _acceptEx(pAcceptSock->getSockHandle(), perIOData))
			{
				return false;
			}

			return true;
		});
	}

	bool CWinSockServer::poolAccept(UINT uClientCount)
	{
		if (!_acceptEx(uClientCount))
		{
			return false;
		}

		if (!CIOCP::poolBind(m_sock))
		{
			return false;
		}

		return true;
	}

	bool CWinSockServer::iocpAccept(UINT uIOCPThreadCount, UINT uClientCount)
	{
		if (!_acceptEx(uClientCount))
		{
			return false;
		}

		if (!m_iocp.create(uIOCPThreadCount, __AcceptDeliverCoefficient, m_sock))
		{
			return false;
		}
		
		return true;
	}

	UINT CWinSockServer::broadcast(const string& strData)
	{
		UINT uCount = 0;

		m_AcceptSockMgr.enumerate([&](CWinSock& sock) {
			DWORD uLen = (DWORD)strData.size();
			if (E_WinSockResult::WSR_OK == sock.sendEx((char*)strData.c_str(), uLen))
			{
				uCount++;
			}

			return true;
		});

		return uCount;
	}

	bool CWinSockServer::shutdown()
	{
		if (!m_iocp.shutdown())
		{
			return false;
		}
		
		if (!__super::close())
		{
			return false;
		}

		m_AcceptSockMgr.shutdown();

		return true;
	}

	void CWinSockServer::handleCPCallback(OVERLAPPED& overlapped, DWORD dwNumberOfBytesTransferred, ULONG_PTR lpCompletionKey)
	{
		auto& perIOData = (tagAcceptPerIOData&)overlapped;
		CWinSock *pCurrAccept = perIOData.getAcceptSock();
		if (NULL == pCurrAccept)
		{
			return;
		}

		m_AcceptSockMgr.newAccept(pCurrAccept);

		CWinSock *pNewAccept = perIOData.forward(m_AcceptSockMgr);
		if (NULL == pNewAccept)
		{
			return;
		}

		if (E_WinSockResult::WSR_OK != _acceptEx(pNewAccept->getSockHandle(), perIOData))
		{
			return;
		}
	}
};
