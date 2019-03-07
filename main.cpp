

#include "inc/Console.h"

#include "inc/ServerSock.h"

#include "inc/ClientSock.h"

using namespace NS_WinSock;

#define __ServerIP "127.0.0.1"
#define __ServerPort (IPPORT_RESERVED+5)

enum class E_ClientSockOperate
{
	CSO_Create
	, CSO_Connect
	, CSO_checkConnected
	, CSO_Send
	, CSO_Close
};

struct tagTestPara
{
	UINT uFailNum = 0;

	bool bAutoReconn = false;

	bool bRestartClient = false;

	bool bQuit = false;
};

static void startClient(UINT uClientCount, UINT uGroupCount, tagTestPara& para)
{
	auto fnClientSockOperate = [&](CClientSock& socClient, E_ClientSockOperate eOperate)
	{
		auto fnRecvCPCB = [](CWinSock& WinSock, char *pData, DWORD dwNumberOfBytesTransferred) {
			if (NULL == pData || 0 == dwNumberOfBytesTransferred)
			{
				return false;
			}

			string strData(pData, dwNumberOfBytesTransferred);
			strData.append("--");
			DWORD uLen = (DWORD)strData.length();
			if (E_WinSockResult::WSR_OK != WinSock.send((char*)strData.c_str(), uLen))
			{
				return false;
			}

			return true;
		};

		auto fn = [&]()
		{
			switch (eOperate)
			{
			case E_ClientSockOperate::CSO_Create:
				if (E_WinSockResult::WSR_OK != socClient.createAsync())
				{
					return false;
				}

				break;
			case E_ClientSockOperate::CSO_Connect:
				if (E_WinSockResult::WSR_Error == socClient.connect(__ServerIP, __ServerPort))
				{
					return false;
				}

				break;
			case E_ClientSockOperate::CSO_checkConnected:
				if (E_WinSockResult::WSR_OK != socClient.checkConnected(0))
				{
					return false;
				}

				if (E_WinSockResult::WSR_OK != socClient.asyncReceive(fnRecvCPCB))
				{
					return false;
				}

				break;
			case E_ClientSockOperate::CSO_Send:
			{
				char lpData[600]{ 'a' };
				socClient.asyncSend(lpData, sizeof lpData);

				break;
			}
			case E_ClientSockOperate::CSO_Close:
				if (!socClient.close(true, 0))
				{
					return false;
				}
			}

			return true;
		};

		if (!fn())
		{
			para.uFailNum++;
			return false;
		}

		return true;
	};
	
	struct tagClientGroup
	{
		vector<CClientSock> vecClientSock;

		thread thrConn;
		thread thrTest;
	};
	vector<tagClientGroup> vecClientGroup(uGroupCount);

	tagClock timer("createClients");
	for (auto& ClientGroup : vecClientGroup)
	{
		ClientGroup.vecClientSock.resize(uClientCount/uGroupCount);
		for (auto& socClient : ClientGroup.vecClientSock)
		{
			if (!fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Create))
			{
				return;
			}
		}
	}
	timer.print();

	timer = tagClock("connectServer(no-block)");
	for (auto& ClientGroup : vecClientGroup)
	{
		ClientGroup.thrConn = thread([&] {
			for (auto& socClient : ClientGroup.vecClientSock)
			{
				if (!fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Connect))
				{
					return;
				}
			}
		});
	}
	for (auto& ClientGroup : vecClientGroup)
	{
		ClientGroup.thrConn.join();
	}
	timer.print();
	
	for (auto& ClientGroup : vecClientGroup)
	{
		ClientGroup.thrTest = thread([&] {
			::Sleep(3000);
			for (auto& socClient : ClientGroup.vecClientSock)
			{
				if (!fnClientSockOperate(socClient, E_ClientSockOperate::CSO_checkConnected))
				{
					(void)fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Close);
				}
			}

			while (true)
			{
				if (para.bRestartClient || para.bQuit)
				{
					break;
				}

				for (UINT uIndex = 0; uIndex < ClientGroup.vecClientSock.size(); uIndex++)
				{
					::Sleep(3);
					if (para.bRestartClient || para.bQuit)
					{
						break;
					}

					auto& socClient = ClientGroup.vecClientSock[uIndex];
				
					if (para.bAutoReconn)
					{
						::Sleep(1);
						
						if (E_SockConnStatus::SCS_Connecting == socClient.getStatus())
						{
							if (!fnClientSockOperate(socClient, E_ClientSockOperate::CSO_checkConnected))
							{
								(void)fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Close);
							}
							else
							{
								if (1)
								{
									//fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Send);

									(void)fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Close);
									(void)fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Create);
									(void)fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Connect);
								}
							}
						}
						else if (E_SockConnStatus::SCS_Connected == socClient.getStatus())
						{
							(void)fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Close);
							
							if (0)
							{
								(void)fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Create);
								(void)fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Connect);
							}
						}
						else if (E_SockConnStatus::SCS_None == socClient.getStatus())
						{
							(void)fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Create);
							(void)fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Connect);
						}
					}
					else
					{
						if (uIndex % 50 == 0)
						{
							::Sleep(3);
						}

						if (E_SockConnStatus::SCS_Connected == socClient.getStatus())
						{
							fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Send);
						}
					}
				}
			}

			for (auto& socClient : ClientGroup.vecClientSock)
			{
				fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Close);
			}
		});
	}

	for (auto& ClientGroup : vecClientGroup)
	{
		ClientGroup.thrTest.join();
	}

	CConsole::inst().print([](ostream& out) {
		out << "test stoped";
	});
}

static CServerSock g_ServerSock;

static UINT g_uHistoryMsgSum = 0;

static bool startServer(UINT uClientCount)
{
	tagClock clock("createServer");
	if (E_WinSockResult::WSR_OK != g_ServerSock.create())
	{
		CConsole::inst().printT("ServerSock.create fail");
		return false;
	}

	if (E_WinSockResult::WSR_OK != g_ServerSock.listen(__ServerPort))
	{
		CConsole::inst().printT("ServerSock.listen fail");
		return false;
	}

	//if (!g_ServerSock.asyncAccept(uClientCount))
	//{
	//	CConsole::inst().print("ServerSock.poolAccept fail");
	//	return false;
	//}

	auto fnRecvCB = [](CWinSock& WinSock, char *pData, DWORD dwNumberOfBytesTransferred) {
		g_uHistoryMsgSum++;
		return true;
	};

	if (E_WinSockResult::WSR_OK != g_ServerSock.asyncAccept(uClientCount, NULL, fnRecvCB, NULL, thread::hardware_concurrency()))
	{
		CConsole::inst().printT("ServerSock.iocpAccept fail");
		return false;
	}
	clock.print();

	return true;
}

static UINT testWinSock(UINT uClientCount, UINT uGroupCount)
{
	if (uGroupCount > uClientCount)
	{
		uGroupCount = uClientCount;
	}

	if (!CWinSock::init(2, 2))
	{
		CConsole::inst().printT("CWinSock::init fail");
		return false;
	}

	if (!startServer(uClientCount))
	{
		return false;
	}

	tagTestPara para;
	thread thrClient([&] {
		startClient(uClientCount, uGroupCount, para);
	});

	auto fnPrintServerInfo = [&] {
		tagAcceptSockSum acceptSockSum;
		list<pair<UINT, UINT>> lstSnapshot;
		g_ServerSock.getClientInfo(acceptSockSum, lstSnapshot);

		CConsole::inst().print([&](ostream& out) {
			out << "Active Client:" << acceptSockSum.uCurrConnCount
				<< " Recycled:" << acceptSockSum.uRecycleCount
				<< " History Connected:" << acceptSockSum.uHistoryConnSum
				<< " History Recycled:" << acceptSockSum.uHistoryRecycleSum
				<< " Total Message:" << g_uHistoryMsgSum;

			UINT uAcceptSum = 0;
			UINT uFreeSum = 0;
			UINT uIndex = 0;
			for (auto pr : lstSnapshot)
			{
				uAcceptSum += pr.first;
				uFreeSum += pr.second;

				out << "\nthread" << ++uIndex << " Accept:" << pr.first << " Free:" << pr.second;
			}

			out << "\nsum            " << uAcceptSum << "      " << uFreeSum << '\n';
		});
	};

	fnPrintServerInfo();

	thread thrMonitor([&] {
		while (!para.bQuit)
		{
			::Sleep(3000);

			fnPrintServerInfo();
		}
	});

	CConsole::inst().waitInput(true, [&](const string& strInput) {
		if ("autoReconn" == strInput)
		{
			para.bAutoReconn = true;
		}
		else if ("restart" == strInput)
		{
			para.bRestartClient = true;
			thrClient.join();
			para.bRestartClient = false;

			thrClient = thread([&] {
				::Sleep(5000);
				startClient(uClientCount, uGroupCount, para);
			});

			return true;
		}
		else if ("exit" == strInput)
		{
			para.bQuit = true;

			return false;
		}
		else
		{
			char lpBC[10]{ 0 };
			if (1 == sscanf_s(strInput.c_str(), "bc:%9s", lpBC, (UINT)sizeof(lpBC)))
			{
				tagClock clock("broadcast");
				UINT uRet = g_ServerSock.broadcast(lpBC);
				clock.print("broadcasted: " + to_string(uRet));
			}
		}

		return true;
	});

	thrMonitor.join();
	
	thrClient.join();

	if (!g_ServerSock.shutdown())
	{
		return false;
	}

	return para.uFailNum;
}

int main()
{
	printf("input 'autoReconn' to test auto reconnect, 'restart' to restart all clients\n\
      'bc:xxx' to broadcast, 'exit' to quit\n\n");

	UINT uFailNum = testWinSock(10000, 2);

	CConsole::inst().print([uFailNum](ostream& out) {
		out << "test finish, FailNum:" << uFailNum;
	});
	
	(void)getchar();
	(void)getchar();

    return 0;
}
