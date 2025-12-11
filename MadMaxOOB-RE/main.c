#include <Windows.h>
#ifdef __INTEL_LLVM_COMPILER
#include <assert.h>
#endif // __INTEL_LLVM_COMPILER
#include <dxgi.h>

typedef unsigned long long QWORD, QWORD64, *PQWORD, *PQWORD64;

#ifndef __dxgitype_h__
typedef struct DXGI_MODE_DESC {
    UINT Width;
    UINT Height;
    DXGI_RATIONAL RefreshRate;
    DXGI_FORMAT Format;
    DXGI_MODE_SCANLINE_ORDER ScanlineOrdering;
    DXGI_MODE_SCALING Scaling;
} DXGI_MODE_DESC;
#endif // __dxgitype_h__

#define UNDEFINED       ((LPBYTE) 0x0)
#define RBP             UNDEFINED
#define RSP             UNDEFINED
#define R12             UNDEFINED
#define MONITOR_COUNT   2                       // this can be dynamically determined via DXGI adapter enumeration
                                                //   but for my local testing this was sufficient xd

typedef struct _MM_MODE_LIST_ENTRY {
    UINT64 ModeCount;
    DXGI_MODE_DESC *pModes;                     // pointer to DXGI_MODE_DESC array
} MM_MODE_LIST_ENTRY, *PMM_MODE_LIST_ENTRY, *LPMM_MODE_LIST_ENTRY;

typedef struct _MM_MODE_HEADER {
    MM_MODE_LIST_ENTRY Entries[MONITOR_COUNT];  // Entries[0] = primary, [1] = second monitor, etc.
    CHAR szDeviceName[ANYSIZE_ARRAY];           // ASCII Device Name
} MM_MODE_HEADER, *PMM_MODE_HEADER, *LPMM_MODE_HEADER;

typedef struct _MM_MODE_RESULT {
    DWORD dwWidth;                              // (0x0)
    DWORD dwHeight;                             // (0x4)
    DWORD dwRefreshHz;                          // (0x8)
    BYTE bFlag;                                 // (0xC) – always set to 0 here
} MM_MODE_RESULT, *PMM_MODE_RESULT;

typedef struct _MM_CUSTOM_DXGI_ADAPTER_DESC {
    WCHAR  wszDescription[128];
    SIZE_T cbDedicatedVideoMemory;
    SIZE_T cbDedicatedSystemMemory;
    SIZE_T cbSharedSystemMemory;
    DWORD  dwDeviceId;
    DWORD  dwVendorId;
} MM_CUSTOM_DXGI_ADAPTER_DESC, *PMM_CUSTOM_DXGI_ADAPTER_DESC;

typedef struct _MM_MONITOR_OBJECT {
    LPVOID  Unknown0;                           // (0x00) Unknown Data Pointer
    LPVOID  Unknown1;                           // (0x08) Multi-chain pointer to code
    LPVOID  Unknown2;                           // (0x10) Multi-chain pointer to code
    LPVOID  Unknown3;                           // (0x18) Multi-chain pointer to code
    LPVOID  Unknown4;                           // (0x20) Multi-chain pointer to code
    LPVOID  Unknown5;                           // (0x28) Multi-chain pointer to code
    LPVOID  Unknown6;                           // (0x30) Multi-chain pointer to code
    DWORD   dwFallbackMonitorIdx;               // (0x38) FallbackMonitorIndex raw value (fallback value should be 0)
    DWORD   Unknown7;                           // (0x3C) Unknown Raw Data
    LPVOID  Unknown8;                           // (0x40) Unknown Data Pointer
    DWORD   dwPossibleMaxMonitorCount;          // (0x48) Possible MaxMonitorCount
    DWORD   Unknown9;                           // (0x4C) Unknown Raw Data
    PMM_MODE_HEADER pModeHdr;                   // (0x50) Pointer to Monitor Mode header/struct
    DWORD   Unknown10;                          // (0x58) Unknown Raw Data
    DWORD   Unknown11;                          // (0x5C) Unknown Raw Data
    MM_CUSTOM_DXGI_ADAPTER_DESC AdapterDesc;    // (0x60) Custom Adapter Description
} MM_MONITOR_OBJECT, *PMM_MONITOR_OBJECT, *LPMM_MONITOR_OBJECT;

static_assert(offsetof(MM_MONITOR_OBJECT, pModeHdr) == 0x50, "MM_MONITOR_OBJECT structure offset is incorrect");
static_assert(offsetof(DXGI_MODE_DESC, RefreshRate) == 0x8, "DXGI_MODE_DESC.RefreshRate offset is incorrect");
static_assert(offsetof(DXGI_MODE_DESC, RefreshRate.Denominator) == 0xC, "DXGI_MODE_DESC.RefreshRate.Denominator offset is incorrect");
static_assert(sizeof(DXGI_MODE_DESC) == 0x1C, "DXGI_MODE_DESC structure size is incorrect");

typedef struct _MM_MODE_ROW {
    DWORD32 dwWidth;                            // v154 - incremented/decremented by 5
    DWORD32 dwHeight;                           // v157 - incremented/decremented by 5
    DWORD32 dwRefreshOrId;                      // v153 - incremented/decremented by 5
    DWORD32 dwModeIndexOrScore;                 // v155 - incremented/decremented by 5
    BYTE    bFlagA;                             // v252 - unknown
    BYTE    bFlagB;                             // v156 - incremented/decremented by 20
} MM_MODE_ROW, *PMM_MODE_ROW;
static_assert(sizeof(MM_MODE_ROW) == 0x14, "MM_MODE_ROW size is incorrect");

typedef struct _MM_MONITOR_CTX {
    MM_MODE_ROW PerMonitorEntries[256];         // Devs reserved 256 entries of 20 bytes for each monitor
                                                //  and assumed that no monitor would have more than 256 modes, LOL.
    DWORD dwRowCount;                           // Number of mode rows, at +0x1400 - per disasm - used as loop bound for dwModeIdx -
                                                // per local testing, this gotten OOB overwritten with value 0x690 (1680), which is larger over the max 256 entries.
// ...             ...
} MM_MONITOR_CTX, *PMM_MONITOR_CTX;
static_assert(offsetof(MM_MONITOR_CTX, dwRowCount) == 0x1400, "MM_MONITOR_CTX.dwRowCount offset is incorrect");

typedef struct _MM_ENGINE_CONTEXT {
    BYTE Padding[0x1F18];
    PMM_MONITOR_OBJECT pMonitorObject;          // at +0x1F18, per disasm
    // ... other stuff
} MM_ENGINE_CONTEXT, *PMM_ENGINE_CONTEXT;

typedef struct _NN_MONITOR_BLOCK {
    BYTE   RowStuff[0x1400];                    // 0x1400 bytes of sizeof(Entry14) (0x14) - sized rows or whatever
    DWORD  RowCount;                            // at +0x1400
    BYTE   Pad[0x4];                            // 0x1404 total
    // then more arrays at 0x38D8, 0x38DC, 0x38E0...
} MM_MONITOR_BLOCK, *PMM_MONITOR_BLOCK;

MM_MONITOR_BLOCK g_aMonitors[MONITOR_COUNT];    // stride 0x1404

/*
 * sub_142F5C420
    [a1] RCX = unknown (qword64),
    [a2] EDX = CurrentModeIndexIterator,
    [a3] R8D = MonitorIndex,
    [a4] R9 = unknown (qword64)
*/
DWORD __fastcall sub_142F5C420(
    PMM_MONITOR_OBJECT pMonitorObject,          // monitor object
    DWORD dwCurrentModeIdx,                     // iterator
    DWORD dwCurrentMonitorIdx,                  // current monitor index
    MM_MODE_RESULT *pModeResult                 // output result
) {
    if (0xFFFFFFFF == dwCurrentMonitorIdx) {
        dwCurrentMonitorIdx = pMonitorObject->dwFallbackMonitorIdx;
    }

    PMM_MODE_HEADER pHdr = pMonitorObject->pModeHdr;

    QWORD64 qwModeOffset = (QWORD64) dwCurrentModeIdx * sizeof(DXGI_MODE_DESC);
    DXGI_MODE_DESC *pModes = pHdr->Entries[dwCurrentMonitorIdx].pModes;

    DXGI_MODE_DESC *pCurrentMode = (DXGI_MODE_DESC*) ((QWORD64) pModes + qwModeOffset);
    pModeResult->dwWidth = pCurrentMode->Width;
    pModeResult->dwHeight = pCurrentMode->Height;

    // Calculate refresh rate in Hz
    DWORD dwNumerator = pCurrentMode->RefreshRate.Numerator;
    DWORD dwDenominator = pCurrentMode->RefreshRate.Denominator;

    // No zero checking, this is what causes the startup crash
    DWORD dwRefreshHz = dwNumerator / dwDenominator;

    pModeResult->bFlag = 0;
    pModeResult->dwRefreshHz = dwRefreshHz;

    return dwRefreshHz;
}
#define ExtractModeInfo sub_142F5C420

VOID __fastcall sub_142563500(
    PMM_ENGINE_CONTEXT pEngineCtx,              // pointer to engine context (before rcx+1F18)
    DWORD dwCurrentMonitorIdx,                  // current monitor index
    DWORD dwCurrentModeIdx,                     // iterator
    DWORD *pWidthOut,                           // outarg (reg R9)
    DWORD *pHeightOut,                          // outarg (stack)
    DWORD *pRefreshOut                          // outarg (stack)
) {
    // stack cookie check omitted..

    // RCX = [rcx+1F18]
    PMM_MONITOR_OBJECT pMonitorObject =
        pEngineCtx->pMonitorObject;             // *(PMM_MONITOR_OBJECT*)((BYTE*)pEngineCtx + 0x1F18);

    MM_MODE_RESULT modeResult;                  // at [rsp+20..2C]

    // EDX = dwModeIdx, R8D = dwMonitorIdx
    sub_142F5C420(
        pMonitorObject,
        dwCurrentModeIdx,                       // EDX
        dwCurrentMonitorIdx,                    // R8D
        &modeResult                             // R9
    );

    // Write back to the three caller-provided outputs
    *pWidthOut = modeResult.dwWidth;            // [rsp+20] -> RSI - this is where fatal overwrite occurs:
                                                //   when the idx is large enough, the pWidthOut pointer value
                                                //   is miscalculated before passed to this function,
                                                //   and ends up pointing to PMM_MONITOR_CTX::dwRowCount instead.
                                                // This overwrites the number of maximum entries, which is a value
                                                //   used for loop bounds later on.
                                                // Going over the valid mode count causes incorrect data
                                                //   being used for calculations in sub_142F5C420, 
                                                //   which in turn causes STATUS_INTEGER_DIVIDE_BY_ZERO exception, crashing the game.
    *pHeightOut = modeResult.dwHeight;          // [rsp+24]
    *pRefreshOut = modeResult.dwRefreshHz;      // [rsp+28]

    // stack cookie check omitted..
}
#define GetModeInfo sub_142563500

// This function is one giant fooking spaghetti, like seriously, its fucked.
// I'm not gonna even try describing or reconstructing it, the main thing to know
//  is that it calls GetModeInfo in a loop which assesses all available modes for all monitors,
//  and the bound of this loop is the value that gets OOB overwritten with dwWidthOut value.
QWORD __fastcall sub_141E3845F(
    // huge amount of unknown args
) {
    // ...
}