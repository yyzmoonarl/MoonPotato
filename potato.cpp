// build: Visual Studio (MSVC) 或 MSBuild
// 编译示例 (Developer Command Prompt):
// cl /EHsc /W4 /MD your_program.cpp

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <limits>

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
        // fallback to ANSI code page if conversion fails
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

int wmain_wrapper(int argc, wchar_t* argvW[]) {
    // If we want to support wide entry on Windows, but we'll primarily use main below.
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "用法: " << argv[0] << " <PID> \"<command line>\"\n";
        std::cerr << "示例: " << argv[0] << " 1234 \"cmd.exe /c whoami > C:\\temp\\output.txt\"\n";
        return 1;
    }

    // 解析 PID
    unsigned long pid = 0;
    {
        std::istringstream iss(argv[1]);
        if (!(iss >> pid) || pid == 0) {
            std::cerr << "无效的 PID: " << argv[1] << std::endl;
            return 1;
        }
    }

    // 合并 argv[2..] 为一条命令行（以防用户没有把整条命令放入单个引号）
    std::ostringstream cmdConcat;
    for (int i = 2; i < argc; ++i) {
        if (i > 2) cmdConcat << ' ';
        cmdConcat << argv[i];
    }
    std::string cmdlineNarrow = cmdConcat.str();
    if (cmdlineNarrow.empty()) {
        std::cerr << "命令行为空\n";
        return 1;
    }

    // 将命令行转换为宽字符并放入可修改缓冲区
    std::wstring cmdlineWide = ToWideString(cmdlineNarrow);
    if (cmdlineWide.empty()) {
        std::cerr << "命令行到宽字符转换失败\n";
        return 1;
    }
    std::vector<wchar_t> cmdBuffer(cmdlineWide.begin(), cmdlineWide.end());
    cmdBuffer.push_back(L'\0'); // CreateProcessWithTokenW 要求可写缓冲区

    // 打开目标进程
    DWORD targetPid = static_cast<DWORD>(pid);
    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, targetPid);
    if (processHandle == NULL) {
        // 有些系统/权限下可能需要更高权限才能打开进程
        PrintLastError("OpenProcess 失败");
        return 1;
    }

    // 打开进程令牌
    HANDLE tokenHandle = NULL;
    if (!OpenProcessToken(processHandle, TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID, &tokenHandle)) {
        PrintLastError("OpenProcessToken 失败");
        CloseHandle(processHandle);
        return 1;
    }

    // 复制为主令牌（Primary token），以便用 CreateProcessWithTokenW 创建新进程
    HANDLE duplicateTokenHandle = NULL;
    if (!DuplicateTokenEx(tokenHandle, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &duplicateTokenHandle)) {
        PrintLastError("DuplicateTokenEx 失败");
        CloseHandle(tokenHandle);
        CloseHandle(processHandle);
        return 1;
    }

    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInformation;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    ZeroMemory(&processInformation, sizeof(processInformation));
    startupInfo.cb = sizeof(startupInfo);

    // CreateProcessWithTokenW - 使用 LOGON_WITH_PROFILE 以便加载用户配置文件（可根据需要改为 0）
    BOOL created = CreateProcessWithTokenW(
        duplicateTokenHandle,
        LOGON_WITH_PROFILE,
        NULL,                 // lpApplicationName
        cmdBuffer.data(),     // lpCommandLine (必须是可写缓冲区)
        CREATE_UNICODE_ENVIRONMENT,
        NULL,                 // lpEnvironment - NULL 表示使用令牌的环境
        NULL,                 // lpCurrentDirectory
        &startupInfo,
        &processInformation
    );

    if (!created) {
        PrintLastError("CreateProcessWithTokenW 失败");
        CloseHandle(duplicateTokenHandle);
        CloseHandle(tokenHandle);
        CloseHandle(processHandle);
        return 1;
    }

    std::cout << "子进程创建成功，PID = " << processInformation.dwProcessId << std::endl;

    // 可选择：不等待子进程结束，立即关闭句柄并返回
    // 如果需要等待并显示退出码，取消下面注释
    /*
    WaitForSingleObject(processInformation.hProcess, INFINITE);
    DWORD exitCode = 0;
    if (GetExitCodeProcess(processInformation.hProcess, &exitCode)) {
        std::cout << "子进程退出码: " << exitCode << std::endl;
    }
    */

    // 关闭句柄
    CloseHandle(processInformation.hThread);
    CloseHandle(processInformation.hProcess);
    CloseHandle(duplicateTokenHandle);
    CloseHandle(tokenHandle);
    CloseHandle(processHandle);

    return 0;
}
