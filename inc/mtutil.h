#pragma once

#pragma warning(disable: 4275)

#include <mutex>
using namespace std;

namespace NS_mtutil
{
	//QueueUserWorkItem(Function, Context, WT_EXECUTEDEFAULT))
	
	class CCSLock
	{
	public:
		CCSLock()
		{
			InitializeCriticalSection(&m_cs);
		}

		~CCSLock()
		{
			DeleteCriticalSection(&m_cs);
		}

	public:
		void lock()
		{
			EnterCriticalSection(&m_cs);
		}

		void unlock()
		{
			LeaveCriticalSection(&m_cs);
		}

	private:
		CRITICAL_SECTION m_cs;
	};

	class CWinEvent
	{
	public:
		CWinEvent(BOOL bManualReset)
		{
			m_hEvent = ::CreateEvent(NULL, bManualReset, FALSE, NULL);
		}

		~CWinEvent()
		{
			(void)::CloseHandle(m_hEvent);
		}

		bool check()
		{
			return WAIT_OBJECT_0 == ::WaitForSingleObject(m_hEvent, 0);
		}

		bool wait(DWORD dwTimeout = INFINITE)
		{
			return WAIT_OBJECT_0 == ::WaitForSingleObject(m_hEvent, dwTimeout);
		}

		bool notify()
		{
			return TRUE==SetEvent(m_hEvent);
		}

		bool reset()
		{
			return TRUE == ResetEvent(m_hEvent);
		}

	private:
		HANDLE m_hEvent = INVALID_HANDLE_VALUE;
	};

	class CCASLock
	{
	public:
		CCASLock()
		{
		}
		
	private:
		char m_lockFlag = 0;

	private:
#pragma intrinsic(_InterlockedCompareExchange8, _InterlockedExchange8)
		bool _lock(UINT uRetryTimes=0, UINT uSleepTime=0)
		{
			while (_InterlockedCompareExchange8(&m_lockFlag, 1, 0))
			{
				if (0 != uRetryTimes && 0 == --uRetryTimes)
				{
					return false;
				}

				if (0 == uSleepTime)
				{
					this_thread::yield();
				}
				else
				{
					::Sleep(uSleepTime);
				}
			}

			return true;
		}

	public:
		bool try_lock(UINT uRetryTimes=1)
		{
			return _lock(uRetryTimes, 0);
		}

		void lock(UINT uSleepTime = 0)
		{
			_lock(0, uSleepTime);
		}

		void unlock()
		{
			//(void)_InterlockedExchange8(&m_lockFlag, 0);
			m_lockFlag = 0;
		}
	};

	class CCondVar : public condition_variable
	{
	public:
		CCondVar()
		{
		}

		void wait()
		{
			std::unique_lock<mutex> lock(m_mtx);
			__super::wait(lock);
		}

		void notify()
		{
			std::unique_lock<mutex> lock(m_mtx);
			__super::notify_all();
		}

	private:
		mutex m_mtx;
	};
}
