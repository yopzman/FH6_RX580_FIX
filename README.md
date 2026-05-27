# DirectX 12 + CPU Proxy Fix — `d3d12.dll` + `version.dll`

This repository contains a low-level proxy fix that uses two DLLs — `d3d12.dll` and `version.dll` — to bypass unsupported GPU and CPU checks for older hardware.

The fix is built around two proxy DLLs that make the application believe the system supports modern DirectX 12 features and a sufficient core count.

> [!WARNING]
> **Experimental Project**: This project is highly experimental and can crash at any time. Use at your own risk!
> 
> **Driver Requirement**: This fix specifically requires the [AMD Agility SDK Driver](https://www.amd.com/en/resources/support-articles/release-notes/RN-RAD-MS-AGILITY-SDK-2023-6-711.html) to function correctly.
> 
> **Other Games**: The GPU proxy (`d3d12.dll`) can be used to bypass DirectX 12 Feature Level checks in other games as well (successfully tested in *Assassin's Creed Shadows*).

---

## 📁 Project Structure

Here is how the project is organized:

```
d3d12_version_proxy/
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
