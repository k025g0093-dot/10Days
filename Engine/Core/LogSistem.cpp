#include "LogSistem.h"


std::ofstream logStream;

void Log(std::ostream& os, const std::string& message) {
    os << message << std::endl;
    OutputDebugStringA(message.c_str());
}

void InitializeLog() {
    auto now = std::chrono::system_clock::now();
    auto nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };
    std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);

    std::string logFilePath = std::string("logs/") + dateString + ".log";
    logStream.open(logFilePath);

}


#pragma region dump

static int Dump(EXCEPTION_POINTERS* exception) {
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps//%04d_%02d_%02d_%02d%02d.dmp",
		time.wYear, time.wMonth, time.wDay,
		time.wHour, time.wMinute);

	HANDLE dumpFileHandle = CreateFile(filePath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_READ,
		0, CREATE_ALWAYS, 0, 0);

	DWORD processID = GetCurrentProcessId();
	DWORD threadID = GetCurrentThreadId();

	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
	minidumpInformation.ThreadId = threadID;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = true;

	MiniDumpWriteDump(GetCurrentProcess(), processID, dumpFileHandle,
		MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

	CloseHandle(dumpFileHandle);
	return EXCEPTION_EXECUTE_HANDLER;
}

LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
	Dump(exception);
	return EXCEPTION_EXECUTE_HANDLER;
}
#pragma endregion
