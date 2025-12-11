// Only for Steam (Denuvo) version of Mad Max
//  Target EXE:
//   * MadMax.exe 1.0.3.0 (76,814,792 bytes)
//   * sha256: cd719a721db92575aa32ff01e07a590f47dd34b702b1bcd98ec698a07358bb0d
// 
// Put XInput9_1_0.dll file inside the game folder and start the game normally.
// Do NOT combine with the dinput8.dll or any other MadMaxCrashFix variation.

#include <Windows.h>
#include "MadMaxSpec.h"

#include <stdio.h>

#include "../MinHook/include/MinHook.h"

#define _NO_CACHED_REFRESH_RATE // disable caching of refresh rate (comment out to enable caching)

#define FUNC_GETMODEINFO_ADDR   0x142563500ULL
#define INVALID_REFRESH_RATE    0xFFFFFFFFU
#define SAFE_REFRESH_RATE       60U

#define TRAMPOLINE_SIZE         14

HANDLE g_hThread = NULL;
HINSTANCE g_hOriginalDll = NULL;

DWORD** g_apRowCount = NULL;
DWORD g_dwMaxMonitors = 0;

STATIC PMONITOR_TRACK g_pMonTrack = NULL;
STATIC PMM_MODE_ROW   g_pOverflowRow = NULL;

#ifndef _NO_CACHED_REFRESH_RATE
STATIC DWORD g_dwCachedRefreshRate = INVALID_REFRESH_RATE;
#endif // _NO_CACHED_REFRESH_RATE

PFN_GetModeInfo OriginalGetModeInfo = NULL;

/*
 * [sub_142563500 (GetModeInfo)]:
    0000000142563500 | 53               | push rbx |
    0000000142563501 | 56               | push rsi |
    0000000142563502 | 57               | push rdi |
    0000000142563503 | 48:83EC 40       | sub rsp,40 |
    0000000142563507 | 48:8B05 7AB028FF | mov rax,qword ptr ds:[1417EE588] |

*/
BYTE g_abOriginalBytes[] = {
    0x53,                                           // PUSH RBX
    0x56,                                           // PUSH RSI
    0x57,                                           // PUSH RDI
    0x48, 0x83, 0xEC, 0x40,                         // SUB RSP, 0x40
    0x48, 0x8B, 0x05, 0x7A, 0xB0, 0x28, 0xFF        // MOV RAX, QWORD PTR DS:[1417EE588]
};
static_assert(sizeof(g_abOriginalBytes) == 14, "Original bytes size incorrect");

DWORD GetCurrentScreenRefreshRate(VOID) {
    DEVMODEA devMode = { 0 };
    if (!EnumDisplaySettingsA(
        NULL,
        ENUM_CURRENT_SETTINGS,
        &devMode
    )) {
        return SAFE_REFRESH_RATE;       // fallback to presumably safe refresh rate 60Hz
    }

    return devMode.dmDisplayFrequency;
}

VOID __fastcall __hk_GetModeInfo(
    PVOID pEngineCtx,
    DWORD dwMonitorIdx,
    DWORD dwModeIdx,
    DWORD* pWidthOut,
    DWORD* pHeightOut,
    DWORD* pRefreshOut
) {
    if (dwMonitorIdx >= g_dwMaxMonitors) {
        OriginalGetModeInfo(
            pEngineCtx,
            dwMonitorIdx,
            dwModeIdx,
            pWidthOut,
            pHeightOut,
            pRefreshOut
        );

        return;
    }

    MONITOR_TRACK* pMonTrack = &g_pMonTrack[dwMonitorIdx];

    // infer rowBase on first call
    if (NULL == pMonTrack->pRowBase && dwModeIdx == 0 && NULL != pWidthOut) {
        pMonTrack->pRowBase = (PBYTE) pWidthOut;
        g_apRowCount[dwMonitorIdx] = (PDWORD) (pMonTrack->pRowBase + 0x1400);
    }

    // calc rowIdx
    INT iRowIdx = -1;                   // row index
    BOOL bInBounds = FALSE;             // flag for out of bounds access

    PDWORD pdwWidthOut = pWidthOut;
    PDWORD pdwHeightOut = pHeightOut;
    PDWORD pdwRefreshOut = pRefreshOut;

    if (NULL != pMonTrack->pRowBase && NULL != pWidthOut) {
        ptrdiff_t qwDelta = (LPBYTE) pWidthOut - pMonTrack->pRowBase;
        if (qwDelta >= 0 && (qwDelta % sizeof(MM_MODE_ROW)) == 0) {
            iRowIdx = (INT) (qwDelta / sizeof(MM_MODE_ROW));
            bInBounds = (iRowIdx >= 0 && iRowIdx < MM_MAX_MODE_ROWS);
        }
    }

    // track last observed safe row idx
    if (bInBounds && iRowIdx > (INT) pMonTrack->dwLastRowIndex) {
        pMonTrack->dwLastRowIndex = (DWORD) iRowIdx;
    }

    // redirect OOB writes to hell
    if (!bInBounds && NULL != pMonTrack->pRowBase) {
        PMM_MODE_ROW pOverflowRow = &g_pOverflowRow[dwMonitorIdx];
        // OOB - redirect outputs to overflow row
        pdwWidthOut = &pOverflowRow->dwWidth;
        pdwHeightOut = &pOverflowRow->dwHeight;
        pdwRefreshOut = &pOverflowRow->dwRefreshOrId;
    }

    OriginalGetModeInfo(
        pEngineCtx,
        dwMonitorIdx,
        dwModeIdx,
        pdwWidthOut,
        pdwHeightOut,
        pdwRefreshOut
    );

    // sanitize potential garbo returns
    if (NULL != pdwRefreshOut && bInBounds) {
        DWORD dwRefreshRate = *pRefreshOut;
        // man i swear if someone's playing mad max on a 1000Hz+ monitor..
        if (dwRefreshRate == 0 || dwRefreshRate > 1000) {
#ifndef _NO_CACHED_REFRESH_RATE
            *pdwRefreshOut = (INVALID_REFRESH_RATE != g_dwCachedRefreshRate)
                ? g_dwCachedRefreshRate
                : GetCurrentScreenRefreshRate();
#else
            *pdwRefreshOut = GetCurrentScreenRefreshRate();
#endif // _NO_CACHED_REFRESH_RATE
        }
    }
}

BOOL InstallHook(
    VOID
) {
    MH_STATUS mhStatus = MH_Initialize();

    if (MH_OK != mhStatus) {
        fprintf(
            stderr,
            "[ERROR] MH_Initialize failed with status code %d.\n",
            mhStatus
        );
        return FALSE;
    }

    // wait for stoopid denuvo to decrypt game memory
    while (0 != memcmp(
        (LPVOID) FUNC_GETMODEINFO_ADDR,
        g_abOriginalBytes,
        sizeof(g_abOriginalBytes)
    )) {
        Sleep(50);
    }

    mhStatus = MH_CreateHook(
        (LPVOID) FUNC_GETMODEINFO_ADDR,
        &__hk_GetModeInfo,
        (LPVOID*) &OriginalGetModeInfo
    );

    if (MH_OK != mhStatus) {
        fprintf(
            stderr,
            "[ERROR] MH_CreateHook failed with status code %d.\n",
            mhStatus
        );
        return FALSE;
    }

    mhStatus = MH_EnableHook(
        (LPVOID) FUNC_GETMODEINFO_ADDR
    );

    return TRUE;
}

VOID RemoveHook(
    VOID
) {
    MH_DisableHook(
        (LPVOID) FUNC_GETMODEINFO_ADDR
    );

    MH_RemoveHook(
        (LPVOID) FUNC_GETMODEINFO_ADDR
    );

    MH_Uninitialize();
}

DWORD WINAPI ThreadProc(
    LPVOID lpParam
) {
#ifdef _DEBUG
    FILE* lpStdout = NULL, * lpStderr = NULL, * lpStdin = NULL;

    AllocConsole();
    freopen_s(&lpStdout, "CONOUT$", "w", stdout);
    freopen_s(&lpStderr, "CONOUT$", "w", stderr);
    freopen_s(&lpStdin, "CONIN$", "r", stdin);
#endif

#ifndef _NO_CACHED_REFRESH_RATE
    g_dwCachedRefreshRate = GetCurrentScreenRefreshRate();
#endif // _NO_CACHED_REFRESH_RATE

    // its okay if this fails, the export proxies have guardrails
    g_hOriginalDll = LoadLibraryA("C:\\Windows\\System32\\XInput9_1_0.dll");

    // Get largest number of DXGI outputs (monitors) across all adapters
    g_dwMaxMonitors = GetDxgiOutputCount();

    printf(
        "[INFO] Detected %lu monitor(s) in the system.\n",
        g_dwMaxMonitors
    );

    // dont free this, `GetModeInfo` runs all the time while the game is running
    g_pMonTrack = (PMONITOR_TRACK) HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(MONITOR_TRACK) * g_dwMaxMonitors
    );

    g_apRowCount = (PDWORD*) HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(PDWORD) * g_dwMaxMonitors
    );

    g_pOverflowRow = (PMM_MODE_ROW) HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(MM_MODE_ROW) * g_dwMaxMonitors
    );

    if (NULL == g_pMonTrack || NULL == g_apRowCount || NULL == g_pOverflowRow) {
        fprintf(
            stderr,
            "[ERROR] Failed to allocate monitor tracking structures.\n"
        );

        return EXIT_FAILURE;
    }

    if (!InstallHook()) {
        fprintf(
            stderr,
            "[ERROR] Failed to install GetModeInfo inline hook.\n"
        );
        return EXIT_FAILURE;
    }

    printf(
        "[INFO] Mad Max monitor mode crash fix applied successfully.\n"
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