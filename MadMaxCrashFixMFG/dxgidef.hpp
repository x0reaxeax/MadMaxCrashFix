#ifndef _MMCF_MMG_HPP
#define _MMCF_MMG_HPP

#include <dxgi1_3.h>
#include <Windows.h>

#define GLOBAL

typedef HRESULT(__stdcall* PFN_CreateDXGIFactory1)(REFIID, void**);
typedef HRESULT(__stdcall* PFN_CreateDXGIFactory2)(UINT, REFIID, void**);

typedef HRESULT(__stdcall* PFN_EnumAdapters)(IDXGIFactory*, UINT, IDXGIAdapter**);
typedef HRESULT(__stdcall* PFN_EnumAdapters1)(IDXGIFactory1*, UINT, IDXGIAdapter1**);

typedef HRESULT(__stdcall* PFN_EnumOutputs)(IDXGIAdapter*, UINT, IDXGIOutput**);

typedef HRESULT(__stdcall* PFN_GetDisplayModeList)(IDXGIOutput*, DXGI_FORMAT, UINT, UINT*, DXGI_MODE_DESC*);
typedef HRESULT(__stdcall* PFN_GetDisplayModeList1)(IDXGIOutput1*, DXGI_FORMAT, UINT, UINT*, DXGI_MODE_DESC1*);

constexpr DWORD IDX_IDXGIFactory_EnumAdapters = 7;
constexpr DWORD IDX_IDXGIFactory1_EnumAdapters1 = 12;

constexpr DWORD IDX_IDXGIAdapter_EnumOutputs = 7;
constexpr DWORD IDX_IDXGIAdapter_GetDesc = 8;
constexpr DWORD IDX_IDXGIAdapter1_GetDesc1 = 10;

constexpr DWORD IDX_IDXGIOutput_GetDisplayModeList = 8;
constexpr DWORD IDX_IDXGIOutput1_GetDisplayModeList1 = 19;

LPVOID LoadSystemDxgi(void);

FARPROC ResolveDxgiProc(
    _In_ LPCSTR szProcName
);

#endif // _MMCF_MMG_HPP