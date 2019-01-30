#pragma once

namespace NS_WinSock
{
	class ICPCallback;
	struct tagPerIOData : OVERLAPPED
	{
		tagPerIOData(ICPCallback& t_iocpHandler, ULONG ulWSABUFSize, char *pWSABUF)
			: iocpHandler(t_iocpHandler)
		{
			bAvalid = true;

			wsaBuf.len = ulWSABUFSize;
			wsaBuf.buf = 0 < ulWSABUFSize?pWSABUF:NULL;
		}

		WSABUF wsaBuf;

		ICPCallback& iocpHandler;

		bool bAvalid = false;
	};
	
#define __PerIOAlloc LocalAlloc
#define __PerIOFree ::LocalFree

	template<ULONG _ulWSABUFSize, class T>
	struct tagPerIODataTemplate : tagPerIOData
	{
		static const size_t _TotalSize()
		{
			return sizeof(T) + _ulWSABUFSize;
		}

		tagPerIODataTemplate(ICPCallback& iocpHandler)
			: tagPerIOData(iocpHandler, _ulWSABUFSize, (char*)this + sizeof T)
		{
		}

		void* operator new[](size_t size)
		{
			throw 0;
			return NULL; // 不允许new数组
		}

		void operator delete[](void*ptr)
		{
			throw 0; // 不允许delete数组
		}

		void* operator new(size_t size)
		{
			throw 0;
			return NULL; // 不允许new
			//auto ptr = __PerIOAlloc(GPTR, size + _ulWSABUFSize);
			//if (NULL == ptr)
			//{
			//	CWinSock::printSockErr("__PerIOAlloc", GetLastError());
			//	return NULL;
			//}

			//return ptr;
		}

		void operator delete(void* ptr)
		{
			// 不允许delete
		}

		static T* alloc(ICPCallback& iocpHandleer)
		{
			auto ptr = __PerIOAlloc(GPTR, _TotalSize());
			if (NULL == ptr)
			{
				CWinSock::printSockErr("__PerIOAlloc", GetLastError());
				return NULL;
			}

			return ::new(ptr) T(iocpHandleer);
		}

		void asign(ICPCallback& iocpHandleer)
		{
			(void)::new(this) T(iocpHandleer);
		}
		
		void free()
		{
			((T*)this)->T::~T();
			if (NULL != __PerIOFree(this))
			{
				CWinSock::printSockErr("__PerIOFree", GetLastError());
			}
		}
	};

	template <typename T>
	struct tagPerIOArray
	{
		using PerIOArray = tagPerIOArray<T>;

		tagPerIOArray(HLOCAL t_hLocal, size_t t_uPerIONum)
			: uPerIONum(t_uPerIONum)
			, hLocal(t_hLocal)
		{
		}

		HLOCAL hLocal = NULL;
		size_t uPerIONum = 0;

		T*  operator [](size_t uIndex)
		{
			if (NULL == hLocal)
			{
				return NULL;
			}

			if (uIndex >= uPerIONum)
			{
				return NULL;
			}

			char* ptr = (char*)hLocal + T::_TotalSize()*uIndex;
			T* pPerIOData = (T*)ptr;

			if (!pPerIOData->bAvalid)
			{
				return NULL;
			}

			return pPerIOData;
		}

		T* asign(size_t uIndex, ICPCallback& CPHandler)
		{
			if (NULL == hLocal)
			{
				return NULL;
			}

			if (uIndex >= uPerIONum)
			{
				return NULL;
			}

			char* ptr = (char*)hLocal + _TotalSize()*uIndex;
			T* pPerIOData = (T*)ptr;

			return pPerIOData->T::T(CPHandler);
		}

		static PerIOArray* alloc(size_t uNum, const function<ICPCallback*(UINT uIndex)>& fnAllocing=NULL
			, const function<bool(T& perIOData)>& fnAlloced=NULL)
		{
			if (0 == uNum)
			{
				return NULL;
			}

			HLOCAL hLocal = __PerIOAlloc(GPTR, T::_TotalSize()*uNum);
			if (NULL == hLocal)
			{
				CWinSock::printSockErr("GlobalAlloc", GetLastError());
				return NULL;
			}

			PerIOArray *pPerIOArray = new PerIOArray(hLocal, uNum);

			if (NULL == fnAllocing)
			{
				return pPerIOArray;
			}

			char* ptr = (char*)pPerIOArray->hLocal;
			for (UINT uIndex = 0; uIndex < uNum; uIndex++)
			{
				ICPCallback* pCPHandler = fnAllocing(uIndex);
				if (NULL != pCPHandler)
				{
					T *pPerIOData = ::new(ptr) T(*pCPHandler);

					if (fnAlloced)
					{
						if (!fnAlloced(*pPerIOData))
						{
							free(pPerIOArray);
							return false;
						}
					}
				}

				ptr += T::_TotalSize();
			}

			return pPerIOArray;
		}

		static void free(PerIOArray* pPerIOArray)
		{
			if (NULL != pPerIOArray)
			{
				if (NULL != pPerIOArray->hLocal)
				{
					char* ptr = (char*)pPerIOArray->hLocal;
					for (UINT uIndex = 0; uIndex < pPerIOArray->uPerIONum; uIndex++)
					{
						T* pPerIOData = ((T*)ptr);
						if (pPerIOData->bAvalid)
						{
							pPerIOData->T::~T();
						}

						ptr += T:: _TotalSize();
					}

					if (NULL != __PerIOFree(pPerIOArray->hLocal))
					{
						CWinSock::printSockErr("__PerIOFree", GetLastError());
					}
				}

				delete pPerIOArray;
			}
		}
	};

	template<class T>
	class CPerIODataArray
	{
		using TD_PerIOArray = tagPerIOArray<T>;
		using FN_CB = function<bool(T& perIOData)>;

	private:
		TD_PerIOArray *m_pPerIOArray = NULL;

	public:
		CPerIODataArray()
		{
		}

		~CPerIODataArray()
		{
			TD_PerIOArray::free(m_pPerIOArray);
		}

		bool asign(size_t size, ICPCallback& iocpHandler, const FN_CB& fnCB = NULL)
		{
			m_pPerIOArray = TD_PerIOArray::alloc(size, [&iocpHandler](UINT uIndex) {return &iocpHandler;}, fnCB);
			if (NULL == m_pPerIOArray)
			{
				return false;
			}
			return true;
		}

		bool asign(const vector<ICPCallback*>& vecCPHandler, const FN_CB& fnCB = NULL)
		{
			auto itr = vecCPHandler.begin();
			m_pPerIOArray = TD_PerIOArray::allocEx(vecCPHandler.size(), m_perIOVector, [](UINT uIndex) {
				if (itr == vecCPHandler.end())
				{
					return NULL;
				}

				return *itr++;
			}, fnCB);

			if (NULL == m_pPerIOArray)
			{
				return false;
			}

			return true;
		}

		T* asign(size_t uIndex, ICPCallback& CPHandler)
		{
			if (NULL == m_pPerIOArray)
			{
				return NULL;
			}

			return m_pPerIOArray->asign(uIndex, CPHandler);
		}

		int size()
		{
			if (NULL == m_pPerIOArray)
			{
				return -1;
			}

			return (int)m_pPerIOArray->uPerIONum;
		}

		T* operator [](size_t uIndex)
		{
			if (NULL == m_pPerIOArray)
			{
				return NULL;
			}
			
			return m_pPerIOArray->operator[](uIndex);
		}
	};
};
