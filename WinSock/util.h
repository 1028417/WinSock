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
	class CUtil
	{
	public:
		static void* beginWin32Thread(void(*fnCB)(void* pPara), void* pPara = NULL)
		{
			return (void*)::_beginthread(fnCB, 0, pPara);
		}

		static bool queueUserWorkItem(LPTHREAD_START_ROUTINE Function, PVOID Context = NULL)
		{
			if (!QueueUserWorkItem(Function, Context, WT_EXECUTEDEFAULT))
			{
				return false;
			}

			return true;
		}
	};

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

	struct tagClock
	{
		tagClock()
		{
			time = clock();
		}

		void printTime(const string& strInfo)
		{
			CConsole::inst().printEx([&](ostream& out) {
				clock_t ptrTime = time;
				time = clock();
				out << strInfo.c_str() << ", tickCount:" << time - ptrTime;
			});
		}

		void printTime(const function<void(ostream& out)>& fnCB)
		{
			CConsole::inst().printEx([&](ostream& out) {
				fnCB(out);

				clock_t ptrTime = time;
				time = clock();
				out << ", tickCount:" << time - ptrTime;
			});
		}

		clock_t time = 0;
	};

	class CWinEvent
	{
	public:
		CWinEvent()
		{
			m_hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		}

	private:
		HANDLE m_hEvent = INVALID_HANDLE_VALUE;

	public:
		bool wait(DWORD dwTimeout = INFINITE)
		{
			return WAIT_OBJECT_0 == ::WaitForSingleObject(m_hEvent, dwTimeout);
		}

		bool notify()
		{
			return TRUE==SetEvent(m_hEvent);
		}
	};

	class CAtomicLock
	{
	public:
		CAtomicLock()
		{
		}

	private:
		char m_cLock = 0;
		char *m_pcLock = &m_cLock;

		//UINT m_uFlag = 100;

	public:
	//#pragma intrinsic(_InterlockedCompareExchange8, _InterlockedExchange8)
		void lock()
		{
			//UINT uCounter = m_uFlag;
			while (_InterlockedCompareExchange8(m_pcLock, 1, 0))
			{
				//if (0 == uCounter)
				//{
				//	::Sleep(1);
				//	uCounter = m_uFlag;
				//}
				//else
				//{
				//	uCounter--;
				//}
			}

			//return false;
		}

		void forceUnlock()
		{
			m_cLock = 0;
		}

		//bool unlock()
		//{
		//	return 1 == _InterlockedExchange8(m_pcLock, 0);
		//}
		void unlock()
		{
			(void)_InterlockedExchange8(m_pcLock, 0);
		}
	};

	class CCASLock
	{
	public:
		CCASLock()
		{
		}

	private:
		char m_cLock = 0;

	public:
		void lock()
		{
			char *pcLock = &m_cLock;
			while (_InterlockedCompareExchange8(pcLock, 1, 0))
			{
				(void)::Sleep(3);
			}
		}

		void unlock()
		{
			m_cLock = 0;
		}
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
