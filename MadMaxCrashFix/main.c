// Only for GoG version of Mad Max
// 
// Put the exe file inside the game folder and run it.
// 
// Steam version to this day comes with Denuvo,
// but is literally unplayable, 
// because apparently it's too much work to patch an instruction,
// after hoarding money for years now for a game that doesnt work.
// 
// Fuck your mother, Denuvo.

#include <Windows.h>
#include <stdio.h>

int main(void) {

    DEVMODEA devMode = { 0 };
    if (!EnumDisplaySettingsA(
        NULL,
        ENUM_CURRENT_SETTINGS,
        &devMode
    )) {
        fprintf(
            stderr,
            "[-] EnumDisplaySettingsA failed: E%lu\n",
            GetLastError()
        );
        return EXIT_FAILURE;
    }

    printf(
        "[+] Refresh rate: %lu Hz\n",
        devMode.dmDisplayFrequency
    );

    LPCSTR szTargetExe = "MadMax.exe";
    STARTUPINFOA si = { 0 };
    PROCESS_INFORMATION pi = { 0 };

    if (!CreateProcessA(
        szTargetExe,
        NULL,
        NULL,
        NULL,
        FALSE,
        CREATE_SUSPENDED,
        NULL,
        NULL,
        &si,
        &pi
    )) {
        fprintf(
            stderr,
            "[-] CreateProcessA failed: E%lu\n",
            GetLastError()
        );
        return EXIT_FAILURE;
    }

    printf(
        "[+] %s (PID: %lu) created in suspended state\n",
        szTargetExe,
        pi.dwProcessId
    );

    printf("[*] Patching MadMax.exe\n");

    LPVOID lpTargetAddress = (LPVOID) 0x140F17E5E;
    BYTE abOriginalBytes[] = { //  div     dword ptr ds : [rcx + r8 + 0xC]
        0x42, 0xF7, 0x74, 0x01, 0x0C
    };

    BYTE abPatch[] = { // mov eax, refreshrate
        0xB8, 0x00, 0x00, 0x00, 0x00
    };

    memcpy(
        &abPatch[1],
        &devMode.dmDisplayFrequency,
        sizeof(devMode.dmDisplayFrequency)
    );

    // Verify original bytes
    BYTE abVerifyBytes[sizeof(abOriginalBytes)] = { 0 };
    SIZE_T nBytesRead = 0;
    if (!ReadProcessMemory(
        pi.hProcess,
        lpTargetAddress,
        abVerifyBytes,
        sizeof(abOriginalBytes),
        &nBytesRead
    )) {
        fprintf(
            stderr,
            "[-] ReadProcessMemory failed: E%lu\n",
            GetLastError()
        );
        return EXIT_FAILURE;
    }

    if (0 != memcmp(
        abVerifyBytes,
        abOriginalBytes,
        sizeof(abOriginalBytes)
    )) {
        fprintf(
            stderr,
            "[-] Original bytes mismatch\n"
        );
        return EXIT_FAILURE;
    }

    // Patch target process
    DWORD dwOldProtect;
    if (!VirtualProtectEx(
        pi.hProcess,
        lpTargetAddress,
        sizeof(abPatch),
        PAGE_EXECUTE_READWRITE,
        &dwOldProtect
    )) {
        fprintf(
            stderr,
            "[-] VirtualProtectEx failed: E%lu\n",
            GetLastError()
        );
        return EXIT_FAILURE;
    }

    if (!WriteProcessMemory(
        pi.hProcess,
        lpTargetAddress,
        abPatch,
        sizeof(abPatch),
        &nBytesRead
    )) {
        fprintf(
            stderr,
            "[-] WriteProcessMemory failed: E%lu\n",
            GetLastError()
        );
        return EXIT_FAILURE;
    }

    if (!VirtualProtectEx(
        pi.hProcess,
        lpTargetAddress,
        sizeof(abPatch),
        dwOldProtect,
        &dwOldProtect
    )) {
        fprintf(
            stderr,
            "[-] VirtualProtectEx failed: E%lu\n",
            GetLastError()
        );
        return EXIT_FAILURE;
    }

    printf("[+] MadMax.exe patched\n");

    ResumeThread(pi.hThread);

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return EXIT_SUCCESS;
}