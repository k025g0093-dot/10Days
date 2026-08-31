#pragma once
#include <fstream>
#include <string>
#include <Windows.h>
#include <chrono>
#include <format>
#include <strsafe.h>
#include <minidumpapiset.h>
extern std::ofstream logStream;

void Log(std::ostream& os, const std::string& message);
void InitializeLog();

static int Dump(EXCEPTION_POINTERS* exception);


LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception);