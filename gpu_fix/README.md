# 🎮 GPU Fix Proxy (`d3d12.dll`)

This fix bypasses the **FH201** and **FH205** hardware errors you get when trying to play Forza Horizon 6 on an AMD Radeon RX 580 (or similar Polaris GPU).

## 🛠️ How It Works

This fix acts as a custom `d3d12.dll`. When the game tries to use DirectX 12, this proxy steps in and makes a few changes so the game doesn't crash:

1. **Spoofing the Feature Level**:
   - The game demands DirectX 12 Feature Level `12_1`, but the RX 580 only supports `12_0`. The proxy tells the game that the hardware is running at `12_1`, letting it boot.
2. **Handling Unsupported Formats**:
   - The game checks for a bunch of modern video and HDR formats. Normally, if the GPU says "no", the game crashes instantly with a fatal error. The proxy simply replies "yes, but with zero capabilities" so the game can safely fall back to something else.
3. **Disabling Missing Features**:
   - Because we told the game it's running `12_1`, it tries to use things the RX 580 doesn't have, like Rasterizer Ordered Views (ROVs) and Mesh Shaders. The proxy forces these flags off to prevent pipeline crashes.
4. **Preventing Driver Timeouts**:
   - It gives the GPU more time to load heavy assets without Windows thinking the driver crashed (increasing the TDR timeout to 30 seconds).

---

## 💻 Compiling it Yourself

### What You Need
- Visual Studio 2022 (with C++ Desktop Development).
- Microsoft Macro Assembler (MASM) - usually included with the VS C++ tools.

### How to Build
1. Open PowerShell inside this folder (`gpu_fix/`).
2. Run the build script:
   ```powershell
   .\build_proxy.ps1
   ```
3. You'll find your compiled `d3d12.dll` in the `build/` folder.

---

## 🚀 Installation

1. Copy the `d3d12.dll` file.
2. Drop it in the same folder as `ForzaHorizon6.exe`.
3. Start the game.
