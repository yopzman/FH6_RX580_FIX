# 🧠 CPU Fix Proxy (`version.dll`)

This fix is made to bypass the **FH101** error you get when trying to run Forza Horizon 6 on a CPU with fewer than 12 logical processors.

## 🛠️ How It Works

The fix uses a DLL proxy (`version.dll`) that intercepts the game's startup sequence and does three main things:

1. **PEB Spoofing**:
   - It quietly edits the `ActiveProcessorAffinityMask` right as the game starts up. This method is incredibly fast and avoids triggering any Windows loader locks.
2. **Environment Variable Trick**:
   - It forces the `NUMBER_OF_PROCESSORS` system variable to `12` so the game thinks you have enough cores.
3. **Safe IAT Hooking**:
   - The proxy scans the game's memory to intercept hardware queries (like `GetSystemInfo`).
   - **Why this is better now**: In older versions, doing this immediately caused random crashes. Now, the scan runs safely in the background just after the game finishes loading, ensuring 100% stability.

---

## 💻 Compiling it Yourself

### What You Need
- Visual Studio 2022 (or Build Tools 2022).
- The C++ Desktop Development workload.

### How to Build
1. Open PowerShell inside this folder (`cpu_fix/`).
2. Run the build script:
   ```powershell
   .\build_cpu_fix.ps1
   ```
3. Grab your fresh `version.dll` from the `build/` folder.

---

## 🚀 Installation

1. Copy the `version.dll` file.
2. Drop it in the same folder as `ForzaHorizon6.exe`.
3. Start the game.
