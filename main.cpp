
#include "stdafx.h"

#include "WinSock/WinSockServer.h"

#include "WinSock/WinSockClient.h"

using namespace NS_WinSock;

#define __ServerIP "127.0.0.1"
#define __ServerPort (IPPORT_RESERVED+1)
#define __ClientCount 60000

enum class E_ClientSockOperate
{
	CSO_Create
	, CSO_Connect
	, CSO_checkConnected
	, CSO_Send
	, CSO_Close
};

enum E_TestAction
{
	TA_None,

	TA_Restart,

	TA_AutoSend,
	TA_AutoReconn,

	TA_Exit,	
};

struct tagTestPara
{
	UINT uFailNum = 0;
	E_TestAction eTestAction = TA_None;
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
				if (!socClient.create(__ServerIP, __ServerPort, true, true))
				{
					return false;
				}

				break;
			case E_ClientSockOperate::CSO_Connect:
				if (E_WinSockResult::WSR_Error == socClient.connect())
				{
					return false;
				}

				break;
			case E_ClientSockOperate::CSO_checkConnected:
				if (E_WinSockResult::WSR_OK == socClient.checkConnected(0))
				{
					if (!socClient.poolBind(fnRecvCPCB))
					{
						return false;
					}
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

	tagClock clock;
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
	clock.printTime("createClient ok");

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
	clock.printTime("connectServer ok");
	
	auto enumClient = [&](const function<void(CWinSockClient& socClient)>& fnCB)
	{
		for (auto& ClientGroup : vecClientGroup)
		{
			for (auto& socClient : ClientGroup.vecWinSockClient)
			{
				if (TA_Exit == para.eTestAction || TA_Restart == para.eTestAction)
				{
					break;
				}

				fnCB(socClient);
			}
		}
	};

	for (auto& ClientGroup : vecClientGroup)
	{
		ClientGroup.thrTest = thread([&] {
			while (true)
			{
				E_TestAction& eTestAction = para.eTestAction;
				if (TA_Exit == eTestAction || TA_Restart == eTestAction)
				{
					break;
				}

				::Sleep(3);
				if (TA_AutoSend == eTestAction)
				{
					::Sleep(1000);
				}
				
				for (auto& socClient : ClientGroup.vecWinSockClient)
				{
					if (TA_AutoSend == eTestAction)
					{
						if (E_SockConnStatus::SCS_Connecting == socClient.getStatus())
						{
							(void)fnClientSockOperate(socClient, E_ClientSockOperate::CSO_checkConnected);
						}
						else if (E_SockConnStatus::SCS_Connected == socClient.getStatus())
						{
							fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Send);
						}
					}
					else if (TA_AutoReconn == eTestAction)
					{
						if (E_SockConnStatus::SCS_Connecting == socClient.getStatus())
						{
							if (fnClientSockOperate(socClient, E_ClientSockOperate::CSO_checkConnected))
							{
								if (1)
								{
									(void)fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Close);
									(void)fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Create);
									(void)fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Connect);
								}
							}
							else
							{
								(void)fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Close);
							}
						}
						else if (E_SockConnStatus::SCS_Connected == socClient.getStatus())
						{
							fnClientSockOperate(socClient, E_ClientSockOperate::CSO_Close);
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

	if (!WinSockServer.iocpAccept(thread::hardware_concurrency(), __ClientCount))
	{
		CConsole::inst().print("WinSockServer.iocpAccept fail");
		return false;
	}

	tagTestPara para;
	thread thrClient([&] {
		startClient(uClientCount, uGroupCount, para);
	});

	auto fnPrintServerInfo = [&] {
		tagAcceptSockSum acceptSockSum;
		list<pair<UINT, UINT>> lstSnapshot;
		WinSockServer.getClientInfo(acceptSockSum, lstSnapshot);

		CConsole::inst().printEx([&](ostream& out) {
			out << "current Client Count:" << acceptSockSum.uCurrConnCount
				<< " HistoryConnSum:" << acceptSockSum.uHistoryConnSum
				<< " RecycleCount:" << acceptSockSum.uRecycleCount
				<< " HistoryRecycleSum:" << acceptSockSum.uHistoryRecycleSum
				<< " HistoryMsgSum:" << acceptSockSum.uHistoryMsgSum;

			UINT uAcceptSum = 0;
			UINT uFreeSum = 0;
			for (auto pr : lstSnapshot)
			{
				uAcceptSum += pr.first;
				uFreeSum += pr.second;

				out << "\nAccpNum:" << pr.first << " FreeNum:" << pr.second;
			}

			out << "\nAcceptSum:" << uAcceptSum << " FreeSum:" << uFreeSum << '\n';
		});
	};

	fnPrintServerInfo();

	thread thrMonitor([&] {
		::Sleep(4000);
		
		for (UINT uIndex = 0; TA_Exit!=para.eTestAction; uIndex++)
		{
			::Sleep(1000);

			//if (uIndex % 3 != 0)
			//{
			//	continue;
			//}
			//uIndex = 0;
				
			fnPrintServerInfo();
		}
	});

	CConsole::inst().waitInput(true, [&](const string& strInput) {
		if ("restart" == strInput)
		{
			para.eTestAction = TA_Restart;
			thrClient.join();
			para.eTestAction = TA_None;

			thrClient = thread([&] {
				::Sleep(5000);
				startClient(uClientCount, uGroupCount, para);
			});

			return true;
		}
		else if ("autoSend" == strInput)
		{
			para.eTestAction = TA_AutoSend;
		}
		else if ("autoReconn" == strInput)
		{
			para.eTestAction = TA_AutoReconn;
		}
		else if ("exit" == strInput)
		{
			para.eTestAction = TA_Exit;

			return false;
		}
		else
		{
			char lpBC[10]{ 0 };
			if (1 == sscanf_s(strInput.c_str(), "bc:%9s", lpBC, (UINT)sizeof(lpBC)))
			{
				tagClock clock;
				UINT uRet = WinSockServer.broadcast(lpBC);
				clock.printTime([uRet](ostream& out) {
					out << "broadcast ok: " << uRet;
				});
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
	printf("input 'exit' to quit, 'restart' to restart, 'bc:xxx' to broadcast\n"
		"'autoSend' to test auto send, 'autoReconn' to auto test auto reconnect\n\n");

	UINT uFailNum = testWinSock(__ClientCount, 8);
	
	CConsole::inst().printEx([uFailNum](ostream& out) {
		out << "test finish, FailNum:" << uFailNum;
	});
	
	(void)getchar();
	(void)getchar();

    return 0;
}
