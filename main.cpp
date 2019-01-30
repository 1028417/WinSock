
#include "stdafx.h"

#include "WinSock/WinSockServer.h"

#include "WinSock/WinSockClient.h"

using namespace NS_WinSock;

#define __ServerIP "127.0.0.1"
#define __ServerPort (IPPORT_RESERVED+1)

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
	auto fnClientSockOperate = [&](CWinSockClient& socClient, E_ClientSockOperate eOperate)
	{
		auto fnRecvCPCB = [](CWinSock& WinSock
			, char *pData, DWORD dwNumberOfBytesTransferred, ULONG_PTR lpCompletionKey) {
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
				if (!socClient.create(true, true))
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

				if (!socClient.poolBind(fnRecvCPCB))
				{
					return false;
				}

				break;
			case E_ClientSockOperate::CSO_Send:
			{
				char lpData[600]{ 'a' };
				DWORD uLen = sizeof lpData;
				socClient.sendEx(lpData, uLen);

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
		vector<CWinSockClient> vecWinSockClient;

		thread thrConn;
		thread thrTest;
	};
	vector<tagClientGroup> vecClientGroup(uGroupCount);

	tagTImer timer("createClients");
	for (auto& ClientGroup : vecClientGroup)
	{
		ClientGroup.vecWinSockClient.resize(uClientCount/uGroupCount);
		for (auto& socClient : ClientGroup.vecWinSockClient)
		{
			if (!fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Create))
			{
				return;
			}
		}
	}
	timer.print();

	timer = tagTImer("connectServer(no-block)");
	for (auto& ClientGroup : vecClientGroup)
	{
		ClientGroup.thrConn = thread([&] {
			for (auto& socClient : ClientGroup.vecWinSockClient)
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
			for (auto& socClient : ClientGroup.vecWinSockClient)
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

				for (UINT uIndex = 0; uIndex < ClientGroup.vecWinSockClient.size(); uIndex++)
				{
					if (para.bRestartClient || para.bQuit)
					{
						break;
					}

					auto& socClient = ClientGroup.vecWinSockClient[uIndex];
				
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

			for (auto& socClient : ClientGroup.vecWinSockClient)
			{
				fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Close);
			}
		});
	}

	for (auto& ClientGroup : vecClientGroup)
	{
		ClientGroup.thrTest.join();
	}

	CConsole::inst().printEx([](ostream& out) {
		out << "test stoped";
	});
}

static UINT testWinSock(UINT uClientCount, UINT uGroupCount)
{
	if (uGroupCount > uClientCount)
	{
		uGroupCount = uClientCount;
	}

	tagTImer clock("createServer");
	if (!CWinSock::init(2, 2))
	{
		CConsole::inst().print("CWinSock::init fail");
		return false;
	}

	CWinSockServer WinSockServer;
	if (!WinSockServer.create(__ServerPort))
	{
		CConsole::inst().print("WinSockServer.init fail");
		return false;
	}

	//WinSockServer.accept(3);

	//if (!WinSockServer.poolAccept(1000))
	//{
	//	CConsole::inst().print("WinSockServer.poolAccept fail");
	//	return false;
	//}

	if (!WinSockServer.iocpAccept(thread::hardware_concurrency(), uClientCount))
	{
		CConsole::inst().print("WinSockServer.iocpAccept fail");
		return false;
	}
	clock.print();

	tagTestPara para;
	thread thrClient([&] {
		startClient(uClientCount, uGroupCount, para);
	});

	auto fnPrintServerInfo = [&] {
		tagAcceptSockSum acceptSockSum;
		list<pair<UINT, UINT>> lstSnapshot;
		WinSockServer.getClientInfo(acceptSockSum, lstSnapshot);

		CConsole::inst().printEx([&](ostream& out) {
			out << "Active Client:" << acceptSockSum.uCurrConnCount
				<< " Recycled:" << acceptSockSum.uRecycleCount
				<< " History Connected:" << acceptSockSum.uHistoryConnSum
				<< " History Recycled:" << acceptSockSum.uHistoryRecycleSum
				<< " Total Message:" << acceptSockSum.uHistoryMsgSum;

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
				tagTImer clock("broadcast");
				UINT uRet = WinSockServer.broadcast(lpBC);
				clock.print("broadcasted: " + to_string(uRet));
			}
		}

		return true;
	});

	thrMonitor.join();
	
	thrClient.join();

	if (!WinSockServer.shutdown())
	{
		return false;
	}

	return para.uFailNum;
}

int main()
{
	printf("input 'autoReconn' to test auto reconnect, 'restart' to restart all clients\n\
      'bc:xxx' to broadcast, 'exit' to quit\n\n");

	UINT uFailNum = testWinSock(60000, 8);
	
	CConsole::inst().printEx([uFailNum](ostream& out) {
		out << "test finish, FailNum:" << uFailNum;
	});
	
	(void)getchar();
	(void)getchar();

    return 0;
}
