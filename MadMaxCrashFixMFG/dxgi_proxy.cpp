#include "dxgidef.hpp"
#include "config.hpp"

#include <new>
#include <ios>
#include <vector>
#include <atomic>

extern HMODULE g_hSystemDxgi;

GLOBAL std::atomic<INT> g_IdxAdapterWithMostOutputs{ -1 };

PFN_EnumAdapters        g_pfnOriginalEnumAdapters = nullptr;
PFN_EnumAdapters1       g_pfnOriginalEnumAdapters1 = nullptr;
PFN_EnumOutputs         g_pfnOriginalEnumOutputs = nullptr;
PFN_GetDisplayModeList  g_pfnOriginalGetDisplayModeList = nullptr;

HRESULT __stdcall Hooked_EnumAdapters(IDXGIFactory*, UINT, IDXGIAdapter**);
HRESULT __stdcall Hooked_EnumAdapters1(IDXGIFactory1*, UINT, IDXGIAdapter1**);
HRESULT __stdcall Hooked_EnumOutputs(IDXGIAdapter*, UINT, IDXGIOutput**);
HRESULT __stdcall Hooked_GetDisplayModeList(IDXGIOutput*, DXGI_FORMAT, UINT, UINT*, DXGI_MODE_DESC*);

INT FindAdapterWithMostOutputs(
    IDXGIFactory1* pFactory
) {
    if (nullptr == pFactory) {
        return -1;
    }

    IDXGIAdapter* pAdapter = nullptr;

    UINT uOutputIter = 0;
    UINT uNumOutputs = 0;
    UINT uLargestOutputCount = 0;
    INT iLargestOutputAdapterIndex = -1;
    IDXGIOutput *pOutput = nullptr;

    std::vector<IDXGIAdapter*> vAdapters;
    std::vector<IDXGIOutput*> vOutputs;

    HRESULT hr;

    while (DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters(uOutputIter, &pAdapter)) {
        vAdapters.push_back(pAdapter);
        uOutputIter++;
    }

    if (0 == uOutputIter) {
        goto _FINAL;
    }

    LogLine(L"Adapters found: " + std::to_wstring(uOutputIter));

    for (UINT i = 0; i < uOutputIter; i++) {
        DXGI_ADAPTER_DESC adapterDesc = { 0 };

        pAdapter = vAdapters[i];
        uNumOutputs = 0;

        hr = pAdapter->GetDesc(&adapterDesc);
        if (FAILED(hr)) {
            continue;
        }

        LogLine(
            L"Enumerating outputs for adapter " + std::to_wstring(i) + L": " +
            std::wstring(adapterDesc.Description)
        );

        while (DXGI_ERROR_NOT_FOUND != pAdapter->EnumOutputs(uNumOutputs, &pOutput)) {
            pOutput->Release();
            uNumOutputs++;
        }

        LogLine(
            L"Adapter " + std::to_wstring(i) + L" has " + std::to_wstring(uNumOutputs) + L" outputs"
        );

        if (uNumOutputs > uLargestOutputCount) {
            uLargestOutputCount = uNumOutputs;
            iLargestOutputAdapterIndex = (INT) i;
        }
    }

_FINAL:
    pAdapter->Release();
    pFactory->Release();

    return iLargestOutputAdapterIndex;
}

/**
* CreateDXGIFactory1
*     -> hook returned IDXGIFactory1 vtable
*         slot 7  = EnumAdapters        // IDXGIFactory
*         slot 12 = EnumAdapters1       // IDXGIFactory1
*
* EnumAdapters / EnumAdapters1
*     -> when adapter is returned, hook adapter vtable
*         slot 7 = EnumOutputs
*
* EnumOutputs
*     -> when output is returned, hook output vtable
*         slot 8  = GetDisplayModeList
*/

// :]
namespace Hookers {
    template<typename TFn>
    bool HookVtableEntry(
        PVOID pComObject,
        SIZE_T index,
        TFn pHook,
        TFn& pOriginal
    ) {
        if (nullptr == pComObject) {
            return false;
        }

        void** pVtable = *reinterpret_cast<void***>(pComObject);
        if (nullptr == pVtable) {
            LogLine(L"Failed to get vtable from COM object");
            return false;
        }

        void* pHookVoid = reinterpret_cast<void*>(pHook);

        if (pVtable[index] == pHookVoid) {
            // already hooked
            return true; 
        }

        if (nullptr == pOriginal) {
            pOriginal = reinterpret_cast<TFn>(pVtable[index]);
        }

        DWORD dwOldProtect = 0;
        if (!VirtualProtect(
            &pVtable[index],
            sizeof(void*),
            PAGE_EXECUTE_READWRITE,
            &dwOldProtect
        )) {
            LogLine(L"VirtualProtect failed: " + std::to_wstring(GetLastError()));
            return false;
        }

        pVtable[index] = pHookVoid;

        VirtualProtect(
            &pVtable[index],
            sizeof(void*),
            dwOldProtect,
            &dwOldProtect
        );

        return true;
    }

    bool EnumAdapters(
        IDXGIFactory* pFactory
    ) {
        return HookVtableEntry(
            pFactory,
            IDX_IDXGIFactory_EnumAdapters,   // idx 7
            &Hooked_EnumAdapters,
            g_pfnOriginalEnumAdapters
        );
    }

    bool EnumAdapters1(IDXGIFactory1* pFactory) {
        return HookVtableEntry(
            pFactory,
            IDX_IDXGIFactory1_EnumAdapters1,  // slot 12
            &Hooked_EnumAdapters1,
            g_pfnOriginalEnumAdapters1
        );
    }

    bool EnumOutputs(IDXGIAdapter* pAdapter) {
        return HookVtableEntry(
            pAdapter,
            IDX_IDXGIAdapter_EnumOutputs,
            &Hooked_EnumOutputs,
            g_pfnOriginalEnumOutputs
        );
    }

    bool GetDisplayModeList(IDXGIOutput* pOutput) {
        return HookVtableEntry(
            pOutput,
            IDX_IDXGIOutput_GetDisplayModeList,
            &Hooked_GetDisplayModeList,
            g_pfnOriginalGetDisplayModeList
        );
    }
} // namespace Hookers

std::wstring FormatHexResult(
    HRESULT hr
) {
    WCHAR szHr[32] = {};
    swprintf_s(szHr, L"0x%08X", static_cast<DWORD32>(hr));
    return std::wstring(szHr);
}

void LogMatchedMode(
    UINT modeIndex,
    const DXGI_MODE_DESC& modeDesc
) {
    UINT uRefreshRate = 0;
    if (0 != modeDesc.RefreshRate.Denominator) {
        uRefreshRate = modeDesc.RefreshRate.Numerator / modeDesc.RefreshRate.Denominator;
    }

    WCHAR szModeDesc[64] = {};
    swprintf_s(
        szModeDesc, 
        L"Found matching filtered mode %u: %ux%u @ %uHz", 
        modeIndex,
        modeDesc.Width, 
        modeDesc.Height, 
        uRefreshRate
    );
    LogLine(szModeDesc);
}

HRESULT __stdcall Hooked_EnumAdapters(
    IDXGIFactory *pThis,
    UINT Adapter,
    IDXGIAdapter **ppAdapter
) {
    if (nullptr == pThis || nullptr == ppAdapter) {
        LogLine(L"Hooked_EnumAdapters: Invalid parameters");
        return E_FAIL;
    }

    if (nullptr == g_pfnOriginalEnumAdapters) {
        LogLine(L"Hooked_EnumAdapters: Original function pointer is null");
        return E_FAIL;
    }

    HRESULT hr = g_pfnOriginalEnumAdapters(
        pThis, 
        Adapter, 
        ppAdapter
    );

    if (FAILED(hr)) {
        LogLine(
            L"Hooked_EnumAdapters: Original function call failed with HRESULT " + FormatHexResult(hr)
        );
        return hr;
    }

    if (nullptr == *ppAdapter) {
        LogLine(
            L"Hooked_EnumAdapters: Original function call succeeded but returned null adapter for index " + std::to_wstring(Adapter)
        );
        return E_FAIL;
    }
    
    LocalConfig::Config config = LocalConfig::LoadConfig();
    if (Adapter == config.dwAdapterIdx) {
        if (!Hookers::EnumOutputs(*ppAdapter)) {
            LogLine(L"Hooked_EnumAdapters: Failed to hook EnumOutputs for adapter " + std::to_wstring(Adapter));
            return E_FAIL;
        }
        LogLine(L"Hooked_EnumAdapters: Successfully hooked EnumOutputs for adapter " + std::to_wstring(Adapter));
    } else {
        LogLine(L"Hooked_EnumAdapters: Skipping hooking EnumOutputs for adapter " + std::to_wstring(Adapter));
    }
    
    return hr;
}

HRESULT __stdcall Hooked_EnumAdapters1(
    IDXGIFactory1 *pThis,
    UINT Adapter,
    IDXGIAdapter1 **ppAdapter
) {
    if (nullptr == pThis || nullptr == ppAdapter) {
        return E_FAIL;
    }
    if (nullptr == g_pfnOriginalEnumAdapters1) {
        return E_FAIL;
    }
    HRESULT hr = g_pfnOriginalEnumAdapters1(
        pThis, 
        Adapter, 
        ppAdapter
    );
    if (FAILED(hr)) {
        LogLine(
            L"Hooked_EnumAdapters1: Original function call failed with HRESULT " + FormatHexResult(hr)
        );
        return hr;
    }

    if (nullptr == *ppAdapter) {
        LogLine(
            L"Hooked_EnumAdapters1: Original function call succeeded but returned null adapter for index " + std::to_wstring(Adapter)
        );
        return E_FAIL;
    }
    
    LocalConfig::Config config = LocalConfig::LoadConfig();
    if (Adapter == config.dwAdapterIdx) {
        if (!Hookers::EnumOutputs(*ppAdapter)) {
            LogLine(L"Hooked_EnumAdapters1: Failed to hook EnumOutputs for adapter " + std::to_wstring(Adapter));
            return E_FAIL;
        }
        LogLine(L"Hooked_EnumAdapters1: Successfully hooked EnumOutputs for adapter " + std::to_wstring(Adapter));
    } else {
        LogLine(L"Hooked_EnumAdapters1: Skipping hooking EnumOutputs for adapter " + std::to_wstring(Adapter));
    }
    
    return hr;
}

HRESULT __stdcall Hooked_EnumOutputs(
    IDXGIAdapter *pThis,
    UINT Output,
    IDXGIOutput **ppOutput
) {
    if (nullptr == pThis || nullptr == ppOutput) {
        return E_FAIL;
    }

    if (nullptr == g_pfnOriginalEnumOutputs) {
        return E_FAIL;
    }

    HRESULT hr = g_pfnOriginalEnumOutputs(
        pThis, 
        Output, 
        ppOutput
    );

    if (FAILED(hr)) {
        LogLine(
            L"Hooked_EnumOutputs: Original function call failed with HRESULT " + FormatHexResult(hr)
        );
        return hr;
    }

    if (nullptr == *ppOutput) {
        LogLine(
            L"Hooked_EnumOutputs: Original function call succeeded but returned null output for index " + std::to_wstring(Output)
        );
        return E_FAIL;
    }

    if (!Hookers::GetDisplayModeList(*ppOutput)) {
        LogLine(L"Hooked_EnumOutputs: Failed to hook GetDisplayModeList for output " + std::to_wstring(Output));
        return E_FAIL;
    }

    return hr;
}

HRESULT __stdcall Hooked_GetDisplayModeList(
    IDXGIOutput *pThis,
    DXGI_FORMAT EnumFormat,
    UINT Flags,
    UINT *pNumModes,
    DXGI_MODE_DESC *pDesc
) {
    if (nullptr == pThis || nullptr == pNumModes) {
        LogLine(L"Hooked_GetDisplayModeList: Invalid parameters");
        return E_POINTER;
    }

    if (nullptr == g_pfnOriginalGetDisplayModeList) {
        LogLine(L"Hooked_GetDisplayModeList: Original function pointer is null");
        return E_FAIL;
    }

    UINT uNumModes = 0;
    HRESULT hr = g_pfnOriginalGetDisplayModeList(
        pThis,
        EnumFormat,
        Flags,
        &uNumModes,
        NULL
    );

    if (FAILED(hr)) {
        LogLine(
            L"IDXGIOutput::GetDisplayModeList function call failed with HRESULT " + FormatHexResult(hr)
        );
        return hr;
    }

    if (0 == uNumModes) {
        LogLine(L"IDXGIOutput::GetDisplayModeList function call returned zero modes");
        return hr;
    }

    DXGI_MODE_DESC *pModeDescs = new(std::nothrow) DXGI_MODE_DESC[uNumModes];
    if (nullptr == pModeDescs) {
        LogLine(L"Hooked_GetDisplayModeList: Failed to allocate memory for mode descriptions");
        return E_OUTOFMEMORY;
    }

    hr = g_pfnOriginalGetDisplayModeList(
        pThis,
        EnumFormat,
        Flags,
        &uNumModes,
        pModeDescs
    );

    if (FAILED(hr)) {
        LogLine(
            L"IDXGIOutput::GetDisplayModeList function call failed with HRESULT " + FormatHexResult(hr)
        );
        delete[] pModeDescs;
        return hr;
    }


    LocalConfig::Config config = LocalConfig::LoadConfig();

    // TODO: largest available mode doesn't seem to be able to be applied.
    //  check if the filtering is screwing up, by inserting a fugazi entry at the end
    //  local test: latest matched mode is 1920x1080@360Hz

    UINT uTotalFilteredModes = 0;
    // caller capacity only matters when `pDesc != nullptr`
    UINT uCallerCapacity = (nullptr != pDesc && nullptr != pNumModes) ? *pNumModes : 0;
    UINT uModesWritten = 0;
    for (UINT i = 0; i < uNumModes; i++) {
        if (
            pModeDescs[i].Width == config.dwDisplayWidth 
            && 
            pModeDescs[i].Height == config.dwDisplayHeight
        ) {
            if (uTotalFilteredModes >= 256) { // fix: `uTotalFilteredModes` should == `uModesWritten`, but if not, then allocate for all potential filtered modes
                                              //  uModesWritten doesn't increment when pDesc is null, which is the case of requesting mode count
                // game can't handle more than 256 modes, so clamp it and sacrifice some modes,
                //  even tho who tf has more than 256 modes for a single resolution, get some help gluesniffer
                break;
            }
            if (nullptr != pDesc && uModesWritten < uCallerCapacity) {
                pDesc[uModesWritten++] = pModeDescs[i];

                LogMatchedMode(
                    uModesWritten, 
                    pModeDescs[i]
                );
            }
            ++uTotalFilteredModes;
        }
    }

    delete[] pModeDescs;

    if (0 == uModesWritten) {
        LogLine(
            L"Hooked_GetDisplayModeList: Spoofed display mode capacity to " + std::to_wstring(uTotalFilteredModes) + L" modes matching config resolution " +
            std::to_wstring(config.dwDisplayWidth) + L"x" + std::to_wstring(config.dwDisplayHeight)
        );
    } else {
        LogLine(
            L"Hooked_GetDisplayModeList: Clamped " + std::to_wstring(uNumModes) +
            L" modes down to " + std::to_wstring(uTotalFilteredModes) +
            L" modes matching config resolution " + std::to_wstring(config.dwDisplayWidth) +
            L"x" + std::to_wstring(config.dwDisplayHeight)
        );
    }

    *pNumModes = uTotalFilteredModes;

    if (nullptr != pDesc && uCallerCapacity < uTotalFilteredModes) {
        // caller provided a buffer but it's too small to hold all the filtered modes, so we return as many as we could write
        return DXGI_ERROR_MORE_DATA;
    }

    return S_OK;
}

// the game doesn't use this, but it won't start without it
EXTERN_C HRESULT __stdcall CreateDXGIFactory2(
    UINT Flags,
    REFIID riid,
    void** ppFactory
) {
    if (nullptr == LoadSystemDxgi()) {
        return E_POINTER;
    }

    PFN_CreateDXGIFactory2 pfnCreateDXGIFactory2 = reinterpret_cast<PFN_CreateDXGIFactory2>(
        ResolveDxgiProc("CreateDXGIFactory2")
    );

    return pfnCreateDXGIFactory2(Flags, riid, ppFactory);
}

EXTERN_C HRESULT __stdcall CreateDXGIFactory1(
    REFIID riid,
    void** ppFactory
) {
    if (nullptr == ppFactory) {
        return E_POINTER;
    }

    if (nullptr == LoadSystemDxgi()) {
        return E_POINTER;
    }
    
    LogLine(L"MadMaxCrashFix: First call to CreateDXGIFactory1");

    PFN_CreateDXGIFactory1 pfnCreateDXGIFactory1 = reinterpret_cast<PFN_CreateDXGIFactory1>(
        GetProcAddress(g_hSystemDxgi, "CreateDXGIFactory1")
    );
    if (nullptr == pfnCreateDXGIFactory1) {
        MessageBoxA(nullptr, "Failed to get address of CreateDXGIFactory1", "Error", MB_ICONERROR);
        return E_POINTER;
    }

    HRESULT hr = pfnCreateDXGIFactory1(riid, ppFactory);
    if (FAILED(hr)) {
        MessageBoxA(nullptr, "Failed to call CreateDXGIFactory1", "Error", MB_ICONERROR);
        return hr;
    }

    g_IdxAdapterWithMostOutputs.store(FindAdapterWithMostOutputs(static_cast<IDXGIFactory1*>(*ppFactory)));

    // hook IDXGIFactory1::EnumAdapters/1

    bool bHookedAtLeastOne = false;
    if (__uuidof(IDXGIFactory) == riid) {
        if (!Hookers::EnumAdapters(static_cast<IDXGIFactory*>(*ppFactory))) {
            MessageBoxA(nullptr, "Failed to hook EnumAdapters", "Error", MB_ICONERROR);
            return E_FAIL;
        }
        bHookedAtLeastOne = true;
    }

    if (__uuidof(IDXGIFactory1) == riid) {
        if (!Hookers::EnumAdapters1(static_cast<IDXGIFactory1*>(*ppFactory))) {
            MessageBoxA(nullptr, "Failed to hook EnumAdapters1", "Error", MB_ICONERROR);
            return E_FAIL;
        }
        bHookedAtLeastOne = true;
    }

    if (!bHookedAtLeastOne) {
        MessageBoxA(
            nullptr, 
            "CreateDXGIFactory1 returned an unexpected interface that is not IDXGIFactory or IDXGIFactory1", 
            "Error", 
            MB_ICONERROR
        );
        return E_FAIL;
    }
    LogLine(L"Successfully hooked EnumAdapters and EnumAdapters1 on IDXGIFactory1");

    return hr;
}