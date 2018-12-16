
#include "stdafx.h"

#include "WinSock.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MsWSock.lib")

namespace NS_WinSock
{
	LPFN_DISCONNECTEX CWinSock::s_lpfnDisconnectEx = NULL;

	void CWinSock::printSockErr(const string& method, int iErr)
	{
		if (-1 == iErr)
		{
			iErr = WSAGetLastError();
		}

		CConsole::inst().printEx([&](ostream& out) {
			out << method.c_str() << " fail:" << iErr;
		});
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

		/*SYSTEM_INFO si;
		memset(&si, 0, sizeof si);
		GetSystemInfo(&si);
		m_maxConcurrency = si.dwNumberOfProcessors;*/
		//m_maxConcurrency = thread::hardware_concurrency();

		return true;
	}

	//(void)WSACleanup(); 

	SOCKET CWinSock::createSock(bool bOverlapped)
	{
		DWORD dwFlags = 0;
		if (bOverlapped)
		{
			dwFlags = WSA_FLAG_OVERLAPPED;
		}
		SOCKET sock = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, dwFlags);
		if (INVALID_SOCKET == sock)
		{
			printSockErr("WSASocket");
			return INVALID_SOCKET;
		}

		if (bOverlapped)
		{
			if (!SetFileCompletionNotificationModes((HANDLE)sock, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS))
			{
				printSockErr("SetFileCompletionNotificationModes", GetLastError());
			}
		}

		return sock;
	}

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

	bool CWinSock::create(bool bOverlapped, bool bNoBlock)
	{
		m_sock = createSock(bOverlapped);
		if (INVALID_SOCKET == m_sock)
		{
			return false;
		}

		if (bNoBlock)
		{
			if (!setNoBlock(bNoBlock))
			{
				return false;
			}
		}

		m_eStatus = E_SockConnStatus::SCS_None;

		return true;
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
		u_long argp = bVal ? 1 : 0;
		int iRet = ioctlsocket(m_sock, FIONBIO, &argp);
		if (SOCKET_ERROR == iRet)
		{
			printSockErr("ioctlsocket");
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

	bool CWinSock::listen(UINT uPort, int backlog)
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
			return false;
		}

		iRet = ::listen(m_sock, backlog);
		if (SOCKET_ERROR == iRet)
		{
			printSockErr("listen");
			return false;
		}

		return true;
	}

	int CWinSock::accept(SOCKET& socClient, sockaddr_in& addrClient)
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

			return iErr;
		}

		return 0;
	}

	E_WinSockResult CWinSock::connect(const sockaddr_in& addr)
	{
		//int iRet = ::connect(m_sock, (sockaddr*)&addr, sizeof addr);
		int iRet = WSAConnect(m_sock, (sockaddr*)&addr, sizeof addr, NULL, NULL, NULL, NULL);
		if (SOCKET_ERROR == iRet)
		{
			int iErr = WSAGetLastError();
			if (WSAEWOULDBLOCK == iErr)
			{
				return E_WinSockResult::WSR_EWOULDBLOCK;
			}

			printSockErr("connect", iErr);
			return E_WinSockResult::WSR_Error;
		}

		return E_WinSockResult::WSR_OK;
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

	bool CWinSock::disconnect()
	{
		if (NULL == s_lpfnDisconnectEx)
		{
			if (NULL == (s_lpfnDisconnectEx = (LPFN_DISCONNECTEX)GetExtensionFunction(WSAID_DISCONNECTEX)))
			{
				return false;
			}
		}

		if (!cancelIO())
		{
			return false;
		}
		
		if (!s_lpfnDisconnectEx(m_sock, NULL, TF_REUSE_SOCKET, 0))
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
					//	(void)this->setNoBlock(FALSE);
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

	E_WinSockResult CWinSock::send(char* lpData, size_t uLen, DWORD *pdwSentLen)
	{
		if (NULL == lpData || 0 == uLen)
		{
			return E_WinSockResult::WSR_Error;
		}

		WSABUF wsaBuf;
		wsaBuf.buf = lpData;
		wsaBuf.len = uLen;
		uLen = 0;
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
				return E_WinSockResult::WSR_PeerClosed;
			}

			printSockErr("send", iErr);
			return E_WinSockResult::WSR_Error;
		}

		return E_WinSockResult::WSR_OK;
	}

	E_WinSockResult CWinSock::sendEx(char* lpData, size_t uLen)
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

		return _sendEx();
	}

	E_WinSockResult CWinSock::_sendEx()
	{
		m_bAyncSending = true;

		if (NULL == m_pSendPerIO)
		{
			m_pSendPerIO = tagSendPerIOData::alloc(*this);
		}
		else
		{
			m_pSendPerIO->asign(*this);
		}

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
			this->handleCPCallback(*m_pSendPerIO, (DWORD)m_pSendPerIO->InternalHigh, m_sock);
		}

		m_bAyncSending = false;

		return E_WinSockResult::WSR_OK;
	}

	E_WinSockResult CWinSock::receive(char* lpBuff, size_t uBuffSize, DWORD& uRecvLen)
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
				return E_WinSockResult::WSR_PeerClosed;
			}

			printSockErr("_receive", iRet);
			return E_WinSockResult::WSR_Error;
		}

		return E_WinSockResult::WSR_OK;
	}

	E_WinSockResult CWinSock::receiveEx()
	{
		if (NULL == m_pRecvPerIO)
		{
			m_pRecvPerIO = tagRecvPerIOData::alloc(*this);
		}
		else
		{
			m_pRecvPerIO->asign(*this);
		}

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
				return E_WinSockResult::WSR_PeerClosed;
			}

			printSockErr("_receive", iErr);
			return E_WinSockResult::WSR_Error;
		}

		if (HasOverlappedIoCompleted(m_pRecvPerIO))
		{
			this->handleCPCallback(*m_pRecvPerIO, (DWORD)m_pRecvPerIO->InternalHigh, m_sock);
		}

		return E_WinSockResult::WSR_OK;
	}

	bool CWinSock::poolBind(const CB_RecvCB& fnRecvCB, const CB_PeerShutdownedCB& fnPeerShutdownedCB)
	{
		if (!setNoBlock(true))
		{
			return false;
		}
		
		if (!CIOCP::poolBind(m_sock))
		{
			return false;
		}

		m_fnRecvCB = fnRecvCB;

		m_fnPeerShutdownedCB = fnPeerShutdownedCB;
		
		return true;
	}

	void CWinSock::handleCPCallback(OVERLAPPED_ENTRY *lpOverlappedEntry, ULONG ulNumEntries)
	{
		if (NULL == lpOverlappedEntry)
		{
			return;
		}

		while (ulNumEntries)
		{
			if (NULL != lpOverlappedEntry->lpOverlapped)
			{
				OVERLAPPED& overLapped = *lpOverlappedEntry->lpOverlapped;
				if (STATUS_REMOTE_DISCONNECT == lpOverlappedEntry->Internal || STATUS_REMOTE_DISCONNECT == overLapped.Internal)
				{
					_onPeerShutdowned();
				}
				else
				{
					if (checkNTStatus(lpOverlappedEntry->Internal) && checkNTStatus(overLapped.Internal))
					{
						handleCPCallback(overLapped, lpOverlappedEntry->dwNumberOfBytesTransferred, lpOverlappedEntry->lpCompletionKey);
					}
				}
			}

			lpOverlappedEntry++;
			ulNumEntries--;
		}
	}

	void CWinSock::handleCPCallback(OVERLAPPED& overLapped, DWORD dwNumberOfBytesTransferred, ULONG_PTR lpCompletionKey)
	{
		if (&overLapped == m_pRecvPerIO)
		{
			_handleRecvCB(overLapped, dwNumberOfBytesTransferred, lpCompletionKey);
		}
		else if (&overLapped == m_pSendPerIO)
		{
			_sendEx();

			return;
		}
	}

	void CWinSock::_handleRecvCB(OVERLAPPED& overLapped, DWORD dwNumberOfBytesTransferred, ULONG_PTR lpCompletionKey)
	{
		char *lpData = NULL;
		CCharVector dataVector;
		if (0 == m_pRecvPerIO->wsaBuf.len || NULL == m_pRecvPerIO->wsaBuf.buf)
		{
			char lpBuff[256];
			memset(lpBuff, 0, sizeof lpBuff);
			DWORD uRecvLen = 0;
			auto eRet = this->receive(lpBuff, sizeof(lpBuff), uRecvLen);
			if (E_WinSockResult::WSR_OK != eRet || 0 == uRecvLen)
			{
				_onPeerShutdowned();
				
				return;
			}

			lpData = dataVector.push_back(lpBuff, uRecvLen);

			while (true)
			{
				eRet = this->receive(lpBuff, sizeof(lpBuff), uRecvLen);
				if (E_WinSockResult::WSR_OK != eRet || 0 == uRecvLen)
				{
					if (E_WinSockResult::WSR_PeerClosed == eRet)
					{
						_onPeerShutdowned();
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
				_onPeerShutdowned();
				
				return;
			}
			lpData = m_pRecvPerIO->wsaBuf.buf;
		}

		if (m_fnRecvCB)
		{
			if (!m_fnRecvCB(*this, lpData, dwNumberOfBytesTransferred, lpCompletionKey))
			{
				return;
			}
		}
		else
		{
			if (!handleRecvCB(lpData, dwNumberOfBytesTransferred, lpCompletionKey))
			{
				return;
			}
		}

		if (E_WinSockResult::WSR_PeerClosed == receiveEx())
		{
			_onPeerShutdowned();
		}
	}

	void CWinSock::_onPeerShutdowned()
	{
		if (m_fnPeerShutdownedCB)
		{
			m_fnPeerShutdownedCB(*this);
		}
		else
		{
			(void)this->close();
		}
	}
};
