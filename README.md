# Forza Horizon 6 Fix — AMD RX 580 & CPU Core Bypass

This repository contains a fix to get **Forza Horizon 6** running on older hardware, specifically the AMD Radeon RX 580 (Polaris) and CPUs that don't meet the game's strict core count requirements.

The fix uses two proxy DLLs to trick the game into thinking your hardware is supported, bypassing the startup crashes.

---

## 📁 Project Structure

Here is how the project is organized:

```
Forza-Horizon-6-RX-580-FH201-FH205-Fix/
├── 📁 bin/                  # Pre-compiled DLLs (ready to use)
│   ├── 📄 d3d12.dll         # GPU fix
│   └── 📄 version.dll       # CPU fix
├── 📁 cpu_fix/              # CPU fix source code
│   └── ...                  # C++ code, build script, and docs
└── 📁 gpu_fix/              # GPU fix source code
    └── ...                  # C++ code, build script, and docs
```

---

## ⚡ Quick Start (Ready to Use)

If you just want to play the game without building from source, grab the pre-compiled files:

1. Open the [bin/](./bin) folder.
2. Copy `d3d12.dll` and `version.dll`.
3. Paste both files right next to your `ForzaHorizon6.exe` file.
4. Launch the game and enjoy!

---

## 🛠️ How It Works

### 1. 🧠 CPU Fix (`version.dll`)
This bypasses the **FH101** warning dialog you get when your processor doesn't have enough logical cores.
- **How it does it**: It patches the process environment and system variables at startup to report 12 cores, allowing the game to load safely without deadlocks.
- More info: [cpu_fix/README.md](./cpu_fix/README.md)

### 2. 🎮 GPU Fix (`d3d12.dll`)
This fixes the **FH201** and **FH205** errors that prevent the game from running on older cards like the RX 580, which lack native DirectX 12 Feature Level 12_1 support.
- **How it does it**: It spoofs the GPU feature level to `12_1`, safely disables unsupported features (like Mesh Shaders and ROVs), and extends the driver timeout to prevent crashes during loading screens.
- More info: [gpu_fix/README.md](./gpu_fix/README.md)

---

## 💻 Building from Source

If you prefer to compile it yourself, you'll need **Visual Studio 2022** (with C++ Desktop Development and MASM).

### Compiling the CPU Fix
Open PowerShell in the project folder and run:
```powershell
cd cpu_fix
.\build_cpu_fix.ps1
```

### Compiling the GPU Fix
Similarly, for the GPU fix:
```powershell
cd gpu_fix
.\build_proxy.ps1
```

You'll find your newly compiled DLLs inside the `build/` folder of each component.

---

## 📝 Release Notes

The latest version (v2.3) includes a much safer hooking method for the CPU proxy. The game should now be perfectly stable during startup with no random crashes.
