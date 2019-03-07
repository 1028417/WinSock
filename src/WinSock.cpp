
#include "../inc/WinSock.h"

#include "../inc/Console.h"

#include <mstcpip.h>

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MsWSock.lib")

namespace NS_WinSock
{
	void CWinSock::printSockErr(const string& method, int iErr)
	{
		if (-1 == iErr)
		{
			iErr = WSAGetLastError();
		}

		CConsole::inst().print([&](ostream& out) {
			out << method.c_str() << " fail:" << iErr;
		});
	}

	bool CWinSock::checkNTStatus(ULONG_PTR Internal)
	{
		if (STATUS_SUCCESS != Internal)
		{
			if (STATUS_CANCELLED != Internal && STATUS_CONNECTION_ABORTED != Internal
				&& STATUS_CONNECTION_RESET != Internal && STATUS_REMOTE_DISCONNECT != Internal && STATUS_PENDING != Internal)
			{
				CWinSock::printSockErr("checkNTStatus", (int)Internal);
			}

			return false;
		}

		return true;
	}

	bool CWinSock::init(WORD wVersion, WORD wHighVersion)
	{
		WSADATA wsaData;
		int iRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (SOCKET_ERROR == iRet)
		{
			printSockErr("WSAStartup");
			return false;
		}

		return true;
	}

	//(void)WSACleanup(); 

	struct tagTimeval : timeval
	{
		tagTimeval(long tv_sec, long tv_usec)
		{
			timeval::tv_sec = tv_sec;
			timeval::tv_usec = tv_usec;
		}

		tagTimeval(DWORD dwMilliseconds)
		{
			timeval::tv_sec = dwMilliseconds / 1000;
			timeval::tv_usec = dwMilliseconds % 1000;
		}
	};

	bool CWinSock::select(tagSocketSet& sockSet, UINT& uResult, DWORD dwTimeout)
	{
		timeval *pTimeVal = WSA_INFINITE == dwTimeout ? NULL : &tagTimeval(dwTimeout);

		int iRet = ::select(0, &sockSet.readfds, &sockSet.writefds, &sockSet.errorfds, pTimeVal);
		if (0 > iRet)
		{
			printSockErr("::select");
			return false;
		}

		uResult = (UINT)iRet;

		return true;
	}

	E_WinSockResult CWinSock::create(bool bOverlapped)
	{
		DWORD dwFlags = 0;
		if (bOverlapped)
		{
			dwFlags = WSA_FLAG_OVERLAPPED;
		}
		m_sock = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, dwFlags);
		if (INVALID_SOCKET == m_sock)
		{
			printSockErr("WSASocket");
			return E_WinSockResult::WSR_Error;
		}

		if (bOverlapped)
		{
			if (!SetFileCompletionNotificationModes((HANDLE)m_sock, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS))
			{
				printSockErr("SetFileCompletionNotificationModes", GetLastError());
				return E_WinSockResult::WSR_Error;
			}
		}

		return E_WinSockResult::WSR_OK;
	}

	E_WinSockResult CWinSock::createNoBlock(bool bOverlapped)
	{
		E_WinSockResult eRet = create(bOverlapped);
		if (E_WinSockResult::WSR_OK != eRet)
		{
			return eRet;
		}

		if (!setNoBlock(true))
		{
			return E_WinSockResult::WSR_Error;
		}

		return E_WinSockResult::WSR_OK;
	}

	E_WinSockResult CWinSock::createAsync(CIOCP *pIOCP)
	{
		E_WinSockResult eRet = createNoBlock(true);
		if (E_WinSockResult::WSR_OK != eRet)
		{
			return eRet;
		}

		return initAsync(pIOCP);
	}

	bool CWinSock::setOpt(int optname, const void *optval, int optlen)
	{
		int iRet = setsockopt(m_sock, SOL_SOCKET, optname, (const char*)optval, optlen);
		if (SOCKET_ERROR == iRet)
		{
			printSockErr("setsockopt");
		}

		return true;
	}

	bool CWinSock::setNoBlock(bool bVal)
	{
		if (bVal != m_bNoBlock)
		{
			u_long argp = bVal ? 1 : 0;
			int iRet = ioctlsocket(m_sock, FIONBIO, &argp);
			if (SOCKET_ERROR == iRet)
			{
				printSockErr("ioctlsocket");
				return false;
			}

			m_bNoBlock = bVal;
		}

		return true;
	}
	
	bool CWinSock::keepAlive(ULONG keepalivetime, ULONG keepaliveinterval)
	{
		int keepalive = 1; // 开启keepalive属性
		if (!setOpt(SO_KEEPALIVE, &keepalive, sizeof(keepalive)))
		{
			return false;
		}

		tcp_keepalive alive_in = { 0 };
		alive_in.onoff = TRUE;
		alive_in.keepalivetime = keepalivetime;                // 开始首次KeepAlive探测前的TCP空闭时间
		alive_in.keepaliveinterval = keepaliveinterval;                // 两次KeepAlive探测间的时间间隔

		tcp_keepalive alive_out = { 0 };
		unsigned long ulBytesReturn = 0;

		int iRet = WSAIoctl(m_sock, SIO_KEEPALIVE_VALS, &alive_in, sizeof(alive_in),
			&alive_out, sizeof(alive_out), &ulBytesReturn, NULL, NULL);
		if (SOCKET_ERROR == iRet)
		{
			printSockErr("keepAlive");
			return false;
		}

		return true;
	}

	void* CWinSock::GetExtensionFunction(GUID guid)
	{
		void* pRet = NULL;

		DWORD dwBytes = 0;
		int iRet = WSAIoctl(m_sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid
			, sizeof guid, &pRet, sizeof pRet, &dwBytes, NULL, NULL);
		if (SOCKET_ERROR == iRet)
		{
			printSockErr("WSAIoctl");
			return NULL;
		}

		return pRet;
	}

	E_WinSockResult CWinSock::waitEvent(WSAEVENT hSockEvent, long& lEvent, map<UINT, int>& mapEventErr, DWORD dwTimeout)
	{
		lEvent = 0;

		DWORD dwRet = WSAWaitForMultipleEvents(1, &hSockEvent, FALSE, dwTimeout, FALSE);
		if (WSA_WAIT_TIMEOUT == dwRet)
		{
			return E_WinSockResult::WSR_Timeout;
		}

		if (WSA_WAIT_EVENT_0 != dwRet)
		{
			return E_WinSockResult::WSR_Error;
		}

		WSANETWORKEVENTS event;
		memset(&event, 0, sizeof event);
		int iRet = WSAEnumNetworkEvents(m_sock, hSockEvent, &event);
		if (SOCKET_ERROR == iRet)
		{
			printSockErr("WSAEventSelect");
			return E_WinSockResult::WSR_Error;
		}

		lEvent = event.lNetworkEvents;

		UINT uIndex = 0;
		for (auto iErr : event.iErrorCode)
		{
			if (0 != iErr)
			{
				mapEventErr[uIndex] = event.iErrorCode[uIndex];
			}

			uIndex++;
		}

		return E_WinSockResult::WSR_OK;
	}

	bool CWinSock::cancelIO(LPOVERLAPPED lpOverlapped)
	{
		if (!::CancelIoEx((HANDLE)m_sock, NULL))
		{
			DWORD dwErr = GetLastError();
			if (ERROR_NOT_FOUND == dwErr)
			{
				return true;
			}

			printSockErr("CancelIoEx", dwErr);
			return false;
		}

		return true;
	}

	static LPFN_DISCONNECTEX g_lpfnDisconnectEx = NULL;

	bool CWinSock::disconnect()
	{
		if (NULL == g_lpfnDisconnectEx)
		{
			if (NULL == (g_lpfnDisconnectEx = (LPFN_DISCONNECTEX)GetExtensionFunction(WSAID_DISCONNECTEX)))
			{
				return false;
			}
		}

		m_eStatus = E_SockConnStatus::SCS_Closing;

		this_thread::yield();

		if (!cancelIO())
		{
			return false;
		}
		
		if (!g_lpfnDisconnectEx(m_sock, NULL, TF_REUSE_SOCKET, 0))
		{
			return false;
		}

		m_eStatus = E_SockConnStatus::SCS_None;

		return true;
	}

	bool CWinSock::close(bool bLinger, int lingerTime)
	{
		if (INVALID_SOCKET == m_sock)
		{
			return true;
		}

		if (E_SockConnStatus::SCS_Closing == m_eStatus)
		{
			return false;
		}
		E_SockConnStatus ePrevState = m_eStatus;
		m_eStatus = E_SockConnStatus::SCS_Closing;

		this_thread::yield();

		auto fnClose = [&] {
			if (!cancelIO())
			{
				return false;
			}

			if (E_SockConnStatus::SCS_Connected == ePrevState)
			{
				if (bLinger)
				{
					//if (0 < lingerTime)
					//{
					//	(void)this->setNoBlock(false);
					//}

					int lpLinger[] = { 1, lingerTime };
					(void)setOpt(SO_LINGER, lpLinger, sizeof lpLinger);
				}
			}

			int iRet = ::closesocket(m_sock);
			if (SOCKET_ERROR == iRet)
			{
				int iErr = WSAGetLastError();
				//WSAEINPROGRESS：一个阻塞的WINDOWS套接口调用正在运行中。
				//WSAEINTR：通过一个WSACancelBlockingCall()来取消一个（阻塞的）调用。
				//WSAEWOULDBLOCK：该套接口设置为非阻塞方式且SO_LINGER设置为非零超时间隔。
				printSockErr("closesocket", iErr);
				return false;
			}
			m_sock = INVALID_SOCKET;

			m_eStatus = E_SockConnStatus::SCS_None;

			return true;
		};
		if (!fnClose())
		{
			m_eStatus = ePrevState;
			return false;
		}

		tagRecvPerIOData *pRecvPerIO = m_pRecvPerIO;
		if (NULL != pRecvPerIO)
		{
			m_pRecvPerIO = NULL;
			pRecvPerIO->free();
		}

		tagSendPerIOData *pSendPerIO = m_pSendPerIO;
		if (NULL != pSendPerIO)
		{
			pSendPerIO = NULL;
			pSendPerIO->free();
		}

		return true;
	}

	E_WinSockResult CWinSock::send(char* lpData, ULONG uLen, DWORD *pdwSentLen)
	{
		WSABUF wsaBuf;
		wsaBuf.buf = lpData;
		wsaBuf.len = uLen;
		int iRet = WSASend(m_sock, &wsaBuf, 1, pdwSentLen, 0, NULL, NULL);
		if (SOCKET_ERROR == iRet)
		{
			int iErr = WSAGetLastError();
			if (WSAEWOULDBLOCK == iErr)
			{
				return E_WinSockResult::WSR_EWOULDBLOCK;
			}
			else if (WSAECONNRESET == iErr)
			{
				return E_WinSockResult::WSR_PeerDisconnect;
			}

			printSockErr("send", iErr);
			return E_WinSockResult::WSR_Error;
		}

		return E_WinSockResult::WSR_OK;
	}

	E_WinSockResult CWinSock::receive(char* lpBuff, ULONG uBuffSize, DWORD& uRecvLen)
	{
		WSABUF buff;
		buff.buf = lpBuff;
		buff.len = uBuffSize;

		DWORD dwFlag = 0;
		int iRet = ::WSARecv(m_sock, &buff, 1, &uRecvLen, &dwFlag, NULL, NULL);
		if (SOCKET_ERROR == iRet)
		{
			int iErr = WSAGetLastError();
			if (WSAEWOULDBLOCK == iErr)
			{
				return E_WinSockResult::WSR_EWOULDBLOCK;
			}
			else if (WSAECONNRESET == iErr)
			{
				return E_WinSockResult::WSR_PeerDisconnect;
			}

			printSockErr("_receive", iRet);
			return E_WinSockResult::WSR_Error;
		}

		return E_WinSockResult::WSR_OK;
	}

	E_WinSockResult CWinSock::initAsync(CIOCP *pIOCP)
	{
		if (!setNoBlock(true))
		{
			return E_WinSockResult::WSR_Error;
		}

		if (NULL != pIOCP)
		{
			if (!pIOCP->bind(*this))
			{
				return E_WinSockResult::WSR_Error;
			}
		}
		else
		{
			(void)::BindIoCompletionCallback((HANDLE)m_sock, [](
				DWORD dwErrorCode,
				DWORD dwNumberOfBytesTransfered,
				LPOVERLAPPED lpOverlapped) {

				if (NULL == lpOverlapped)
				{
					CWinSock::printSockErr("fnPoolCB", ::GetLastError());
					return;
				}

				tagPerIOData& perIOData = (tagPerIOData&)*lpOverlapped;
				CWinSock& winSock = (CWinSock&)perIOData.iocpHandler;
				winSock.handleCPCallback(dwErrorCode, perIOData, dwNumberOfBytesTransfered);
			}, 0);
		}

		return E_WinSockResult::WSR_OK;
	}

	E_WinSockResult CWinSock::asyncReceive(const CB_Recv& fnRecvCB, const CB_PeerDisconnect& fnPeerShutdownedCB)
	{
		if (NULL == m_pRecvPerIO)
		{
			m_pRecvPerIO = tagRecvPerIOData::alloc(*this);
		}
		else
		{
			m_pRecvPerIO->asign(*this);
		}

		m_cbRecv = fnRecvCB;

		m_cbPeerDisconnect = fnPeerShutdownedCB;

		return _asyncReceive();
	}

	E_WinSockResult CWinSock::_asyncReceive()
	{
		DWORD dwFlag = 0;
		int iRet = ::WSARecv(m_sock, &m_pRecvPerIO->wsaBuf, 1, NULL, &dwFlag, m_pRecvPerIO, NULL);
		if (SOCKET_ERROR == iRet)
		{
			int iErr = WSAGetLastError();
			if (ERROR_IO_PENDING == iErr)
			{
				return E_WinSockResult::WSR_OK;
			}
			else if (WSAECONNRESET == iErr)
			{
				return E_WinSockResult::WSR_PeerDisconnect;
			}

			printSockErr("_receive", iErr);
			return E_WinSockResult::WSR_Error;
		}

		if (HasOverlappedIoCompleted(m_pRecvPerIO))
		{
			this->handleCPCallback(0, *m_pRecvPerIO, (DWORD)m_pRecvPerIO->InternalHigh);
		}

		return E_WinSockResult::WSR_OK;
	}

	E_WinSockResult CWinSock::asyncSend(char* lpData, ULONG uLen)
	{
		if (NULL == lpData || 0 == uLen)
		{
			return E_WinSockResult::WSR_Error;
		}

		m_sendData.add(lpData, uLen);

		if (m_bAyncSending)
		{
			return E_WinSockResult::WSR_OK;
		}

		if (NULL == m_pSendPerIO)
		{
			m_pSendPerIO = tagSendPerIOData::alloc(*this);
		}
		else
		{
			m_pSendPerIO->asign(*this);
		}

		return _asyncSend();
	}

	E_WinSockResult CWinSock::_asyncSend()
	{
		m_bAyncSending = true;

		m_pSendPerIO->wsaBuf.len = (ULONG)m_sendData.get(m_pSendPerIO->wsaBuf.buf, __SendPerIO_BuffSize);
		if (0 == m_pSendPerIO->wsaBuf.len)
		{
			m_bAyncSending = false;
			return E_WinSockResult::WSR_OK;
		}

		int iRet = WSASend(m_sock, &m_pSendPerIO->wsaBuf, 1, NULL, 0, m_pSendPerIO, NULL);
		if (SOCKET_ERROR == iRet)
		{
			m_bAyncSending = false;

			int iErr = WSAGetLastError();
			if (ERROR_IO_PENDING == iErr)
			{
				return E_WinSockResult::WSR_OK;
			}

			printSockErr("send", iErr);
			return E_WinSockResult::WSR_Error;
		}

		if (HasOverlappedIoCompleted(m_pSendPerIO))
		{
			this->handleCPCallback(0, *m_pSendPerIO, (DWORD)m_pSendPerIO->InternalHigh);
		}

		m_bAyncSending = false;

		return E_WinSockResult::WSR_OK;
	}
	
	void CWinSock::handleCPCallback(ULONG_PTR Internal, tagPerIOData& perIOData, DWORD dwNumberOfBytesTransferred)
	{
		if (E_SockConnStatus::SCS_Closing == m_eStatus)
		{
			return;
		}

		if (!checkNTStatus(Internal) || !checkNTStatus(perIOData.Internal))
		{
			if (STATUS_REMOTE_DISCONNECT == Internal || STATUS_REMOTE_DISCONNECT == perIOData.Internal)
			{
				onPeerDisconnect();
			}

			return;
		}

		if (&perIOData == m_pRecvPerIO)
		{
			_handleRecvCB(perIOData, dwNumberOfBytesTransferred);
		}
		else if (&perIOData == m_pSendPerIO)
		{
			_asyncSend();
		}
	}

	void CWinSock::_handleRecvCB(OVERLAPPED& overLapped, DWORD dwNumberOfBytesTransferred)
	{
		char *lpData = NULL;
		CCharVector dataVector;
		if (0 == m_pRecvPerIO->wsaBuf.len || NULL == m_pRecvPerIO->wsaBuf.buf)
		{
			char lpBuff[12800];
			memset(lpBuff, 0, sizeof lpBuff);
			DWORD uRecvLen = 0;
			auto eRet = this->receive(lpBuff, sizeof(lpBuff), uRecvLen);
			if (E_WinSockResult::WSR_OK != eRet || 0 == uRecvLen)
			{
				onPeerDisconnect();
				
				return;
			}

			lpData = dataVector.push_back(lpBuff, uRecvLen);

			while (true)
			{
				uRecvLen = 0;
				eRet = this->receive(lpBuff, sizeof(lpBuff), uRecvLen);
				if (E_WinSockResult::WSR_OK != eRet || 0 == uRecvLen)
				{
					if (E_WinSockResult::WSR_PeerDisconnect == eRet)
					{
						onPeerDisconnect();
						return;
					}

					break;
				}

				lpData = dataVector.push_back(lpBuff, uRecvLen);
			}

			dwNumberOfBytesTransferred = (DWORD)dataVector.getSize();
		}
		else
		{
			if (0 == dwNumberOfBytesTransferred)
			{
				onPeerDisconnect();
				return;
			}
			lpData = m_pRecvPerIO->wsaBuf.buf;
		}

		if (!onReceived(lpData, dwNumberOfBytesTransferred))
		{
			return;
		}

		if (E_WinSockResult::WSR_PeerDisconnect == _asyncReceive())
		{
			onPeerDisconnect();
		}
	}

	bool CWinSock::onReceived(char *lpData, DWORD dwNumberOfBytesTransferred)
	{
		if (m_cbRecv)
		{
			if (!m_cbRecv(*this, lpData, dwNumberOfBytesTransferred))
			{
				return false;
			}
		}

		return true;
	}

	void CWinSock::onPeerDisconnect()
	{
		(void)this->close();
		
		if (m_cbPeerDisconnect)
		{
			m_cbPeerDisconnect(*this);
		}
	}
};
