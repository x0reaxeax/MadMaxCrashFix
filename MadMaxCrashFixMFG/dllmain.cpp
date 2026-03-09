/// This variation of MadMaxCrashFix supports specifying of custom resolutions,
///  which can be useful if desired resolution got cut off by <256 mode clamping

#include "dxgidef.hpp"

#include <cstdio>
#include <string>

HMODULE g_hSystemDxgi = nullptr;

LPVOID LoadSystemDxgi(void) {
    if (nullptr != g_hSystemDxgi) {
        return g_hSystemDxgi;
    }

    LPCWSTR wszRealModulePath = L"C:\\Windows\\System32\\dxgi.dll";
    g_hSystemDxgi = LoadLibraryW(wszRealModulePath);

    if (nullptr == g_hSystemDxgi) {
        MessageBoxA(nullptr, "Failed to load system dxgi.dll", "Error", MB_ICONERROR);
        ExitProcess(EXIT_FAILURE);
    }

    return g_hSystemDxgi;
}

FARPROC ResolveDxgiProc(
    _In_ LPCSTR szProcName
) {
    if (nullptr == LoadSystemDxgi()) {
        return nullptr;
    }

    FARPROC pfnProc = GetProcAddress(
        g_hSystemDxgi,
        szProcName
    );

    if (nullptr == pfnProc) {
        MessageBoxW(
            nullptr,
            (L"Failed to resolve " + std::wstring(szProcName, szProcName + strlen(szProcName)) + L" from system dxgi.dll").c_str(),
            L"RE6 Crash Fix",
            MB_ICONERROR
        );
        ExitProcess(EXIT_FAILURE);
    }
    return pfnProc;
}

BOOL APIENTRY DllMain(
    HMODULE hInstDll,
    DWORD fdwReason,
    LPVOID lpvReserved
) {
    UNREFERENCED_PARAMETER(hInstDll);
    UNREFERENCED_PARAMETER(fdwReason);
    UNREFERENCED_PARAMETER(lpvReserved);
    return TRUE;
}