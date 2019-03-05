
#pragma once

#pragma warning( disable: 4251 )

#include <list>
using std::list;

#include <map>
using std::map;

#include <WinSock2.h>

#include "mtutil.h"
using namespace NS_mtutil;

#include "CharVector.h"

#include "IOCP.h"

#define __ADDRLEN (sizeof(sockaddr_in)+16)

#ifdef __WinSockPrj
#define __WinSockExt __declspec(dllexport)
#else
#define __WinSockExt __declspec(dllimport)
#endif

namespace NS_WinSock
{
	enum class E_WinSockResult
	{
		WSR_OK
		, WSR_Error
		, WSR_EWOULDBLOCK
		, WSR_PeerClosed
		, WSR_Timeout
	};

	enum class E_SockConnStatus
	{
		SCS_None
		, SCS_Connecting
		, SCS_Connected
		, SCS_Excetion
		, SCS_Closing
	};

	struct tagSocketSet
	{
		fd_set readfds;
		fd_set writefds;
		fd_set errorfds;

		tagSocketSet(SOCKET sock)
		{
			memset(&readfds, 0, sizeof readfds);
			memset(&writefds, 0, sizeof writefds);
			memset(&errorfds, 0, sizeof errorfds);

			if (INVALID_SOCKET != sock)
			{
				FD_SET(sock, &readfds);
				FD_SET(sock, &writefds);
				FD_SET(sock, &errorfds);
			}
		}

		bool isReadable(SOCKET sock)
		{
			return 0 != FD_ISSET(sock, &readfds);
		}

		bool isWritable(SOCKET sock)
		{
			return 0 != FD_ISSET(sock, &writefds);
		}

		bool isError(SOCKET sock)
		{
			return 0 != FD_ISSET(sock, &errorfds);
		}
	};

	enum class E_CommPerIOType
	{
		CPIOT_Send
		, CPIOT_Recv
	};

	struct tagRecvPerIOData : tagPerIODataTemplate<0, tagRecvPerIOData>
	{
		tagRecvPerIOData(ICPCallback& iocpHandler)
			: tagPerIODataTemplate(iocpHandler)
		{
		}
	};

#define __SendPerIO_BuffSize 256
	struct tagSendPerIOData : tagPerIODataTemplate<__SendPerIO_BuffSize, tagSendPerIOData>
	{
		tagSendPerIOData(ICPCallback& iocpHandler)
			: tagPerIODataTemplate(iocpHandler)
		{
		}
	};

	class CWinSock;
	using CB_Recv = function<bool(CWinSock&, char *pData, DWORD dwNumberOfBytesTransferred)>;

	using CB_PeerShutdown = function<void(CWinSock&)>;

	using VEC_SendData = vector<char>;

	struct tagSendData
	{
		CCASLock lock;
		list<CCharVector> m_lstData;

		void add(char* lpData, DWORD uLen)
		{
			lock.lock();

			m_lstData.push_back(CCharVector());
			CCharVector& charVec = m_lstData.back();
			charVec.assign(lpData, uLen);

			lock.unlock();
		}

		size_t get(CCharVector& charVec, size_t uSize)
		{
			lock.lock();

			size_t uRet = 0;

			if (!m_lstData.empty())
			{
				CCharVector& frontData = m_lstData.front();
				if (frontData.getSize() <= uSize)
				{
					uRet = frontData.getSize();

					charVec.swap(frontData);

					m_lstData.pop_front();
				}
				else
				{
					uRet = uSize;

					charVec.assign(frontData.getPtr(), uSize);

					frontData.pop_front(uSize);
				}
			}

			lock.unlock();

			return uRet;
		}

		size_t get(char *lpBuff, size_t uSize)
		{
			lock.lock();

			size_t uRet = 0;

			if (!m_lstData.empty())
			{
				CCharVector& frontData = m_lstData.front();
				uRet = frontData.pop_front(lpBuff, uSize);

				if (0 == frontData.getSize())
				{
					m_lstData.pop_front();
				}
			}

			lock.unlock();

			return uRet;
		}
	};

	class __WinSockExt CWinSock : public ICPCallback
	{
	public:
		static void printSockErr(const string& method, int iErr = -1);

		static bool checkNTStatus(ULONG_PTR Internal);

		static bool init(WORD wVersion, WORD wHighVersion);

		static bool select(tagSocketSet& sockSet, UINT& uResult, DWORD dwTimeout = 0);

	public:
		CWinSock()
		{
		}

		CWinSock(SOCKET sock, bool bConnected)
		{
			m_sock = sock;
			m_eStatus = bConnected ? E_SockConnStatus::SCS_Connected : E_SockConnStatus::SCS_None;
		}

		CWinSock(SOCKET sock, E_SockConnStatus eConnStatus)
		{
			m_sock = sock;
			m_eStatus = eConnStatus;
		}

	protected:
		SOCKET m_sock = INVALID_SOCKET;

		E_SockConnStatus m_eStatus = E_SockConnStatus::SCS_None;

	private:
		bool m_bNoBlock = false;

		tagRecvPerIOData *m_pRecvPerIO = NULL;
		tagSendPerIOData *m_pSendPerIO = NULL;

		CB_Recv m_cbRecv;

		CB_PeerShutdown m_cbPeerShutdown;

		volatile bool m_bAyncSending = false;
		
		tagSendData m_sendData;

	private:
		E_WinSockResult _asyncReceive();

		E_WinSockResult _asyncSend();

		void _handleRecvCB(OVERLAPPED& overLapped, DWORD dwNumberOfBytesTransferred);
		
		virtual void handleCPCallback(ULONG_PTR Internal, tagPerIOData& perIOData, DWORD dwNumberOfBytesTransferred) override;

	protected:
		void* GetExtensionFunction(GUID guid);

		E_WinSockResult waitEvent(WSAEVENT hSockEvent, long& lEvent, map<UINT, int>& mapEventErr, DWORD dwTimeout = 0);

		virtual bool onReceived(char *lpData, DWORD dwNumberOfBytesTransferred);

		virtual void onPeerClosed();
		
		bool setOpt(int optname, const void *optval, int optlen);

		bool setNoBlock(bool bVal);

		bool cancelIO(LPOVERLAPPED lpOverlapped = NULL);

	public:
		SOCKET getHandle()
		{
			return m_sock;
		}

		E_SockConnStatus getStatus()
		{
			return m_eStatus;
		}

		E_WinSockResult create(bool bOverlapped);

		E_WinSockResult createNoBlock(bool bOverlapped);

		E_WinSockResult createAsync(CIOCP *pIOCP = NULL);

		bool keepAlive(ULONG keepalivetime, ULONG keepaliveinterval);

		E_WinSockResult send(char* lpData, ULONG uLen, DWORD *pdwSentLen=NULL);

		E_WinSockResult receive(char* lpBuff, ULONG uBuffSize, DWORD& uRecvLen);

		E_WinSockResult initAsync(CIOCP *pIOCP=NULL);

		E_WinSockResult asyncReceive(const CB_Recv& fnRecvCB, const CB_PeerShutdown& fnPeerShutdownedCB=NULL);

		E_WinSockResult asyncSend(char* lpData, ULONG uLen);

		bool disconnect();

		bool close(bool bLinger = false, int lingerTime = 0);
	};
};
