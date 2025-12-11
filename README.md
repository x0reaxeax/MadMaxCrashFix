# MadMaxCrashFix

## Fix for loading screen crash in Mad Max

## Usage

## Steam Version
Only choose ONE of the two versions below. Do NOT use both at the same time.

### "Band-aid fix" version
 * Place `dinput8.dll` from `steamfix.zip` into the game directory where `MadMax.exe` is located.

### "OOB guard" version
 * Place `XInput9_1_0.dll` from `steam_oob_fix.zip` into the game directory where `MadMax.exe` is located.

## GoG Version
Choose a version of your liking.  
It's been reported that the injector version may prevent achievements from being obtained.

 * **Injector (EXE) version**
   * Put the EXE file into the game directory where `MadMax.exe` is located
   * Run the EXE
 * **DLL (dinput8.dll) version**
   * Put `dinput8.dll` into the game directory where `MadMax.exe` is located
   * Start the game as usual

## Notes
 * If the game is laggy in main menu, change the resolution or switch to borderless, apply, then switch back.
 * Turning V-Sync off is advised.

## Technical Description

The game makes repeated calls to DX11 APIs like `IDXGIFactory1::EnumAdapters1`, `IDXGIAdapter::EnumOutputs`, and so on, in order to retrieve the number of available monitor modes for later filtering.   
These modes are then (after filtering / assessing) stored in a structure (`MM_MODE_ROW`), which is hardcoded to hold exactly 256 of these modes. Each present output (monitor) has its own instance of this structure.

[From `MadMaxSpec.h`](https://github.com/x0reaxeax/MadMaxCrashFix/blob/master/MadMaxCrashFixDLL-STEAM%20(PoC%20OOB%20fix)/MadMaxSpec.h)
```c
typedef struct _MM_MODE_ROW {
    DWORD dwWidth;
    DWORD dwHeight;
    DWORD dwRefreshOrId;
    DWORD dwModeIndex;
    BYTE  bFlagA;
    BYTE  bFlagB;
    // 2 bytes autopadded 
} MM_MODE_ROW, * PMM_MODE_ROW;
// sizeof(MM_MODE_ROW) == 20 bytes

typedef struct _MM_MONITOR_BLOCK {
    MM_MODE_ROW Rows[MM_MAX_MODE_ROWS]; // 256 * 20 = 0x1400 bytes
    DWORD       RowCount;               // Number of monitor modes to be processed
    // ... blah blah 
} MM_MONITOR_BLOCK, * PMM_MONITOR_BLOCK;
```

The problem comes into play when a monitor, or (more often than not) multiple monitors report more available modes than the game can **correctly** handle. Under normal and possibly intended circumstances, the hardcoded maximum amount of monitor modes should not cause a problem, even with large amount of available modes (besides the game not showing all the available modes), that is if there were correct bounds checks present in the code, which there are not (or they don't work as intended).  

The game runs multiple filtering and seemingly de-duplicating logics on all acquired modes, in order to determine the correct candidates for the currently used screen.
All of this happens in one very big spaghetti function - `sub_141E3845F`, which lacks the mentioned safety/bounds checks.
This big function calls another function - `sub_142563500`, which I call `GetModeInfo`. In short, this `GetModeInfo` function parses DXGI monitor modes and writes some of their fields to the game's own internal structures. When the game starts evaluating more modes than it can handle and store, the pointers to these internal game structures are overwritten, which causes them to point to other memory that was not intended to be overwritten. In the case of our crash, the pointer to be overwritten points to a value where the current display width is stored.  
This pointer is coincidentally overwritten with another one that points to the value of monitor modes for the currently evaluated monitor.
This value also serves as loop boundary for a loop that iterates over all available monitor modes. I call this variable `RowCount`, and it is located right after the array of `MM_MODE_ROW` structures in the `MM_MONITOR_BLOCK` structure.
In other words, the internal value of monitor modes previously retrieved and processed from DXGI APIs is overwritten with a value which represents the currently evaluated mode's display width.

To show this with an overly simplified and outright BS pseudocode, picture a loop like this:
```c
UINT *pointerToMaxAmountOfDisplayModes = DXGI::GetNumberOfDisplayModes();
for (UINT i = 0; i < *pointerToMaxAmountOfDisplayModes; i++) {
    // do stuff
    UINT *pointerToScreenWidth = someAddress;
    pointerToScreenWidth = pointerToMaxAmountOfDisplayModes; // oopsie, pointer overwritten
    *pointerToScreenWidth = 1920;                            // oopsie, OOB write (for example 1920)
}
```

This behavior/logic/whatchamacallit can be observed with [OOB-Detector](https://github.com/x0reaxeax/MadMaxCrashFix/blob/master/OOB-Detector), which hooks the `GetModeInfo` function to check + log all enumerated modes and OOB cases.

#### Shortened `OOB-Detector` output with comments:
```
[+] 3 adapters found.
    Adapter 0 Description: NVIDIA GeForce RTX 5090
[+] Adapter 0: 2 outputs found.
    Adapter 1 Description: AMD Radeon(TM) Graphics
[+] Adapter 1: 0 outputs found.
    Adapter 2 Description: Microsoft Basic Render Driver
[+] Adapter 2: 0 outputs found.
[+] Largest output count across adapters: 2

[*] Enumerating adapter '0'
    Adapter Description: NVIDIA GeForce RTX 5090
    VendorId: 0x10DE
    DeviceId: 0x2B85
    SubSysId: 0x53011462
    Revision: 0xA1
    DedicatedVideoMemory: 0x7D6400000
    DedicatedSystemMemory: 0x0
    SharedSystemMemory: 0xBB1CAE800
    AdapterLuid: L0x111E9 | H0x0
[+] 2 outputs found on adapter '0'.
[+] Output 0: 216 modes found.                                                   # 216 modes found for first monitor
    Mode 0: 640x480 @ 60/1 Hz
    Mode 1: 640x480 @ 72/1 Hz
    ...
    Mode 215: 2560x1440 @ 143912/1000 Hz
[+] Output 1: 546 modes found.                                                   # 546 modes found for second monitor
    Mode 0: 640x480 @ 24/1 Hz
    Mode 1: 640x480 @ 24/1 Hz

# We can see the same values here - they are read out of the game memory, along with the address where they reside.
[+] Hooked sub_142563500 (GetModeInfo) [Hook: 00007FFD22861220 | Orig: 00007FFD228668A8]
[RowCountAddress] 000000008E262648 val=216                                       # First monitor 'mode count address' and value  = 216
[IB:  RowAdd] 0000: mon=   0 mode=   1 rowIdx=   1 w= 640 h= 480 hz=  72
[IB:  RowAdd] 0001: mon=   0 mode=   2 rowIdx=   2 w= 640 h= 480 hz=  75
[IB:  RowAdd] 0002: mon=   0 mode=   3 rowIdx=   3 w= 640 h= 480 hz=  59
...
[IB:  RowAdd] 0123: mon=   0 mode= 215 rowIdx= 124 w=2560 h=1440 hz= 143
[RowCountAddress] 000000008E263A4C val=546                                       # Second monitor 'mode count address' and value = 546
[IB:  RowAdd] 0124: mon=   1 mode=   1 rowIdx=   1 w= 640 h= 480 hz=  24
[IB:  RowAdd] 0125: mon=   1 mode=   3 rowIdx=   2 w= 640 h= 480 hz=  25
...
[IB:  RowAdd] 0376: mon=   1 mode= 505 rowIdx= 253 w=1680 h=1050 hz=  24
[IB:  RowAdd] 0377: mon=   1 mode= 507 rowIdx= 254 w=1680 h=1050 hz=  25
[IB:  RowAdd] 0378: mon=   1 mode= 509 rowIdx= 255 w=1680 h=1050 hz=  30
# The following entries are detected out-of-bounds writes.
# In the first two entries, we can see that `aliasRowCount` is `true` (1), which indicates that the 'mode count address'
#   is colliding with the address where current mode's width is to be written to.
# This means that the alleged mode count is rewritten with value 1680 (0x6A0), even though the maximum amount of valid modes is 256.
# At this point, the game is already in 'undefined behavior' state.
[OOB: RowAdd] 0000: mon=   1 mode= 511 rowIdx= 256 inBounds=0 aliasRowCount=1 w=1680 h=1050 hz=  50 [pWidthOut=000000008E263A4C]
[OOB: RowAdd] 0001: mon=   1 mode= 512 rowIdx= 256 inBounds=0 aliasRowCount=1 w=1680 h=1050 hz=  60 [pWidthOut=000000008E263A4C]
[OOB: RowAdd] 0002: mon=   1 mode= 513 rowIdx= 257 inBounds=0 aliasRowCount=0 w=1680 h=1050 hz=  60 [pWidthOut=000000008E263A60]
[OOB: RowAdd] 0003: mon=   1 mode= 514 rowIdx= 257 inBounds=0 aliasRowCount=0 w=1680 h=1050 hz= 100 [pWidthOut=000000008E263A60]
```

Since the `PMM_MONITOR_BLOCK::RowCount` value has been overwritten with another arbitrary value, the `GetModeInfo` function is now being fed out-of-bounds nonsense, which it then passes to yet another function - `sub_142F5C420`, which I call `ExtractModeInfo`. This function is used for calculating the current mode's refresh rate and extracting it (along with other mode information like screen width, height, ..) from a DXGI structure into the game's own monitor mode information structure - the previously mentioned `MM_MODE_ROW` structure. In one of these checks, the value 0 is present where the denominator for refresh rate calculation is supposed to be stored.
This is what causes the "division by zero" exception that crashes the game.

Original DXGI structure:
```c
typedef struct DXGI_MODE_DESC {
    UINT Width;
    UINT Height;
    DXGI_RATIONAL RefreshRate;
    DXGI_FORMAT Format;
    DXGI_MODE_SCANLINE_ORDER ScanlineOrdering;
    DXGI_MODE_SCALING Scaling;
} DXGI_MODE_DESC;
```

Information is extracted in `ExtractModeInfo (sub_142F5C420)` into this "result out" structure:
```c
typedef struct _MM_MODE_RESULT {
    DWORD dwWidth;                              // (0x0)
    DWORD dwHeight;                             // (0x4)
    DWORD dwRefreshHz;                          // (0x8)
    BYTE bFlag;                                 // (0xC) – always set to 0 here
} MM_MODE_RESULT, *PMM_MODE_RESULT;
```

Which is then copied into the game's internal `MM_MODE_ROW` structure:
```c
typedef struct _MM_MODE_ROW {
    DWORD32 dwWidth;                            // v154 - incremented/decremented by 5
    DWORD32 dwHeight;                           // v157 - incremented/decremented by 5
    DWORD32 dwRefreshOrId;                      // v153 - incremented/decremented by 5
    DWORD32 dwModeIndexOrScore;                 // v155 - incremented/decremented by 5
    BYTE    bFlagA;                             // v252 - unknown
    BYTE    bFlagB;                             // v156 - incremented/decremented by 20
} MM_MODE_ROW, *PMM_MODE_ROW;
```

(at least if I remember correctly, cuz I clicked 'Don't save database' in IDA and I'm not waiting for that shi to get analyzed again.)
  
How the refresh rate is calculated in `ExtractModeInfo (sub_142F5C420)`:
```c
// No zero checking, this is what causes the startup crash
DWORD dwRefreshHz = dwNumerator / dwDenominator;
```

The original crash fix mod (MadMaxCrashFixDLL & MadMaxCrashFixDLL-STEAM) is forcefully replacing this division instruction and making the `ExtractModeInfo (sub_142F5C420)` function return a correctly calculated refresh rate value.
This however doesn't fix the out-of-bounds problem, although it is seemingly a sufficient fix, even though the game is already in a state of undefined behavior.
This is what I refer to as the "band-aid fix" version.

The C interpretation/reimplementation/recreation of reverse engineered functions `GetModeInfo (sub_142563500)` and `ExtractModeInfo (sub_142F5C420)`, along with most of the important structures can be found in [MadMaxOOB-RE](https://github.com/x0reaxeax/MadMaxCrashFix/tree/master/MadMaxOOB-RE/main.c).

DISCLAIMER: All of the research documented in this project is based on independent analysis in and only in attempts to fix the crash issue in Mad Max. The game is currently being sold in this state, and this bug has been present in it since launch (over 10 years ago), which makes it very unlikely that a proper fix will ever be released by the developers/publishers.

The reverse-engineered structures and functions are specific to the Steam version of the game (the one with stoopid Denuvo).

### "Band-aid fix" version

The crash stems from a "Division by Zero" exception caused by a `DIV` instruction (`div dword ptr ds : [rcx + r8 + 0xC]`) with incorrect source operand.
This instruction is supposed to calculate the refresh rate of currently used screen.

This fix version retrieves the correct refresh rate of the active screen, and patches the offending instruction with a `MOV r32, imm32` instruction, where `imm32` is the immediate 32-bit value of the screen's refresh rate in Hz, and `r32` is the `EAX` register.

I already contacted GoG with technical details, hopefully they will fix it.

### "OOB guard" version
This version hooks `GetModeInfo (sub_142563500)` and adds out-of-bounds checks during mode assessment, in order to prevent OOB writes of extracted monitor mode information.
These OOB writes are not discarded, but rather redirected to a safe location in memory.

This fix is only available for the Steam version of the game at the moment.

## Credits
  * [MinHook](https://github.com/TsudaKageyu/minhook) - Used for function hooking. 

## Disclaimer

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE.

BY DOWNLOADING OR USING THIS SOFTWARE, YOU AGREE TO THE TERMS OF THIS DISCLAIMER.
THE AUTHOR RESERVES THE RIGHT TO CHANGE THE TERMS OF THIS DISCLAIMER AT ANY TIME 
WITHOUT PRIOR NOTICE. THIS SOFTWARE IS INTENDED TO BE USED FOR EDUCATIONAL 
PURPOSES ONLY. THE AUTHOR IS NOT RESPONSIBLE FOR ANY DAMAGE OR LOSS OF DATA 
THAT MAY OCCUR FROM THE USE OF THIS SOFTWARE. THE USE OF THIS SOFTWARE MAY RESULT 
IN PERFORMANCE ISSUES, CRASHES, OR OTHER UNINTENDED BEHAVIOR. THE AUTHOR 
DISCLAIMS ALL WARRANTIES, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED 
TO IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, INCIDENTAL, INDIRECT, 
OR CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION, DAMAGES 
FOR LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, 
OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR INABILITY TO USE THIS 
SOFTWARE, EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
THIS DISCLAIMER MUST BE INCLUDED IN ALL COPIES OR SUBSTANTIAL PORTIONS OF THE SOFTWARE.
THE SOFTWARE CANNOT BE REDISTRIBUTED WITHOUT THIS DISCLAIMER.
ANY INFORMATION TAKEN FROM THIS SOFTWARE IS SUBJECT TO THIS DISCLAIMER.
THIS DISCLAIMER MAY NOT BE MODIFIED OR ALTERED BY ANYONE OTHER THAN THE AUTHOR.
