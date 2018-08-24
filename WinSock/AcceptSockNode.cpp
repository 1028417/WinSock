
#include "stdafx.h"

#include "AcceptSockNode.h"

namespace NS_WinSock
{
	tagAcceptSockNode* tagAcceptSockNode::newNode()
	{
		CWinSock *pWinSock = new CWinSock;
		if (!pWinSock->create(true, false))
		{
			delete pWinSock;
			return NULL;
		}

		return new tagAcceptSockNode(pWinSock);
	}

	tagAcceptSockNode* tagAcceptSockNode::addNextNode()
	{
		tagAcceptSockNode *pNewNode = newNode();
		pNewNode->pNextNode = pNextNode;
		pNextNode = pNewNode;
		return pNextNode;
	}

	tagAcceptSockNode* tagAcceptSockNode::addNextNode(CWinSock *pWinSock)
	{
		tagAcceptSockNode *pNewNode = new tagAcceptSockNode(pWinSock);
		pNewNode->pNextNode = pNextNode;
		pNextNode = pNewNode;
		return pNextNode;
	}

	tagAcceptSockNodeList::~tagAcceptSockNodeList()
	{
		list<tagAcceptSockNode*> lstSockNodes;
		tagAcceptSockNode *pSockNode = pAcceptSockNode;
		while (NULL != pSockNode)
		{
			(void)pSockNode->pWinSock->close();
			delete pSockNode->pWinSock;

			lstSockNodes.push_back(pSockNode);

			pSockNode = pSockNode->pNextNode;
		}
		for (auto pSockNode : lstSockNodes)
		{
			delete pSockNode;
		}
	}
	
	CWinSock* tagAcceptSockNodeList::createNodes(UINT uNum)
	{
		m_uIncr = uNum / 10;
		uNum += m_uIncr;

		while (uNum--)
		{
			if (NULL == pAcceptSockNode)
			{
				pAcceptSockNode = tagAcceptSockNode::newNode();
				if (NULL == pAcceptSockNode)
				{
					break;
				}
			}
			else
			{
				if (NULL == pAcceptSockNode->addNextNode())
				{
					break;
				}
			}

			uFreeCount++;
		}

		return pAcceptSockNode->pWinSock;
	}

	CWinSock* tagAcceptSockNodeList::createNodesEx(UINT uNum, CAcceptSockMgr& acceptSockMgr)
	{
		while (true)
		{
			if (0 < acceptSockMgr.fetchRecycle(uNum, [&](CWinSock& WinSock) {
				if (NULL == pAcceptSockNode)
				{
					pAcceptSockNode = new tagAcceptSockNode(&WinSock);
				}
				else
				{
					pAcceptSockNode->addNextNode(&WinSock);
				}
				uFreeCount++;

				return true;
			}))
			{
				break;
			}
			Sleep(10);
		}

		return pAcceptSockNode->pWinSock;
	}


	CWinSock* tagAcceptSockNodeList::getAcceptSock()
	{
		if (NULL == pAcceptSockNode)
		{
			return NULL;
		}

		return pAcceptSockNode->pWinSock;
	}

	CWinSock* tagAcceptSockNodeList::forward(CAcceptSockMgr& acceptSockMgr)
	{
		uAccpSum++;

		if (NULL != pAcceptSockNode)
		{
			uFreeCount--;

			tagAcceptSockNode *pNextNode = pAcceptSockNode->pNextNode;
			delete pAcceptSockNode;

			pAcceptSockNode = pNextNode;
			if (NULL != pAcceptSockNode)
			{
				return pAcceptSockNode->pWinSock;
			}
		}
		else
		{
			uFreeCount = 0;
		}

		return this->createNodesEx(m_uIncr, acceptSockMgr);
	}


	tagAcceptSockList::~tagAcceptSockList()
	{
		for (auto pWinSock : lstAcceptSock)
		{
			(void)pWinSock->close();
			delete pWinSock;
		}
	}

	CWinSock *tagAcceptSockList::createNodes(UINT uNum)
	{
		m_uIncr = uNum / 10;
		uNum += m_uIncr;

		CWinSock *pAcceptSock = NULL;
		while (uNum--)
		{
			pAcceptSock = new CWinSock;
			if (!pAcceptSock->create(true, false))
			{
				delete pAcceptSock;
				break;
			}
			
			lstAcceptSock.push_back(pAcceptSock);
		}
		
		uFreeCount = (UINT)lstAcceptSock.size();

		if (0 < uFreeCount)
		{
			return lstAcceptSock.front();
		}
		return NULL;
	}

	CWinSock *tagAcceptSockList::createNodesEx(UINT uNum, CAcceptSockMgr& acceptSockMgr)
	{
		while (true)
		{
			if (0 < acceptSockMgr.fetchRecycle(uNum, [&](CWinSock& WinSock) {
				lstAcceptSock.push_back(&WinSock);

				return true;
			}))
			{
				uFreeCount = (UINT)lstAcceptSock.size();

				break;
			}

			Sleep(10);
		}

		if (!lstAcceptSock.empty())
		{
			return lstAcceptSock.front();
		}
		return NULL;
	}

	CWinSock* tagAcceptSockList::getAcceptSock()
	{
		if (lstAcceptSock.empty())
		{
			return NULL;
		}

		return lstAcceptSock.front();
	}

	CWinSock* tagAcceptSockList::forward(CAcceptSockMgr& acceptSockMgr)
	{
		uAccpSum++;
		uFreeCount--;

		lstAcceptSock.pop_front();

		if (!lstAcceptSock.empty())
		{
			return lstAcceptSock.front();
		}
		
		return this->createNodesEx(m_uIncr, acceptSockMgr);
	}
};
