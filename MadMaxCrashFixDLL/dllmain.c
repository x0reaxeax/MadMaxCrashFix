// Only for GoG version of Mad Max
// 
// Put dinput8.dll file inside the game folder and start the game normally.

#include <Windows.h>

#define DLLEXPORT __declspec(dllexport)
#define UNSPECIFIED_PASS
#define VOLATILE volatile

typedef enum _DI8_Return_Value {
    DI_BUFFEROVERFLOW,
    DI_DOWNLOADSKIPPED,
    DI_EFFECTRESTARTED,
    DI_NOEFFECT,
    DI_NOTATTACHED,
    DI_OK,
    DI_POLLEDDEVICE,
    DI_PROPNOEFFECT,
    DI_SETTINGSNOTSAVED,
    DI_TRUNCATED,
    DI_TRUNCATEDANDRESTARTED,
    DI_WRITEPROTECT,
    DIERR_ACQUIRED,
    DIERR_ALREADYINITIALIZED,
    DIERR_BADDRIVERVER,
    DIERR_BETADIRECTINPUTVERSION,
    DIERR_DEVICEFULL,
    DIERR_DEVICENOTREG,
    DIERR_EFFECTPLAYING,
    DIERR_GENERIC,
    DIERR_HANDLEEXISTS,
    DIERR_HASEFFECTS,
    DIERR_INCOMPLETEEFFECT,
    DIERR_INPUTLOST,
    DIERR_INVALIDPARAM,
    DIERR_MAPFILEFAIL,
    DIERR_MOREDATA,
    DIERR_NOAGGREGATION,
    DIERR_NOINTERFACE,
    DIERR_NOTACQUIRED,
    DIERR_NOTBUFFERED,
    DIERR_NOTDOWNLOADED,
    DIERR_NOTEXCLUSIVEACQUIRED,
    DIERR_NOTFOUND,
    DIERR_NOTINITIALIZED,
    DIERR_OBJECTNOTFOUND,
    DIERR_OLDDIRECTINPUTVERSION,
    DIERR_OTHERAPPHASPRIO,
    DIERR_OUTOFMEMORY,
    DIERR_READONLY,
    DIERR_REPORTFULL,
    DIERR_UNPLUGGED,
    DIERR_UNSUPPORTED,
} DI8_Return_Value;

typedef HRESULT (WINAPI *DirectInput8Create_t)(
    HINSTANCE hinst,
    DWORD dwVersion,
    REFIID riidltf,
    LPVOID * ppvOut,
    LPUNKNOWN punkOuter
);

typedef BOOL (WINAPI *DllRegisterServer_t)(
    VOID
);

typedef BOOL (WINAPI *DllUnregisterServer_t)(
    VOID
);

typedef LPVOID (WINAPI *GetdfDIJoystick_t)(
    HINSTANCE hinst,
    DWORD dwVersion,
    REFIID riidltf,
    LPVOID * ppvOut,
    LPUNKNOWN punkOuter
);

HANDLE g_hThread = NULL;
HINSTANCE g_hOriginalDll = NULL;

EXTERN_C DLLEXPORT HRESULT DirectInput8Create(
    HINSTANCE hinst,
    DWORD dwVersion,
    REFIID riidltf,
    LPVOID * ppvOut,
    LPUNKNOWN punkOuter
) {
    if (NULL == g_hOriginalDll) {
        return DIERR_OUTOFMEMORY;
    }

    DirectInput8Create_t pDirectInput8Create = (DirectInput8Create_t) GetProcAddress(
        g_hOriginalDll,
        "DirectInput8Create"
    );

    if (NULL == pDirectInput8Create) {
        return DIERR_OUTOFMEMORY;
    }

    return pDirectInput8Create(
        hinst,
        dwVersion,
        riidltf,
        ppvOut,
        punkOuter
    );
}

EXTERN_C DLLEXPORT BOOL DllRegisterServer(
    VOID
) {
    if (NULL == g_hOriginalDll) {
        return FALSE;
    }

    DllRegisterServer_t pDllRegisterServer = (DllRegisterServer_t) GetProcAddress(
        g_hOriginalDll,
        "DllRegisterServer"
    );

    if (NULL == pDllRegisterServer) {
        return FALSE;
    }

    return pDllRegisterServer();
}

EXTERN_C DLLEXPORT BOOL DllUnregisterServer(
    VOID
) {
    if (NULL == g_hOriginalDll) {
        return FALSE;
    }

    DllUnregisterServer_t pDllUnregisterServer = (DllUnregisterServer_t) GetProcAddress(
        g_hOriginalDll,
        "DllUnregisterServer"
    );

    if (NULL == pDllUnregisterServer) {
        return FALSE;
    }

    return pDllUnregisterServer();
}

EXTERN_C DLLEXPORT LPVOID GetdfDIJoystick(
    HINSTANCE hinst,
    DWORD dwVersion,
    REFIID riidltf,
    LPVOID * ppvOut,
    LPUNKNOWN punkOuter
) {
    if (NULL == g_hOriginalDll) {
        return NULL;
    }

    GetdfDIJoystick_t pGetdfDIJoystick = (GetdfDIJoystick_t) GetProcAddress(
        g_hOriginalDll,
        "GetdfDIJoystick"
    );

    if (NULL == pGetdfDIJoystick) {
        return NULL;
    }

    return pGetdfDIJoystick(
        hinst,
        dwVersion,
        riidltf,
        ppvOut,
        punkOuter
    );
}

DWORD WINAPI ThreadProc(
    LPVOID lpParam
) {
    g_hOriginalDll = LoadLibraryA("C:\\Windows\\System32\\dinput8.dll");

    DEVMODEA devMode = { 0 };
    if (!EnumDisplaySettingsA(
        NULL,
        ENUM_CURRENT_SETTINGS,
        &devMode
    )) {
        return EXIT_FAILURE;
    }

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
    if (0 != memcmp(
        lpTargetAddress,
        abOriginalBytes,
        sizeof(abOriginalBytes)
    )) {
        return EXIT_FAILURE;
    }

    DWORD dwOldProtect;
    if (!VirtualProtect(
        lpTargetAddress,
        sizeof(abPatch),
        PAGE_EXECUTE_READWRITE,
        &dwOldProtect
    )) {
        return EXIT_FAILURE;
    }

    memcpy(
        lpTargetAddress,
        abPatch,
        sizeof(abPatch)
    );

    VirtualProtect(
        lpTargetAddress,
        sizeof(abPatch),
        dwOldProtect,
        &dwOldProtect
    );

    return EXIT_SUCCESS;
}

BOOL APIENTRY DllMain( 
    HMODULE hModule,
    DWORD ul_reason_for_call,
    LPVOID lpReserved
) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            g_hThread = CreateThread(
                NULL,
                0,
                ThreadProc,
                NULL,
                0,
                NULL
            );

            if (NULL == g_hThread) {
                return FALSE;
            }

            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

