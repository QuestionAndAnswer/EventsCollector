#define _WIN32_WINNT 0x0400
#pragma comment( lib, "user32.lib" )

#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <chrono>
#include <vector>
#include <utility>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <fstream>

const DWORD DROP_ON_DISC_TIMEOUT = 60 * 1000;

using namespace std;
using namespace std::chrono;

HHOOK hKeyboardHook;
HHOOK hMouseHook;

typedef pair <milliseconds, stringstream> logEntry;
typedef vector<logEntry> logVector;

class LoggCollector
{
public:
	LoggCollector(DWORD reserve) : logIndex(0), reserve(reserve)
	{
		logs[0].reserve(reserve);
		logs[1].reserve(reserve);
	}

	void writeRecord(milliseconds ms, stringstream&& record)
	{
		m.lock();
		logs[logIndex].push_back(std::move(logEntry(ms, std::move(record))));
		m.unlock();
	}

	logVector* switchLogs()
	{
		m.lock();
		logIndex = logIndex == 0 ? 1 : 0;
		m.unlock();
		const int releasedLogIndex = logIndex == 0 ? 1 : 0;
		return &logs[releasedLogIndex];
	}

	void cleanReleasedLog()
	{
		const int releasedLogIndex = logIndex == 0 ? 1 : 0;
		logs[releasedLogIndex].clear();
		logs[releasedLogIndex].reserve(reserve);
	}

private: 
	logVector logs[2];

	int logIndex;
	DWORD reserve;
	mutex m;

} eventsLog(1000000);

__declspec(dllexport) LRESULT CALLBACK KeyboardEvent(int nCode, WPARAM wParam, LPARAM lParam)
{
	milliseconds ms = duration_cast<milliseconds> (system_clock::now().time_since_epoch());

	KBDLLHOOKSTRUCT hooked_key = *((KBDLLHOOKSTRUCT*)lParam);

	std::stringstream record;
	record << ms.count() << " "
		<< nCode << " "
		<< wParam << " "
		<< hooked_key.vkCode << " "
		<< hooked_key.scanCode << " "
		<< hooked_key.flags << " "
		<< hooked_key.time << " ";

	eventsLog.writeRecord(ms, std::move(record));

	return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

__declspec(dllexport) LRESULT CALLBACK MouseEvent(int nCode, WPARAM wParam, LPARAM lParam)
{
	milliseconds ms = duration_cast<milliseconds> (system_clock::now().time_since_epoch());

	MSLLHOOKSTRUCT hooked_key = *((MSLLHOOKSTRUCT*)lParam);

	std::stringstream record;
	record << ms.count() << " "
		<< nCode << " "
		<< wParam << " "
		<< hooked_key.pt.x << " "
		<< hooked_key.pt.y << " "
		<< hooked_key.mouseData << " "
		<< hooked_key.flags << " "
		<< hooked_key.time << " ";

	eventsLog.writeRecord(ms, std::move(record));

	return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

void MessageLoop()
{
	MSG message;
	while (GetMessage(&message, NULL, 0, 0))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
}

DWORD WINAPI listener(LPVOID lpParam)
{
	HINSTANCE hInstance = GetModuleHandle(NULL);
	if (!hInstance) hInstance = LoadLibraryA((LPCSTR)lpParam);
	if (!hInstance) return 1;

	hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)KeyboardEvent, hInstance, NULL);
	hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC)MouseEvent, hInstance, NULL);
	MessageLoop();
	UnhookWindowsHookEx(hKeyboardHook);
	UnhookWindowsHookEx(hMouseHook);
	return 0;
}

string getLogFileName()
{
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::time_t now_c = std::chrono::system_clock::to_time_t(now - std::chrono::hours(24));
	stringstream fileName;
	fileName << "EventsLogs_";
	fileName << std::put_time(std::localtime(&now_c), "%Y-%m-%d");
	fileName << ".log";
	return fileName.str();
}

void watchWrite()
{
	while (1) {
		Sleep(DROP_ON_DISC_TIMEOUT);

		logVector *log = eventsLog.switchLogs();

		auto logFileName = getLogFileName();

		fstream logFile;
		logFile.open(logFileName, fstream::out | fstream::app );

		for (const logEntry& le : (*log))
			logFile << le.second.rdbuf() << endl;

		cout << "write" << log->size() << endl;

		eventsLog.cleanReleasedLog();

		logFile.close();
	}
}

int main(int argc, char** argv)
{
	HANDLE hListenerThread = NULL;

	hListenerThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)listener, (LPVOID)argv[0], NULL, NULL);

	/* uncomment to hide console window */
	//ShowWindow(FindWindowA("ConsoleWindowClass", NULL), false);

	watchWrite();

	if (hListenerThread) {
		return WaitForSingleObject(hListenerThread, INFINITE);
	} else {
		return 1;
	}
}