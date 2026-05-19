#define WIN32_LEAN_AND_MEAN
#define NOVERSION
#include <windows.h>
#undef NOVERSION

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <intrin.h>
#include <mutex>
#include <string>
#include <tlhelp32.h>

static std::mutex g_logMutex;
static HMODULE g_originalVersion = nullptr;

static constexpr DWORD kMinLogicalProcessors = 12;

static void Log(const char* msg) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ofstream log("ForzaFix_CPU.log", std::ios::app);
    if (!log) return;
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    struct tm tmBuf {};
    localtime_s(&tmBuf, &time);
    log << std::put_time(&tmBuf, "%H:%M:%S") << " - " << msg << std::endl;
}

static bool LoadOriginalVersion() {
    if (g_originalVersion) return true;
    char systemPath[MAX_PATH] {};
    if (!GetSystemDirectoryA(systemPath, MAX_PATH)) return false;
    std::string dllPath = std::string(systemPath) + "\\version.dll";
    g_originalVersion = LoadLibraryA(dllPath.c_str());
    return g_originalVersion != nullptr;
}

// All version.dll exports forwarded via runtime GetProcAddress.
// Using unique internal names to avoid conflicts with winver.h declarations.

#define DEF_FORWARD(exportName) \
    static FARPROC g_p##exportName = nullptr; \
    extern "C" __declspec(naked) void __cdecl proxy_##exportName() { \
        __asm { jmp [g_p##exportName] } \
    }

// On x64, we can't use __declspec(naked) or inline asm.
// Use a simple trampoline approach instead.

// Generic forwarding function type
typedef void(*GenericFunc)();

static FARPROC g_pGetFileVersionInfoA = nullptr;
static FARPROC g_pGetFileVersionInfoByHandle = nullptr;
static FARPROC g_pGetFileVersionInfoExA = nullptr;
static FARPROC g_pGetFileVersionInfoExW = nullptr;
static FARPROC g_pGetFileVersionInfoSizeA = nullptr;
static FARPROC g_pGetFileVersionInfoSizeExA = nullptr;
static FARPROC g_pGetFileVersionInfoSizeExW = nullptr;
static FARPROC g_pGetFileVersionInfoSizeW = nullptr;
static FARPROC g_pGetFileVersionInfoW = nullptr;
static FARPROC g_pVerFindFileA = nullptr;
static FARPROC g_pVerFindFileW = nullptr;
static FARPROC g_pVerInstallFileA = nullptr;
static FARPROC g_pVerInstallFileW = nullptr;
static FARPROC g_pVerLanguageNameA = nullptr;
static FARPROC g_pVerLanguageNameW = nullptr;
static FARPROC g_pVerQueryValueA = nullptr;
static FARPROC g_pVerQueryValueW = nullptr;

// Exported wrappers — the .def file maps the real export names to these
extern "C" {
    __declspec(dllexport) BOOL WINAPI fw_GetFileVersionInfoA(LPCSTR a, DWORD b, DWORD c, LPVOID d) {
        using fn_t = BOOL(WINAPI*)(LPCSTR, DWORD, DWORD, LPVOID);
        return reinterpret_cast<fn_t>(g_pGetFileVersionInfoA)(a, b, c, d);
    }
    __declspec(dllexport) int WINAPI fw_GetFileVersionInfoByHandle(int a, HANDLE b, void* c, void* d) {
        using fn_t = int(WINAPI*)(int, HANDLE, void*, void*);
        return reinterpret_cast<fn_t>(g_pGetFileVersionInfoByHandle)(a, b, c, d);
    }
    __declspec(dllexport) BOOL WINAPI fw_GetFileVersionInfoExA(DWORD f, LPCSTR a, DWORD b, DWORD c, LPVOID d) {
        using fn_t = BOOL(WINAPI*)(DWORD, LPCSTR, DWORD, DWORD, LPVOID);
        return reinterpret_cast<fn_t>(g_pGetFileVersionInfoExA)(f, a, b, c, d);
    }
    __declspec(dllexport) BOOL WINAPI fw_GetFileVersionInfoExW(DWORD f, LPCWSTR a, DWORD b, DWORD c, LPVOID d) {
        using fn_t = BOOL(WINAPI*)(DWORD, LPCWSTR, DWORD, DWORD, LPVOID);
        return reinterpret_cast<fn_t>(g_pGetFileVersionInfoExW)(f, a, b, c, d);
    }
    __declspec(dllexport) DWORD WINAPI fw_GetFileVersionInfoSizeA(LPCSTR a, LPDWORD b) {
        using fn_t = DWORD(WINAPI*)(LPCSTR, LPDWORD);
        return reinterpret_cast<fn_t>(g_pGetFileVersionInfoSizeA)(a, b);
    }
    __declspec(dllexport) DWORD WINAPI fw_GetFileVersionInfoSizeExA(DWORD f, LPCSTR a, LPDWORD b) {
        using fn_t = DWORD(WINAPI*)(DWORD, LPCSTR, LPDWORD);
        return reinterpret_cast<fn_t>(g_pGetFileVersionInfoSizeExA)(f, a, b);
    }
    __declspec(dllexport) DWORD WINAPI fw_GetFileVersionInfoSizeExW(DWORD f, LPCWSTR a, LPDWORD b) {
        using fn_t = DWORD(WINAPI*)(DWORD, LPCWSTR, LPDWORD);
        return reinterpret_cast<fn_t>(g_pGetFileVersionInfoSizeExW)(f, a, b);
    }
    __declspec(dllexport) DWORD WINAPI fw_GetFileVersionInfoSizeW(LPCWSTR a, LPDWORD b) {
        using fn_t = DWORD(WINAPI*)(LPCWSTR, LPDWORD);
        return reinterpret_cast<fn_t>(g_pGetFileVersionInfoSizeW)(a, b);
    }
    __declspec(dllexport) BOOL WINAPI fw_GetFileVersionInfoW(LPCWSTR a, DWORD b, DWORD c, LPVOID d) {
        using fn_t = BOOL(WINAPI*)(LPCWSTR, DWORD, DWORD, LPVOID);
        return reinterpret_cast<fn_t>(g_pGetFileVersionInfoW)(a, b, c, d);
    }
    __declspec(dllexport) DWORD WINAPI fw_VerFindFileA(DWORD a, LPCSTR b, LPCSTR c, LPCSTR d, LPSTR e, PUINT f, LPSTR g, PUINT h) {
        using fn_t = DWORD(WINAPI*)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT, LPSTR, PUINT);
        return reinterpret_cast<fn_t>(g_pVerFindFileA)(a, b, c, d, e, f, g, h);
    }
    __declspec(dllexport) DWORD WINAPI fw_VerFindFileW(DWORD a, LPCWSTR b, LPCWSTR c, LPCWSTR d, LPWSTR e, PUINT f, LPWSTR g, PUINT h) {
        using fn_t = DWORD(WINAPI*)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT, LPWSTR, PUINT);
        return reinterpret_cast<fn_t>(g_pVerFindFileW)(a, b, c, d, e, f, g, h);
    }
    __declspec(dllexport) DWORD WINAPI fw_VerInstallFileA(DWORD a, LPCSTR b, LPCSTR c, LPCSTR d, LPCSTR e, LPCSTR f, LPSTR g, PUINT h) {
        using fn_t = DWORD(WINAPI*)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT);
        return reinterpret_cast<fn_t>(g_pVerInstallFileA)(a, b, c, d, e, f, g, h);
    }
    __declspec(dllexport) DWORD WINAPI fw_VerInstallFileW(DWORD a, LPCWSTR b, LPCWSTR c, LPCWSTR d, LPCWSTR e, LPCWSTR f, LPWSTR g, PUINT h) {
        using fn_t = DWORD(WINAPI*)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT);
        return reinterpret_cast<fn_t>(g_pVerInstallFileW)(a, b, c, d, e, f, g, h);
    }
    __declspec(dllexport) DWORD WINAPI fw_VerLanguageNameA(DWORD a, LPSTR b, DWORD c) {
        using fn_t = DWORD(WINAPI*)(DWORD, LPSTR, DWORD);
        return reinterpret_cast<fn_t>(g_pVerLanguageNameA)(a, b, c);
    }
    __declspec(dllexport) DWORD WINAPI fw_VerLanguageNameW(DWORD a, LPWSTR b, DWORD c) {
        using fn_t = DWORD(WINAPI*)(DWORD, LPWSTR, DWORD);
        return reinterpret_cast<fn_t>(g_pVerLanguageNameW)(a, b, c);
    }
    __declspec(dllexport) BOOL WINAPI fw_VerQueryValueA(LPCVOID a, LPCSTR b, LPVOID* c, PUINT d) {
        using fn_t = BOOL(WINAPI*)(LPCVOID, LPCSTR, LPVOID*, PUINT);
        return reinterpret_cast<fn_t>(g_pVerQueryValueA)(a, b, c, d);
    }
    __declspec(dllexport) BOOL WINAPI fw_VerQueryValueW(LPCVOID a, LPCWSTR b, LPVOID* c, PUINT d) {
        using fn_t = BOOL(WINAPI*)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
        return reinterpret_cast<fn_t>(g_pVerQueryValueW)(a, b, c, d);
    }
}

static void ResolveOriginals() {
    #define RESOLVE(name) g_p##name = GetProcAddress(g_originalVersion, #name)
    RESOLVE(GetFileVersionInfoA);
    RESOLVE(GetFileVersionInfoByHandle);
    RESOLVE(GetFileVersionInfoExA);
    RESOLVE(GetFileVersionInfoExW);
    RESOLVE(GetFileVersionInfoSizeA);
    RESOLVE(GetFileVersionInfoSizeExA);
    RESOLVE(GetFileVersionInfoSizeExW);
    RESOLVE(GetFileVersionInfoSizeW);
    RESOLVE(GetFileVersionInfoW);
    RESOLVE(VerFindFileA);
    RESOLVE(VerFindFileW);
    RESOLVE(VerInstallFileA);
    RESOLVE(VerInstallFileW);
    RESOLVE(VerLanguageNameA);
    RESOLVE(VerLanguageNameW);
    RESOLVE(VerQueryValueA);
    RESOLVE(VerQueryValueW);
    #undef RESOLVE
}

// ============================================================
// CPU Spoofing via IAT patching — comprehensive multi-API hooks
// ============================================================

static constexpr DWORD kMinPhysicalCores = 6;

// Helper: try IAT patch on kernel32 first, then api-ms-win-core-* fallback
static bool PatchIAT(HMODULE module, const char* targetDll, const char* funcName, void* newFunc, void** outOriginal) {
    if (!module) module = GetModuleHandleA(nullptr);
    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(module);
    auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<BYTE*>(module) + dosHeader->e_lfanew);
    auto& importDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!importDir.VirtualAddress) return false;

    auto* importDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        reinterpret_cast<BYTE*>(module) + importDir.VirtualAddress);

    for (; importDesc->Name; ++importDesc) {
        auto* dllName = reinterpret_cast<const char*>(
            reinterpret_cast<BYTE*>(module) + importDesc->Name);
        if (_stricmp(dllName, targetDll) != 0) continue;

        auto* origThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
            reinterpret_cast<BYTE*>(module) + importDesc->OriginalFirstThunk);
        auto* iatThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
            reinterpret_cast<BYTE*>(module) + importDesc->FirstThunk);

        for (; origThunk->u1.AddressOfData; ++origThunk, ++iatThunk) {
            if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) continue;
            auto* importByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                reinterpret_cast<BYTE*>(module) + origThunk->u1.AddressOfData);
            if (strcmp(importByName->Name, funcName) != 0) continue;

            *outOriginal = reinterpret_cast<void*>(iatThunk->u1.Function);
            DWORD oldProtect = 0;
            VirtualProtect(&iatThunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect);
            iatThunk->u1.Function = reinterpret_cast<ULONG_PTR>(newFunc);
            VirtualProtect(&iatThunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);

            char buf[256] {};
            std::snprintf(buf, sizeof(buf), "IAT patch applied: %s!%s", targetDll, funcName);
            Log(buf);
            return true;
        }
    }
    return false;
}

// Try kernel32 first, then common api-ms-win-core-* API set DLLs
static bool PatchIATWithFallback(HMODULE exe, const char* funcName, void* newFunc, void** outOriginal) {
    static const char* dllNames[] = {
        "kernel32.dll",
        "api-ms-win-core-sysinfo-l1-1-0.dll",
        "api-ms-win-core-sysinfo-l1-2-0.dll",
        "api-ms-win-core-systemtopology-l1-1-0.dll",
        "api-ms-win-core-processthreads-l1-1-0.dll",
    };
    for (auto* dll : dllNames) {
        if (PatchIAT(exe, dll, funcName, newFunc, outOriginal)) return true;
    }
    return false;
}

// ---- Hook 1: GetSystemInfo ----
using GetSystemInfo_t = void(WINAPI*)(LPSYSTEM_INFO);
static GetSystemInfo_t g_originalGetSystemInfo = nullptr;

static void WINAPI HookedGetSystemInfo(LPSYSTEM_INFO lpSystemInfo) {
    g_originalGetSystemInfo(lpSystemInfo);
    DWORD original = lpSystemInfo->dwNumberOfProcessors;
    if (lpSystemInfo->dwNumberOfProcessors < kMinLogicalProcessors) {
        lpSystemInfo->dwNumberOfProcessors = kMinLogicalProcessors;
        lpSystemInfo->dwActiveProcessorMask = ((DWORD_PTR)1 << kMinLogicalProcessors) - 1;
    }
    char buf[128] {};
    std::snprintf(buf, sizeof(buf), "GetSystemInfo: real=%lu -> spoofed=%lu",
        (unsigned long)original, (unsigned long)lpSystemInfo->dwNumberOfProcessors);
    Log(buf);
}

// ---- Hook 2: GetNativeSystemInfo ----
using GetNativeSystemInfo_t = void(WINAPI*)(LPSYSTEM_INFO);
static GetNativeSystemInfo_t g_originalGetNativeSystemInfo = nullptr;

static void WINAPI HookedGetNativeSystemInfo(LPSYSTEM_INFO lpSystemInfo) {
    g_originalGetNativeSystemInfo(lpSystemInfo);
    DWORD original = lpSystemInfo->dwNumberOfProcessors;
    if (lpSystemInfo->dwNumberOfProcessors < kMinLogicalProcessors) {
        lpSystemInfo->dwNumberOfProcessors = kMinLogicalProcessors;
        lpSystemInfo->dwActiveProcessorMask = ((DWORD_PTR)1 << kMinLogicalProcessors) - 1;
    }
    char buf[128] {};
    std::snprintf(buf, sizeof(buf), "GetNativeSystemInfo: real=%lu -> spoofed=%lu",
        (unsigned long)original, (unsigned long)lpSystemInfo->dwNumberOfProcessors);
    Log(buf);
}

// ---- Hook 3: GetLogicalProcessorInformation ----
using GLPI_t = BOOL(WINAPI*)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);
static GLPI_t g_originalGLPI = nullptr;

static BOOL WINAPI HookedGetLogicalProcessorInformation(
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer, PDWORD returnLength)
{
    // Step 1: get real data to count real cores
    DWORD realSize = 0;
    g_originalGLPI(nullptr, &realSize);

    DWORD entrySize = sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    DWORD realEntryCount = realSize / entrySize;
    auto* realBuf = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, realSize);
    if (!realBuf) return g_originalGLPI(buffer, returnLength);

    DWORD tmpSize = realSize;
    if (!g_originalGLPI(realBuf, &tmpSize)) {
        HeapFree(GetProcessHeap(), 0, realBuf);
        return g_originalGLPI(buffer, returnLength);
    }
    realEntryCount = tmpSize / entrySize;

    // Count real physical cores
    DWORD realCores = 0;
    for (DWORD i = 0; i < realEntryCount; i++) {
        if (realBuf[i].Relationship == RelationProcessorCore) realCores++;
    }

    if (realCores >= kMinPhysicalCores) {
        // No spoofing needed, pass through
        HeapFree(GetProcessHeap(), 0, realBuf);
        return g_originalGLPI(buffer, returnLength);
    }

    // Step 2: build spoofed data — keep real non-core entries, replace cores
    DWORD coresToAdd = kMinPhysicalCores - realCores;
    DWORD newEntryCount = realEntryCount + coresToAdd;
    DWORD newSize = newEntryCount * entrySize;
    ULONG_PTR allMask = ((ULONG_PTR)1 << kMinLogicalProcessors) - 1;

    if (!buffer || *returnLength < newSize) {
        *returnLength = newSize;
        HeapFree(GetProcessHeap(), 0, realBuf);
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    // Copy real entries, updating package/NUMA masks
    DWORD writeIdx = 0;
    for (DWORD i = 0; i < realEntryCount; i++) {
        buffer[writeIdx] = realBuf[i];
        if (buffer[writeIdx].Relationship == RelationProcessorPackage ||
            buffer[writeIdx].Relationship == RelationNumaNode) {
            buffer[writeIdx].ProcessorMask = allMask;
        }
        writeIdx++;
    }

    // Append fake core entries (2 threads each, HT enabled)
    ULONG_PTR nextBit = (ULONG_PTR)1 << (realCores * 2);
    for (DWORD i = 0; i < coresToAdd; i++) {
        auto& e = buffer[writeIdx++];
        memset(&e, 0, entrySize);
        e.Relationship = RelationProcessorCore;
        e.ProcessorMask = nextBit | (nextBit << 1);
        e.ProcessorCore.Flags = 1; // SMT / Hyper-Threading
        nextBit <<= 2;
    }

    *returnLength = newSize;
    HeapFree(GetProcessHeap(), 0, realBuf);

    char buf2[128] {};
    std::snprintf(buf2, sizeof(buf2),
        "GetLogicalProcessorInformation: real=%lu cores -> spoofed=%lu cores",
        (unsigned long)realCores, (unsigned long)kMinPhysicalCores);
    Log(buf2);
    return TRUE;
}

// ---- Hook 4: GetLogicalProcessorInformationEx ----
using GLPIEx_t = BOOL(WINAPI*)(LOGICAL_PROCESSOR_RELATIONSHIP,
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, PDWORD);
static GLPIEx_t g_originalGLPIEx = nullptr;

static BOOL WINAPI HookedGetLogicalProcessorInformationEx(
    LOGICAL_PROCESSOR_RELATIONSHIP relationship,
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX buffer, PDWORD returnLength)
{
    // For relationships we don't care about, pass through
    if (relationship != RelationProcessorCore && relationship != RelationAll) {
        return g_originalGLPIEx(relationship, buffer, returnLength);
    }

    // Get real data
    DWORD realSize = 0;
    g_originalGLPIEx(relationship, nullptr, &realSize);
    if (realSize == 0) return g_originalGLPIEx(relationship, buffer, returnLength);

    BYTE* realBuf = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, realSize);
    if (!realBuf) return g_originalGLPIEx(relationship, buffer, returnLength);

    DWORD tmpSize = realSize;
    if (!g_originalGLPIEx(relationship, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)realBuf, &tmpSize)) {
        HeapFree(GetProcessHeap(), 0, realBuf);
        return g_originalGLPIEx(relationship, buffer, returnLength);
    }

    // Count real physical cores
    DWORD realCores = 0;
    BYTE* ptr = realBuf;
    BYTE* end = realBuf + tmpSize;
    while (ptr < end) {
        auto* info = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)ptr;
        if (info->Relationship == RelationProcessorCore) realCores++;
        ptr += info->Size;
    }

    if (realCores >= kMinPhysicalCores) {
        HeapFree(GetProcessHeap(), 0, realBuf);
        return g_originalGLPIEx(relationship, buffer, returnLength);
    }

    // Build fake core entries to append
    // Each PROCESSOR_RELATIONSHIP entry (with 1 group) is a fixed size
    DWORD coresToAdd = kMinPhysicalCores - realCores;

    // Size of one RelationProcessorCore Ex entry
    DWORD coreEntrySize = sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX);
    DWORD extraSize = coresToAdd * coreEntrySize;
    DWORD newSize = tmpSize + extraSize;

    if (!buffer || *returnLength < newSize) {
        *returnLength = newSize;
        HeapFree(GetProcessHeap(), 0, realBuf);
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    // Copy real data
    memcpy(buffer, realBuf, tmpSize);

    // Update Package entries to cover all processors
    ptr = (BYTE*)buffer;
    end = (BYTE*)buffer + tmpSize;
    while (ptr < end) {
        auto* info = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)ptr;
        if (info->Relationship == RelationProcessorPackage) {
            info->Processor.GroupMask[0].Mask = ((KAFFINITY)1 << kMinLogicalProcessors) - 1;
        }
        if (info->Relationship == RelationNumaNode) {
            info->NumaNode.GroupMask.Mask = ((KAFFINITY)1 << kMinLogicalProcessors) - 1;
        }
        ptr += info->Size;
    }

    // Append fake core entries
    BYTE* writePtr = (BYTE*)buffer + tmpSize;
    KAFFINITY nextBit = (KAFFINITY)1 << (realCores * 2);
    for (DWORD i = 0; i < coresToAdd; i++) {
        auto* entry = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)writePtr;
        memset(entry, 0, coreEntrySize);
        entry->Relationship = RelationProcessorCore;
        entry->Size = coreEntrySize;
        entry->Processor.Flags = LTP_PC_SMT; // Hyper-Threading
        entry->Processor.GroupCount = 1;
        entry->Processor.GroupMask[0].Mask = nextBit | (nextBit << 1);
        entry->Processor.GroupMask[0].Group = 0;
        nextBit <<= 2;
        writePtr += coreEntrySize;
    }

    *returnLength = newSize;
    HeapFree(GetProcessHeap(), 0, realBuf);

    char buf2[128] {};
    std::snprintf(buf2, sizeof(buf2),
        "GetLogicalProcessorInformationEx: real=%lu cores -> spoofed=%lu cores",
        (unsigned long)realCores, (unsigned long)kMinPhysicalCores);
    Log(buf2);
    return TRUE;
}

// ---- Hook 5: GetActiveProcessorCount ----
using GetActiveProcessorCount_t = DWORD(WINAPI*)(WORD);
static GetActiveProcessorCount_t g_originalGetActiveProcessorCount = nullptr;

static DWORD WINAPI HookedGetActiveProcessorCount(WORD groupNumber) {
    DWORD real = g_originalGetActiveProcessorCount(groupNumber);
    DWORD spoofed = real;
    if (real < kMinLogicalProcessors) spoofed = kMinLogicalProcessors;
    char buf[128] {};
    std::snprintf(buf, sizeof(buf), "GetActiveProcessorCount(group=%u): real=%lu -> spoofed=%lu",
        (unsigned)groupNumber, (unsigned long)real, (unsigned long)spoofed);
    Log(buf);
    return spoofed;
}

// ============================================================
// Install all CPU hooks
// ============================================================

// ============================================================
// PEB and environment patching (catches direct memory reads)
// ============================================================

static void PatchPEB() {
    // PEB.NumberOfProcessors is at offset 0xB8 on x64, 0x64 on x86
    // Game engines often read this directly instead of calling APIs
#ifdef _WIN64
    BYTE* peb = (BYTE*)__readgsqword(0x60);
    constexpr size_t kPebNumProcOffset = 0xB8;
#else
    BYTE* peb = (BYTE*)__readfsdword(0x30);
    constexpr size_t kPebNumProcOffset = 0x64;
#endif
    DWORD* pNumProcs = (DWORD*)(peb + kPebNumProcOffset);
    DWORD original = *pNumProcs;
    if (original < kMinLogicalProcessors) {
        DWORD oldProtect = 0;
        VirtualProtect(pNumProcs, sizeof(DWORD), PAGE_READWRITE, &oldProtect);
        *pNumProcs = kMinLogicalProcessors;
        VirtualProtect(pNumProcs, sizeof(DWORD), oldProtect, &oldProtect);
        char buf[128] {};
        std::snprintf(buf, sizeof(buf), "PEB.NumberOfProcessors: %lu -> %lu",
            (unsigned long)original, (unsigned long)kMinLogicalProcessors);
        Log(buf);
    }

    // Also patch PEB.ActiveProcessorAffinityMask (offset 0xC0 on x64, 0x68 on x86)
#ifdef _WIN64
    constexpr size_t kPebAffinityOffset = 0xC0;
#else
    constexpr size_t kPebAffinityOffset = 0x68;
#endif
    ULONG_PTR* pAffinity = (ULONG_PTR*)(peb + kPebAffinityOffset);
    ULONG_PTR newMask = ((ULONG_PTR)1 << kMinLogicalProcessors) - 1;
    if (*pAffinity != newMask) {
        DWORD oldProtect = 0;
        VirtualProtect(pAffinity, sizeof(ULONG_PTR), PAGE_READWRITE, &oldProtect);
        *pAffinity = newMask;
        VirtualProtect(pAffinity, sizeof(ULONG_PTR), oldProtect, &oldProtect);
        Log("PEB.ActiveProcessorAffinityMask: updated to match spoofed count");
    }
}

static void PatchEnvironment() {
    char numStr[16] {};
    std::snprintf(numStr, sizeof(numStr), "%lu", (unsigned long)kMinLogicalProcessors);
    SetEnvironmentVariableA("NUMBER_OF_PROCESSORS", numStr);
    Log("Environment NUMBER_OF_PROCESSORS set to 12");
}

// ============================================================
// Scan ALL loaded modules' IATs (not just main exe)
// ============================================================

static void PatchAllModuleIATs() {
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        (LPCSTR)&PatchAllModuleIATs, &self);

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        Log("CreateToolhelp32Snapshot failed, patching main exe only");
        return;
    }

    struct HookEntry {
        const char* funcName;
        void* hookFunc;
        void** originalPtr;
    };
    HookEntry hooks[] = {
        { "GetSystemInfo", (void*)&HookedGetSystemInfo, (void**)&g_originalGetSystemInfo },
        { "GetNativeSystemInfo", (void*)&HookedGetNativeSystemInfo, (void**)&g_originalGetNativeSystemInfo },
        { "GetLogicalProcessorInformation", (void*)&HookedGetLogicalProcessorInformation, (void**)&g_originalGLPI },
        { "GetLogicalProcessorInformationEx", (void*)&HookedGetLogicalProcessorInformationEx, (void**)&g_originalGLPIEx },
        { "GetActiveProcessorCount", (void*)&HookedGetActiveProcessorCount, (void**)&g_originalGetActiveProcessorCount },
    };

    // Ensure we have original function pointers before patching
    for (auto& h : hooks) {
        if (!*h.originalPtr) {
            *h.originalPtr = (void*)GetProcAddress(GetModuleHandleA("kernel32.dll"), h.funcName);
        }
    }

    static const char* dllNames[] = {
        "kernel32.dll",
        "api-ms-win-core-sysinfo-l1-1-0.dll",
        "api-ms-win-core-sysinfo-l1-2-0.dll",
        "api-ms-win-core-systemtopology-l1-1-0.dll",
        "api-ms-win-core-processthreads-l1-1-0.dll",
    };

    int patchCount = 0;
    MODULEENTRY32 me = { sizeof(me) };
    if (Module32First(snap, &me)) {
        do {
            HMODULE hMod = me.hModule;
            // Skip our own DLL and kernel32
            if (hMod == self) continue;
            if (hMod == GetModuleHandleA("kernel32.dll")) continue;

            for (auto& h : hooks) {
                void* tmpOrig = nullptr;
                for (auto* dll : dllNames) {
                    if (PatchIAT(hMod, dll, h.funcName, h.hookFunc, &tmpOrig)) {
                        // Save the first original we find
                        if (!*h.originalPtr && tmpOrig) *h.originalPtr = tmpOrig;
                        patchCount++;
                        break;
                    }
                }
            }
        } while (Module32Next(snap, &me));
    }
    CloseHandle(snap);

    char buf[128] {};
    std::snprintf(buf, sizeof(buf), "All-module IAT scan: %d additional patches applied", patchCount);
    Log(buf);
}

// ============================================================
// Install all CPU hooks
// ============================================================

static void InstallCpuHooks() {
    // === CRITICAL: Patch PEB FIRST (game may read before any API call) ===
    PatchPEB();
    PatchEnvironment();

    // Set up original function pointers from kernel32 as defaults
    g_originalGetSystemInfo = (GetSystemInfo_t)
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetSystemInfo");
    g_originalGetNativeSystemInfo = (GetNativeSystemInfo_t)
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetNativeSystemInfo");
    g_originalGLPI = (GLPI_t)
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetLogicalProcessorInformation");
    g_originalGLPIEx = (GLPIEx_t)
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetLogicalProcessorInformationEx");
    g_originalGetActiveProcessorCount = (GetActiveProcessorCount_t)
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetActiveProcessorCount");

    // Patch main exe IAT
    HMODULE exe = GetModuleHandleA(nullptr);
    PatchIATWithFallback(exe, "GetSystemInfo",
        (void*)&HookedGetSystemInfo, (void**)&g_originalGetSystemInfo);
    PatchIATWithFallback(exe, "GetNativeSystemInfo",
        (void*)&HookedGetNativeSystemInfo, (void**)&g_originalGetNativeSystemInfo);
    PatchIATWithFallback(exe, "GetLogicalProcessorInformation",
        (void*)&HookedGetLogicalProcessorInformation, (void**)&g_originalGLPI);
    PatchIATWithFallback(exe, "GetLogicalProcessorInformationEx",
        (void*)&HookedGetLogicalProcessorInformationEx, (void**)&g_originalGLPIEx);
    PatchIATWithFallback(exe, "GetActiveProcessorCount",
        (void*)&HookedGetActiveProcessorCount, (void**)&g_originalGetActiveProcessorCount);

    // Scan ALL loaded modules' IATs
    PatchAllModuleIATs();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        if (!LoadOriginalVersion()) return FALSE;
        ResolveOriginals();

        Log("=== version.dll CPU fix proxy loaded (FH101 bypass v3 - PEB patch) ===");

        SYSTEM_INFO si {};
        GetSystemInfo(&si);
        char buf[256] {};
        std::snprintf(buf, sizeof(buf), "Real CPU: %lu logical processors, mask=0x%llX",
            (unsigned long)si.dwNumberOfProcessors,
            (unsigned long long)si.dwActiveProcessorMask);
        Log(buf);

        // Log real physical core count
        DWORD realCoreSize = 0;
        GetLogicalProcessorInformation(nullptr, &realCoreSize);
        if (realCoreSize > 0) {
            auto* coreInfo = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)HeapAlloc(
                GetProcessHeap(), HEAP_ZERO_MEMORY, realCoreSize);
            if (coreInfo && GetLogicalProcessorInformation(coreInfo, &realCoreSize)) {
                DWORD cores = 0;
                DWORD entries = realCoreSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
                for (DWORD i = 0; i < entries; i++) {
                    if (coreInfo[i].Relationship == RelationProcessorCore) cores++;
                }
                std::snprintf(buf, sizeof(buf), "Real CPU: %lu physical cores", (unsigned long)cores);
                Log(buf);
                HeapFree(GetProcessHeap(), 0, coreInfo);
            }
        }

        if (si.dwNumberOfProcessors < kMinLogicalProcessors) {
            std::snprintf(buf, sizeof(buf),
                "Below minimum (%lu < %lu), installing ALL hooks + PEB patch...",
                (unsigned long)si.dwNumberOfProcessors, (unsigned long)kMinLogicalProcessors);
            Log(buf);
            InstallCpuHooks();
        } else {
            Log("CPU meets minimum requirements, no spoofing needed");
        }
    }
    return TRUE;
}
