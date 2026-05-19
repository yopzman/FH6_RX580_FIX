# 🎮 GPU Fix Proxy (`d3d12.dll`) — Mechanics & Compilation

This component is designed to bypass the hard hardware compatibility check gates (**FH201** / **FH205**) for the legacy **AMD Radeon RX 580** (Polaris architecture) GPU during the launch of Forza Horizon 6 and stabilize rendering performance during gameplay.

## 🛠️ Technical Mechanism Details

This fix is dynamically injected as a DLL Proxy `d3d12.dll` in the game's folder. When the game calls the DirectX 12 API, the proxy performs several critical manipulations:

1. **Feature Level Spoofing**:
   - The AMD RX 580 physically supports DirectX 12 up to Feature Level `12_0`. However, Forza Horizon 6 requires at least Feature Level `12_1` to run.
   - The proxy intercepts the `D3D12CreateDevice` call. It forces hardware device creation at the GPU's native level (`12_0`), but returns a success status to the game as if the device is fully operating at Feature Level `12_1`.
2. **Format Support Emulation**:
   - The Forza Horizon 6 engine queries hundreds of DXGI formats (including legacy video compression & HDR formats). When the AMD Polaris driver returns a failure code `E_FAIL` for a format not natively supported, the game immediately suffers a startup crash with code `FHE01` (fatal error).
   - The proxy intercepts `CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT)` and intelligently returns a success code `S_OK` with zero capabilities (`Support = NONE`). This allows the game's internal fallback mechanisms to handle it safely without triggering a crash.
3. **Feature Dispoofing for FL 12_1 Exclusives**:
   - Because the game believes the GPU is running at `12_1`, it might attempt to use hardware features that Polaris lacks. The proxy forces the following flags to be disabled to prevent pipeline creation crashes:
     - **Rasterizer Ordered Views (ROVs)**: Forced to `FALSE` in `OPTIONS` queries.
     - **Mesh Shaders**: Forced to `D3D12_MESH_SHADER_TIER_NOT_SUPPORTED` in `OPTIONS7` queries.
4. **TDR Delay Extension**:
   - Prevents the Polaris GPU driver from suffering a *TDR Reset* (driver crash/hang) during heavy shader asset loading on the loading screen by extending the Windows `TdrDelay` registry tolerance to **30 seconds**.

---

## 💻 Compiling from Source Code

### Prerequisites
- Visual Studio 2022 (or Build Tools 2022).
- C++ Desktop Development workload.
- **MASM (Microsoft Macro Assembler)** installed (automatically included with VS C++).

### Build Steps
1. Open a PowerShell terminal in this folder (`gpu_fix/`).
2. Run the automated build script:
   ```powershell
   .\build_proxy.ps1
   ```
3. Your compiled binary (`d3d12.dll`) will be located in the `build/` folder.

---

## 🚀 File Deployment

1. Copy `d3d12.dll` from the `build/` folder.
2. Paste the file into the directory where `ForzaHorizon6.exe` is located.
3. Launch the game normally.
