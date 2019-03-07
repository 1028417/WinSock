#pragma once

#include "WinSock.h"

namespace NS_WinSock
{
	struct tagAcceptSockSum
	{
		UINT uCurrConnCount = 0;
		UINT uHistoryConnSum = 0;

		UINT uRecycleCount = 0;
		UINT uHistoryRecycleSum = 0;
	};

	using CB_Accept = function<bool(CWinSock&)>;
	
	class CAcceptSockMgr
	{
	public:
		CAcceptSockMgr()
			: m_evNewAccept(FALSE)
			, m_evNewRecycle(FALSE)
		{
			m_uMaxMsgCount = 100;
		}

	private:
		CB_Accept m_cbAccept;
		CB_Recv m_cbRecv;
		CB_PeerDisconnect m_cbPeerDisconnect;

		bool m_bShutdown = false;

		thread m_thrNewAccept;
		CWinEvent m_evNewAccept;
		mutex m_mtxNewAccept;
		list<CWinSock*> m_lstNewAccept;

		thread m_thrNewRecycle;
		CWinEvent m_evNewRecycle;
		mutex m_mtxNewRecycle;
		list<CWinSock*> m_lstNewRecycle;

		mutex m_mtxAccept;
		list<CWinSock*> m_lstAcceptSock;

		mutex m_mtxRecycle;
		list<CWinSock*> m_lstRecycleSock;

		UINT m_uMaxMsgCount = 0;

	public:
		tagAcceptSockSum m_sum;

	public:
		void init(const CB_Accept& cbAccept, const CB_Recv& cbRecv, const CB_PeerDisconnect& cbPeerDisconnect=NULL);

		void accept(CWinSock *pWinSock);

		void enumerate(const function<bool(CWinSock& socClient)>& cb);
		
		UINT fetchRecycle(UINT uCount, const function<void(CWinSock& socClient)>& cb);

		void shutdown();

	private:
		void _acceptLoop();
		bool _accept(CWinSock& WinSock);

		void _recycleLoop();
		void _recycle(const list<CWinSock*>& lstRecycle);
	};
};
