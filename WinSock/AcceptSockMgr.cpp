
#include "stdafx.h"

#include "AcceptSockMgr.h"

namespace NS_WinSock
{
	void CAcceptSockMgr::init(const FN_AcceptSockMgrCB& fnOnAccepted, const FN_AcceptSockMgrCB& fnShuntdown)
	{
		m_thrNewAccept = thread([=] {
			while (!m_bShutdown)
			{
				m_evNewAccept.wait();

				if (m_bShutdown)
				{
					break;
				}

				list<CWinSock*> lstNewAccept;
				m_mtxNewAccept.lock();
				if (m_lstNewAccept.empty())
				{
					m_mtxNewAccept.unlock();
					continue;
				}
				lstNewAccept.swap(m_lstNewAccept);
				m_mtxNewAccept.unlock();

				list<CWinSock*> lstAcceptFail;
				for (auto itr = lstNewAccept.begin(); itr != lstNewAccept.end(); )
				{
					if (!fnOnAccepted(*this, **itr))
					{
						lstAcceptFail.push_back(*itr);

						itr = lstNewAccept.erase(itr);
						continue;
					}

					itr++;
				}

				if (!lstAcceptFail.empty())
				{
					this->recycle(lstAcceptFail);
				}

				if (!lstNewAccept.empty())
				{
					lock_guard<mutex> lock(m_mtxAccept);
					m_lstAcceptSock.insert(m_lstAcceptSock.end(), lstNewAccept.begin(), lstNewAccept.end());
					
					m_sum.uCurrConnCount = (UINT)m_lstAcceptSock.size();
					m_sum.uHistoryConnSum += (UINT)lstNewAccept.size();
				}
			}
		});

		m_thrNewRecycle = thread([=] {
			while (!m_bShutdown)
			{
				m_evNewRecycle.wait();

				if (m_bShutdown)
				{
					break;
				}

				list<CWinSock*> lstRecycle;
				m_mtxNewRecycle.lock();
				if (m_lstNewRecycle.empty())
				{
					m_mtxNewRecycle.unlock();
					continue;
				}
				lstRecycle.swap(m_lstNewRecycle);
				m_mtxNewRecycle.unlock();

				this->remove(lstRecycle);

				list<CWinSock*> lstDelete;
				for (auto itr = lstRecycle.begin(); itr != lstRecycle.end(); )
				{
					if (!fnShuntdown(*this, **itr))
					{
						lstDelete.push_back(*itr);
						itr = lstRecycle.erase(itr);
						continue;
					}

					itr++;
				}
				this->recycle(lstRecycle);
				for (auto pWinSock : lstDelete)
				{
					delete pWinSock;
				}
			}
		});
	}

	void CAcceptSockMgr::newAccept(CWinSock *pWinSock)
	{
		m_mtxNewAccept.lock();
		m_lstNewAccept.push_back(pWinSock);
		m_mtxNewAccept.unlock();

		m_evNewAccept.notify();
	}

	void CAcceptSockMgr::newRecycle(CWinSock *pWinSock)
	{
		m_mtxNewRecycle.lock();
		m_lstNewRecycle.push_back(pWinSock);
		m_mtxNewRecycle.unlock();

		m_evNewRecycle.notify();
	}

	void CAcceptSockMgr::remove(const list<CWinSock*>& lstRemove)
	{
		if (!lstRemove.empty())
		{
			lock_guard<mutex> lock(m_mtxAccept);
			for (auto pRemove : lstRemove)
			{
				auto itr = std::find(m_lstAcceptSock.begin(), m_lstAcceptSock.end(), pRemove);
				if (itr != m_lstAcceptSock.end())
				{
					(void)m_lstAcceptSock.erase(itr);
				}
			}
			m_sum.uCurrConnCount = (UINT)m_lstAcceptSock.size();
		}
	}

	void CAcceptSockMgr::recycle(const list<CWinSock*>& lstRecycle)
	{
		if (!lstRecycle.empty())
		{
			lock_guard<mutex> lock(m_mtxRecycle);
			m_lstRecycleSock.insert(m_lstRecycleSock.end(), lstRecycle.begin(), lstRecycle.end());
			m_sum.uRecycleCount = (UINT)m_lstRecycleSock.size();
			m_sum.uHistoryRecycleSum += (UINT)lstRecycle.size();
		}
	}

	void CAcceptSockMgr::enumerate(const function<bool(CWinSock& socClient)>& fnCB)
	{
		lock_guard<mutex> lock(m_mtxAccept);
		for (auto pAcceptSock : m_lstAcceptSock)
		{
			if (!fnCB(*pAcceptSock))
			{
				break;
			}
		}
	}

	void CAcceptSockMgr::shutdown()
	{
		m_bShutdown = true;

		m_evNewAccept.notify();
		m_evNewRecycle.notify();
		m_thrNewAccept.join();
		m_thrNewRecycle.join();

		m_mtxAccept.lock();
		list<CWinSock*> lstAcceptSock;
		lstAcceptSock.swap(m_lstAcceptSock);
		m_mtxAccept.unlock();
		for (auto pAcceptSock : lstAcceptSock)
		{
			(void)pAcceptSock->close();
			delete pAcceptSock;
		}

		m_mtxRecycle.lock();
		list<CWinSock*> lstRecycleSock;
		lstRecycleSock.swap(m_lstRecycleSock);
		m_mtxRecycle.unlock();
		for (auto pWinSock : lstRecycleSock)
		{
			(void)pWinSock->close();
			delete pWinSock;
		}
	}

	size_t CAcceptSockMgr::getCount()
	{
		lock_guard<mutex> lock(m_mtxAccept);

		return m_lstAcceptSock.size();
	}

	UINT CAcceptSockMgr::fetchRecycle(UINT uCount, const function<void(CWinSock& socClient)>& fnCB)
	{
		lock_guard<mutex> lock(m_mtxRecycle);

		UINT uRet = 0;
		for (auto itr = m_lstRecycleSock.begin(); itr != m_lstRecycleSock.end(); )
		{
			fnCB(**itr);
			
			itr = m_lstRecycleSock.erase(itr);
			
			uRet++;

			if (uCount <= 1)
			{
				break;
			}
			uCount--;
		}

		m_sum.uRecycleCount = (UINT)m_lstRecycleSock.size();

		return uRet;
	}

	UINT CAcceptSockMgr::addMsg(const char* lpMsg, UINT uLen)
	{
		vector<char> vecRecvData(uLen);
		memcpy(&vecRecvData[0], lpMsg, uLen);

		lock_guard<mutex> lock(m_mtxMsg);
		if (m_lstRecvMsg.size() >= m_uMaxMsgCount)
		{
			m_lstRecvMsg.pop_front();
		}

		m_lstRecvMsg.push_back(vecRecvData);

		m_sum.uHistoryMsgSum++;

		return (UINT)m_lstRecvMsg.size();
	}
};
