
#include "stdafx.h"

#include "IOCP.h"

#include "WinSock.h"

static const ULONG_PTR __IOCPShutdownSign = (ULONG_PTR)-1;

using VEC_OVERLAPPED_ENTRY = vector<OVERLAPPED_ENTRY>;

namespace NS_WinSock
{
	bool checkNTStatus(ULONG_PTR Internal)
	{
		if (STATUS_SUCCESS != Internal)
		{
			if (STATUS_CANCELLED != Internal && STATUS_CONNECTION_ABORTED != Internal
				&& STATUS_CONNECTION_RESET != Internal && STATUS_REMOTE_DISCONNECT != Internal)
			{
				CWinSock::printSockErr("checkNTStatus", (int)Internal);
			}

			return false;
		}

		return true;
	}

	static void _processCallback(ICPCallback *pIOCPHandler, VEC_OVERLAPPED_ENTRY& vecOverlappedEntry)
	{
		if (NULL != pIOCPHandler)
		{
			pIOCPHandler->handleCPCallback(&vecOverlappedEntry.front(), (ULONG)vecOverlappedEntry.size());
		}
		else
		{
			for (auto& overLappedEntry : vecOverlappedEntry)
			{
				auto& perIOData = (tagPerIOData&)*overLappedEntry.lpOverlapped;
				perIOData.iocpHandler.handleCPCallback(&overLappedEntry, 1);
			}
		}
	}

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

	bool CIOCP::poolBind(SOCKET sock)
	{
		LPOVERLAPPED_COMPLETION_ROUTINE fnPoolCB = [](
			DWORD dwErrorCode,
			DWORD dwNumberOfBytesTransfered,
			LPOVERLAPPED lpOverlapped) {

			if (NULL == lpOverlapped)
			{
				CWinSock::printSockErr("fnPoolCB", ::GetLastError());
				return;
			}

			OVERLAPPED_ENTRY overlappedEntry{ 0, lpOverlapped, dwErrorCode, dwNumberOfBytesTransfered };
			VEC_OVERLAPPED_ENTRY vecOverlappedEntry(1, overlappedEntry);
			_processCallback(NULL, vecOverlappedEntry);
		};

		(void)::BindIoCompletionCallback((HANDLE)sock, fnPoolCB, 0);
		//if (!::BindIoCompletionCallback((HANDLE)sock, fnPoolCB, 0))
		//{
		//	CWinSock::printSockErr("BindIoCompletionCallback", GetLastError());
		//	return false;
		//}
		
		return true;
	}

	bool CIOCP::create(UINT uThreadCount, UINT uNumQuery, SOCKET sock)
	{
		m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
		if (INVALID_HANDLE_VALUE == m_hCompletionPort)
		{
			CWinSock::printSockErr("CreateIoCompletionPort", GetLastError());
			return false;
		}

		if (INVALID_SOCKET != sock)
		{
			if (!bind(sock))
			{
				return false;
			}
		}

		m_uNumQuery = uNumQuery;
		do {
			m_vecThread.push_back((HANDLE)CUtil::beginWin32Thread(_iocpThread, this));
		} while (0 < uThreadCount && 0 < --uThreadCount);

		return true;
	}

	bool CIOCP::bind(SOCKET sock)
	{
		HANDLE hRet = CreateIoCompletionPort((HANDLE)sock, m_hCompletionPort, sock, 0);
		if (hRet != m_hCompletionPort)
		{
			CWinSock::printSockErr("CreateIoCompletionPort", GetLastError());
			return false;
		}

		return true;
	}

	bool CIOCP::_queryIOCP()
	{
		if (1 < m_uNumQuery)
		{
			vector<OVERLAPPED_ENTRY> vec(m_uNumQuery);
			ULONG ulNumEntriesRemoved = 0;
			if (!GetQueuedCompletionStatusEx(m_hCompletionPort, &vec.front()
				, (ULONG)vec.size(), &ulNumEntriesRemoved, INFINITE, FALSE))
			{
				CWinSock::printSockErr("GetQueuedCompletionStatusEx", GetLastError());
			}

			bool bShutdown = false;
			
			VEC_OVERLAPPED_ENTRY vecOverlappedEntry;
			for (auto& overlappedEntry : vec)
			{
				if (0 == ulNumEntriesRemoved)
				{
					break;
				}
				ulNumEntriesRemoved--;

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

				vecOverlappedEntry.push_back(overlappedEntry);
			}

			if (!vecOverlappedEntry.empty())
			{
				_processCallback(m_pIOCPHandler, vecOverlappedEntry);
			}

			if (bShutdown)
			{
				return false;
			}
		}
		else
		{
			DWORD dwNumberOfBytesTransferred = 0;
			ULONG_PTR lpCompletionKey = 0;
			LPOVERLAPPED lpOverlapped = NULL;
			if (GetQueuedCompletionStatus(m_hCompletionPort, &dwNumberOfBytesTransferred
				, &lpCompletionKey, &lpOverlapped, INFINITE))
			{
				if (__IOCPShutdownSign == lpCompletionKey)
				{
					return false;
				}

				if (NULL != lpOverlapped)
				{
					OVERLAPPED_ENTRY overlappedEntry{ lpCompletionKey, lpOverlapped, 0, dwNumberOfBytesTransferred };
					VEC_OVERLAPPED_ENTRY lstOverlappedEntry(1, overlappedEntry);
					_processCallback(m_pIOCPHandler, lstOverlappedEntry);
				}
				else
				{
					CWinSock::printSockErr("GetQueuedCompletionStatus", GetLastError());
				}
			}
			else
			{
				DWORD dwErr = GetLastError();
				if (ERROR_OPERATION_ABORTED != dwErr)
				{
					CWinSock::printSockErr("GetQueuedCompletionStatus", dwErr);
				}
			}
		}

		return true;
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
};
