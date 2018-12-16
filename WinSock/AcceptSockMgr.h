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

		UINT uHistoryMsgSum = 0;
	};

	class CAcceptSockMgr;
	using FN_AcceptSockMgrCB = function<bool(CAcceptSockMgr& AcceptSockMgr, CWinSock& WinSock)>;

	class CAcceptSockMgr
	{
	public:
		CAcceptSockMgr()
		{
			m_uMaxMsgCount = 100;
		}

	private:
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

		mutex m_mtxMsg;
		list<vector<char>> m_lstRecvMsg;

		UINT m_uMaxMsgCount = 0;

	public:
		tagAcceptSockSum m_sum;

	public:
		void init(const FN_AcceptSockMgrCB& fnOnAccepted, const FN_AcceptSockMgrCB& fnShuntdown);

		void newAccept(CWinSock *pWinSock);

		void newRecycle(CWinSock *pWinSock);

		void enumerate(const function<bool(CWinSock& socClient)>& fnCB);

		void shutdown();

		size_t getCount();

		UINT fetchRecycle(UINT uCount, const function<void(CWinSock& socClient)>& fnCB);

		UINT addMsg(const char* lpMsg, UINT uLen);

	private:
		void remove(const list<CWinSock*>& lstRemov);
		void recycle(const list<CWinSock*>& lstRecycle);
	};
};
