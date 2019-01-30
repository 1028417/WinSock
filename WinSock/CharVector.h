#pragma once

#include <vector>
using std::vector;

class CCharVector
{
public:
	CCharVector()
	{
	}

	CCharVector(size_t uSize)
		: m_vecData(uSize)
	{
	}

private:
	vector<char> m_vecData;

public:
	char* getPtr()
	{
		if (m_vecData.empty())
		{
			return NULL;
		}

		return &m_vecData.front();
	}

	size_t getSize()
	{
		return m_vecData.size();
	}

	void clear()
	{
		m_vecData.clear();
	}

	void swap(CCharVector& charVec)
	{
		charVec.m_vecData.swap(m_vecData);
	}

	char* assign(char* lpData, size_t uSize)
	{
		m_vecData.clear();

		return push_back(lpData, uSize);
	}

	char* push_back(char* lpData, size_t uSize)
	{
		if (NULL == lpData || 0 == uSize)
		{
			return NULL;
		}

		size_t uPreSize = m_vecData.size();
		m_vecData.resize(uPreSize + uSize);

		memcpy(&m_vecData.front() + uPreSize, lpData, uSize);

		return &m_vecData.front();
	}

	size_t pop_front(char *lpBuff, size_t uPopLen)
	{
		if (NULL == lpBuff || 0 == uPopLen)
		{
			return 0;
		}

		size_t uSize = m_vecData.size();
		if (0 == uSize)
		{
			return 0;
		}
		
		if (uSize <= uPopLen)
		{
			memcpy(lpBuff, &m_vecData.front(), uSize);
			m_vecData.clear();
			return uSize;
		}

		memcpy(lpBuff, &m_vecData.front(), uPopLen);

		size_t uNewSize = uSize - uPopLen;
		memcpy(&m_vecData.front(), &m_vecData.front() + uPopLen, uNewSize);
		m_vecData.resize(uNewSize);

		return uPopLen;
	}

	size_t pop_front(size_t uPopLen)
	{
		if (0 == uPopLen)
		{
			return 0;
		}

		size_t uSize = m_vecData.size();
		if (uSize <= uPopLen)
		{
			m_vecData.clear();
			return uSize;
		}
		
		size_t uNewSize = uSize - uPopLen;
		memcpy(&m_vecData.front(), &m_vecData.front() + uPopLen, uNewSize);
		m_vecData.resize(uNewSize);

		return uPopLen;
	}
};
