
#include "../inc/ClientSock.h"

#include <WS2tcpip.h>

namespace NS_WinSock
{
	E_WinSockResult CClientSock::connect(const char *pszServerAddr, UINT uServerPort, bool bWait, DWORD dwTimeout)
	{
		sockaddr_in addrServer;
		memset(&addrServer, 0, sizeof addrServer);
		addrServer.sin_family = AF_INET;
		addrServer.sin_port = htons(uServerPort);
		(void)inet_pton(AF_INET, pszServerAddr, (void*)&addrServer.sin_addr.S_un.S_addr);

		auto eRet = _connect(addrServer);
		if (E_WinSockResult::WSR_OK == eRet)
		{
			m_eStatus = E_SockConnStatus::SCS_Connected;
		}
		else if (E_WinSockResult::WSR_EWOULDBLOCK == eRet)
		{
			m_eStatus = E_SockConnStatus::SCS_Connecting;

			if (bWait)
			{
				return checkConnected(dwTimeout);
			}
		}

		return eRet;
	}

	E_WinSockResult CClientSock::_connect(const sockaddr_in& addr)
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

	E_WinSockResult CClientSock::checkConnected(DWORD dwTimeout)
	{
		if (E_SockConnStatus::SCS_Connected == m_eStatus)
		{
			return E_WinSockResult::WSR_OK;
		}

		if (E_SockConnStatus::SCS_Connecting != m_eStatus)
		{
			return E_WinSockResult::WSR_Error;
		}

		tagSocketSet sockSet(m_sock);
		UINT uRet = 0;
		if (!select(sockSet, uRet, dwTimeout))
		{
			m_eStatus = E_SockConnStatus::SCS_Excetion;
			return E_WinSockResult::WSR_Error;
		}
		if (0 == uRet)
		{
			return E_WinSockResult::WSR_Timeout;
		}

		if (sockSet.isError(m_sock))
		{
			int optval = -1;
			int optlen = sizeof optval;
			int iRet = getsockopt(m_sock, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen);
			if (SOCKET_ERROR == iRet)
			{
				printSockErr("getsockopt");
			}
			else
			{
				printSockErr("select", optval);
			}

			m_eStatus = E_SockConnStatus::SCS_Excetion;
			return E_WinSockResult::WSR_Error;
		}

		if (!sockSet.isWritable(m_sock))
		{
			return E_WinSockResult::WSR_Timeout;
		}

		m_eStatus = E_SockConnStatus::SCS_Connected;
		return E_WinSockResult::WSR_OK;
	}

#define __Event_CheckConnected (FD_CONNECT | FD_WRITE)

	E_WinSockResult CClientSock::waitConnected(DWORD dwTimeout)
	{
		if (E_SockConnStatus::SCS_Connected == m_eStatus)
		{
			return E_WinSockResult::WSR_OK;
		}

		if (E_SockConnStatus::SCS_Connecting != m_eStatus)
		{
			return E_WinSockResult::WSR_Error;
		}

		if (WSA_INVALID_EVENT == m_hEventWaitConnected)
		{
			m_hEventWaitConnected = WSACreateEvent();
			if (WSA_INVALID_EVENT == m_hEventWaitConnected)
			{
				printSockErr("WSACreateEvent");
				return E_WinSockResult::WSR_Error;
			}

			int iRet = WSAEventSelect(m_sock, m_hEventWaitConnected, __Event_CheckConnected);
			if (SOCKET_ERROR == iRet)
			{
				(void)WSACloseEvent(m_hEventWaitConnected);
				m_hEventWaitConnected = WSA_INVALID_EVENT;

				printSockErr("WSAEventSelect");
				return E_WinSockResult::WSR_Error;
			}
		}

		long lEvent = __Event_CheckConnected;
		map<UINT, int> mapEventErr;
		auto eRet = waitEvent(m_hEventWaitConnected, lEvent, mapEventErr, dwTimeout);
		if (E_WinSockResult::WSR_OK != eRet)
		{
			return eRet;
		}

		if (!mapEventErr.empty())
		{
			auto itr = mapEventErr.find(FD_CONNECT_BIT);
			if (itr != mapEventErr.end())
			{
				//printSockErr("sockError", itr->second);
			}

			m_eStatus = E_SockConnStatus::SCS_Excetion;
			return E_WinSockResult::WSR_Error;
		}

		if (__Event_CheckConnected != lEvent)
		{
			m_eStatus = E_SockConnStatus::SCS_Excetion;
			return E_WinSockResult::WSR_Error;
		}

		m_eStatus = E_SockConnStatus::SCS_Connected;
		return E_WinSockResult::WSR_OK;
	}

	bool CClientSock::close(bool bLinger, int lingerTime)
	{
		if (!__super::close(bLinger, lingerTime))
		{
			return false;
		}

		WSAEVENT hEventWaitConnected = m_hEventWaitConnected;
		if (WSA_INVALID_EVENT != hEventWaitConnected)
		{
			m_hEventWaitConnected = WSA_INVALID_EVENT;
			if (!WSACloseEvent(hEventWaitConnected))
			{
				printSockErr("WSACloseEvent");
				return false;
			}
		}

		return true;
	}
};
