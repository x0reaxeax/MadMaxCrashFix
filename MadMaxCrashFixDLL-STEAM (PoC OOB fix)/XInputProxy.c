#include "MadMaxSpec.h"

//#include <xinput.h>

#ifndef XINPUT_DLL_A
typedef struct _XINPUT_GAMEPAD {
    WORD                                wButtons;
    BYTE                                bLeftTrigger;
    BYTE                                bRightTrigger;
    SHORT                               sThumbLX;
    SHORT                               sThumbLY;
    SHORT                               sThumbRX;
    SHORT                               sThumbRY;
} XINPUT_GAMEPAD, * PXINPUT_GAMEPAD;

typedef struct _XINPUT_STATE {
    DWORD                               dwPacketNumber;
    XINPUT_GAMEPAD                      Gamepad;
} XINPUT_STATE, * PXINPUT_STATE;

typedef struct _XINPUT_VIBRATION {
    WORD                                wLeftMotorSpeed;
    WORD                                wRightMotorSpeed;
} XINPUT_VIBRATION, * PXINPUT_VIBRATION;

typedef struct _XINPUT_CAPABILITIES {
    BYTE                                Type;
    BYTE                                SubType;
    WORD                                Flags;
    XINPUT_GAMEPAD                      Gamepad;
    XINPUT_VIBRATION                    Vibration;
} XINPUT_CAPABILITIES, * PXINPUT_CAPABILITIES;
#endif // XINPUT_DLL_A

typedef DWORD(WINAPI* XInputGetCapabilities_t)(
    DWORD               dwUserIndex,
    DWORD               dwFlags,
    XINPUT_CAPABILITIES* pCapabilities
    );

typedef DWORD(WINAPI* XInputGetDSoundAudioDeviceGuids_t)(
    DWORD dwUserIndex,
    GUID* pDSoundRenderGuid,
    GUID* pDSoundCaptureGuid
    );

typedef DWORD(WINAPI* XInputGetState_t)(
    DWORD        dwUserIndex,
    XINPUT_STATE* pState
    );

typedef DWORD(WINAPI* XInputSetState_t)(
    DWORD            dwUserIndex,
    XINPUT_VIBRATION* pVibration
    );

EXTERN_C DLLEXPORT DWORD XInputGetCapabilities(
    DWORD               dwUserIndex,
    DWORD               dwFlags,
    XINPUT_CAPABILITIES* pCapabilities
) {
    if (NULL == g_hOriginalDll) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }

    XInputGetCapabilities_t pXInputGetCapabilities = (XInputGetCapabilities_t) GetProcAddress(
        g_hOriginalDll,
        "XInputGetCapabilities"
    );

    if (NULL == pXInputGetCapabilities) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }

    return pXInputGetCapabilities(
        dwUserIndex,
        dwFlags,
        pCapabilities
    );
}

EXTERN_C DLLEXPORT DWORD XInputGetDSoundAudioDeviceGuids(
    DWORD dwUserIndex,
    GUID* pDSoundRenderGuid,
    GUID* pDSoundCaptureGuid
) {
    if (NULL == g_hOriginalDll) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    XInputGetDSoundAudioDeviceGuids_t pXInputGetDSoundAudioDeviceGuids = (XInputGetDSoundAudioDeviceGuids_t) GetProcAddress(
        g_hOriginalDll,
        "XInputGetDSoundAudioDeviceGuids"
    );
    if (NULL == pXInputGetDSoundAudioDeviceGuids) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    return pXInputGetDSoundAudioDeviceGuids(
        dwUserIndex,
        pDSoundRenderGuid,
        pDSoundCaptureGuid
    );
}

EXTERN_C DLLEXPORT DWORD XInputGetState(
    DWORD        dwUserIndex,
    XINPUT_STATE* pState
) {
    if (NULL == g_hOriginalDll) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    XInputGetState_t pXInputGetState = (XInputGetState_t) GetProcAddress(
        g_hOriginalDll,
        "XInputGetState"
    );
    if (NULL == pXInputGetState) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    return pXInputGetState(
        dwUserIndex,
        pState
    );
}

EXTERN_C DLLEXPORT DWORD XInputSetState(
    DWORD            dwUserIndex,
    XINPUT_VIBRATION* pVibration
) {
    if (NULL == g_hOriginalDll) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    XInputSetState_t pXInputSetState = (XInputSetState_t) GetProcAddress(
        g_hOriginalDll,
        "XInputSetState"
    );
    if (NULL == pXInputSetState) {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    return pXInputSetState(
        dwUserIndex,
        pVibration
    );
}