#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <string>
#include <vector>

static std::mutex g_logMutex;
static HMODULE g_originalD3D12 = nullptr;

using D3D12CreateDevice_t = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
using CheckFeatureSupport_t = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, D3D12_FEATURE, void*, UINT);
using GetDeviceRemovedReason_t = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*);
using CreateGraphicsPipelineState_t = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_GRAPHICS_PIPELINE_STATE_DESC*, REFIID, void**);
using CreateComputePipelineState_t = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_COMPUTE_PIPELINE_STATE_DESC*, REFIID, void**);
using CreatePipelineState_t = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device2*, const D3D12_PIPELINE_STATE_STREAM_DESC*, REFIID, void**);

static D3D12_PIPELINE_STATE_STREAM_DESC MutateStreamCS(
    const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc,
    std::vector<uint8_t>& outBuffer
);

// --- Pipeline Library Hook Types ---
using CreatePipelineLibrary_t = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device1*, const void*, SIZE_T, REFIID, void**);
using LoadGraphicsPipeline_t = HRESULT(STDMETHODCALLTYPE*)(ID3D12PipelineLibrary*, LPCWSTR, const D3D12_GRAPHICS_PIPELINE_STATE_DESC*, REFIID, void**);
using LoadComputePipeline_t = HRESULT(STDMETHODCALLTYPE*)(ID3D12PipelineLibrary*, LPCWSTR, const D3D12_COMPUTE_PIPELINE_STATE_DESC*, REFIID, void**);
using LoadPipeline_t = HRESULT(STDMETHODCALLTYPE*)(ID3D12PipelineLibrary1*, LPCWSTR, const D3D12_PIPELINE_STATE_STREAM_DESC*, REFIID, void**);

static D3D12CreateDevice_t g_originalD3D12CreateDevice = nullptr;
static CheckFeatureSupport_t g_originalCheckFeatureSupport = nullptr;
static GetDeviceRemovedReason_t g_originalGetDeviceRemovedReason = nullptr;
static CreateGraphicsPipelineState_t g_originalCreateGraphicsPipelineState = nullptr;
static CreateComputePipelineState_t g_originalCreateComputePipelineState = nullptr;
static CreatePipelineState_t g_originalCreatePipelineState = nullptr;

// --- Pipeline Library Globals ---
static CreatePipelineLibrary_t g_originalCreatePipelineLibrary = nullptr;
static LoadGraphicsPipeline_t g_originalLoadGraphicsPipeline = nullptr;
static LoadComputePipeline_t g_originalLoadComputePipeline = nullptr;
static LoadPipeline_t g_originalLoadPipeline = nullptr;

// Precompiled Empty Compute Shader (SM 6.0 DXIL)
static const uint8_t kEmptyComputeShader[] = {
    0x44, 0x58, 0x42, 0x43, 0xCE, 0xF5, 0x82, 0x65, 0xF1, 0xED, 0xFF, 0x8A, 0x3D, 0x85, 0xDE, 0xDE,
    0xB3, 0x13, 0xF9, 0xB1, 0x01, 0x00, 0x00, 0x00, 0x04, 0x08, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x3C, 0x00, 0x00, 0x00, 0x4C, 0x00, 0x00, 0x00, 0x5C, 0x00, 0x00, 0x00, 0x6C, 0x00, 0x00, 0x00,
    0xC0, 0x00, 0x00, 0x00, 0x68, 0x04, 0x00, 0x00, 0x84, 0x04, 0x00, 0x00, 0x53, 0x46, 0x49, 0x30,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x53, 0x47, 0x31,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4F, 0x53, 0x47, 0x31,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x50, 0x53, 0x56, 0x30,
    0x4C, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x6D, 0x61, 0x69, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x53, 0x54, 0x41, 0x54, 0xA0, 0x03, 0x00, 0x00, 0x60, 0x00, 0x05, 0x00, 0xE8, 0x00, 0x00, 0x00,
    0x44, 0x58, 0x49, 0x4C, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x88, 0x03, 0x00, 0x00,
    0x42, 0x43, 0xC0, 0xDE, 0x21, 0x0C, 0x00, 0x00, 0xDF, 0x00, 0x00, 0x00, 0x0B, 0x82, 0x20, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x07, 0x81, 0x23, 0x91, 0x41, 0xC8, 0x04, 0x49,
    0x06, 0x10, 0x32, 0x39, 0x92, 0x01, 0x84, 0x0C, 0x25, 0x05, 0x08, 0x19, 0x1E, 0x04, 0x8B, 0x62,
    0x80, 0x0C, 0x45, 0x02, 0x42, 0x92, 0x0B, 0x42, 0x64, 0x10, 0x32, 0x14, 0x38, 0x08, 0x18, 0x4B,
    0x0A, 0x32, 0x32, 0x88, 0x48, 0x90, 0x14, 0x20, 0x43, 0x46, 0x88, 0xA5, 0x00, 0x19, 0x32, 0x42,
    0xE4, 0x48, 0x0E, 0x90, 0x91, 0x21, 0xC4, 0x50, 0x41, 0x51, 0x81, 0x8C, 0xE1, 0x83, 0xE5, 0x8A,
    0x04, 0x19, 0x46, 0x06, 0x89, 0x20, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x32, 0x22, 0xC8, 0x08,
    0x20, 0x64, 0x85, 0x04, 0x93, 0x21, 0xA4, 0x84, 0x04, 0x93, 0x21, 0xE3, 0x84, 0xA1, 0x90, 0x14,
    0x12, 0x4C, 0x86, 0x8C, 0x0B, 0x84, 0x64, 0x4C, 0x10, 0x14, 0x23, 0x00, 0x25, 0x00, 0x65, 0x20,
    0x60, 0x8E, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x13, 0x14, 0x72, 0xC0, 0x87, 0x74, 0x60, 0x87,
    0x36, 0x68, 0x87, 0x79, 0x68, 0x03, 0x72, 0xC0, 0x87, 0x0D, 0xAF, 0x50, 0x0E, 0x6D, 0xD0, 0x0E,
    0x7A, 0x50, 0x0E, 0x6D, 0x00, 0x0F, 0x7A, 0x30, 0x07, 0x72, 0xA0, 0x07, 0x73, 0x20, 0x07, 0x6D,
    0x90, 0x0E, 0x71, 0xA0, 0x07, 0x73, 0x20, 0x07, 0x6D, 0x90, 0x0E, 0x78, 0xA0, 0x07, 0x73, 0x20,
    0x07, 0x6D, 0x90, 0x0E, 0x71, 0x60, 0x07, 0x7A, 0x30, 0x07, 0x72, 0xD0, 0x06, 0xE9, 0x30, 0x07,
    0x72, 0xA0, 0x07, 0x73, 0x20, 0x07, 0x6D, 0x90, 0x0E, 0x76, 0x40, 0x07, 0x7A, 0x60, 0x07, 0x74,
    0xD0, 0x06, 0xE6, 0x10, 0x07, 0x76, 0xA0, 0x07, 0x73, 0x20, 0x07, 0x6D, 0x60, 0x0E, 0x73, 0x20,
    0x07, 0x7A, 0x30, 0x07, 0x72, 0xD0, 0x06, 0xE6, 0x60, 0x07, 0x74, 0xA0, 0x07, 0x76, 0x40, 0x07,
    0x6D, 0xE0, 0x0E, 0x78, 0xA0, 0x07, 0x71, 0x60, 0x07, 0x7A, 0x30, 0x07, 0x72, 0xA0, 0x07, 0x76,
    0x40, 0x07, 0x43, 0x9E, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB2,
    0x40, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x32, 0x1E, 0x98, 0x0C, 0x19, 0x11, 0x4C, 0x90,
    0x8C, 0x09, 0x26, 0x47, 0xC6, 0x04, 0x43, 0x62, 0x09, 0x8C, 0x00, 0x14, 0x43, 0x21, 0x00, 0x00,
    0x79, 0x18, 0x00, 0x00, 0x39, 0x00, 0x00, 0x00, 0x1A, 0x03, 0x4C, 0x90, 0x46, 0x02, 0x13, 0x44,
    0x8F, 0x0C, 0x6F, 0xEC, 0xED, 0x4D, 0x0C, 0x24, 0xC6, 0x05, 0xC7, 0x45, 0xA6, 0x06, 0x46, 0xC6,
    0x25, 0x26, 0x06, 0x04, 0x45, 0x66, 0x26, 0x27, 0x47, 0x26, 0xA6, 0x46, 0x46, 0x26, 0x65, 0x43,
    0x10, 0x4C, 0x10, 0x88, 0x60, 0x82, 0x40, 0x08, 0x1B, 0x84, 0x81, 0xD8, 0x20, 0x10, 0x04, 0x85,
    0xB1, 0xB9, 0x09, 0x02, 0x31, 0x6C, 0x18, 0x0E, 0x84, 0x98, 0x20, 0x08, 0xC0, 0x06, 0x60, 0xC3,
    0x30, 0x2C, 0xCB, 0x86, 0x80, 0xD9, 0x30, 0x0C, 0x4A, 0x43, 0xA2, 0x2D, 0x2C, 0xCD, 0x6D, 0x82,
    0x40, 0x10, 0x1B, 0x86, 0x61, 0x18, 0x36, 0x08, 0x50, 0xB4, 0xA1, 0x50, 0x1E, 0x00, 0x90, 0x58,
    0xA4, 0xB9, 0xCD, 0xD1, 0xCD, 0x6D, 0x10, 0xA8, 0xA1, 0x0A, 0x1B, 0x9B, 0x5D, 0x9B, 0x4B, 0x1A,
    0x59, 0x99, 0x1B, 0xDD, 0x94, 0x20, 0xA8, 0x42, 0x86, 0xE7, 0x62, 0x57, 0x26, 0x37, 0x97, 0xF6,
    0xE6, 0x36, 0x25, 0x20, 0x9A, 0x90, 0xE1, 0xB9, 0xD8, 0x85, 0xB1, 0xD9, 0x95, 0xC9, 0x4D, 0x09,
    0x8A, 0x3A, 0x64, 0x78, 0x2E, 0x73, 0x68, 0x61, 0x64, 0x65, 0x72, 0x4D, 0x6F, 0x64, 0x65, 0x6C,
    0x53, 0x02, 0xA4, 0x12, 0x19, 0x9E, 0x0B, 0x5D, 0x1E, 0x5C, 0x59, 0x90, 0x9B, 0xDB, 0x1B, 0x5D,
    0x18, 0x5D, 0xDA, 0x9B, 0xDB, 0xDC, 0x94, 0xA0, 0xA9, 0x43, 0x86, 0xE7, 0x52, 0xE6, 0x46, 0x27,
    0x97, 0x07, 0xF5, 0x96, 0xE6, 0x46, 0x37, 0x37, 0x25, 0x90, 0xBA, 0x90, 0xE1, 0xB9, 0x8C, 0xBD,
    0xD5, 0xB9, 0xD1, 0x95, 0xC9, 0xCD, 0x4D, 0x09, 0x28, 0x00, 0x00, 0x00, 0x79, 0x18, 0x00, 0x00,
    0x4C, 0x00, 0x00, 0x00, 0x33, 0x08, 0x80, 0x1C, 0xC4, 0xE1, 0x1C, 0x66, 0x14, 0x01, 0x3D, 0x88,
    0x43, 0x38, 0x84, 0xC3, 0x8C, 0x42, 0x80, 0x07, 0x79, 0x78, 0x07, 0x73, 0x98, 0x71, 0x0C, 0xE6,
    0x00, 0x0F, 0xED, 0x10, 0x0E, 0xF4, 0x80, 0x0E, 0x33, 0x0C, 0x42, 0x1E, 0xC2, 0xC1, 0x1D, 0xCE,
    0xA1, 0x1C, 0x66, 0x30, 0x05, 0x3D, 0x88, 0x43, 0x38, 0x84, 0x83, 0x1B, 0xCC, 0x03, 0x3D, 0xC8,
    0x43, 0x3D, 0x8C, 0x03, 0x3D, 0xCC, 0x78, 0x8C, 0x74, 0x70, 0x07, 0x7B, 0x08, 0x07, 0x79, 0x48,
    0x87, 0x70, 0x70, 0x07, 0x7A, 0x70, 0x03, 0x76, 0x78, 0x87, 0x70, 0x20, 0x87, 0x19, 0xCC, 0x11,
    0x0E, 0xEC, 0x90, 0x0E, 0xE1, 0x30, 0x0F, 0x6E, 0x30, 0x0F, 0xE3, 0xF0, 0x0E, 0xF0, 0x50, 0x0E,
    0x33, 0x10, 0xC4, 0x1D, 0xDE, 0x21, 0x1C, 0xD8, 0x21, 0x1D, 0xC2, 0x61, 0x1E, 0x66, 0x30, 0x89,
    0x3B, 0xBC, 0x83, 0x3B, 0xD0, 0x43, 0x39, 0xB4, 0x03, 0x3C, 0xBC, 0x83, 0x3C, 0x84, 0x03, 0x3B,
    0xCC, 0xF0, 0x14, 0x76, 0x60, 0x07, 0x7B, 0x68, 0x07, 0x37, 0x68, 0x87, 0x72, 0x68, 0x07, 0x37,
    0x80, 0x87, 0x70, 0x90, 0x87, 0x70, 0x60, 0x07, 0x76, 0x28, 0x07, 0x76, 0xF8, 0x05, 0x76, 0x78,
    0x87, 0x77, 0x80, 0x87, 0x5F, 0x08, 0x87, 0x71, 0x18, 0x87, 0x72, 0x98, 0x87, 0x79, 0x98, 0x81,
    0x2C, 0xEE, 0xF0, 0x0E, 0xEE, 0xE0, 0x0E, 0xF5, 0xC0, 0x0E, 0xEC, 0x30, 0x03, 0x62, 0xC8, 0xA1,
    0x1C, 0xE4, 0xA1, 0x1C, 0xCC, 0xA1, 0x1C, 0xE4, 0xA1, 0x1C, 0xDC, 0x61, 0x1C, 0xCA, 0x21, 0x1C,
    0xC4, 0x81, 0x1D, 0xCA, 0x61, 0x06, 0xD6, 0x90, 0x43, 0x39, 0xC8, 0x43, 0x39, 0x98, 0x43, 0x39,
    0xC8, 0x43, 0x39, 0xB8, 0xC3, 0x38, 0x94, 0x43, 0x38, 0x88, 0x03, 0x3B, 0x94, 0xC3, 0x2F, 0xBC,
    0x83, 0x3C, 0xFC, 0x82, 0x3B, 0xD4, 0x03, 0x3B, 0xB0, 0xC3, 0x8C, 0xC8, 0x21, 0x07, 0x7C, 0x70,
    0x03, 0x72, 0x10, 0x87, 0x73, 0x70, 0x03, 0x7B, 0x08, 0x07, 0x79, 0x60, 0x87, 0x70, 0xC8, 0x87,
    0x77, 0xA8, 0x07, 0x7A, 0x98, 0x81, 0x3C, 0xE4, 0x80, 0x0F, 0x6E, 0x40, 0x0F, 0xE5, 0xD0, 0x0E,
    0xF0, 0x00, 0x00, 0x00, 0x71, 0x20, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x06, 0x40, 0x30, 0x00,
    0xD2, 0x00, 0x00, 0x00, 0x61, 0x20, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x13, 0x04, 0x01, 0x02,
    0x00, 0x00, 0x00, 0x00
};

static LONG g_featureLogBudget = 500;
static LONG g_deviceIndex = 0;
static LONG g_formatFailLogBudget = 20;
static LONG g_shaderReplacementLogBudget = 100;
static D3D_FEATURE_LEVEL g_actualDeviceFeatureLevel = D3D_FEATURE_LEVEL_11_0;
static bool g_renderDeviceCreated = false;

static constexpr size_t kCheckFeatureSupportVtableIndex = 13;
static constexpr size_t kGetDeviceRemovedReasonVtableIndex = 14;

static void PatchVtableSlot(void** vtable, size_t index, void* newFunc, void** outOriginal, const char* name, LONG devIdx);

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
    log.flush();
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

static void* g_originalBarrier = nullptr;

static D3D12_RESOURCE_STATES MapLayoutToState(D3D12_BARRIER_LAYOUT layout) {
    switch (static_cast<int>(layout)) {
    case D3D12_BARRIER_LAYOUT_UNDEFINED: return D3D12_RESOURCE_STATE_COMMON;
    case D3D12_BARRIER_LAYOUT_COMMON: return D3D12_RESOURCE_STATE_COMMON;
    case D3D12_BARRIER_LAYOUT_GENERIC_READ: return D3D12_RESOURCE_STATE_GENERIC_READ;
    case D3D12_BARRIER_LAYOUT_RENDER_TARGET: return D3D12_RESOURCE_STATE_RENDER_TARGET;
    case D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE: return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    case D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ: return D3D12_RESOURCE_STATE_DEPTH_READ;
    case D3D12_BARRIER_LAYOUT_SHADER_RESOURCE: return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
    case D3D12_BARRIER_LAYOUT_COPY_SOURCE: return D3D12_RESOURCE_STATE_COPY_SOURCE;
    case D3D12_BARRIER_LAYOUT_COPY_DEST: return D3D12_RESOURCE_STATE_COPY_DEST;
    case D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE: return D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
    case D3D12_BARRIER_LAYOUT_RESOLVE_DEST: return D3D12_RESOURCE_STATE_RESOLVE_DEST;
    case D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE: return D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE;
    
    // Direct/Compute Queue Specifics
    case 0x14: return D3D12_RESOURCE_STATE_COMMON; // D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COMMON
    case 0x1e: return D3D12_RESOURCE_STATE_COMMON; // D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COMMON
    case 0x15: return D3D12_RESOURCE_STATE_GENERIC_READ; // D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ
    case 0x1f: return D3D12_RESOURCE_STATE_GENERIC_READ; // D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_GENERIC_READ
    case 0x16: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS; // D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS
    case 0x20: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS; // D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_UNORDERED_ACCESS
    case 0x17: return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE; // D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE
    case 0x21: return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE; // D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_SHADER_RESOURCE
    case 0x18: return D3D12_RESOURCE_STATE_COPY_SOURCE; // D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COPY_SOURCE
    case 0x22: return D3D12_RESOURCE_STATE_COPY_SOURCE; // D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COPY_SOURCE
    case 0x19: return D3D12_RESOURCE_STATE_COPY_DEST; // D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COPY_DEST
    case 0x23: return D3D12_RESOURCE_STATE_COPY_DEST; // D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COPY_DEST

    default:
        return D3D12_RESOURCE_STATE_COMMON;
    }
}

static void TranslateBarrier(
    ID3D12GraphicsCommandList* cmdList,
    UINT32 NumBarrierGroups,
    const D3D12_BARRIER_GROUP* pBarrierGroups
) {
    if (NumBarrierGroups == 0 || !pBarrierGroups) {
        return;
    }

    UINT32 totalBarriers = 0;
    for (UINT32 i = 0; i < NumBarrierGroups; ++i) {
        totalBarriers += pBarrierGroups[i].NumBarriers;
    }

    if (totalBarriers == 0) {
        return;
    }

    D3D12_RESOURCE_BARRIER* legacyBarriers = nullptr;
    D3D12_RESOURCE_BARRIER stackBuffer[32];
    if (totalBarriers <= 32) {
        legacyBarriers = stackBuffer;
    } else {
        legacyBarriers = new D3D12_RESOURCE_BARRIER[totalBarriers];
    }

    UINT32 dstIdx = 0;
    for (UINT32 i = 0; i < NumBarrierGroups; ++i) {
        const D3D12_BARRIER_GROUP& group = pBarrierGroups[i];
        if (group.NumBarriers == 0) continue;

        if (group.Type == D3D12_BARRIER_TYPE_GLOBAL) {
            for (UINT32 j = 0; j < group.NumBarriers; ++j) {
                D3D12_RESOURCE_BARRIER& legacy = legacyBarriers[dstIdx++];
                legacy.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                legacy.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                legacy.UAV.pResource = nullptr;
            }
        }
        else if (group.Type == D3D12_BARRIER_TYPE_TEXTURE) {
            for (UINT32 j = 0; j < group.NumBarriers; ++j) {
                const D3D12_TEXTURE_BARRIER& tex = group.pTextureBarriers[j];
                D3D12_RESOURCE_BARRIER& legacy = legacyBarriers[dstIdx++];
                
                D3D12_BARRIER_LAYOUT layoutBefore = tex.LayoutBefore;
                if (layoutBefore == D3D12_BARRIER_LAYOUT_UNDEFINED) {
                    layoutBefore = D3D12_BARRIER_LAYOUT_COMMON;
                }

                legacy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                legacy.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                legacy.Transition.pResource = tex.pResource;
                
                if (tex.Subresources.NumMipLevels == 0 || tex.Subresources.IndexOrFirstMipLevel == 0xffffffff) {
                    legacy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                } else {
                    legacy.Transition.Subresource = tex.Subresources.IndexOrFirstMipLevel;
                }

                legacy.Transition.StateBefore = MapLayoutToState(layoutBefore);
                legacy.Transition.StateAfter = MapLayoutToState(tex.LayoutAfter);
            }
        }
        else if (group.Type == D3D12_BARRIER_TYPE_BUFFER) {
            for (UINT32 j = 0; j < group.NumBarriers; ++j) {
                const D3D12_BUFFER_BARRIER& buf = group.pBufferBarriers[j];
                D3D12_RESOURCE_BARRIER& legacy = legacyBarriers[dstIdx++];
                
                legacy.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                legacy.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                legacy.UAV.pResource = buf.pResource;
            }
        }
    }

    if (dstIdx > 0) {
        cmdList->ResourceBarrier(dstIdx, legacyBarriers);
    }

    if (legacyBarriers != stackBuffer) {
        delete[] legacyBarriers;
    }
}

static void STDMETHODCALLTYPE HookedBarrier(
    ID3D12GraphicsCommandList7* self,
    UINT32 NumBarrierGroups,
    const D3D12_BARRIER_GROUP* pBarrierGroups
) {
    static bool loggedBarrier = false;
    if (!loggedBarrier) {
        loggedBarrier = true;
        Log("HookedBarrier: First Enhanced Barrier intercepted and translated to legacy ResourceBarrier!");
    }

    TranslateBarrier(self, NumBarrierGroups, pBarrierGroups);
}

// Returns the shader minor version (e.g. 6 for SM 6.6, 2 for SM 6.2).
// Returns 0 if the bytecode is invalid or not DXIL.
static uint8_t GetShaderMinorVersion(const D3D12_SHADER_BYTECODE& bytecode) {
    if (!bytecode.pShaderBytecode || bytecode.BytecodeLength < 20) {
        return 0;
    }
    const uint8_t* data = reinterpret_cast<const uint8_t*>(bytecode.pShaderBytecode);
    uint32_t magic = *reinterpret_cast<const uint32_t*>(data);
    if (magic != 0x43425844) { // 'DXBC'
        return 0;
    }
    uint32_t numChunks = *reinterpret_cast<const uint32_t*>(data + 28);
    const uint32_t* offsets = reinterpret_cast<const uint32_t*>(data + 32);
    for (uint32_t i = 0; i < numChunks; ++i) {
        if (i >= 16) break; // safety cap
        uint32_t offset = offsets[i];
        if (offset + 8 > bytecode.BytecodeLength) break;
        uint32_t chunkMagic = *reinterpret_cast<const uint32_t*>(data + offset);
        if (chunkMagic == 0x4C495844) { // 'DXIL'
            if (offset + 8 + 8 <= bytecode.BytecodeLength) {
                const uint8_t* dxilData = data + offset + 8;
                uint16_t programVersion = *reinterpret_cast<const uint16_t*>(dxilData);
                uint8_t minor = programVersion & 0xF;
                return minor;
            }
        }
    }
    return 0;
}

// Simpler detection: checks if a compute shader bytecode contains SM >= 6.5 DXIL
// by scanning for the DXIL chunk and reading the program version header.
// This is the same logic as LogShaderInfo but returns true/false.
static bool IsHighSMShader(const D3D12_SHADER_BYTECODE& bytecode) {
    uint8_t minor = GetShaderMinorVersion(bytecode);
    if (minor >= 3) return true;
    
    // Fallback: if bytecode is larger than typical SM 6.2 shaders and 
    // GetShaderMinorVersion failed, scan for STAT chunk version indicator
    // The STAT chunk at the start has a version field: 0x60 0x00 0x05 0x00 = SM 6.0
    // Check for SM 6.5+ marker in STAT chunk
    if (!bytecode.pShaderBytecode || bytecode.BytecodeLength < 64) return false;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(bytecode.pShaderBytecode);
    uint32_t magic = *reinterpret_cast<const uint32_t*>(data);
    if (magic != 0x43425844) return false;
    
    uint32_t numChunks = *reinterpret_cast<const uint32_t*>(data + 28);
    const uint32_t* offsets = reinterpret_cast<const uint32_t*>(data + 32);
    for (uint32_t i = 0; i < numChunks && i < 16; ++i) {
        uint32_t offset = offsets[i];
        if (offset + 12 > bytecode.BytecodeLength) break;
        uint32_t chunkMagic = *reinterpret_cast<const uint32_t*>(data + offset);
        // 'STAT' chunk = 0x54415453
        if (chunkMagic == 0x54415453) {
            if (offset + 12 <= bytecode.BytecodeLength) {
                // STAT header has version at +8: minor_hi:4 | major:4 | type:8 | ...
                const uint8_t* statData = data + offset + 8;
                uint16_t statVersion = *reinterpret_cast<const uint16_t*>(statData);
                uint8_t statMinor = statVersion & 0xF;
                uint8_t statMajor = (statVersion >> 4) & 0xF;
                if (statMajor == 6 && statMinor >= 3) return true;
            }
        }
    }
    return false;
}


static void LogShaderInfo(const char* pipelineType, const D3D12_SHADER_BYTECODE& bytecode) {
    if (!bytecode.pShaderBytecode || bytecode.BytecodeLength < 20) {
        return;
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(bytecode.pShaderBytecode);
    uint32_t magic = *reinterpret_cast<const uint32_t*>(data);
    if (magic != 0x43425844) { // 'DXBC'
        return;
    }

    uint32_t numChunks = *reinterpret_cast<const uint32_t*>(data + 28);
    const uint32_t* offsets = reinterpret_cast<const uint32_t*>(data + 32);

    for (uint32_t i = 0; i < numChunks; ++i) {
        uint32_t offset = offsets[i];
        if (offset + 8 > bytecode.BytecodeLength) break;

        uint32_t chunkMagic = *reinterpret_cast<const uint32_t*>(data + offset);

        if (chunkMagic == 0x4C495844) { // 'DXIL'
            if (offset + 8 + 8 <= bytecode.BytecodeLength) {
                const uint8_t* dxilData = data + offset + 8;
                uint16_t programVersion = *reinterpret_cast<const uint16_t*>(dxilData);
                uint8_t typeVal = (programVersion >> 8) & 0xFF;
                uint8_t major = (programVersion >> 4) & 0xF;
                uint8_t minor = programVersion & 0xF;
                
                const char* typeStr = "unknown";
                if (typeVal == 0) typeStr = "pixel";
                else if (typeVal == 1) typeStr = "vertex";
                else if (typeVal == 2) typeStr = "geometry";
                else if (typeVal == 3) typeStr = "hull";
                else if (typeVal == 4) typeStr = "domain";
                else if (typeVal == 5) typeStr = "compute";
                else if (typeVal == 6) typeStr = "library";
                
                char buf[256] {};
                std::snprintf(buf, sizeof(buf), "  [Shader] %s target: %s_%u_%u (size: %zu)",
                    pipelineType, typeStr, major, minor, bytecode.BytecodeLength);
                Log(buf);
            }
        }
    }
}

static size_t GetSubobjectSize(uint32_t type) {
    switch (type) {
    case 0: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE
        return 8 + sizeof(ID3D12RootSignature*);
    case 1: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS
    case 2: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS
    case 3: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS
    case 4: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS
    case 5: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS
    case 20: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS
    case 24: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS
    case 25: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS
        return 8 + sizeof(D3D12_SHADER_BYTECODE);
    case 6: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT
        return 8 + sizeof(D3D12_STREAM_OUTPUT_DESC);
    case 7: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND
        return 8 + sizeof(D3D12_BLEND_DESC);
    case 8: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK
        return 8 + sizeof(UINT);
    case 9: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER
        return 8 + sizeof(D3D12_RASTERIZER_DESC);
    case 10: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL
        return 8 + sizeof(D3D12_DEPTH_STENCIL_DESC);
    case 11: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT
        return 8 + sizeof(D3D12_INPUT_LAYOUT_DESC);
    case 12: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE
        return 8 + sizeof(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE);
    case 13: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY
        return 8 + sizeof(D3D12_PRIMITIVE_TOPOLOGY_TYPE);
    case 14: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS
        return 8 + sizeof(D3D12_RT_FORMAT_ARRAY);
    case 15: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT
        return 8 + sizeof(DXGI_FORMAT);
    case 16: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC
        return 8 + sizeof(DXGI_SAMPLE_DESC);
    case 17: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK
        return 8 + sizeof(UINT);
    case 18: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO
        return 8 + sizeof(D3D12_CACHED_PIPELINE_STATE);
    case 19: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS
        return 8 + sizeof(D3D12_PIPELINE_STATE_FLAGS);
    case 21: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING
        return 8 + sizeof(D3D12_VIEW_INSTANCING_DESC);
    case 22: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1
        return 8 + 40; // sizeof(D3D12_DEPTH_STENCIL_DESC1)
    case 23: // D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL2
        return 8 + 48; // D3D12_DEPTH_STENCIL_DESC2
    default:
        return 0;
    }
}

static void ParseStreamAndLogShaders(const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc) {
    if (!pDesc || !pDesc->pPipelineStateSubobjectStream || pDesc->SizeInBytes == 0) {
        return;
    }

    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(pDesc->pPipelineStateSubobjectStream);
    const uint8_t* end = ptr + pDesc->SizeInBytes;

    while (ptr < end) {
        uint32_t type = *reinterpret_cast<const uint32_t*>(ptr);
        
        if (type == 1) { // VS
            const auto* bytecode = reinterpret_cast<const D3D12_SHADER_BYTECODE*>(ptr + 8);
            LogShaderInfo("Stream VS", *bytecode);
        }
        else if (type == 2) { // PS
            const auto* bytecode = reinterpret_cast<const D3D12_SHADER_BYTECODE*>(ptr + 8);
            LogShaderInfo("Stream PS", *bytecode);
        }
        else if (type == 20) { // CS
            const auto* bytecode = reinterpret_cast<const D3D12_SHADER_BYTECODE*>(ptr + 8);
            LogShaderInfo("Stream CS", *bytecode);
        }
        else if (type == 24) { // AS
            const auto* bytecode = reinterpret_cast<const D3D12_SHADER_BYTECODE*>(ptr + 8);
            LogShaderInfo("Stream AS", *bytecode);
        }
        else if (type == 25) { // MS
            const auto* bytecode = reinterpret_cast<const D3D12_SHADER_BYTECODE*>(ptr + 8);
            LogShaderInfo("Stream MS", *bytecode);
        }

        size_t size = GetSubobjectSize(type);
        if (size == 0) {
            char buf[128] {};
            std::snprintf(buf, sizeof(buf), "ParseStream: Unknown subobject type %u, aborting parse.", type);
            Log(buf);
            return;
        }
        ptr += size;
    }
}

// Empty Vertex Shader (SM 6.0 DXIL) - outputs SV_Position = float4(0,0,0,1)
static const uint8_t kEmptyVertexShader[] = {
    0x44, 0x58, 0x42, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x08, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x3C, 0x00, 0x00, 0x00, 0x4C, 0x00, 0x00, 0x00, 0x5C, 0x00, 0x00, 0x00, 0x6C, 0x00, 0x00, 0x00,
    0xC0, 0x00, 0x00, 0x00, 0x68, 0x04, 0x00, 0x00, 0x84, 0x04, 0x00, 0x00, 0x53, 0x46, 0x49, 0x30,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x53, 0x47, 0x31,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4F, 0x53, 0x47, 0x31,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x50, 0x53, 0x56, 0x30,
    0x4C, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x6D, 0x61, 0x69, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Empty Pixel Shader (SM 6.0 DXIL) - outputs float4(0,0,0,0)
static const uint8_t kEmptyPixelShader[] = {
    0x44, 0x58, 0x42, 0x43, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x08, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x3C, 0x00, 0x00, 0x00, 0x4C, 0x00, 0x00, 0x00, 0x5C, 0x00, 0x00, 0x00, 0x6C, 0x00, 0x00, 0x00,
    0xC0, 0x00, 0x00, 0x00, 0x68, 0x04, 0x00, 0x00, 0x84, 0x04, 0x00, 0x00, 0x53, 0x46, 0x49, 0x30,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x53, 0x47, 0x31,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4F, 0x53, 0x47, 0x31,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x50, 0x53, 0x56, 0x30,
    0x4C, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x6D, 0x61, 0x69, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static HRESULT STDMETHODCALLTYPE HookedCreateGraphicsPipelineState(
    ID3D12Device* self,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc,
    REFIID riid,
    void** ppPipelineState
) {
    if (pDesc) {
        LogShaderInfo("Graphics VS", pDesc->VS);
        LogShaderInfo("Graphics PS", pDesc->PS);
    }

    // Check for high-SM shaders in VS/PS and replace them
    if (pDesc && ppPipelineState) {
        bool vsHigh = IsHighSMShader(pDesc->VS);
        bool psHigh = IsHighSMShader(pDesc->PS);
        if (vsHigh || psHigh) {
            if (InterlockedDecrement(&g_shaderReplacementLogBudget) >= 0) {
                char buf[256] {};
                uint8_t vsMinor = GetShaderMinorVersion(pDesc->VS);
                uint8_t psMinor = GetShaderMinorVersion(pDesc->PS);
                std::snprintf(buf, sizeof(buf),
                    "CreateGraphicsPipelineState: High SM detected (VS=6.%u, PS=6.%u) — replacing with empty fallback",
                    vsMinor, psMinor);
                Log(buf);
            }
            D3D12_GRAPHICS_PIPELINE_STATE_DESC fallbackDesc = *pDesc;
            if (vsHigh) {
                fallbackDesc.VS.pShaderBytecode = kEmptyVertexShader;
                fallbackDesc.VS.BytecodeLength = sizeof(kEmptyVertexShader);
            }
            if (psHigh) {
                fallbackDesc.PS.pShaderBytecode = kEmptyPixelShader;
                fallbackDesc.PS.BytecodeLength = sizeof(kEmptyPixelShader);
            }
            HRESULT hrFallback = g_originalCreateGraphicsPipelineState(self, &fallbackDesc, riid, ppPipelineState);
            if (SUCCEEDED(hrFallback)) {
                return S_OK;
            }
            // If fallback fails, try with original shaders below
        }
    }
    
    HRESULT hr = g_originalCreateGraphicsPipelineState(self, pDesc, riid, ppPipelineState);
    if (FAILED(hr)) {
        char buf[256] {};
        std::snprintf(buf, sizeof(buf), "*** CreateGraphicsPipelineState FAILED: HRESULT 0x%08lX ***", static_cast<unsigned long>(hr));
        Log(buf);
    }
    return hr;
}

static HRESULT STDMETHODCALLTYPE HookedCreateComputePipelineState(
    ID3D12Device* self,
    const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc,
    REFIID riid,
    void** ppPipelineState
) {
    if (pDesc) {
        LogShaderInfo("Compute CS", pDesc->CS);
    }
    
    // Proactively replace SM >= 6.3 CS shaders BEFORE sending to driver.
    // RX 580 (Polaris/GCN) only supports SM 6.0-6.2. Anything higher will
    // pass CPU-side D3D12 validation but crash the GPU at execution time.
    if (pDesc && ppPipelineState) {
        uint8_t csMinor = GetShaderMinorVersion(pDesc->CS);
        bool isHigh = IsHighSMShader(pDesc->CS);
        if (isHigh) {
            if (InterlockedDecrement(&g_shaderReplacementLogBudget) >= 0) {
                char buf[256] {};
                std::snprintf(buf, sizeof(buf),
                    "CreateComputePipelineState: Replacing SM 6.%u CS with empty SM 6.0 fallback (size: %zu)",
                    csMinor, pDesc->CS.BytecodeLength);
                Log(buf);
            }
            D3D12_COMPUTE_PIPELINE_STATE_DESC fallbackDesc = *pDesc;
            fallbackDesc.CS.pShaderBytecode = kEmptyComputeShader;
            fallbackDesc.CS.BytecodeLength = sizeof(kEmptyComputeShader);
            
            HRESULT hrFallback = g_originalCreateComputePipelineState(self, &fallbackDesc, riid, ppPipelineState);
            if (SUCCEEDED(hrFallback)) {
                return S_OK;
            }
        }
    }
    
    HRESULT hr = g_originalCreateComputePipelineState(self, pDesc, riid, ppPipelineState);
    if (FAILED(hr)) {
        char buf[256] {};
        std::snprintf(buf, sizeof(buf), "*** CreateComputePipelineState FAILED: HRESULT 0x%08lX ***", static_cast<unsigned long>(hr));
        Log(buf);
        
        if (pDesc && ppPipelineState) {
            uint8_t failMinor = GetShaderMinorVersion(pDesc->CS);
            char recBuf[256] {};
            std::snprintf(recBuf, sizeof(recBuf),
                "CreateComputePipelineState: Attempting recovery (SM 6.%u, size: %zu) using empty SM 6.0 CS fallback...",
                failMinor, pDesc->CS.BytecodeLength);
            Log(recBuf);
            D3D12_COMPUTE_PIPELINE_STATE_DESC fallbackDesc = *pDesc;
            fallbackDesc.CS.pShaderBytecode = kEmptyComputeShader;
            fallbackDesc.CS.BytecodeLength = sizeof(kEmptyComputeShader);
            
            HRESULT hrFallback = g_originalCreateComputePipelineState(self, &fallbackDesc, riid, ppPipelineState);
            if (SUCCEEDED(hrFallback)) {
                Log("CreateComputePipelineState: Recovered successfully with empty CS fallback! S_OK returned.");
                return S_OK;
            } else {
                std::snprintf(buf, sizeof(buf), "CreateComputePipelineState: Fallback ALSO failed! HRESULT 0x%08lX", static_cast<unsigned long>(hrFallback));
                Log(buf);
            }
        }
    }
    return hr;
}

// Helper: check if any CS in the stream has SM >= 6.5
static bool StreamHasHighSMCS(const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc) {
    if (!pDesc || !pDesc->pPipelineStateSubobjectStream || pDesc->SizeInBytes == 0) {
        return false;
    }
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(pDesc->pPipelineStateSubobjectStream);
    const uint8_t* end = ptr + pDesc->SizeInBytes;
    while (ptr < end) {
        uint32_t type = *reinterpret_cast<const uint32_t*>(ptr);
        if (type == 20) { // CS
            const auto* bytecode = reinterpret_cast<const D3D12_SHADER_BYTECODE*>(ptr + 8);
            uint8_t minor = GetShaderMinorVersion(*bytecode);
            if (minor >= 3) return true;
        }
        
        size_t size = GetSubobjectSize(type);
        if (size == 0) return false;
        ptr += size;
    }
    return false;
}

static HRESULT STDMETHODCALLTYPE HookedCreatePipelineState(
    ID3D12Device2* self,
    const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc,
    REFIID riid,
    void** ppPipelineState
) {
    ParseStreamAndLogShaders(pDesc);
    
    // Proactively replace SM >= 6.3 CS shaders in stream BEFORE sending to driver.
    if (pDesc && ppPipelineState && StreamHasHighSMCS(pDesc)) {
        if (InterlockedDecrement(&g_shaderReplacementLogBudget) >= 0) {
            Log("CreatePipelineState (Stream): Replacing SM 6.3+ CS with empty SM 6.0 fallback");
        }
        std::vector<uint8_t> mutatedBuffer;
        D3D12_PIPELINE_STATE_STREAM_DESC mutatedDesc = MutateStreamCS(pDesc, mutatedBuffer);
        
        HRESULT hrFallback = g_originalCreatePipelineState(self, &mutatedDesc, riid, ppPipelineState);
        if (SUCCEEDED(hrFallback)) {
            return S_OK;
        }
        // Fall through to original attempt if fallback fails
    }
    
    HRESULT hr = g_originalCreatePipelineState(self, pDesc, riid, ppPipelineState);
    if (FAILED(hr)) {
        char buf[256] {};
        std::snprintf(buf, sizeof(buf), "*** CreatePipelineState (Stream) FAILED: HRESULT 0x%08lX ***", static_cast<unsigned long>(hr));
        Log(buf);
        
        if (pDesc && ppPipelineState) {
            Log("CreatePipelineState (Stream): Attempting recovery using empty SM 6.0 CS fallback...");
            std::vector<uint8_t> mutatedBuffer;
            D3D12_PIPELINE_STATE_STREAM_DESC mutatedDesc = MutateStreamCS(pDesc, mutatedBuffer);
            
            HRESULT hrFallback = g_originalCreatePipelineState(self, &mutatedDesc, riid, ppPipelineState);
            if (SUCCEEDED(hrFallback)) {
                Log("CreatePipelineState (Stream): Recovered successfully with empty CS fallback! S_OK returned.");
                return S_OK;
            } else {
                std::snprintf(buf, sizeof(buf), "CreatePipelineState (Stream): Fallback ALSO failed! HRESULT 0x%08lX", static_cast<unsigned long>(hrFallback));
                Log(buf);
            }
        }
    }
    return hr;
}

static HRESULT STDMETHODCALLTYPE HookedLoadGraphicsPipeline(
    ID3D12PipelineLibrary* self,
    LPCWSTR pName,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc,
    REFIID riid,
    void** ppPipelineState
) {
    if (pName) {
        char nameBuf[256] {};
        std::snprintf(nameBuf, sizeof(nameBuf), "LoadGraphicsPipeline: pName='%ls'", pName);
        Log(nameBuf);
    }
    return g_originalLoadGraphicsPipeline(self, pName, pDesc, riid, ppPipelineState);
}

static HRESULT STDMETHODCALLTYPE HookedLoadComputePipeline(
    ID3D12PipelineLibrary* self,
    LPCWSTR pName,
    const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc,
    REFIID riid,
    void** ppPipelineState
) {
    if (pName) {
        char nameBuf[256] {};
        std::snprintf(nameBuf, sizeof(nameBuf), "LoadComputePipeline: pName='%ls'", pName);
        Log(nameBuf);
    }

    HRESULT hr = g_originalLoadComputePipeline(self, pName, pDesc, riid, ppPipelineState);
    
    bool intercept = FAILED(hr);
    if (pName && std::wcsstr(pName, L"GenerateIndirectCommand")) {
        intercept = true;
        Log("LoadComputePipeline: Intercepting 'GenerateIndirectCommand' specifically!");
    }

    if (intercept && pDesc) {
        Log("LoadComputePipeline: Attempting fallback to dummy SM 6.0 CS!");
        ID3D12Device* device = nullptr;
        HRESULT hrDevice = self->GetDevice(IID_PPV_ARGS(&device));
        if (SUCCEEDED(hrDevice) && device) {
            D3D12_COMPUTE_PIPELINE_STATE_DESC dummyDesc = *pDesc;
            dummyDesc.CS.pShaderBytecode = kEmptyComputeShader;
            dummyDesc.CS.BytecodeLength = sizeof(kEmptyComputeShader);
            
            HRESULT hrDummy = device->CreateComputePipelineState(&dummyDesc, riid, ppPipelineState);
            device->Release();
            if (SUCCEEDED(hrDummy)) {
                Log("LoadComputePipeline: Dummy SM 6.0 CS pipeline created successfully! S_OK returned to engine.");
                return S_OK;
            } else {
                char errBuf[256] {};
                std::snprintf(errBuf, sizeof(errBuf), "LoadComputePipeline: Failed to create dummy fallback pipeline! HRESULT 0x%08lX", hrDummy);
                Log(errBuf);
            }
        } else {
            Log("LoadComputePipeline: Failed to retrieve ID3D12Device from library!");
        }
    }

    return hr;
}

static D3D12_PIPELINE_STATE_STREAM_DESC MutateStreamCS(
    const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc,
    std::vector<uint8_t>& outBuffer
) {
    outBuffer.clear();
    if (!pDesc || !pDesc->pPipelineStateSubobjectStream || pDesc->SizeInBytes == 0) {
        return *pDesc;
    }

    outBuffer.resize(pDesc->SizeInBytes);
    std::memcpy(outBuffer.data(), pDesc->pPipelineStateSubobjectStream, pDesc->SizeInBytes);

    uint8_t* ptr = outBuffer.data();
    uint8_t* end = ptr + pDesc->SizeInBytes;

    while (ptr < end) {
        uint32_t type = *reinterpret_cast<uint32_t*>(ptr);
        
        if (type == 20) { // CS (Compute Shader)
            auto* bytecode = reinterpret_cast<D3D12_SHADER_BYTECODE*>(ptr + 8);
            if (IsHighSMShader(*bytecode)) {
                bytecode->pShaderBytecode = kEmptyComputeShader;
                bytecode->BytecodeLength = sizeof(kEmptyComputeShader);
            }
        }
        
        size_t size = GetSubobjectSize(type);
        if (size == 0) {
            D3D12_PIPELINE_STATE_STREAM_DESC mutatedDesc = *pDesc;
            mutatedDesc.pPipelineStateSubobjectStream = outBuffer.data();
            return mutatedDesc;
        }
        ptr += size;
    }

    D3D12_PIPELINE_STATE_STREAM_DESC mutatedDesc = *pDesc;
    mutatedDesc.pPipelineStateSubobjectStream = outBuffer.data();
    return mutatedDesc;
}

static HRESULT STDMETHODCALLTYPE HookedLoadPipeline(
    ID3D12PipelineLibrary1* self,
    LPCWSTR pName,
    const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc,
    REFIID riid,
    void** ppPipelineState
) {
    if (pName) {
        char nameBuf[256] {};
        std::snprintf(nameBuf, sizeof(nameBuf), "LoadPipeline: pName='%ls'", pName);
        Log(nameBuf);
    }

    HRESULT hr = g_originalLoadPipeline(self, pName, pDesc, riid, ppPipelineState);

    bool intercept = FAILED(hr);
    if (pName && std::wcsstr(pName, L"GenerateIndirectCommand")) {
        intercept = true;
        Log("LoadPipeline: Intercepting 'GenerateIndirectCommand' specifically!");
    }

    if (intercept && pDesc) {
        Log("LoadPipeline: Attempting fallback to dummy SM 6.0 CS!");
        ID3D12Device* device = nullptr;
        HRESULT hrDevice = self->GetDevice(IID_PPV_ARGS(&device));
        if (SUCCEEDED(hrDevice) && device) {
            ID3D12Device2* device2 = nullptr;
            HRESULT hrDevice2 = device->QueryInterface(IID_PPV_ARGS(&device2));
            if (SUCCEEDED(hrDevice2) && device2) {
                std::vector<uint8_t> mutatedBuffer;
                D3D12_PIPELINE_STATE_STREAM_DESC dummyDesc = MutateStreamCS(pDesc, mutatedBuffer);
                
                HRESULT hrDummy = device2->CreatePipelineState(&dummyDesc, riid, ppPipelineState);
                device2->Release();
                device->Release();
                if (SUCCEEDED(hrDummy)) {
                    Log("LoadPipeline: Dummy SM 6.0 CS pipeline created successfully! S_OK returned to engine.");
                    return S_OK;
                } else {
                    char errBuf[256] {};
                    std::snprintf(errBuf, sizeof(errBuf), "LoadPipeline: Failed to create dummy fallback pipeline! HRESULT 0x%08lX", hrDummy);
                    Log(errBuf);
                }
            } else {
                device->Release();
                Log("LoadPipeline: Failed to QueryInterface ID3D12Device2 from device!");
            }
        } else {
            Log("LoadPipeline: Failed to retrieve ID3D12Device from library!");
        }
    }

    return hr;
}

static HRESULT STDMETHODCALLTYPE HookedCreatePipelineLibrary(
    ID3D12Device1* self,
    const void* pLibraryBlob,
    SIZE_T BlobLength,
    REFIID riid,
    void** ppPipelineLibrary
) {
    Log("CreatePipelineLibrary intercepted!");
    HRESULT hr = g_originalCreatePipelineLibrary(self, pLibraryBlob, BlobLength, riid, ppPipelineLibrary);
    if (SUCCEEDED(hr) && ppPipelineLibrary && *ppPipelineLibrary) {
        void*** object = reinterpret_cast<void***>(*ppPipelineLibrary);
        void** vtable = *object;
        
        PatchVtableSlot(vtable, 9,
            reinterpret_cast<void*>(&HookedLoadGraphicsPipeline),
            reinterpret_cast<void**>(&g_originalLoadGraphicsPipeline),
            "LoadGraphicsPipeline (ID3D12PipelineLibrary)", 0);
            
        PatchVtableSlot(vtable, 10,
            reinterpret_cast<void*>(&HookedLoadComputePipeline),
            reinterpret_cast<void**>(&g_originalLoadComputePipeline),
            "LoadComputePipeline (ID3D12PipelineLibrary)", 0);

        PatchVtableSlot(vtable, 13,
            reinterpret_cast<void*>(&HookedLoadPipeline),
            reinterpret_cast<void**>(&g_originalLoadPipeline),
            "LoadPipeline (ID3D12PipelineLibrary1)", 0);
    }
    return hr;
}

static HRESULT STDMETHODCALLTYPE HookedCheckFeatureSupport(
    ID3D12Device* self,
    D3D12_FEATURE feature,
    void* pFeatureSupportData,
    UINT featureSupportDataSize
) {
    CheckFeatureSupport_t original = g_originalCheckFeatureSupport;
    if (!original) {
        return E_FAIL;
    }

    HRESULT hr = original(self, feature, pFeatureSupportData, featureSupportDataSize);

    // ---- Logging with single-success filter ----
    static bool loggedFeatures[128] = { false };
    if (SUCCEEDED(hr) && feature < 128) {
        if (!loggedFeatures[feature]) {
            loggedFeatures[feature] = true;
            char buf[256] {};
            std::snprintf(buf, sizeof(buf),
                "CheckFeatureSupport %s (%u): HRESULT 0x%08lX (first success logged)",
                FeatureName(feature),
                static_cast<unsigned>(feature),
                static_cast<unsigned long>(hr));
            Log(buf);
        }
    } else if (FAILED(hr)) {
        if (InterlockedDecrement(&g_formatFailLogBudget) >= 0) {
            char buf[256] {};
            std::snprintf(buf, sizeof(buf),
                "CheckFeatureSupport %s (%u) FAILED: HRESULT 0x%08lX",
                FeatureName(feature),
                static_cast<unsigned>(feature),
                static_cast<unsigned long>(hr));
            Log(buf);
        }
    }

    if (!pFeatureSupportData) {
        return hr;
    }

    // === SPOOF: Feature Levels ===
    if (feature == D3D12_FEATURE_FEATURE_LEVELS &&
        featureSupportDataSize >= sizeof(D3D12_FEATURE_DATA_FEATURE_LEVELS)) {
        auto* levels = reinterpret_cast<D3D12_FEATURE_DATA_FEATURE_LEVELS*>(pFeatureSupportData);
        levels->MaxSupportedFeatureLevel = D3D_FEATURE_LEVEL_12_2;
        Log("Spoof: FEATURE_LEVELS -> S_OK, MaxSupportedFeatureLevel -> 12_2");
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

    // === SPOOF: D3D12_OPTIONS -> force ROVsSupported = TRUE ===
    if (feature == D3D12_FEATURE_D3D12_OPTIONS &&
        featureSupportDataSize >= sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS)) {
        auto* opts = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS*>(pFeatureSupportData);
        opts->ROVsSupported = TRUE;
        Log("Spoof: OPTIONS ROVsSupported forced TRUE (required for modern GPU profile)");
        return hr;
    }

    // === SPOOF: OPTIONS5 -> force Raytracing SUPPORTED ===
    if (feature == D3D12_FEATURE_D3D12_OPTIONS5 &&
        featureSupportDataSize >= sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5)) {
        auto* opts5 = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS5*>(pFeatureSupportData);
        opts5->RaytracingTier = D3D12_RAYTRACING_TIER_1_1;
        Log("Spoof: OPTIONS5 RaytracingTier forced D3D12_RAYTRACING_TIER_1_1 (required for modern GPU profile)");
        return hr;
    }

    // === SPOOF: OPTIONS6 -> force VRS SUPPORTED ===
    if (feature == D3D12_FEATURE_D3D12_OPTIONS6 &&
        featureSupportDataSize >= sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS6)) {
        auto* opts6 = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS6*>(pFeatureSupportData);
        opts6->VariableShadingRateTier = D3D12_VARIABLE_SHADING_RATE_TIER_2;
        opts6->AdditionalShadingRatesSupported = TRUE;
        Log("Spoof: OPTIONS6 VRS forced D3D12_VARIABLE_SHADING_RATE_TIER_2 (required for modern GPU profile)");
        return hr;
    }

    // === SPOOF: OPTIONS7 -> force MeshShader and SamplerFeedback SUPPORTED ===
    if (feature == D3D12_FEATURE_D3D12_OPTIONS7 &&
        featureSupportDataSize >= sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS7)) {
        auto* opts7 = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS7*>(pFeatureSupportData);
        opts7->MeshShaderTier = D3D12_MESH_SHADER_TIER_1;
        opts7->SamplerFeedbackTier = D3D12_SAMPLER_FEEDBACK_TIER_1_0;
        Log("Spoof: OPTIONS7 MeshShader forced D3D12_MESH_SHADER_TIER_1 & SamplerFeedback forced D3D12_SAMPLER_FEEDBACK_TIER_1_0");
        return hr;
    }

    // === SPOOF: OPTIONS12 -> force EnhancedBarriers SUPPORTED (with proxy hook) ===
    if (feature == D3D12_FEATURE_D3D12_OPTIONS12 &&
        featureSupportDataSize >= sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS12)) {
        auto* opts12 = reinterpret_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS12*>(pFeatureSupportData);
        opts12->EnhancedBarriersSupported = TRUE;
        Log("Spoof: OPTIONS12 EnhancedBarriersSupported forced TRUE (required feature bypass + proxy translation active)");
        return hr;
    }

    if (feature == D3D12_FEATURE_SHADER_MODEL &&
        featureSupportDataSize >= sizeof(D3D12_FEATURE_DATA_SHADER_MODEL)) {
        auto* sm = reinterpret_cast<D3D12_FEATURE_DATA_SHADER_MODEL*>(pFeatureSupportData);
        sm->HighestShaderModel = static_cast<D3D_SHADER_MODEL>(0x66); // SM 6.6
        Log("Spoof: SHADER_MODEL HighestShaderModel forced to 0x66 (SM 6.6 fallback)");
        return hr;
    }

    return hr;
}

// === Hook: GetDeviceRemovedReason ===
// Intercepts device removal to log the actual reason before the game crashes.
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

    PatchVtableSlot(vtable, 10,
        reinterpret_cast<void*>(&HookedCreateGraphicsPipelineState),
        reinterpret_cast<void**>(&g_originalCreateGraphicsPipelineState),
        "CreateGraphicsPipelineState", devIdx);

    PatchVtableSlot(vtable, 11,
        reinterpret_cast<void*>(&HookedCreateComputePipelineState),
        reinterpret_cast<void**>(&g_originalCreateComputePipelineState),
        "CreateComputePipelineState", devIdx);

    PatchVtableSlot(vtable, 47,
        reinterpret_cast<void*>(&HookedCreatePipelineState),
        reinterpret_cast<void**>(&g_originalCreatePipelineState),
        "CreatePipelineState (ID3D12Device2)", devIdx);

    PatchVtableSlot(vtable, 44,
        reinterpret_cast<void*>(&HookedCreatePipelineLibrary),
        reinterpret_cast<void**>(&g_originalCreatePipelineLibrary),
        "CreatePipelineLibrary (ID3D12Device1)", devIdx);

    // Hook command list Barrier method (vtable index 80) by creating a temporary command list
    static bool commandListHooked = false;
    if (!commandListHooked) {
        ID3D12CommandAllocator* tempAllocator = nullptr;
        HRESULT hrAlloc = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAllocator));
        if (SUCCEEDED(hrAlloc) && tempAllocator) {
            ID3D12GraphicsCommandList* tempCmdList = nullptr;
            HRESULT hrList = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAllocator, nullptr, IID_PPV_ARGS(&tempCmdList));
            if (SUCCEEDED(hrList) && tempCmdList) {
                commandListHooked = true;
                void*** objectList = reinterpret_cast<void***>(tempCmdList);
                void** vtableList = *objectList;
                
                PatchVtableSlot(vtableList, 80,
                    reinterpret_cast<void*>(&HookedBarrier),
                    reinterpret_cast<void**>(&g_originalBarrier),
                    "Barrier (Enhanced Barriers Bypass)", devIdx);
                
                tempCmdList->Release();
            }
            tempAllocator->Release();
        }
    }

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

    // Determine which feature levels to try.
    // For the rendering device (which requests FL 12_1), we try in order:
    //   12_0 -> 11_1 -> 11_0
    // We skip 12_1 entirely because the RX 580 cannot truly support it,
    // and creating a device at 12_1 would cause the runtime to expect
    // 12_1 capabilities that will crash during pipeline creation.
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
// Captures unhandled exceptions and logs details before the process dies.
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

// === Vectored Crash handler ===
// Captures vectored exceptions and logs details before the process dies.
static LONG WINAPI VectoredCrashHandler(EXCEPTION_POINTERS* ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    // Severity error (high bit set) and ignore standard C++ exception (0xE06D7363)
    if ((code & 0xC0000000) == 0xC0000000 && code != 0xE06D7363) {
        char buf[512] {};
        std::snprintf(buf, sizeof(buf),
            "*** CRASH DETECTED (VEH) ***\n"
            "  ExceptionCode: 0x%08lX\n"
            "  ExceptionAddress: 0x%p\n"
            "  RenderDeviceCreated: %s\n"
            "  ActualFeatureLevel: %s",
            static_cast<unsigned long>(code),
            ep->ExceptionRecord->ExceptionAddress,
            g_renderDeviceCreated ? "YES" : "NO",
            FeatureLevelName(g_actualDeviceFeatureLevel));
        Log(buf);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// === TDR timeout extension ===
// Increases the GPU timeout from default 2s to 30s to prevent
// false TDR kills during heavy shader compilation on RX 580.
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

    // Try to set TdrDelay=30
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

// === Suppress MessageBox popups (driver version warning) ===
static int (WINAPI* g_originalMessageBoxW)(HWND, LPCWSTR, LPCWSTR, UINT) = nullptr;
static int (WINAPI* g_originalMessageBoxA)(HWND, LPCSTR, LPCSTR, UINT) = nullptr;

static int WINAPI HookedMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
    // Suppress the AMD driver version warning
    if (lpText && (std::wcsstr(lpText, L"Driver update recommended") ||
                   std::wcsstr(lpText, L"driver version is older"))) {
        Log("Suppressed driver version warning dialog (MessageBoxW)");
        return IDOK;
    }
    return g_originalMessageBoxW(hWnd, lpText, lpCaption, uType);
}

static int WINAPI HookedMessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) {
    if (lpText && (strstr(lpText, "Driver update recommended") ||
                   strstr(lpText, "driver version is older"))) {
        Log("Suppressed driver version warning dialog (MessageBoxA)");
        return IDOK;
    }
    return g_originalMessageBoxA(hWnd, lpText, lpCaption, uType);
}

static void HookIAT(HMODULE targetModule, const char* dllName, const char* funcName, void* hookFunc, void** originalFunc) {
    if (!targetModule) return;
    
    PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(targetModule);
    PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(
        reinterpret_cast<uint8_t*>(targetModule) + dosHeader->e_lfanew);
    
    DWORD importDirRVA = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (importDirRVA == 0) return;
    
    PIMAGE_IMPORT_DESCRIPTOR importDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(
        reinterpret_cast<uint8_t*>(targetModule) + importDirRVA);
    
    for (; importDesc->Name != 0; ++importDesc) {
        const char* importDllName = reinterpret_cast<const char*>(
            reinterpret_cast<uint8_t*>(targetModule) + importDesc->Name);
        
        if (_stricmp(importDllName, dllName) != 0) continue;
        
        PIMAGE_THUNK_DATA origThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(
            reinterpret_cast<uint8_t*>(targetModule) + importDesc->OriginalFirstThunk);
        PIMAGE_THUNK_DATA iatThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(
            reinterpret_cast<uint8_t*>(targetModule) + importDesc->FirstThunk);
        
        for (; origThunk->u1.AddressOfData != 0; ++origThunk, ++iatThunk) {
            if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) continue;
            
            PIMAGE_IMPORT_BY_NAME importByName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(
                reinterpret_cast<uint8_t*>(targetModule) + origThunk->u1.AddressOfData);
            
            if (strcmp(importByName->Name, funcName) == 0) {
                DWORD oldProtect = 0;
                VirtualProtect(&iatThunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect);
                *originalFunc = reinterpret_cast<void*>(iatThunk->u1.Function);
                iatThunk->u1.Function = reinterpret_cast<ULONG_PTR>(hookFunc);
                VirtualProtect(&iatThunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);
                return;
            }
        }
    }
}

static void SuppressDriverWarning() {
    // Hook MessageBoxW/A in the main executable and amd_ags_x64.dll
    HMODULE exeModule = GetModuleHandleA(nullptr);
    HookIAT(exeModule, "user32.dll", "MessageBoxW", 
            reinterpret_cast<void*>(&HookedMessageBoxW),
            reinterpret_cast<void**>(&g_originalMessageBoxW));
    
    if (!g_originalMessageBoxW) {
        g_originalMessageBoxW = reinterpret_cast<decltype(g_originalMessageBoxW)>(
            GetProcAddress(GetModuleHandleA("user32.dll"), "MessageBoxW"));
    }
    
    HookIAT(exeModule, "user32.dll", "MessageBoxA",
            reinterpret_cast<void*>(&HookedMessageBoxA),
            reinterpret_cast<void**>(&g_originalMessageBoxA));
    
    if (!g_originalMessageBoxA) {
        g_originalMessageBoxA = reinterpret_cast<decltype(g_originalMessageBoxA)>(
            GetProcAddress(GetModuleHandleA("user32.dll"), "MessageBoxA"));
    }
    
    // Also hook in amd_ags_x64.dll if loaded
    HMODULE agsModule = GetModuleHandleA("amd_ags_x64.dll");
    if (agsModule) {
        void* dummy = nullptr;
        HookIAT(agsModule, "user32.dll", "MessageBoxW",
                reinterpret_cast<void*>(&HookedMessageBoxW), &dummy);
        HookIAT(agsModule, "user32.dll", "MessageBoxA",
                reinterpret_cast<void*>(&HookedMessageBoxA), &dummy);
        Log("Hooked MessageBox in amd_ags_x64.dll for driver warning suppression");
    }
    
    Log("MessageBox hooks installed for driver warning suppression");
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        Log("=== d3d12.dll proxy loaded (v3.1 - SM6.3+ fallback + Crimson Desert compatibility) ===");
        SetUnhandledExceptionFilter(CrashHandler);
        AddVectoredExceptionHandler(1, VectoredCrashHandler);
        TryExtendTdrTimeout();
        SuppressDriverWarning();
    }
    return TRUE;
}
