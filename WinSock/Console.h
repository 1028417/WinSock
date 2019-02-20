#pragma once

#include <string>

#include <iostream>

#include <mutex>
using namespace std;

class CConsole
{
public:
	static CConsole& inst()
	{
		static CConsole inst;
		return inst;
	}

private:
	CConsole()
	{
	}

	mutex m_mtx;

public:
	template <unsigned int _scanfSize = 256>
	void waitInput(bool bBlock, const function<bool(const string& strInput)>& fnCB)
	{
		auto fnLoop = [&] {
			char lpInput[_scanfSize + 1]{ 0 };

			while (true)
			{
				if (1 == scanf_s("%s", lpInput, _scanfSize))
				{
					if (!fnCB(lpInput))
					{
						break;
					}
				}
			}
		};

		if (bBlock)
		{
			fnLoop();
		}
		else
		{
			thread thr(fnLoop);
			thr.detach();
		}
	}

	void print(const function<void(ostream& out)>& fnCB)
	{
		m_mtx.lock();

		fnCB(cout);

		cout << '\n';

		m_mtx.unlock();
	}

	template<typename T>
	void printT(T content)
	{
		m_mtx.lock();

		cout << content << '\n';

		m_mtx.unlock();
	}
};

struct tagClock
{
	string strOperateName;

	clock_t time = 0;

	tagClock(const string& t_strOperateName)
		: strOperateName(t_strOperateName)
	{
		CConsole::inst().print([&](ostream& out) {
			out << strOperateName << "...";
		});

		time = clock();
	}

	void print(const string& strMsg = "")
	{
		CConsole::inst().print([&](ostream& out) {
			clock_t ptrTime = time;
			time = clock();
			out << (!strMsg.empty() ? strMsg : strOperateName) << " ok, tickCount:" << time - ptrTime;
		});
	}
};
