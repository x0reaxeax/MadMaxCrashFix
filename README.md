# MadMaxCrashFix

### Fix for GoG version of Mad Max

### Usage
Choose a version of your liking.  
It's been reported that the injector version may prevent achievements from being obtained.

 * **Injector (EXE) version**
   * Put the EXE file into the game directory where `MadMax.exe` is located
   * Run the EXE
 * **DLL (dinput8.dll) version**
   * Put `dinput8.dll` into the game directory where `MadMax.exe` is located
   * Start the game as usual

### Notes
 * If the game is laggy in main menu, change the resolution or switch to borderless, apply, then switch back.
 * Turning V-Sync off is advised.

### Technical Description

The crash is caused by a "Division by Zero" exception caused by a `DIV` instruction (`div dword ptr ds : [rcx + r8 + 0xC]`) with incorrect source operand.
This instruction is supposed to retrieve the refresh rate of currently used screen.


This program retrieves the correct refresh rate of the active screen, and patches the offending instruction with a `MOV r32, imm32` instruction, where `imm32` is the immediate 32-bit value of the screen's refresh rate in Hz, and `r32` is the `EAX` register.

I already contacted GoG with technical details, hopefully they will fix it.

### Steam version?
Since the Steam version is still for some reason poisoned with Denuvo, I don't have a fix and this program won't work with it.

Go fuck yourself, Denuvo.


### Disclaimer

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
