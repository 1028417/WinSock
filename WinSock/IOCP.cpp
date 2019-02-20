
#include <process.h>

#include "IOCP.h"

#include "WinSock.h"

static const ULONG_PTR __IOCPShutdownSign = (ULONG_PTR)-1;

using VEC_OVERLAPPED_ENTRY = vector<OVERLAPPED_ENTRY>;

namespace NS_WinSock
{
	static void _iocpThread(LPVOID pPara)
	{
		CIOCP *pIOCP = (CIOCP*)pPara;
		if (NULL == pIOCP)
		{
			return;
		}

		while (true)
		{
			if (!pIOCP->_queryIOCP())
			{
				break;
			}
		}
	};

	bool CIOCP::create(UINT uThreadCount, ULONG uNumQuery)
	{
		m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
		if (INVALID_HANDLE_VALUE == m_hCompletionPort)
		{
			CWinSock::printSockErr("CreateIoCompletionPort", GetLastError());
			return false;
		}
		
		m_uNumQuery = uNumQuery;
		if (0 == m_uNumQuery)
		{
			m_uNumQuery = 1;
		}

		do {
			m_vecThread.push_back((HANDLE)::_beginthread(_iocpThread, 0, this));
		} while (0 < uThreadCount && 0 < --uThreadCount);

		return true;
	}
	
	bool CIOCP::bind(CWinSock& sock)
	{
		HANDLE hRet = CreateIoCompletionPort((HANDLE)sock.getSockHandle(), m_hCompletionPort, 0, 0);
		if (hRet != m_hCompletionPort)
		{
			CWinSock::printSockErr("CreateIoCompletionPort", GetLastError());
			return false;
		}

		return true;
	}
	
	bool CIOCP::_queryIOCP()
	{
		vector<OVERLAPPED_ENTRY> vecOverlappedEntry(m_uNumQuery);
		ULONG ulNumEntriesRemoved = 0;
		if (!GetQueuedCompletionStatusEx(m_hCompletionPort, &vecOverlappedEntry.front()
			, m_uNumQuery, &ulNumEntriesRemoved, INFINITE, FALSE))
		{
			CWinSock::printSockErr("GetQueuedCompletionStatusEx", GetLastError());
		}

		bool bShutdown = false;
		
		for (UINT uIdx = 0; uIdx < ulNumEntriesRemoved && uIdx < vecOverlappedEntry.size(); uIdx++)
		{
			auto& overlappedEntry = vecOverlappedEntry[uIdx];

			if (__IOCPShutdownSign == overlappedEntry.lpCompletionKey)
			{
				bShutdown = true;
				continue;
			}

			if (NULL == overlappedEntry.lpOverlapped)
			{
				CWinSock::printSockErr("GetQueuedCompletionStatusEx", GetLastError());
				continue;
			}

			handleCPCallback(overlappedEntry.Internal, *overlappedEntry.lpOverlapped, overlappedEntry.dwNumberOfBytesTransferred, overlappedEntry.lpCompletionKey);
		}

		if (bShutdown)
		{
			return false;
		}

		return true;
	}

	void CIOCP::handleCPCallback(ULONG_PTR Internal, OVERLAPPED& overlapped, DWORD dwNumberOfBytesTransferred, ULONG_PTR lpCompletionKey)
	{
		auto& perIOData = (tagPerIOData&)overlapped;
		perIOData.iocpHandler.handleCPCallback(Internal, perIOData, dwNumberOfBytesTransferred);
	}

	bool CIOCP::shutdown()
	{
		while (!m_vecThread.empty())
		{
			if (!::PostQueuedCompletionStatus(m_hCompletionPort, 0, __IOCPShutdownSign, NULL))
			{
				return false;
			}

			DWORD dwRet = ::WaitForMultipleObjects((DWORD)m_vecThread.size(), &m_vecThread.front(), FALSE, INFINITE);
			if (WAIT_FAILED == dwRet)
			{
				return false;
			}
			//WAIT_TIMEOUT

			auto itr = m_vecThread.begin() + (dwRet - WAIT_OBJECT_0);
			(void)m_vecThread.erase(itr);
		}

		if (INVALID_HANDLE_VALUE != m_hCompletionPort)
		{
			(void)CloseHandle(m_hCompletionPort);
			m_hCompletionPort = INVALID_HANDLE_VALUE;
		}

		return true;
	}


	//bool CIOCPEx::create(UINT uThreadCount, ULONG uNumQuery, const CB_IOCP& cb)
	//{
	//	m_cb = cb;

	//	return __super::create(uThreadCount, uNumQuery);
	//}

	//bool CIOCPEx::bind(SOCKET sock, void *lpCompletionKey)
	//{
	//	HANDLE hRet = CreateIoCompletionPort((HANDLE)sock, m_hCompletionPort, (ULONG_PTR)lpCompletionKey, 0);
	//	if (hRet != m_hCompletionPort)
	//	{
	//		CWinSock::printSockErr("CreateIoCompletionPort", GetLastError());
	//		return false;
	//	}

	//	return true;
	//}

	//void CIOCPEx::handleCPCallback(ULONG_PTR Internal, OVERLAPPED& overlapped, DWORD dwNumberOfBytesTransferred, ULONG_PTR lpCompletionKey)
	//{
	//	m_cb(Internal, overlapped, dwNumberOfBytesTransferred, (void*)lpCompletionKey);
	//}
};
