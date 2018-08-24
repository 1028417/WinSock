
#pragma once

#include "AcceptSockNode.h"

namespace NS_WinSock
{
	template<class T, ULONG _ulRecvSize = 0>
	struct PER_IO_DATA_ACCEPT_BASE : tagPerIODataTemplate<__ADDRLEN * 2 + _ulRecvSize, T>
	{
		PER_IO_DATA_ACCEPT_BASE(ICPCallback& iocpHandler)
			: tagPerIODataTemplate(iocpHandler)
			, dwReceiveDataLength(_ulRecvSize)
		{
		}

		DWORD dwReceiveDataLength = 0;
	};

	struct tagAcceptPerIOData : PER_IO_DATA_ACCEPT_BASE<tagAcceptPerIOData>, tagAcceptSockNodeList
	{
		tagAcceptPerIOData(ICPCallback& iocpHandler)
			: PER_IO_DATA_ACCEPT_BASE(iocpHandler)
			, tagAcceptSockNodeList(*this)
		{
		}
	};

	class CWinSockServer : public CWinSock
	{
	public:
		CWinSockServer()
			: m_iocp(this)
		{
		}

	private:
		LPFN_ACCEPTEX m_lpfnAcceptEx = NULL;

		CIOCP m_iocp;

		CPerIODataArray<tagAcceptPerIOData> m_AcceptPerIOArray;

		CAcceptSockMgr m_AcceptSockMgr;

	private:
		bool _acceptEx(UINT uClientCount);

		E_WinSockResult _acceptEx(SOCKET socAccept, tagAcceptPerIOData& perIOData);

	public:
		bool create(UINT uPort);

		void accept(UINT uThreadCount = 0);

		bool poolAccept(UINT uClientCount = 0);

		bool iocpAccept(UINT uIOCPThreadCount, UINT uClientCount = 0);

		UINT broadcast(const string& strData);

		void getClientInfo(tagAcceptSockSum& acceptSockSum, list<pair<UINT, UINT>>& lstSnapshot)
		{
			acceptSockSum = m_AcceptSockMgr.m_sum;

			for (int iIndex = 0; iIndex < m_AcceptPerIOArray.size(); iIndex++)
			{
				tagAcceptPerIOData *pAcceptPerIOData = m_AcceptPerIOArray[iIndex];
				if (NULL != pAcceptPerIOData)
				{
					lstSnapshot.push_back({ pAcceptPerIOData->uAccpSum, pAcceptPerIOData->uFreeCount });
				}
			}
		}

		bool shutdown();

	protected:
		virtual void handleCPCallback(OVERLAPPED& overlapped, DWORD dwNumberOfBytesTransferred, ULONG_PTR lpCompletionKey) override;
	};
};
