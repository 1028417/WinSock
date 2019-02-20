#pragma once

#include "WinSock.h"

#include "AcceptSockMgr.h"

namespace NS_WinSock
{
	struct tagAcceptSockNode
	{
		tagAcceptSockNode(CWinSock *t_pWinSock)
			: pWinSock(t_pWinSock)
		{
		}

		CWinSock *pWinSock;

		tagAcceptSockNode *pNextNode = NULL;

		static tagAcceptSockNode* newNode();

		tagAcceptSockNode* addNextNode();

		tagAcceptSockNode* addNextNode(CWinSock *pWinSock);
	};

	struct tagAcceptSockNodeList
	{
		tagAcceptSockNode *pAcceptSockNode = NULL;

		UINT uFreeCount = 0;
		UINT uAccpSum = 0;

		UINT m_uIncr = 10;

		tagAcceptSockNodeList()
		{
		}

		~tagAcceptSockNodeList();

		CWinSock* initNodes(UINT uNum);
		CWinSock* createNewNodes(UINT uNum, CAcceptSockMgr& acceptSockMgr);

		CWinSock* getAcceptSock();

		CWinSock* forward(CAcceptSockMgr& acceptSockMgr);
	};

	struct tagAcceptSockList
	{
		list<CWinSock*> lstAcceptSock;

		UINT uFreeCount = 0;
		UINT uAccpSum = 0;

		UINT m_uIncr = 10;

		tagAcceptSockList()
		{
		}

		~tagAcceptSockList();

		CWinSock *initNodes(UINT uNum);
		CWinSock *createNewNodes(UINT uNum, CAcceptSockMgr& acceptSockMgr);

		CWinSock* getAcceptSock();

		CWinSock* forward(CAcceptSockMgr& acceptSockMgr);
	};
}
