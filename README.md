# Forza Horizon 6 Fix — AMD RX 580 & Legacy Hardware Support

Welcome to the **Forza Horizon 6** compatibility solution repository for the legacy **AMD Radeon RX 580** graphics card (Polaris architecture) and processors with core counts below the minimum requirements.

This project provides an integrated **Dual-Proxy DLL** solution that bypasses the game's hardware check gates, intelligently emulates/spoofs DirectX 12 capabilities, and prevents startup crashes.

---

## 📁 Project Structure

This repository cleanly separates hardware fixes into two main components:

```
Forza-Horizon-6-RX-580-FH201-FH205-Fix/
├── 📁 bin/                  # Pre-compiled binary DLL files ready for use
│   ├── 📄 d3d12.dll         # GPU Fix Proxy
│   └── 📄 version.dll       # CPU Fix Proxy
├── 📁 cpu_fix/              # CPU fix source code (Bypass FH101)
│   ├── 📁 src/              # C++ code & version.dll export definitions
│   ├── 📁 build/            # CPU Fix compilation output
│   ├── 📄 build_cpu_fix.ps1 # CPU Fix automated build script
│   └── 📄 README.md         # CPU Fix technical documentation
└── 📁 gpu_fix/              # GPU fix source code (Bypass FH201 & FH205)
    ├── 📁 src/              # C++, Assembly, & d3d12.dll export definitions
    ├── 📁 build/            # GPU Fix compilation output
    ├── 📄 build_proxy.ps1   # GPU Fix automated build script
    └── 📄 README.md         # GPU Fix technical documentation
```

---

## ⚡ Instant Usage Guide (Ready to Use)

If you do not want to compile the source code yourself, you can directly copy the pre-compiled DLLs we provided:

1. Navigate to the [bin/](./~bin) folder.
2. Copy the `d3d12.dll` and `version.dll` files.
3. Paste both files into the directory where the game's executable `ForzaHorizon6.exe` is located.
4. Run Forza Horizon 6 normally!

---

## 🛠️ Component Fix Details

### 1. 🧠 CPU Fix Proxy (`version.dll`)
Handles bypassing startup failures caused by the **FH101** warning dialog (Processor does not meet minimum core requirements).
- **Mechanism**: Performs instant patching on the PEB (*Process Environment Block*), manipulates system environment variables, and hooks the `GetSystemInfo` / `GetLogicalProcessorInformationEx` API calls asynchronously via a *safe background thread* to avoid *loader lock deadlocks*.
- Read more: [cpu_fix/README.md](./~cpu_fix/README.md)

### 2. 🎮 GPU Fix Proxy (`d3d12.dll`)
Handles bypassing **FH201** / **FH205** errors and stabilizes the game on AMD RX 580 (Polaris) graphics cards which do not natively support DirectX 12 Feature Level 12_1.
- **Mechanism**: Spoofs the device Feature Level to `12_1`, hooks failed DirectX 12 format feature queries to return a safe emulated format (`S_OK` with zero capabilities), disables *Rasterizer Ordered Views* (ROVs) and *Mesh Shaders* which Polaris hardware lacks, and extends the GPU driver TDR tolerance to 30 seconds to prevent driver hangs.
- Read more: [gpu_fix/README.md](./~gpu_fix/README.md)

---

## 💻 Manual Compilation (Build from Source)

All build scripts require **Visual Studio 2022** with **C++ Desktop Development** and **MASM (Microsoft Macro Assembler)** components installed.

### Compiling CPU Fix
Open a PowerShell terminal in the repository directory and run:
```powershell
cd cpu_fix
.\build_cpu_fix.ps1
```

### Compiling GPU Fix
Open a PowerShell terminal in the repository directory and run:
```powershell
cd gpu_fix
.\build_proxy.ps1
```

Your compiled DLLs will be saved in the `build/` folder of their respective components.

---

## 📝 Release Notes & Stability

The latest version (v2.3) introduces **Safe Asynchronous Module Hooking** for the CPU proxy. With this fix, import address table (IAT) scanning runs safely outside the Windows OS loader lock. The game is now guaranteed to be 100% stable and free from random startup crashes during initial initialization.
