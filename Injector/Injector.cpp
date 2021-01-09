//Copyright (c) 2020 Alexandra Kravtsova (aleksylum)

#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include "tchar.h"
#include <atlstr.h>
#include <filesystem>
#include <fstream>

const char* TARGET_DLL = "\\Injectee.dll";
const char* LOG_FILE = "C:\\ProgramData\\Meditation\\Diag\\Injector.txt";
const int MAX_LOG_SIZE = 1024;

void LogMessage(const char* filetext) {
	std::ofstream myfile;
	myfile.open(LOG_FILE, std::fstream::app);
	myfile << filetext;
	myfile << "\n";
	myfile.close();
}

void Log(const char* prefix, char* data) {
	char message[MAX_LOG_SIZE];
	strcpy(message, prefix);
	strcat(message, data);
	LogMessage(message);
}

BOOL _tmain(int argc, CHAR* argv[])
{
	std::wcout << "start\n";
	CHAR* dllPath;
	DWORD dllSize;
	DWORD processId = strtoul(argv[1], &argv[2], 10);
	std::wcout << processId;
	try
	{
		CHAR path[MAX_PATH];
		GetCurrentDirectory(MAX_PATH, path);

		dllPath = (CHAR*)malloc(strlen(path) + strlen(TARGET_DLL) + 1);

		strcpy(dllPath, path);
		strcat(dllPath, TARGET_DLL);

		dllSize = strlen(dllPath) + 1;
	
	}
	catch (std::exception& e)
	{
		Log("[EXCEPTION] During getting loadable dll ", (char*)TARGET_DLL);
		LogMessage(e.what());
		return FALSE;
	}
	
	HANDLE processHandle;
	try
	{
		processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
		if(processHandle == NULL)
			throw std::exception("Handle is NULL for process");
	}
	catch (std::exception& e)
	{
		Log("[EXCEPTION] During getting handle for process ", argv[1]);
		LogMessage(e.what());
		return FALSE;
	}

	LPVOID virtualAlloc;

	try
	{
		virtualAlloc = VirtualAllocEx(processHandle, NULL, dllSize,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (virtualAlloc == NULL)
			throw std::exception("Can not allocate memory in target process");

	}
	catch (std::exception& e)
	{
		Log("[EXCEPTION] During allocating memory in target process ", argv[1]);
		LogMessage(e.what());
		CloseHandle(processHandle);
		return FALSE;
	}

	BOOL writeResult;
	HMODULE kernel32Handle;
	LPVOID loadLibraryA;
	HANDLE treadHandle;
	try 
	{
		writeResult = WriteProcessMemory(processHandle, virtualAlloc, dllPath, dllSize, NULL);
		if (!writeResult)
			throw std::exception("Can not write dll in target process");

		kernel32Handle = GetModuleHandle(_TEXT("kernel32.dll"));
		if(kernel32Handle == NULL)
			throw std::exception("Handle of kernel32.dll is NULL");

		loadLibraryA = GetProcAddress(kernel32Handle, "LoadLibraryA");//https://docs.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibrarya
		if (loadLibraryA == NULL)
			throw std::exception("Handle of LoadLibraryA is NULL");

		treadHandle = CreateRemoteThread(processHandle, NULL, 0, LPTHREAD_START_ROUTINE(loadLibraryA), virtualAlloc, 0, 0);
		if (treadHandle == NULL)
			throw std::exception("Thread handle is NULL");
	}
	catch (std::exception& e)
	{
		Log("[EXCEPTION] During getting handle of kernel32.dll ", argv[1]);
		LogMessage(e.what());
		VirtualFreeEx(processHandle, virtualAlloc, dllSize, MEM_RELEASE);
		CloseHandle(processHandle);
		return FALSE;
	}

	CloseHandle(treadHandle);
	VirtualFreeEx(processHandle, virtualAlloc, dllSize, MEM_RELEASE);
	CloseHandle(processHandle);
	std::wcout << "\n succeeded";

	return TRUE;
}
