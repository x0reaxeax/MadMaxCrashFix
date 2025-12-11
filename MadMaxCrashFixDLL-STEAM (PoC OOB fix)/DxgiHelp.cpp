#include "MadMaxSpec.h"

#include <Windows.h>
#include <dxgi.h>

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
    IDXGIOutput* pOutput = nullptr;

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

    for (UINT i = 0; i < uOutputIter; i++) {
        pAdapter = vAdapters[i];
        uNumOutputs = 0;

        while (DXGI_ERROR_NOT_FOUND != pAdapter->EnumOutputs(uNumOutputs, &pOutput)) {
            pOutput->Release();
            uNumOutputs++;
        }

        if (uNumOutputs > uLargestOutputCount) {
            uLargestOutputCount = uNumOutputs;
        }
    }

_FINAL:
    pAdapter->Release();
    pFactory->Release();

    return uLargestOutputCount;
}