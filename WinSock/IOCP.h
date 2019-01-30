#pragma once

#include <mutex>

using namespace std;

#include <WinSock2.h>

#include <mswsock.h>

#include "PerIOData.h"

#define STATUS_SUCCESS 0
#define STATUS_CANCELLED 0xC0000120
#define STATUS_CONNECTION_ABORTED 0xC0000241
#define STATUS_CONNECTION_RESET 0xC000020D
#define STATUS_REMOTE_DISCONNECT 0xC000013C

namespace NS_WinSock
{
	bool checkNTStatus(ULONG_PTR Internal);

	class ICPCallback
	{
	public:
		ICPCallback() {}

		virtual void handleCPCallback(OVERLAPPED_ENTRY *lpOverlappedEntry, ULONG ulNumEntries) = 0;
	};

	class CIOCP
	{
		friend void _iocpThread(LPVOID pPara);
		friend bool CIOCP::bind(SOCKET sock, CIOCP *pIOCP);

	public:
		CIOCP(ICPCallback *pIOCPHandler = NULL)
			: m_pIOCPHandler(pIOCPHandler)
		{
		}

	private:
		ICPCallback *m_pIOCPHandler = NULL;

		HANDLE m_hCompletionPort = INVALID_HANDLE_VALUE;

		UINT m_uNumQuery = 0;

		vector<HANDLE> m_vecThread;

	private:
		bool _queryIOCP();

	public:
		static bool poolBind(SOCKET sock);

		bool create(UINT uThreadCount, UINT uNumQuery, SOCKET sock = INVALID_SOCKET);

		bool bind(SOCKET sock);

		bool shutdown();
	};
};
