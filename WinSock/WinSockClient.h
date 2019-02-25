
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

		CWinSockClient(SOCKET sock, E_SockConnStatus eStatus = E_SockConnStatus::SCS_None)
			: CWinSock(sock, eStatus)
		{
		}

	private:
		WSAEVENT m_hEventWaitConnected = WSA_INVALID_EVENT;

	public:
		E_WinSockResult connect(const char *pszServerAddr, UINT uServerPort, bool bWait=false, DWORD dwTimeout=0);

		E_WinSockResult checkConnected(DWORD dwTimeout=0);

		E_WinSockResult waitConnected(DWORD dwTimeout);

		bool close(bool bLinger = false, int lingerTime = 0);
	};
};
