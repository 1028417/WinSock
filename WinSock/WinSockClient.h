
#pragma once

#include "WinSock.h"

namespace NS_WinSock
{
	class __WinSockExt CWinSockClient : public CWinSock
	{
	public:
		CWinSockClient()
		{
		}

		CWinSockClient(SOCKET sock, bool bConnected=true)
			: CWinSock(sock, bConnected)
		{
		}

		CWinSockClient(SOCKET sock, E_SockConnStatus eConnStatus)
			: CWinSock(sock, eConnStatus)
		{
		}

	private:
		WSAEVENT m_hEventWaitConnected = WSA_INVALID_EVENT;

	private:
		E_WinSockResult _connect(const sockaddr_in& addr);

	public:
		E_WinSockResult connect(const char *pszServerAddr, UINT uServerPort, bool bWait=false, DWORD dwTimeout=0);

		E_WinSockResult checkConnected(DWORD dwTimeout=0);

		E_WinSockResult waitConnected(DWORD dwTimeout);

		bool close(bool bLinger = false, int lingerTime = 0);
	};
};
