
#include "../inc/AcceptSockMgr.h"

namespace NS_WinSock
{
	void CAcceptSockMgr::init(const CB_Accept& cbAccept, const CB_Recv& cbRecv, const CB_PeerDisconnect& cbPeerDisconnect)
	{
		m_cbAccept = cbAccept;
		m_cbRecv = cbRecv;
		m_cbPeerDisconnect = cbPeerDisconnect;

		m_thrNewAccept = thread([&] {
			_acceptLoop();
		});

		m_thrNewRecycle = thread([&] {
			_recycleLoop();
		});
	}

	void CAcceptSockMgr::_acceptLoop()
	{
		while (true)
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
				if (!_accept(**itr))
				{
					lstAcceptFail.push_back(*itr);

					itr = lstNewAccept.erase(itr);
					continue;
				}

				itr++;
			}

			if (!lstNewAccept.empty())
			{
				lock_guard<mutex> lock(m_mtxAccept);
				m_lstAcceptSock.insert(m_lstAcceptSock.end(), lstNewAccept.begin(), lstNewAccept.end());

				m_sum.uCurrConnCount = (UINT)m_lstAcceptSock.size();
				m_sum.uHistoryConnSum += (UINT)lstNewAccept.size();
			}

			if (!lstAcceptFail.empty())
			{
				this->_recycle(lstAcceptFail);
			}
		}
	}

	bool CAcceptSockMgr::_accept(CWinSock& WinSock)
	{
		//GetAcceptExSockaddrs(

		if (m_cbAccept)
		{
			if (!m_cbAccept(WinSock))
			{
				return false;
			}
		}

		//if (!WinSock.setOpt(SO_UPDATE_ACCEPT_CONTEXT, &m_sock, sizeof SOCKET))
		//{
		//	return false;
		//}

		if (E_WinSockResult::WSR_OK != WinSock.initAsync())
		{
			return false;
		}

		if (E_WinSockResult::WSR_OK != WinSock.asyncReceive(m_cbRecv, [&](CWinSock& WinSock) {
			m_mtxNewRecycle.lock();
			m_lstNewRecycle.push_back(&WinSock);
			m_mtxNewRecycle.unlock();

			m_evNewRecycle.notify();
		}))
		{
			return false;
		}

		return true;
	}

	void CAcceptSockMgr::_recycleLoop()
	{
		while (true)
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

			m_mtxAccept.lock();
			for (auto pRemove : lstRecycle)
			{
				auto itr = std::find(m_lstAcceptSock.begin(), m_lstAcceptSock.end(), pRemove);
				if (itr != m_lstAcceptSock.end())
				{
					(void)m_lstAcceptSock.erase(itr);
				}
			}
			m_sum.uCurrConnCount = (UINT)m_lstAcceptSock.size();
			m_mtxAccept.unlock();

			list<CWinSock*> lstDelete;
			for (auto itr = lstRecycle.begin(); itr != lstRecycle.end(); )
			{
				auto pWinSock = *itr;

				if (m_cbPeerDisconnect)
				{
					m_cbPeerDisconnect(*pWinSock);
				}

				if (!pWinSock->disconnect())
				{
					pWinSock->close();

					lstDelete.push_back(pWinSock);

					itr = lstRecycle.erase(itr);

					continue;
				}

				itr++;
			}

			if (!lstRecycle.empty())
			{
				this->_recycle(lstRecycle);
			}

			for (auto pWinSock : lstDelete)
			{
				delete pWinSock;
			}
		}
	}

	void CAcceptSockMgr::_recycle(const list<CWinSock*>& lstRecycle)
	{
		if (!lstRecycle.empty())
		{
			lock_guard<mutex> lock(m_mtxRecycle);
			m_lstRecycleSock.insert(m_lstRecycleSock.end(), lstRecycle.begin(), lstRecycle.end());
			m_sum.uRecycleCount = (UINT)m_lstRecycleSock.size();
			m_sum.uHistoryRecycleSum += (UINT)lstRecycle.size();
		}
	}

	void CAcceptSockMgr::accept(CWinSock *pWinSock)
	{
		m_mtxNewAccept.lock();
		m_lstNewAccept.push_back(pWinSock);
		m_mtxNewAccept.unlock();

		m_evNewAccept.notify();
	}

	void CAcceptSockMgr::enumerate(const function<bool(CWinSock& socClient)>& cb)
	{
		lock_guard<mutex> lock(m_mtxAccept);
		for (auto pAcceptSock : m_lstAcceptSock)
		{
			if (!cb(*pAcceptSock))
			{
				break;
			}
		}
	}

	UINT CAcceptSockMgr::fetchRecycle(UINT uCount, const function<void(CWinSock& socClient)>& cb)
	{
		lock_guard<mutex> lock(m_mtxRecycle);

		UINT uRet = 0;
		for (auto itr = m_lstRecycleSock.begin(); itr != m_lstRecycleSock.end(); )
		{
			cb(**itr);
			
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
};
