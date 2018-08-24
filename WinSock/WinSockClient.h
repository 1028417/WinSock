
#pragma once

#include "WinSock.h"

namespace NS_WinSock
{
	class CWinSockClient : public CWinSock
	{
	public:
		CWinSockClient()
		{
			memset(&m_addrServer, 0, sizeof m_addrServer);
		}

	private:
		sockaddr_in m_addrServer;

		WSAEVENT m_hEventWaitConnected = WSA_INVALID_EVENT;

	public:
		bool create(const char *pszServerAddr, UINT uServerPort, bool bOverLapped, bool bNoBlock);

		E_WinSockResult connect(DWORD dwTimeout=0);

		E_WinSockResult checkConnected(DWORD dwTimeout=0);

		E_WinSockResult waitConnected(DWORD dwTimeout);

		bool close(bool bLinger = false, int lingerTime = 0);
	};
};
