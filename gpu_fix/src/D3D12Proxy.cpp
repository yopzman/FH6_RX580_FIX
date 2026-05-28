#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <string>

static std::mutex g_logMutex;
static HMODULE g_originalD3D12 = nullptr;

using D3D12CreateDevice_t = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
using CheckFeatureSupport_t = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, D3D12_FEATURE, void*, UINT);
using GetDeviceRemovedReason_t = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*);

static D3D12CreateDevice_t g_originalD3D12CreateDevice = nullptr;
static CheckFeatureSupport_t g_originalCheckFeatureSupport = nullptr;
static GetDeviceRemovedReason_t g_originalGetDeviceRemovedReason = nullptr;
static LONG g_featureLogBudget = 500;
static LONG g_deviceIndex = 0;
static LONG g_formatFailLogBudget = 20;
static D3D_FEATURE_LEVEL g_actualDeviceFeatureLevel = D3D_FEATURE_LEVEL_11_0;
static bool g_renderDeviceCreated = false;

static constexpr size_t kCheckFeatureSupportVtableIndex = 13;
static constexpr size_t kGetDeviceRemovedReasonVtableIndex = 14;

static void Log(const char* msg) {
    std::lock_guard<std::mutex> lock(g_logMutex);

    std::ofstream log("gpu_fix.log", std::ios::app);
    if (!log) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    struct tm tmBuf {};
    localtime_s(&tmBuf, &time);
    log << std::put_time(&tmBuf, "%H:%M:%S") << " - " << msg << std::endl;
}

static const char* FeatureLevelName(D3D_FEATURE_LEVEL level) {
    switch (level) {
    case D3D_FEATURE_LEVEL_12_2: return "12_2";
    case D3D_FEATURE_LEVEL_12_1: return "12_1";
    case D3D_FEATURE_LEVEL_12_0: return "12_0";
    case D3D_FEATURE_LEVEL_11_1: return "11_1";
    case D3D_FEATURE_LEVEL_11_0: return "11_0";
    default: return "unknown";
    }
}

static const char* FeatureName(D3D12_FEATURE feature) {
    switch (feature) {
    case D3D12_FEATURE_D3D12_OPTIONS:        return "OPTIONS";
    case D3D12_FEATURE_ARCHITECTURE:         return "ARCHITECTURE";
    case D3D12_FEATURE_FEATURE_LEVELS:       return "FEATURE_LEVELS";
    case D3D12_FEATURE_FORMAT_SUPPORT:       return "FORMAT_SUPPORT";
    case D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS: return "MULTISAMPLE_QUALITY_LEVELS";
    case D3D12_FEATURE_FORMAT_INFO:          return "FORMAT_INFO";
    case D3D12_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT: return "GPU_VIRTUAL_ADDRESS_SUPPORT";
    case D3D12_FEATURE_SHADER_MODEL:         return "SHADER_MODEL";
    case D3D12_FEATURE_D3D12_OPTIONS1:       return "OPTIONS1";
    case D3D12_FEATURE_D3D12_OPTIONS2:       return "OPTIONS2";
    case D3D12_FEATURE_D3D12_OPTIONS3:       return "OPTIONS3";
    case D3D12_FEATURE_D3D12_OPTIONS4:       return "OPTIONS4";
    case D3D12_FEATURE_D3D12_OPTIONS5:       return "OPTIONS5";
    case D3D12_FEATURE_D3D12_OPTIONS6:       return "OPTIONS6";
    case D3D12_FEATURE_D3D12_OPTIONS7:       return "OPTIONS7";
    case D3D12_FEATURE_D3D12_OPTIONS8:       return "OPTIONS8";
    case D3D12_FEATURE_D3D12_OPTIONS9:       return "OPTIONS9";
    case D3D12_FEATURE_D3D12_OPTIONS10:      return "OPTIONS10";
    case D3D12_FEATURE_D3D12_OPTIONS11:      return "OPTIONS11";
    case D3D12_FEATURE_D3D12_OPTIONS12:      return "OPTIONS12";
    case D3D12_FEATURE_D3D12_OPTIONS13:      return "OPTIONS13";
    case static_cast<D3D12_FEATURE>(10):     return "PROTECTED_RESOURCE_SESSION_SUPPORT";
    case static_cast<D3D12_FEATURE>(12):     return "ROOT_SIGNATURE";
    case static_cast<D3D12_FEATURE>(16):     return "ARCHITECTURE1";
    case static_cast<D3D12_FEATURE>(43):     return "OPTIONS14";
    case static_cast<D3D12_FEATURE>(44):     return "OPTIONS15";
    case static_cast<D3D12_FEATURE>(45):     return "OPTIONS16";
    case static_cast<D3D12_FEATURE>(46):     return "OPTIONS17";
    case static_cast<D3D12_FEATURE>(47):     return "OPTIONS18";
    case static_cast<D3D12_FEATURE>(48):     return "OPTIONS19";
    default: {
        static thread_local char unknownBuf[32];
        std::snprintf(unknownBuf, sizeof(unknownBuf), "UNKNOWN(%u)",
            static_cast<unsigned>(feature));
        return unknownBuf;
    }
    }
}

static bool LoadOriginalD3D12() {
    if (g_originalD3D12CreateDevice) {
        return true;
    }

    char systemPath[MAX_PATH] {};
    if (!GetSystemDirectoryA(systemPath, MAX_PATH)) {
        Log("GetSystemDirectoryA failed");
        return false;
    }

    std::string dllPath = std::string(systemPath) + "\\d3d12.dll";
    g_originalD3D12 = LoadLibraryA(dllPath.c_str());
    if (!g_originalD3D12) {
        Log("LoadLibraryA failed for original d3d12.dll");
        return false;
    }

    g_originalD3D12CreateDevice = reinterpret_cast<D3D12CreateDevice_t>(
        GetProcAddress(g_originalD3D12, "D3D12CreateDevice")
    );

    if (!g_originalD3D12CreateDevice) {
        Log("GetProcAddress failed for original D3D12CreateDevice");
        return false;
    }

    return true;
}

extern "C" FARPROC WINAPI GetOriginalProcByName(const char* name) {
    if (!LoadOriginalD3D12()) {
        return nullptr;
    }
    return GetProcAddress(g_originalD3D12, name);
}

extern "C" FARPROC WINAPI GetOriginalProcByOrdinal(WORD ordinal) {
    if (!LoadOriginalD3D12()) {
        return nullptr;
    }
    return GetProcAddress(g_originalD3D12, reinterpret_cast<LPCSTR>(ordinal));
}

static HRESULT STDMETHODCALLTYPE HookedCheckFeatureSupport(
    ID3D12Device* self,
    D3D12_FEATURE feature,
    void* pFeatureSupportData,
    UINT featureSupportDataSize
) {
    // === DIRECT BYPASS FOR UNSAFE FORMATS ===
    // If format >= 190 is queried, directly return S_OK with NONE support.
    // This completely bypasses the driver call and avoids any crash in amdxc64.dll.
    if (feature == D3D12_FEATURE_FORMAT_SUPPORT &&
        featureSupportDataSize >= sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT) &&
        pFeatureSupportData) {
        auto* data = reinterpret_cast<D3D12_FEATURE_DATA_FORMAT_SUPPORT*>(pFeatureSupportData);
        if (data->Format >= 190) {
            data->Support1 = D3D12_FORMAT_SUPPORT1_NONE;
            data->Support2 = D3D12_FORMAT_SUPPORT2_NONE;
            if (InterlockedDecrement(&g_formatFailLogBudget) >= 0) {
                char logBuf[256] {};
                std::snprintf(logBuf, sizeof(logBuf),
                    "CheckFeatureSupport FORMAT_SUPPORT (3): format=%u directly bypassed to prevent amdxc64.dll crash",
                    static_cast<unsigned>(data->Format));
                Log(logBuf);
            }
            return S_OK;
        }
    }

    CheckFeatureSupport_t original = g_originalCheckFeatureSupport;
    if (!original) {
        return E_FAIL;
    }

    // Call driver inside Structured Exception Handling for absolute safety.
    HRESULT hr = E_FAIL;
    __try {
        hr = original(self, feature, pFeatureSupportData, featureSupportDataSize);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (InterlockedDecrement(&g_formatFailLogBudget) >= 0) {
            char buf[256] {};
            std::snprintf(buf, sizeof(buf),
                "*** SEH EXCEPTION CAUGHT inside driver during CheckFeatureSupport %s (%u) ***",
                FeatureName(feature),
                static_cast<unsigned>(feature));
            Log(buf);
        }
        hr = E_FAIL;
    }

    // ---- Logging ----
    if (feature == D3D12_FEATURE_FORMAT_SUPPORT) {
        if (FAILED(hr) && InterlockedDecrement(&g_formatFailLogBudget) >= 0) {
            char buf[256] {};
            DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
            if (pFeatureSupportData && featureSupportDataSize >= sizeof(DXGI_FORMAT)) {
                fmt = *reinterpret_cast<DXGI_FORMAT*>(pFeatureSupportData);
            }
            std::snprintf(buf, sizeof(buf),
                "CheckFeatureSupport FORMAT_SUPPORT (3): format=%u HRESULT 0x%08lX -> spoofed S_OK",
                static_cast<unsigned>(fmt),
                static_cast<unsigned long>(hr));
            Log(buf);
        }
    } else if (InterlockedDecrement(&g_featureLogBudget) >= 0 || FAILED(hr)) {
        char buf[256] {};
        std::snprintf(buf, sizeof(buf),
            "CheckFeatureSupport %s (%u): HRESULT 0x%08lX",
            FeatureName(feature),
            static_cast<unsigned>(feature),
            static_cast<unsigned long>(hr));
        Log(buf);
    }

    if (!pFeatureSupportData) {
        return hr;
    }

    // === SPOOF: Feature Levels ===
    if (feature == D3D12_FEATURE_FEATURE_LEVELS &&
        featureSupportDataSize >= sizeof(D3D12_FEATURE_DATA_FEATURE_LEVELS)) {
        auto* levels = reinterpret_cast<D3D12_FEATURE_DATA_FEATURE_LEVELS*>(pFeatureSupportData);
        levels->MaxSupportedFeatureLevel = D3D_FEATURE_LEVEL_12_1;
        Log("Spoof: FEATURE_LEVELS -> S_OK, MaxSupportedFeatureLevel -> 12_1");
        return S_OK;
    }

    // === SPOOF: FORMAT_SUPPORT -> return S_OK with zero support on failure ===
    if (feature == D3D12_FEATURE_FORMAT_SUPPORT &&
        featureSupportDataSize >= sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)) {
        if (FAILED(hr)) {
            auto* fmtSupport = reinterpret_cast<D3D12_FEATURE_DATA_FORMAT_SUPPORT*>(pFeatureSupportData);
            fmtSupport->Support1 = D3D12_FORMAT_SUPPORT1_NONE;
            fmtSupport->Support2 = D3D12_FORMAT_SUPPORT2_NONE;
            return S_OK;
        }
        return hr;
    }

    // === SPOOF: D3D12_OPTIONS -> ensure ROVsSupported = FALSE ===
    if (feature == D3D12_FEATURE_D3D12_OPTIONS &&
        featureSupportDataSize >= sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS)) {
        auto* opts = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS*>(pFeatureSupportData);
        if (opts->ROVsSupported) {
            opts->ROVsSupported = FALSE;
            Log("Spoof: OPTIONS ROVsSupported forced FALSE (RX 580 limitation)");
        }
        return hr;
    }

    // === SPOOF: OPTIONS7 -> ensure MeshShaderTier = MeshShader and SamplerFeedback NOT SUPPORTED ===
    if (feature == D3D12_FEATURE_D3D12_OPTIONS7 &&
        featureSupportDataSize >= sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS7)) {
        auto* opts7 = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS7*>(pFeatureSupportData);
        if (opts7->MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED ||
            opts7->SamplerFeedbackTier != D3D12_SAMPLER_FEEDBACK_TIER_NOT_SUPPORTED) {
            opts7->MeshShaderTier = D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
            opts7->SamplerFeedbackTier = D3D12_SAMPLER_FEEDBACK_TIER_NOT_SUPPORTED;
            Log("Spoof: OPTIONS7 MeshShader and SamplerFeedback forced NOT_SUPPORTED (RX 580 limitation)");
        }
        return hr;
    }

    return hr;
}

// === Hook: GetDeviceRemovedReason ===
static HRESULT STDMETHODCALLTYPE HookedGetDeviceRemovedReason(ID3D12Device* self) {
    GetDeviceRemovedReason_t original = g_originalGetDeviceRemovedReason;
    if (!original) {
        return E_FAIL;
    }

    HRESULT hr = original(self);
    if (hr != S_OK) {
        char buf[256] {};
        const char* reason = "UNKNOWN";
        switch (static_cast<unsigned long>(hr)) {
        case 0x887A0006: reason = "DXGI_ERROR_DEVICE_HUNG (GPU timeout/TDR)"; break;
        case 0x887A0005: reason = "DXGI_ERROR_DEVICE_REMOVED"; break;
        case 0x887A0007: reason = "DXGI_ERROR_DEVICE_RESET"; break;
        case 0x887A0001: reason = "DXGI_ERROR_INVALID_CALL"; break;
        case 0x80070057: reason = "E_INVALIDARG"; break;
        case 0x8007000E: reason = "E_OUTOFMEMORY"; break;
        }
        std::snprintf(buf, sizeof(buf),
            "*** GetDeviceRemovedReason: 0x%08lX (%s) ***",
            static_cast<unsigned long>(hr), reason);
        Log(buf);
    }
    return hr;
}

static void PatchVtableSlot(void** vtable, size_t index, void* newFunc, void** outOriginal, const char* name, LONG devIdx) {
    void* current = vtable[index];

    if (current == newFunc) {
        char buf[128] {};
        std::snprintf(buf, sizeof(buf), "[Device#%ld] %s already hooked, skipped",
            static_cast<long>(devIdx), name);
        Log(buf);
        return;
    }

    if (!*outOriginal) {
        InterlockedCompareExchangePointer(outOriginal, current, nullptr);
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        char buf[128] {};
        std::snprintf(buf, sizeof(buf), "[Device#%ld] VirtualProtect failed for %s",
            static_cast<long>(devIdx), name);
        Log(buf);
        return;
    }

    vtable[index] = newFunc;

    DWORD ignored = 0;
    VirtualProtect(&vtable[index], sizeof(void*), oldProtect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), &vtable[index], sizeof(void*));

    char buf[128] {};
    std::snprintf(buf, sizeof(buf), "[Device#%ld] Patch applied: %s",
        static_cast<long>(devIdx), name);
    Log(buf);
}

static void PatchDevice(IUnknown* deviceUnknown, LONG devIdx) {
    if (!deviceUnknown) {
        return;
    }

    ID3D12Device* device = nullptr;
    HRESULT hr = deviceUnknown->QueryInterface(IID_PPV_ARGS(&device));
    if (FAILED(hr) || !device) {
        char buf[128] {};
        std::snprintf(buf, sizeof(buf),
            "[Device#%ld] Failed to QI ID3D12Device for patching",
            static_cast<long>(devIdx));
        Log(buf);
        return;
    }

    void*** object = reinterpret_cast<void***>(device);
    void** vtable = *object;

    PatchVtableSlot(vtable, kCheckFeatureSupportVtableIndex,
        reinterpret_cast<void*>(&HookedCheckFeatureSupport),
        reinterpret_cast<void**>(&g_originalCheckFeatureSupport),
        "CheckFeatureSupport", devIdx);

    PatchVtableSlot(vtable, kGetDeviceRemovedReasonVtableIndex,
        reinterpret_cast<void*>(&HookedGetDeviceRemovedReason),
        reinterpret_cast<void**>(&g_originalGetDeviceRemovedReason),
        "GetDeviceRemovedReason", devIdx);

    device->Release();
}

extern "C" HRESULT WINAPI D3D12CreateDevice(
    IUnknown* pAdapter,
    D3D_FEATURE_LEVEL minimumFeatureLevel,
    REFIID riid,
    void** ppDevice
) {
    LONG devIdx = InterlockedIncrement(&g_deviceIndex);

    char logBuf[256] {};
    std::snprintf(logBuf, sizeof(logBuf),
        "=== [Device#%ld] D3D12CreateDevice intercepted (requested FL %s) ===",
        static_cast<long>(devIdx),
        FeatureLevelName(minimumFeatureLevel));
    Log(logBuf);

    if (!LoadOriginalD3D12()) {
        if (ppDevice) {
            *ppDevice = nullptr;
        }
        return E_FAIL;
    }

    if (!ppDevice) {
        return g_originalD3D12CreateDevice(pAdapter, minimumFeatureLevel, riid, ppDevice);
    }

    const D3D_FEATURE_LEVEL tryLevels[] = {
        minimumFeatureLevel,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    D3D_FEATURE_LEVEL lastTried = static_cast<D3D_FEATURE_LEVEL>(0);
    HRESULT lastHr = E_FAIL;

    for (D3D_FEATURE_LEVEL level : tryLevels) {
        if (level == lastTried) {
            continue;
        }
        lastTried = level;

        *ppDevice = nullptr;
        lastHr = g_originalD3D12CreateDevice(pAdapter, level, riid, ppDevice);

        char buf[256] {};
        std::snprintf(buf, sizeof(buf),
            "[Device#%ld] Attempt D3D12CreateDevice FL %s: HRESULT 0x%08lX",
            static_cast<long>(devIdx),
            FeatureLevelName(level),
            static_cast<unsigned long>(lastHr));
        Log(buf);

        if (SUCCEEDED(lastHr)) {
            g_actualDeviceFeatureLevel = level;

            if (minimumFeatureLevel == D3D_FEATURE_LEVEL_12_1) {
                g_renderDeviceCreated = true;
                std::snprintf(buf, sizeof(buf),
                    "[Device#%ld] RENDER DEVICE created at FL %s (game requested 12_1)",
                    static_cast<long>(devIdx),
                    FeatureLevelName(level));
                Log(buf);
            } else {
                std::snprintf(buf, sizeof(buf),
                    "[Device#%ld] Device created successfully (FL %s)",
                    static_cast<long>(devIdx),
                    FeatureLevelName(level));
                Log(buf);
            }

            PatchDevice(reinterpret_cast<IUnknown*>(*ppDevice), devIdx);
            return lastHr;
        }
    }

    *ppDevice = nullptr;
    std::snprintf(logBuf, sizeof(logBuf),
        "[Device#%ld] TOTAL FAILURE - no feature level worked",
        static_cast<long>(devIdx));
    Log(logBuf);
    return lastHr;
}

// === Crash handler ===
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
    char buf[512] {};
    std::snprintf(buf, sizeof(buf),
        "*** CRASH DETECTED ***\n"
        "  ExceptionCode: 0x%08lX\n"
        "  ExceptionAddress: 0x%p\n"
        "  RenderDeviceCreated: %s\n"
        "  ActualFeatureLevel: %s",
        static_cast<unsigned long>(ep->ExceptionRecord->ExceptionCode),
        ep->ExceptionRecord->ExceptionAddress,
        g_renderDeviceCreated ? "YES" : "NO",
        FeatureLevelName(g_actualDeviceFeatureLevel));
    Log(buf);
    return EXCEPTION_CONTINUE_SEARCH;
}

// === TDR timeout extension ===
static void TryExtendTdrTimeout() {
    HKEY hKey = nullptr;
    LSTATUS status = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
        0, KEY_READ, &hKey);

    if (status == ERROR_SUCCESS) {
        DWORD tdrDelay = 0;
        DWORD size = sizeof(tdrDelay);
        status = RegQueryValueExA(hKey, "TdrDelay", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&tdrDelay), &size);
        RegCloseKey(hKey);

        if (status == ERROR_SUCCESS && tdrDelay >= 10) {
            char buf[128] {};
            std::snprintf(buf, sizeof(buf),
                "TDR timeout already extended: TdrDelay=%lu seconds",
                static_cast<unsigned long>(tdrDelay));
            Log(buf);
            return;
        }
    }

    hKey = nullptr;
    status = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
        0, KEY_WRITE, &hKey);

    if (status == ERROR_SUCCESS) {
        DWORD newDelay = 30;
        status = RegSetValueExA(hKey, "TdrDelay", 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&newDelay), sizeof(newDelay));
        RegCloseKey(hKey);

        if (status == ERROR_SUCCESS) {
            Log("TDR timeout extended to 30 seconds (restart required for effect)");
        } else {
            Log("TDR: Could not write TdrDelay (run as admin for auto-config)");
        }
    } else {
        Log("TDR: Could not open registry key (non-admin, skipped)");
    }
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        Log("=== d3d12.dll proxy loaded (v2.3 - Clean + Format Bypass) ===");
        SetUnhandledExceptionFilter(CrashHandler);
        TryExtendTdrTimeout();
    }
    return TRUE;
}
