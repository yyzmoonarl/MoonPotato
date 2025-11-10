#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

void PrintLastError(const char* msg) {
    DWORD err = GetLastError();
    LPWSTR buf = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR)&buf, 0, nullptr);
    std::wcerr << L"[Error] " << msg << L" (code " << err << L"): "
               << (buf ? buf : L"(no message)") << std::endl;
    if (buf) LocalFree(buf);
}

std::wstring ToWideString(const std::string& s) {
    if (s.empty()) return std::wstring();
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    if (needed == 0) {
        needed = MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), NULL, 0);
        if (needed == 0) return std::wstring();
        std::wstring out(needed, L'\0');
        MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), &out[0], needed);
        return out;
    }
    std::wstring out(needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], needed);
    return out;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <PID> \"<command>\"\n";
        std::cerr << "Example:\n  " << argv[0] << " 1234 \"cmd.exe /c whoami > C:\\\\temp\\\\output.txt\"\n";
        return 1;
    }

    unsigned long pid = 0;
    {
        std::istringstream iss(argv[1]);
        if (!(iss >> pid) || pid == 0) {
            std::cerr << "Invalid PID\n";
            return 1;
        }
    }

    std::ostringstream cmdConcat;
    for (int i = 2; i < argc; ++i) {
        if (i > 2) cmdConcat << ' ';
        cmdConcat << argv[i];
    }
    std::string cmdlineNarrow = cmdConcat.str();
    std::wstring cmdlineWide = ToWideString(cmdlineNarrow);
    std::vector<wchar_t> cmdBuffer(cmdlineWide.begin(), cmdlineWide.end());
    cmdBuffer.push_back(L'\0');

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!hProcess) {
        PrintLastError("OpenProcess failed");
        return 1;
    }

    HANDLE hToken = NULL;
    if (!OpenProcessToken(hProcess, TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID, &hToken)) {
        PrintLastError("OpenProcessToken failed");
        CloseHandle(hProcess);
        return 1;
    }

    HANDLE hDupToken = NULL;
    if (!DuplicateTokenEx(hToken, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &hDupToken)) {
        PrintLastError("DuplicateTokenEx failed");
        CloseHandle(hToken);
        CloseHandle(hProcess);
        return 1;
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    BOOL ok = CreateProcessWithTokenW(
        hDupToken,
        LOGON_WITH_PROFILE,
        NULL,
        cmdBuffer.data(),
        CREATE_UNICODE_ENVIRONMENT,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!ok) {
        PrintLastError("CreateProcessWithTokenW failed");
        CloseHandle(hDupToken);
        CloseHandle(hToken);
        CloseHandle(hProcess);
        return 1;
    }

    std::cout << "Process created successfully. PID = " << pi.dwProcessId << std::endl;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(hDupToken);
    CloseHandle(hToken);
    CloseHandle(hProcess);
    return 0;
}
