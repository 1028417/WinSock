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
		
		virtual void handleCPCallback(ULONG_PTR Internal, tagPerIOData& perIOData, DWORD dwNumberOfBytesTransferred) = 0;
	};


	class CIOCP
	{
		friend void _iocpThread(LPVOID pPara);

	public:
		CIOCP()
		{
		}

	protected:
		HANDLE m_hCompletionPort = INVALID_HANDLE_VALUE;

	private:
		ULONG m_uNumQuery = 1;

		vector<HANDLE> m_vecThread;

	private:
		bool _queryIOCP();

	protected:
		virtual void handleCPCallback(ULONG_PTR Internal, OVERLAPPED& overlapped, DWORD dwNumberOfBytesTransferred, ULONG_PTR lpCompletionKey);

	public:
		bool create(UINT uThreadCount, ULONG uNumQuery);

		bool bind(class CWinSock& sock);

		bool shutdown();
	};

	using CB_IOCP = function<void(SOCKET sock, ULONG_PTR Internal, OVERLAPPED& overlapped, DWORD dwNumberOfBytesTransferred)>;

	class CIOCPEx : public CIOCP
	{
	public:
		CIOCPEx()
		{
		}

	private:
		CB_IOCP m_cb;

	public:
		bool create(UINT uThreadCount, ULONG uNumQuery, const CB_IOCP& cb);

		bool bind(SOCKET sock);

	private:
		void handleCPCallback(ULONG_PTR Internal, OVERLAPPED& overlapped, DWORD dwNumberOfBytesTransferred, ULONG_PTR lpCompletionKey) override;
	};
};
