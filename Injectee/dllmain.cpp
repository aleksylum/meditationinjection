//Copyright (c) 2020 Alexandra Kravtsova (aleksylum)

#include "pch.h"
#include <windows.h>
#include "Processthreadsapi.h"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <windows.h>
#include <iostream>
#include <sqltypes.h>
#include <sql.h>
#include <sqlext.h>
#include <list>
#include <filesystem>
#include <fstream>

using namespace std;

const LPCWSTR LIBRARY = L"C:\\Users\\nhor_nhor\\Desktop\\meditation\\Meditation.Patches\\bin\\Debug\\x64\\Meditation.Patches.dll";//TODO NHOR: do relative
const char* LOG_FILE = "C:\\ProgramData\\Meditation\\Diag\\Injectee.txt";
const int MAX_LOG_SIZE = 1024;

#define SQL_RESULT_LEN 2000
#define SQL_RETURN_CODE_LEN 2000
SQLHANDLE SqlConnectHandle = NULL;
SQLHANDLE SqlStatementHandle = NULL;
SQLHANDLE SqlEnvironmentHandle = NULL;
SQLWCHAR RetConectString[SQL_RETURN_CODE_LEN];

void FreeSqlStatementHandle() {
	if (SqlStatementHandle != NULL) {
		SQLFreeHandle(SQL_HANDLE_STMT, SqlStatementHandle);
		SqlStatementHandle = NULL;
	}
}

void CloseHandles() {

	FreeSqlStatementHandle();

	if (SqlConnectHandle != NULL) {
		SQLDisconnect(SqlConnectHandle);
		SQLFreeHandle(SQL_HANDLE_DBC, SqlConnectHandle);
		SqlConnectHandle = NULL;
	}
	if (SqlEnvironmentHandle != NULL) {
		SQLFreeHandle(SQL_HANDLE_ENV, SqlEnvironmentHandle);
		SqlEnvironmentHandle = NULL;
	}
}

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

std::wstring ConvertStringToWString(const std::string& str)
{
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

BOOL CheckSqlResult(SQLRETURN result) {

	if (result != SQL_SUCCESS && result != SQL_SUCCESS_WITH_INFO && result != SQL_NO_DATA) {
		std::cout << result << std::endl;// LOG IT
		throw ("Bad SQL Result status");
		//return FALSE;
	}
	return TRUE;
}

void UpdateStatus(std::list <string> updates) {

	for (string result : updates)
	{
		try {
			wstring ws = ConvertStringToWString(result);
			SQLWCHAR* x = (SQLWCHAR*)ws.c_str();
			wcout << x << endl;
			SQLRETURN retres = SQLExecDirectW(SqlStatementHandle, x, SQL_NTS);
			CheckSqlResult(retres);
			cout << retres << endl;
		}
		catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
			Log("[EXCEPTION] During status updating ", (char*)e.what());
		}
	}
}

void Patch(HMODULE lib) {

	typedef const char* (__cdecl* _getPatchQuery)();
	auto getPatchQuery = (_getPatchQuery)GetProcAddress(lib, "_getPatchQuery");
	auto patchQuery = getPatchQuery();// return smth like $@"SELECT [value] FROM {PatchTableName} WHERE [pid] = {process.Id} AND [process_name] = '{process.ProcessName}' AND [status] = {(Int32) status}"

	wstring ws = ConvertStringToWString(patchQuery);
	SQLWCHAR* x = (SQLWCHAR*)ws.c_str();
	SQLRETURN allForPatchingResult = SQLExecDirectW(SqlStatementHandle, x, SQL_NTS);
	CheckSqlResult(allForPatchingResult);

	typedef const char* (__cdecl* _patch)(const char* a);
	auto patchMet = (_patch)GetProcAddress(lib, "_patch");

	SQLCHAR queryRes[SQL_RESULT_LEN];
	SQLLEN ptrQueryRes;

	std::list <string> results;
	while (true) {
		auto r = SQLFetch(SqlStatementHandle);
		std::cout << r;

		if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO)
			break;

		try {
			SQLGetData(SqlStatementHandle, 1, SQL_CHAR, queryRes, SQL_RESULT_LEN, &ptrQueryRes);
			cout << "\nResult:\n";
			cout << queryRes << endl;

			const char* res = patchMet((char*)queryRes);

			cout << res << endl;
			if (strlen(res)) {
				cout << res << endl;
				string s = string(res);
				results.push_back(s);
			}
		}
		catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
			Log("[EXCEPTION] During patching ", (char*)e.what());
		}
	}

	FreeSqlStatementHandle();

	SQLRETURN allocResult = SQLAllocHandle(SQL_HANDLE_STMT, SqlConnectHandle, &SqlStatementHandle);
	CheckSqlResult(allocResult);

	UpdateStatus(results);
}

void Unpatch(HMODULE lib) {
	cout << "\n TRYING TO UNPATCH:\n";
	typedef const char* (__cdecl* _getUnpatchQuery)();
	auto getUnpatchQuery = (_getUnpatchQuery)GetProcAddress(lib, "_getUnpatchQuery");
	auto unpatchQuery = getUnpatchQuery();
	cout << unpatchQuery;
	wstring ws = ConvertStringToWString(unpatchQuery);
	SQLWCHAR* x = (SQLWCHAR*)ws.c_str();
	SQLRETURN allForUnpResult = SQLExecDirectW(SqlStatementHandle, x, SQL_NTS);
	CheckSqlResult(allForUnpResult);

	typedef const char* (__cdecl* _unpatch)(const char* a);
	auto unpatchMet = (_unpatch)GetProcAddress(lib, "_unpatch");

	SQLCHAR queryRes[SQL_RESULT_LEN];
	SQLLEN ptrQueryRes;

	std::list <string> results;
	while (true) {
		auto r = SQLFetch(SqlStatementHandle);
		std::cout << r;

		if (r != SQL_SUCCESS && r != SQL_SUCCESS_WITH_INFO)
			break;

		try {
			SQLGetData(SqlStatementHandle, 1, SQL_CHAR, queryRes, SQL_RESULT_LEN, &ptrQueryRes);
			cout << "\nResult:\n";
			cout << queryRes << endl;
			const char* res = unpatchMet((char*)queryRes);
			cout << res << endl;

			if (strlen(res)) {
				cout << res << endl;
				string s = string(res);
				results.push_back(s);
			}
		}
		catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
			Log("[EXCEPTION] During unpatching ", (char*)e.what());
		}
	}

	FreeSqlStatementHandle();

	SQLRETURN allocResult = SQLAllocHandle(SQL_HANDLE_STMT, SqlConnectHandle, &SqlStatementHandle);
	CheckSqlResult(allocResult);

	UpdateStatus(results);
	cout << "\n  UNPATCH SUCCEEDED:\n";
}

BOOL Execute() {

	if (SQL_SUCCESS != SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &SqlEnvironmentHandle))
		return FALSE;
	if (SQL_SUCCESS != SQLSetEnvAttr(SqlEnvironmentHandle, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0))
		return FALSE;
	if (SQL_SUCCESS != SQLAllocHandle(SQL_HANDLE_DBC, SqlEnvironmentHandle, &SqlConnectHandle))
		return FALSE;

	HMODULE lib = LoadLibrary(LIBRARY);

	typedef const char* (__cdecl* _getConnString)();
	auto getConnString = (_getConnString)GetProcAddress(lib, "_getConnString");
	const char* connRes = getConnString();
	wstring ws = ConvertStringToWString(connRes);

	switch (SQLDriverConnectW(SqlConnectHandle,
		NULL,
		(SQLWCHAR*)ws.c_str(),
		SQL_NTS,
		RetConectString,
		1024,
		NULL,
		SQL_DRIVER_NOPROMPT)) {
	case SQL_SUCCESS:
	case SQL_SUCCESS_WITH_INFO:
		cout << "Successfully connected to SQL Server";
		cout << "\n";
		break;
	default:
		throw ("Could not connect to SQL Server");
	}

	SQLRETURN allocResult = SQL_SUCCESS != SQLAllocHandle(SQL_HANDLE_STMT, SqlConnectHandle, &SqlStatementHandle);
	CheckSqlResult(allocResult);

	Patch(lib);

	FreeSqlStatementHandle();

	SQLRETURN allocResult2 = SQL_SUCCESS != SQLAllocHandle(SQL_HANDLE_STMT, SqlConnectHandle, &SqlStatementHandle);
	CheckSqlResult(allocResult2);

	Unpatch(lib);

	return TRUE;
}

extern "C" __declspec(dllexport) BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		cout << "HiHi I am here now!!!";
		try
		{
			Execute();
		}
		catch (const std::exception& e) {

			std::cerr << e.what() << std::endl;
			Log("[EXCEPTION] During DllMain executing ", (char*)e.what());

		}
		CloseHandles();
		break;
		
	case DLL_PROCESS_DETACH:
		break;

	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;
	}
	return  TRUE;
}

