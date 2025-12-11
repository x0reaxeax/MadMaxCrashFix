#include  <Windows.h>
#include "../MinHook/include/MinHook.h"

#include <stdio.h>

#define LOG_PATH            "MadMaxHook.log"
#define LOG_BUFFER_SIZE     8192
#define MONITOR_COUNT       2
#define MM_MAX_MODE_ROWS    256

typedef unsigned long long QWORD, QWORD64, *PQWORD, *PQWORD64;

typedef struct _MM_MODE_ROW {
    DWORD Width;        // v154
    DWORD Height;       // v157
    DWORD RefreshOrId;  // v153 (refresh Hz or some ID)
    DWORD ModeIndex;    // v155 (original DXGI mode index / score)
    BYTE  FlagA;        // v252
    BYTE  FlagB;        // v156
    BYTE  Pad[2];       // align to 20 bytes total
} MM_MODE_ROW, *PMM_MODE_ROW;


typedef struct _MM_MONITOR_BLOCK {
    MM_MODE_ROW Rows[MM_MAX_MODE_ROWS]; // 256 * 20 = 0x1400 bytes
    DWORD       RowCount;          // @ +0x1400 – the field that gets smashed
    // ... maybe more stuff after, we don’t care here
} MM_MONITOR_BLOCK, *PMM_MONITOR_BLOCK;

typedef struct _MONITOR_TRACK {
    BYTE  *pRowBase;            // inferred base address of Rows[0]
    DWORD dwLastRowIndex;       // last rowIdx observed
} MONITOR_TRACK, *PMONITOR_TRACK;

typedef void (__fastcall *PFN_GetModeInfo)(
    void *pEngineCtx,
    DWORD monitorIdx,
    DWORD modeIdx,
    DWORD *pWidthOut,
    DWORD *pHeightOut,
    DWORD *pRefreshOut
);

static MONITOR_TRACK g_MonTrack[MAX_MONITORS];

static DWORD *g_RowCountPtr[MAX_MONITORS] = { 0 };

static PFN_GetModeInfo g_OriginalGetModeInfo = NULL;

static GLOBALHANDLE g_hThread = NULL;
static GLOBALHANDLE g_hLogFile = INVALID_HANDLE_VALUE;
static HWND g_hConsole = NULL;
static PCHAR g_szLogBuffer = NULL;

static QWORD64 g_qwRowCountAddr = 0;
static QWORD g_qwModeAddCounter = 0;
static QWORD g_qwModeEnumCounter = 0;

HANDLE *InitalizeLogFile(void) {
    g_hLogFile = CreateFileA(
        LOG_PATH,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (INVALID_HANDLE_VALUE == g_hLogFile) {
        fprintf(
            stderr,
            "[-] Failed to create log file '%s': E%lu\n",
            LOG_PATH,
            GetLastError()
        );
        return NULL;
    }

    g_szLogBuffer = (PCHAR) HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        LOG_BUFFER_SIZE
    );

    return &g_hLogFile;
}

VOID LogMessage(
    LPCSTR szFormat,
    ...
) {
    static CHAR szBackupBuffer[1024] = { 0 };
    SIZE_T cbBufSiz = LOG_BUFFER_SIZE;
    PCHAR szTargetBuffer = g_szLogBuffer;

    if (NULL == g_szLogBuffer) {
        ZeroMemory(szBackupBuffer, sizeof(szBackupBuffer));
        szTargetBuffer = szBackupBuffer;
        cbBufSiz = sizeof(szBackupBuffer);
    }
    ZeroMemory(szTargetBuffer, cbBufSiz);

    va_list args;
    va_start(args, szFormat);
    vsnprintf_s(szTargetBuffer, cbBufSiz, _TRUNCATE, szFormat, args);
    va_end(args);

    if (INVALID_HANDLE_VALUE != g_hLogFile) {
        DWORD dwBytesWritten = 0;
        WriteFile(
            g_hLogFile,
            szTargetBuffer,
            (DWORD) strlen(szTargetBuffer),
            &dwBytesWritten,
            NULL
        );
    }

    printf("%s", szTargetBuffer);
}

/*
0000000141E389C8 | 48:8B4D 80               | mov rcx,qword ptr ss:[rbp-80]           | pEngineCtx
0000000141E389CC | 44:3B91 00140000         | cmp r10d,dword ptr ds:[rcx+1400]        | [pEngineCtx + 0x1400] = RowCount
0000000141E389D3 | 4C:8B45 C0               | mov r8,qword ptr ss:[rbp-40]            |
0000000141E389D7 | 0F82 FDFEFFFF            | jb madmax.141E388DA                     |
*/

VOID __fastcall hk_GetModeInfo(
    PVOID pEngineCtx,
    DWORD dwMonitorIdx,
    DWORD dwModeIdx,
    DWORD *pWidthOut,
    DWORD *pHeightOut,
    DWORD *pRefreshOut
) {
    g_OriginalGetModeInfo(pEngineCtx, dwMonitorIdx, dwModeIdx, pWidthOut, pHeightOut, pRefreshOut);

    if (dwMonitorIdx >= MAX_MONITORS) {
        return;
    }

    MONITOR_TRACK *pMonitorTrack = &g_MonTrack[dwMonitorIdx];

    // base of Rows[0]
    if (NULL != pWidthOut && dwModeIdx == 0 && pMonitorTrack->pRowBase == NULL) {
        pMonitorTrack->pRowBase = (BYTE*) pWidthOut;

        // derive RowCount pointer
        g_RowCountPtr[dwMonitorIdx] = (DWORD*) (pMonitorTrack->pRowBase + 0x1400);
        printf(
            "[RowCountAddress] %p val=%lu\n",
            g_RowCountPtr[dwMonitorIdx],
            *g_RowCountPtr[dwMonitorIdx]
        );
    }

    INT  iRowIndex = -1;
    BOOL bInBounds = FALSE;
    BOOL bAliasCount = FALSE;

    if (pMonitorTrack->pRowBase && pWidthOut) {
        ptrdiff_t delta = (BYTE*) pWidthOut - pMonitorTrack->pRowBase;

        if (delta >= 0 && (delta % sizeof(MM_MODE_ROW)) == 0) {
            iRowIndex = (int) (delta / sizeof(MM_MODE_ROW));
            bInBounds = (iRowIndex >= 0 && iRowIndex < MM_MAX_MODE_ROWS);
        }

        // alias detection using snatched RowCountPtr (see disasm above)
        if (NULL != g_RowCountPtr[dwMonitorIdx] && pWidthOut == (DWORD*) g_RowCountPtr[dwMonitorIdx]) {
            bAliasCount = TRUE;
        }
    }


    // log OOB or any case above iRowIdx > 255
    if (!bInBounds || iRowIndex > 255 || bAliasCount) {
        CHAR szFormatBuf[512] = { 0 };
        snprintf(
            szFormatBuf,
            sizeof(szFormatBuf),
            "[OOB: RowAdd] %04llu: mon=%4u mode=%4u rowIdx=%4d inBounds=%d aliasRowCount=%d "
            "w=%4u h=%4u hz=%4u "
            "[pWidthOut=%p]\n",
            g_qwModeEnumCounter++,
            dwMonitorIdx,
            dwModeIdx,
            iRowIndex,
            bInBounds,
            bAliasCount,
            pWidthOut ? *pWidthOut : 0,
            pHeightOut ? *pHeightOut : 0,
            pRefreshOut ? *pRefreshOut : 0,
            pWidthOut
        );
        LogMessage(szFormatBuf);
    }

    if (bInBounds && (DWORD) iRowIndex > pMonitorTrack->dwLastRowIndex) {
        pMonitorTrack->dwLastRowIndex = (DWORD) iRowIndex;

        CHAR szFormatBuf[512] = { 0 };
        snprintf(
            szFormatBuf,
            sizeof(szFormatBuf),
            "[IB:  RowAdd] %04llu: mon=%4u mode=%4u rowIdx=%4d w=%4u h=%4u hz=%4u\n",
            g_qwModeAddCounter++,
            dwMonitorIdx,
            dwModeIdx,
            iRowIndex,
            pWidthOut ? *pWidthOut : 0,
            pHeightOut ? *pHeightOut : 0,
            pRefreshOut ? *pRefreshOut : 0
        );
        LogMessage(szFormatBuf);
    }
}
DWORD WINAPI ThreadEntry(
    LPVOID lpParam
) {
    FILE *lpStdout = NULL, *lpStderr = NULL, *lpStdin = NULL;

    AllocConsole();
    freopen_s(&lpStdout, "CONOUT$", "w", stdout);
    freopen_s(&lpStderr, "CONOUT$", "w", stderr);
    freopen_s(&lpStdin, "CONIN$", "r", stdin);

    g_hConsole = GetConsoleWindow();

    if (NULL != InitalizeLogFile()) {
        printf("[+] Log file initialized at '%s'\n", LOG_PATH);
    }

    MH_STATUS mhStatus = MH_Initialize();
    if (MH_OK != mhStatus) {
        LogMessage("[-] MH_Initialize failed: %d\n", mhStatus);
        return EXIT_FAILURE;
    }

    printf("[+] MinHook initialized.\n");

    mhStatus = MH_CreateHook(
        (LPVOID) 0x142563500,
        &hk_GetModeInfo,
        (LPVOID*) &g_OriginalGetModeInfo
    );

    if (MH_OK != mhStatus) {
        LogMessage("[-] MH_CreateHook failed for GetModeInfo: %d\n", mhStatus);
        return EXIT_FAILURE;
    }

    if (NULL == g_OriginalGetModeInfo) {
        LogMessage("[-] Original GetModeInfo pointer is NULL!\n");
        return EXIT_FAILURE;
    }

    mhStatus = MH_EnableHook(
        (LPVOID) 0x142563500
    );

    if (MH_OK != mhStatus) {
        LogMessage("[-] MH_EnableHook failed for GetModeInfo: %d\n", mhStatus);
        return EXIT_FAILURE;
    }

    printf(
        "[+] Hooked sub_142563500 (GetModeInfo) [Hook: %p | Orig: %p]\n",
        &hk_GetModeInfo,
        &g_OriginalGetModeInfo
    );

    return EXIT_SUCCESS;
}

BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,
    DWORD fdwReason,
    LPVOID lpvReserved
) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            g_hThread = CreateThread(
                NULL,
                0,
                ThreadEntry,
                NULL,
                0,
                NULL
            );

            if (NULL == g_hThread) {
                return FALSE;
            }

            break;
        default:
            break;
    }
    return TRUE;
}