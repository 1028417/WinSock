
#include "../inc/ServerSock.h"

namespace NS_WinSock
{
	static const UINT __AcceptDeliverCoefficient = 8;

	E_WinSockResult CServerSock::create()
	{
		E_WinSockResult eRet = __super::create(true);
		if (E_WinSockResult::WSR_OK != eRet)
		{
			return eRet;
		}

		m_lpfnAcceptEx = (LPFN_ACCEPTEX)GetExtensionFunction(WSAID_ACCEPTEX);
		if (NULL == m_lpfnAcceptEx)
		{
			return E_WinSockResult::WSR_Error;
		}

		int optval = 1;
		if (!setOpt(SO_REUSEADDR, &optval, sizeof optval))
		{
			return E_WinSockResult::WSR_Error;
		}

		return E_WinSockResult::WSR_OK;
	}

	E_WinSockResult CServerSock::listen(UINT uPort, int backlog)
	{
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(uPort);
		addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
		//inet_pton(AF_INET, "127.0.0.1", (void*)&addr.sin_addr);
		int iRet = ::bind(m_sock, (sockaddr*)&addr, sizeof addr);
		if (SOCKET_ERROR == iRet)
		{
			printSockErr("bind");
			return E_WinSockResult::WSR_Error;
		}

		iRet = ::listen(m_sock, backlog);
		if (SOCKET_ERROR == iRet)
		{
			printSockErr("listen");
			return E_WinSockResult::WSR_Error;
		}
		
		return E_WinSockResult::WSR_OK;
	}

	E_WinSockResult CServerSock::accept(SOCKET& socClient, sockaddr_in& addrClient)
	{
		socClient = INVALID_SOCKET;

		int addrlen = sizeof addrClient;
		//SOCKET socClient = ::accept(m_sock, (sockaddr*)&addrClient, &addrlen);
		socClient = WSAAccept(m_sock, (sockaddr*)&addrClient, &addrlen, [](
			IN LPWSABUF lpCallerId,
			IN LPWSABUF lpCallerData,
			IN OUT LPQOS lpSQOS,
			IN OUT LPQOS lpGQOS,
			IN LPWSABUF lpCalleeId,
			IN LPWSABUF lpCalleeData,
			OUT GROUP FAR * g,
			IN DWORD_PTR dwCallbackData)
		{
			//CWinSock *pWinSock = (CWinSock*)dwCallbackData;
			//if (NULL != pWinSock)
			//{
			//	if (!pWinSock->acceptCB())
			//	{
			//		return CF_REJECT;
			//	}
			//}

			return CF_ACCEPT;
		}, (DWORD_PTR)this);

		if (INVALID_SOCKET == socClient)
		{
			int iErr = WSAGetLastError();
			if (WSAEWOULDBLOCK != iErr && WSATRY_AGAIN != iErr)
			{
				printSockErr("accept", iErr);
			}

			return E_WinSockResult::WSR_Error;
		}

		//inet_ntop(addrClient.sin_family, &addrClient.sin_addr, pszAddr, sizeof(pszAddr));

		return E_WinSockResult::WSR_OK;
	}

	E_WinSockResult CServerSock::_acceptEx(SOCKET socAccept, tagAcceptPerIOData& perIOData)
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
			this->handleCPCallback(0, perIOData, (DWORD)perIOData.InternalHigh);
		}

		return E_WinSockResult::WSR_OK;
	}

	bool CServerSock::_acceptEx(UINT uClientCount)
	{
		return m_AcceptPerIOArray.asign(__AcceptDeliverCoefficient, *this
			, [&](tagAcceptPerIOData& perIOData) {
			UINT uNum = uClientCount / __AcceptDeliverCoefficient;
			CWinSock *pAcceptSock = perIOData.initNodes(uNum);
			if (NULL == pAcceptSock)
			{
				return false;
			}

			if (E_WinSockResult::WSR_OK != _acceptEx(pAcceptSock->getHandle(), perIOData))
			{
				return false;
			}

			return true;
		});
	}

	E_WinSockResult CServerSock::asyncAccept(UINT uClientCount, const CB_Accept& cbAccept
		, const CB_Recv& cbRecv, const CB_PeerDisconnect& cbPeerDisconnect, UINT uIOCPThreadCount)
	{
		CIOCP *pIOCP = NULL;
		if (0 != uIOCPThreadCount)
		{
			if (!m_iocp.create(uIOCPThreadCount, __AcceptDeliverCoefficient))
			{
				return E_WinSockResult::WSR_Error;
			}

			pIOCP = &m_iocp;
		}

		E_WinSockResult eRet = initAsync(pIOCP);
		if (E_WinSockResult::WSR_OK != eRet)
		{
			return eRet;
		}

		if (!_acceptEx(uClientCount))
		{
			return E_WinSockResult::WSR_Error;
		}

		m_AcceptSockMgr.init(cbAccept, cbRecv, cbPeerDisconnect);

		return E_WinSockResult::WSR_OK;
	}

	UINT CServerSock::broadcast(const string& strData)
	{
		UINT uCount = 0;

		m_AcceptSockMgr.enumerate([&](CWinSock& sock) {
			DWORD uLen = (DWORD)strData.size();
			if (E_WinSockResult::WSR_OK == sock.asyncSend((char*)strData.c_str(), uLen))
			{
				uCount++;
			}

			return true;
		});

		return uCount;
	}

	bool CServerSock::shutdown()
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

	void CServerSock::handleCPCallback(ULONG_PTR Internal, tagPerIOData& perIOData, DWORD dwNumberOfBytesTransferred)
	{
		if (!checkNTStatus(Internal) || !checkNTStatus(perIOData.Internal))
		{
			return;
		}

		auto& AcceptPerIOData = (tagAcceptPerIOData&)perIOData;
		CWinSock *pCurrAccept = AcceptPerIOData.getAcceptSock();
		if (NULL == pCurrAccept)
		{
			return;
		}

		m_AcceptSockMgr.accept(pCurrAccept);

		CWinSock *pNewAccept = AcceptPerIOData.forward(m_AcceptSockMgr);
		if (NULL == pNewAccept)
		{
			return;
		}

		if (E_WinSockResult::WSR_OK != _acceptEx(pNewAccept->getHandle(), AcceptPerIOData))
		{
			return;
		}
	}
};
