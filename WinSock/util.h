#pragma once

#include <iostream>
#include <functional>

#include <string>
#include <list>
#include <vector>
#include <map>
#include <algorithm>

#include <mutex>
#include <process.h>
#include <atomic>

using namespace std;

#include <WinSock2.h>

#include <mswsock.h>

#include <winapifamily.h>

namespace NS_WinSock
{
	//QueueUserWorkItem(Function, Context, WT_EXECUTEDEFAULT))

	class CConsole
	{
	public:
		static CConsole& inst()
		{
			static CConsole inst;
			return inst;
		}

	private:
		CConsole()
		{
		}

		mutex m_mtx;

	public:
		template <UINT _scanfSize = 256>
		void waitInput(bool bBlock, const function<bool(const string& strInput)>& fnCB)
		{
			auto fnLoop = [&] {
				char lpInput[_scanfSize + 1]{ 0 };

				while (true)
				{
					if (1 == scanf_s("%s", lpInput, _scanfSize))
					{
						if (!fnCB(lpInput))
						{
							break;
						}
					}
				}
			};

			if (bBlock)
			{
				fnLoop();
			}
			else
			{
				thread thr(fnLoop);
				thr.detach();
			}
		}

		template<typename T>
		void print(T content)
		{
			m_mtx.lock();

			cout << content << '\n';

			m_mtx.unlock();
		}

		void printEx(const function<void(ostream& out)>& fnCB)
		{
			m_mtx.lock();

			fnCB(cout);

			cout << '\n';

			m_mtx.unlock();
		}
	};

	struct tagTImer
	{
		string strOperateName;

		clock_t time = 0;

		tagTImer(const string& t_strOperateName)
			: strOperateName(t_strOperateName)
		{
			CConsole::inst().printEx([&](ostream& out) {
				out << strOperateName << "...";
			});

			time = clock();
		}

		void print(const string& strMsg="")
		{
			CConsole::inst().printEx([&](ostream& out) {
				clock_t ptrTime = time;
				time = clock();
				out << (!strMsg.empty()? strMsg:strOperateName) << " ok, tickCount:" << time - ptrTime;
			});
		}
	};

	class CWinEvent
	{
	public:
		CWinEvent()
		{
			m_hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		}

		bool wait(DWORD dwTimeout = INFINITE)
		{
			return WAIT_OBJECT_0 == ::WaitForSingleObject(m_hEvent, dwTimeout);
		}

		bool notify()
		{
			return TRUE==SetEvent(m_hEvent);
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

		#pragma intrinsic(_InterlockedCompareExchange8, _InterlockedExchange8)
		bool lock(UINT uRetryTimes=0, UINT uSleepTime=0)
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

		void unlock()
		{
			//m_lockFlag = 0;
			(void)_InterlockedExchange8(&m_lockFlag, 0);
		}

	private:
		char m_lockFlag = 0;
	};

	class CCharVector
	{
	public:
		CCharVector()
		{
		}

		CCharVector(size_t uSize, char* lpData = NULL)
			: m_vecData(uSize)
		{
		}

	private:
		vector<char> m_vecData;

	public:
		char* getPtr()
		{
			if (m_vecData.empty())
			{
				return NULL;
			}

			return &m_vecData.front();
		}

		size_t getSize()
		{
			return m_vecData.size();
		}

		void clear()
		{
			m_vecData.clear();
		}

		void swap(CCharVector& charVec)
		{
			charVec.m_vecData.swap(m_vecData);
		}

		char* assign(char* lpData, size_t uSize)
		{
			m_vecData.clear();

			return push_back(lpData, uSize);
		}

		char* push_back(char* lpData, size_t uSize)
		{
			if (NULL == lpData || 0 == uSize)
			{
				return NULL;
			}

			size_t uPreSize = m_vecData.size();
			m_vecData.resize(uPreSize + uSize);

			memcpy(&m_vecData.front() + uPreSize, lpData, uSize);

			return &m_vecData.front();
		}

		size_t pop_front(char *lpBuff, size_t uPopLen)
		{
			if (NULL == lpBuff || 0 == uPopLen)
			{
				return 0;
			}

			size_t uSize = m_vecData.size();
			if (0 == uSize)
			{
				return 0;
			}
			
			if (uSize <= uPopLen)
			{
				memcpy(lpBuff, &m_vecData.front(), uSize);
				m_vecData.clear();
				return uSize;
			}

			memcpy(lpBuff, &m_vecData.front(), uPopLen);

			size_t uNewSize = uSize - uPopLen;
			memcpy(&m_vecData.front(), &m_vecData.front() + uPopLen, uNewSize);
			m_vecData.resize(uNewSize);

			return uPopLen;
		}

		size_t pop_front(size_t uPopLen)
		{
			if (0 == uPopLen)
			{
				return 0;
			}

			size_t uSize = m_vecData.size();
			if (uSize <= uPopLen)
			{
				m_vecData.clear();
				return uSize;
			}
			
			size_t uNewSize = uSize - uPopLen;
			memcpy(&m_vecData.front(), &m_vecData.front() + uPopLen, uNewSize);
			m_vecData.resize(uNewSize);

			return uPopLen;
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
};
