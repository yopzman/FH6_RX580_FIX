# 🧠 CPU Fix Proxy (`version.dll`) — Mechanics & Compilation

This component is designed to resolve the hard **FH101** warning dialog during the launch of Forza Horizon 6. The game requires a minimum of 12 *logical processors* to pass the startup compatibility check.

## 🛠️ Technical Mechanism Details

This fix utilizes runtime DLL Proxying against the native Windows `version.dll` library. When the game loads this proxy, the module executes a three-stage bypass sequentially:

1. **PEB Spoofing (Process Environment Block)**:
   - Modifies the `ActiveProcessorAffinityMask` bitmask directly inside the process's PEB structure at startup (`gs:[60h]` on x64).
   - This technique is extremely fast and 100% safe as it does not trigger the loader lock or require Windows APIs.
2. **Environment Variable Spoofing**:
   - Manipulates the `NUMBER_OF_PROCESSORS` system environment variable to `12` using the `SetEnvironmentVariableA` API.
3. **Safe Asynchronous IAT Hooking**:
   - The proxy performs a scan of the main game's (`ForzaHorizon6.exe`) Import Address Table (IAT) to hook native Windows hardware query functions:
     - `GetSystemInfo`
     - `GetNativeSystemInfo`
     - `GetLogicalProcessorInformation`
     - `GetLogicalProcessorInformationEx`
     - `GetActiveProcessorCount`
   - **Loader Lock Bypass**: Unlike older proxy versions that scanned modules synchronously within `DllMain` (which frequently triggered deadlocks/random startup crashes), this version launches the global IAT scan asynchronously via a **background thread** with a 50ms delay after the Loader Lock has been fully released. This guarantees 100% game stability.

---

## 💻 Compiling from Source Code

### Prerequisites
- Visual Studio 2022 (or Build Tools 2022).
- C++ Desktop Development workload installed.

### Build Steps
1. Open a PowerShell terminal in this folder (`cpu_fix/`).
2. Run the automated build script:
   ```powershell
   .\build_cpu_fix.ps1
   ```
3. Your compiled binary (`version.dll`) will be located in the `build/` folder.

---

## 🚀 File Deployment

1. Copy `version.dll` from the `build/` folder.
2. Paste the file into the directory where `ForzaHorizon6.exe` is located.
3. Launch the game normally.
