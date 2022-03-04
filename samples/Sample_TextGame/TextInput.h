#pragma once

#include "Task.h"

#include <thread>
#include <string>
#include <iostream>
#include <list>

#include <conio.h>

using namespace Squid;

class TextInput
{
public:
	TextInput()
	{
		m_inputThread = std::thread([this] {
			InputThread();
		});
	}
	~TextInput()
	{
		m_terminate = true;
		m_inputThread.join();
	}
	void ClearInput()
	{
		std::lock_guard<std::mutex> inputLock(m_inputMutex);
		m_inputStr.clear();
	}
	Task<std::string> WaitForInput(bool in_echoText = true)
	{
		TASK_NAME(__FUNCTION__);

		ClearInput();
		std::string input;
		while(true)
		{
			auto inputMaybe = GetNextInputChar();
			if(inputMaybe)
			{
				auto c = inputMaybe.value();
				if(isalnum(c) || c == 32)
				{
					if(in_echoText)
					{
						std::cout << c;
					}
					input += c;
				}
				else if(c == 8) // Backspace
				{
					std::cout << c << ' ' << c;
					input = input.substr(0, input.size() - 1);
				}
				else if(c == '\r')
				{
					if(in_echoText)
					{
						std::cout << std::endl;
					}
					break;
				}
			}
			co_await Suspend();
		}
		co_return input;
	}
	Task<char> WaitForInputChar()
	{
		TASK_NAME(__FUNCTION__);

		ClearInput();
		while(true)
		{
			auto inputMaybe = GetNextInputChar();
			if(inputMaybe)
			{
				auto c = inputMaybe.value();
				if(isalnum(c) || c == 32 || c == 8 || c == '\r')
				{
					co_return c;
				}
			}
			co_await Suspend();
		}
		co_return '\0';
	}

private:
	std::thread m_inputThread;
	bool m_terminate = false;
	std::mutex m_inputMutex;
	std::list<char> m_inputStr;

	std::optional<char> GetNextInputChar()
	{
		std::lock_guard<std::mutex> inputLock(m_inputMutex);
		if(m_inputStr.size())
		{
			char c = m_inputStr.front();
			m_inputStr.pop_front();
			return c;
		}
		return {};
	}
	void InputThread()
	{
		while(!m_terminate)
		{
			if(_kbhit())
			{
				char c = _getch();
				{
					std::lock_guard<std::mutex> inputLock(m_inputMutex);
					m_inputStr.push_back(c);
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}
};
