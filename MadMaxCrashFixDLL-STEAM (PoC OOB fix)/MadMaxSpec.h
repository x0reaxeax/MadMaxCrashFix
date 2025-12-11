#ifndef _MAD_MAX_SPEC_H
#define _MAD_MAX_SPEC_H

#include <Windows.h>

#ifdef __INTEL_LLVM_COMPILER
#ifdef __cplusplus
#include <cassert>
#else
#include <assert.h>
#endif // __cplusplus
#endif // __INTEL_LLVM_COMPILER

#define DLLEXPORT           __declspec(dllexport)
#define NAKED               __declspec(naked)
#define NORETURN            __declspec(noreturn)
#define UNREACHABLE_CODE    __assume(0)
#define UNSPECIFIED_PASS
#define VOLATILE            volatile
#define STATIC              static
#define INLINE              inline

#define MM_MAX_MODE_ROWS    256

typedef unsigned long long  QWORD, QWORD64, * PQWORD, * PQWORD64;

typedef struct _MM_MODE_ROW {
    DWORD dwWidth;                      // v154
    DWORD dwHeight;                     // v157
    DWORD dwRefreshOrId;                // v153 (refresh Hz or some ID)
    DWORD dwModeIndex;                  // v155 (original DXGI mode index / score)
    BYTE  bFlagA;                       // v252
    BYTE  bFlagB;                       // v156
    // 2 bytes autopadded 
} MM_MODE_ROW, * PMM_MODE_ROW;
static_assert(sizeof(MM_MODE_ROW) == 0x14, "MM_MODE_ROW size incorrect");

typedef struct _MM_MONITOR_BLOCK {
    MM_MODE_ROW Rows[MM_MAX_MODE_ROWS]; // 256 * 20 = 0x1400 bytes
    DWORD       RowCount;               // @ +0x1400 - the field that gets smashed
    // ... blah blah 
} MM_MONITOR_BLOCK, * PMM_MONITOR_BLOCK;
static_assert(sizeof(MM_MONITOR_BLOCK) == 0x1404, "MM_MONITOR_BLOCK size incorrect");

typedef struct _MONITOR_TRACK {
    BYTE* pRowBase;                     // inferred base address of Rows[0]
    DWORD dwLastRowIndex;               // last rowIdx observed
    DWORD dwOriginalRowCount;           // original safe RowCount captured
    BOOL  bCapturedOriginalCount;       // flag for signaling restoration of bad RowCount
} MONITOR_TRACK, * PMONITOR_TRACK;

typedef VOID(__fastcall* PFN_GetModeInfo)(
    PVOID pEngineCtx,
    DWORD monitorIdx,
    DWORD modeIdx,
    DWORD* pWidthOut,
    DWORD* pHeightOut,
    DWORD* pRefreshOut
);

EXTERN_C PMONITOR_TRACK g_pMonTrack;
EXTERN_C DWORD** g_apRowCount;

EXTERN_C HINSTANCE g_hOriginalDll;

#ifdef __cplusplus
extern "C" {
#endif

    UINT GetDxgiOutputCount(
        VOID
    );

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _MAD_MAX_SPEC_H