#include <Windows.h>
#include <dxgi.h>

#include <stdio.h>
#include <vector>

#pragma comment(lib, "dxgi.lib")

UINT GetDxgiOutputCount(
    VOID
) {

    IDXGIFactory* pFactory = nullptr;
    IDXGIAdapter* pAdapter = nullptr;

    UINT uOutputIter = 0;
    UINT uNumOutputs = 0;
    UINT uLargestOutputCount = 0;
    IDXGIOutput *pOutput = nullptr;

    std::vector<IDXGIAdapter*> vAdapters;
    std::vector<IDXGIOutput*> vOutputs;

    HRESULT hr = CreateDXGIFactory1(
        __uuidof(IDXGIFactory1),
        (VOID**) &pFactory
    );

    if (FAILED(hr)) {
        return 0;
    }

    while (DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters(uOutputIter, &pAdapter)) {
        vAdapters.push_back(pAdapter);
        uOutputIter++;
    }

    if (0 == uOutputIter) {
        goto _FINAL;
    }

    printf("[+] %u adapters found.\n", uOutputIter);

    for (UINT i = 0; i < uOutputIter; i++) {
        DXGI_ADAPTER_DESC adapterDesc = { 0 };

        pAdapter = vAdapters[i];
        uNumOutputs = 0;

        hr = pAdapter->GetDesc(&adapterDesc);
        if (FAILED(hr)) {
            fprintf(
                stderr,
                "[-] GetDesc failed on adapter: (hRes: 0x%X)\n",
                hr
            );
            continue;
        }

        wprintf(
            L"Adapter %u Description: %s\n",
            i,
            adapterDesc.Description
        );


        while (DXGI_ERROR_NOT_FOUND != pAdapter->EnumOutputs(uNumOutputs, &pOutput)) {
            pOutput->Release();
            uNumOutputs++;
        }

        printf("[+] Adapter %u: %u outputs found.\n", i, uNumOutputs);

        if (uNumOutputs > uLargestOutputCount) {
            uLargestOutputCount = uNumOutputs;
        }
    }

_FINAL:
    pAdapter->Release();
    pFactory->Release();

    return uLargestOutputCount;
}

int main(void) {
    UINT uOutputCount = GetDxgiOutputCount();
    printf("[+] Largest output count across adapters: %u\n", uOutputCount);

    INT iRet = EXIT_FAILURE;
    IDXGIFactory* pFactory = NULL;
    IDXGIAdapter* pAdapter = NULL;
    DXGI_ADAPTER_DESC adapterDesc;

    UINT uNumModes = 0;
    DXGI_FORMAT dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    UINT uFlags = DXGI_ENUM_MODES_INTERLACED;

    UINT uNumOutputs = 0;
    IDXGIOutput *pOutput = nullptr;

    std::vector<IDXGIOutput*> vOutputs;

    HRESULT hr = CreateDXGIFactory1(
        __uuidof(IDXGIFactory1),
        (void**) &pFactory
    );
    if (FAILED(hr)) {
        return EXIT_FAILURE;
    }

    hr = pFactory->EnumAdapters(0, &pAdapter);
    if (FAILED(hr)) {
        pFactory->Release();
        return EXIT_FAILURE;
    }

    hr = pAdapter->GetDesc(&adapterDesc);
    if (FAILED(hr)) {
        fprintf(
            stderr,
            "[-] GetDesc failed on adapter: (hRes: 0x%X)\n",
            hr
        );
        goto _EXIT;
    }
    // Print adapter description
    wprintf(
        L"    Adapter Description: %s\n"
        L"    VendorId: 0x%X\n"
        L"    DeviceId: 0x%X\n"
        L"    SubSysId: 0x%X\n"
        L"    Revision: 0x%X\n"
        L"    DedicatedVideoMemory: 0x%llX\n"
        L"    DedicatedSystemMemory: 0x%llX\n"
        L"    SharedSystemMemory: 0x%llX\n"
        L"    AdapterLuid: L0x%X | H0x%X\n",
        adapterDesc.Description,
        adapterDesc.VendorId,
        adapterDesc.DeviceId,
        adapterDesc.SubSysId,
        adapterDesc.Revision,
        adapterDesc.DedicatedVideoMemory,
        adapterDesc.DedicatedSystemMemory,
        adapterDesc.SharedSystemMemory,
        adapterDesc.AdapterLuid.LowPart,
        adapterDesc.AdapterLuid.HighPart
    );

    while (DXGI_ERROR_NOT_FOUND != pAdapter->EnumOutputs(uNumOutputs, &pOutput)) {
        vOutputs.push_back(pOutput);
        uNumOutputs++;
    }

    if (0 == uNumOutputs) {
        fwprintf(
            stderr,
            L"[-] No outputs found on adapter '%s'.\n",
            adapterDesc.Description
        );
        goto _EXIT;
    }

    printf("[+] %u outputs found on adapter.\n", uNumOutputs);

    for (UINT outputIdx = 0; outputIdx < uNumOutputs; outputIdx++) {
        IDXGIOutput* pCurrentOutput = vOutputs[outputIdx];
        hr = pCurrentOutput->GetDisplayModeList(
            dxgiFormat,
            uFlags,
            &uNumModes,
            NULL
        );
        if (FAILED(hr)) {
            fprintf(
                stderr,
                "[-] GetDisplayModeList failed to get mode count on output % d: (hRes: 0x % X)\n",
                outputIdx,
                hr
            );
            continue;
        }
        std::vector<DXGI_MODE_DESC> vModes(uNumModes);
        hr = pCurrentOutput->GetDisplayModeList(
            dxgiFormat,
            uFlags,
            &uNumModes,
            vModes.data()
        );
        if (FAILED(hr)) {
            fprintf(
                stderr,
                "[-] GetDisplayModeList failed to get modes on output %d: (hRes: 0x%X)\n",
                outputIdx,
                hr
            );
            continue;
        }
        printf("[+] Output %u: %d modes found.\n", outputIdx, uNumModes);
        for (UINT modeIdx = 0; modeIdx < uNumModes; modeIdx++) {
            DXGI_MODE_DESC& mode = vModes[modeIdx];
            printf(
                "    Mode %d: %ux%u @ %u/%u Hz\n",
                modeIdx,
                mode.Width,
                mode.Height,
                mode.RefreshRate.Numerator,
                mode.RefreshRate.Denominator
            );
        }
        pCurrentOutput->Release();
    }

    iRet = EXIT_SUCCESS;

    printf("[+] DXGI mode enumeration completed.\n");
_EXIT:
    pAdapter->Release();
    pFactory->Release();

    return iRet;
}