import sys

with open('src/D3D12Proxy.cpp', 'r') as f:
    code = f.read()

helper = '''static size_t GetStreamElementSize(uint32_t type) {
  size_t innerSize = 0;
  switch (type) {
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE: innerSize = sizeof(void*); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS:
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS:
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS:
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS: innerSize = sizeof(D3D12_SHADER_BYTECODE); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT: innerSize = sizeof(D3D12_STREAM_OUTPUT_DESC); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND: innerSize = sizeof(D3D12_BLEND_DESC); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK: innerSize = sizeof(UINT); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER: innerSize = sizeof(D3D12_RASTERIZER_DESC); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL: innerSize = sizeof(D3D12_DEPTH_STENCIL_DESC); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT: innerSize = sizeof(D3D12_INPUT_LAYOUT_DESC); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE: innerSize = sizeof(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY: innerSize = sizeof(D3D12_PRIMITIVE_TOPOLOGY_TYPE); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS: innerSize = sizeof(D3D12_RT_FORMAT_ARRAY); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT: innerSize = sizeof(DXGI_FORMAT); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC: innerSize = sizeof(DXGI_SAMPLE_DESC); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK: innerSize = sizeof(UINT); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO: innerSize = sizeof(D3D12_CACHED_PIPELINE_STATE); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS: innerSize = sizeof(D3D12_PIPELINE_STATE_FLAGS); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1: innerSize = sizeof(D3D12_DEPTH_STENCIL_DESC1); break;
  case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING: innerSize = sizeof(D3D12_VIEW_INSTANCING_DESC); break;
  case 24: /* AS */ innerSize = sizeof(D3D12_SHADER_BYTECODE); break;
  case 25: /* MS */ innerSize = sizeof(D3D12_SHADER_BYTECODE); break;
  default: return 0;
  }
  return (sizeof(void*) + innerSize + 7) & ~7;
}

static void
ParseStreamAndLogShaders(const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc) {'''

# 1. ParseStreamAndLogShaders
old1 = '''static void
ParseStreamAndLogShaders(const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc) {
  if (!pDesc || !pDesc->pPipelineStateSubobjectStream ||
      pDesc->SizeInBytes == 0) {
    return;
  }

  const uint8_t *ptr =
      reinterpret_cast<const uint8_t *>(pDesc->pPipelineStateSubobjectStream);
  const uint8_t *end = ptr + pDesc->SizeInBytes;

  while (ptr < end) {
    uint32_t type = *reinterpret_cast<const uint32_t *>(ptr);

    if (type == 2) { // VS
      const auto *bytecode =
          reinterpret_cast<const D3D12_SHADER_BYTECODE *>(ptr + sizeof(void *));
      LogShaderInfo("Stream VS", *bytecode);
      ptr += sizeof(void *) + sizeof(D3D12_SHADER_BYTECODE);
    } else if (type == 3) { // PS
      const auto *bytecode =
          reinterpret_cast<const D3D12_SHADER_BYTECODE *>(ptr + sizeof(void *));
      LogShaderInfo("Stream PS", *bytecode);
      ptr += sizeof(void *) + sizeof(D3D12_SHADER_BYTECODE);
    } else if (type == 8) { // CS
      const auto *bytecode =
          reinterpret_cast<const D3D12_SHADER_BYTECODE *>(ptr + sizeof(void *));
      LogShaderInfo("Stream CS", *bytecode);
      ptr += sizeof(void *) + sizeof(D3D12_SHADER_BYTECODE);
    } else {
      size_t size = 8;
      switch (type) {
      case 0:
        size = 16;
        break;
      case 1:
        size = 16;
        break;
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 8:
      case 13:
      case 20:
        size = 24;
        break;
      case 7:
        size = 48;
        break;
      case 9:
        size = 328;
        break;
      case 10:
      case 14:
      case 15:
      case 17:
      case 18:
      case 19:
        size = 16;
        break;
      case 11:
        size = 56;
        break;
      case 12:
        size = 64;
        break;
      case 16:
        size = 48;
        break;
      case 21:
        size = 32;
        break;
      default:
        return;
      }
      ptr += size;
    }
  }
}'''

new1 = helper + '''
  if (!pDesc || !pDesc->pPipelineStateSubobjectStream ||
      pDesc->SizeInBytes == 0) {
    return;
  }

  const uint8_t *ptr =
      reinterpret_cast<const uint8_t *>(pDesc->pPipelineStateSubobjectStream);
  const uint8_t *end = ptr + pDesc->SizeInBytes;

  while (ptr < end) {
    uint32_t type = *reinterpret_cast<const uint32_t *>(ptr);

    if (type == D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS) {
      const auto *bytecode =
          reinterpret_cast<const D3D12_SHADER_BYTECODE *>(ptr + sizeof(void *));
      LogShaderInfo("Stream VS", *bytecode);
    } else if (type == D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS) {
      const auto *bytecode =
          reinterpret_cast<const D3D12_SHADER_BYTECODE *>(ptr + sizeof(void *));
      LogShaderInfo("Stream PS", *bytecode);
    } else if (type == D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS) {
      const auto *bytecode =
          reinterpret_cast<const D3D12_SHADER_BYTECODE *>(ptr + sizeof(void *));
      LogShaderInfo("Stream CS", *bytecode);
    }

    size_t size = GetStreamElementSize(type);
    if (size == 0) return;
    ptr += size;
  }
}'''

# 2. StreamHasHighSMCS
old2 = '''static bool StreamHasHighSMCS(const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc) {
  if (!pDesc || !pDesc->pPipelineStateSubobjectStream ||
      pDesc->SizeInBytes == 0) {
    return false;
  }
  const uint8_t *ptr =
      reinterpret_cast<const uint8_t *>(pDesc->pPipelineStateSubobjectStream);
  const uint8_t *end = ptr + pDesc->SizeInBytes;
  while (ptr < end) {
    uint32_t type = *reinterpret_cast<const uint32_t *>(ptr);
    if (type == 8) { // CS
      const auto *bytecode =
          reinterpret_cast<const D3D12_SHADER_BYTECODE *>(ptr + sizeof(void *));
      uint8_t minor = GetShaderMinorVersion(*bytecode);
      if (minor >= 3)
        return true;
      ptr += sizeof(void *) + sizeof(D3D12_SHADER_BYTECODE);
    } else if (type == 2 || type == 3) { // VS or PS
      ptr += sizeof(void *) + sizeof(D3D12_SHADER_BYTECODE);
    } else {
      size_t size = 8;
      switch (type) {
      case 0:
        size = 16;
        break;
      case 1:
        size = 16;
        break;
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 8:
      case 13:
      case 20:
        size = 24;
        break;
      case 7:
        size = 48;
        break;
      case 9:
        size = 328;
        break;
      case 10:
      case 14:
      case 15:
      case 17:
      case 18:
      case 19:
        size = 16;
        break;
      case 11:
        size = 56;
        break;
      case 12:
        size = 64;
        break;
      case 16:
        size = 48;
        break;
      case 21:
        size = 32;
        break;
      default:
        return false;
      }
      ptr += size;
    }
  }
  return false;
}'''

new2 = '''static bool StreamHasHighSMCS(const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc) {
  if (!pDesc || !pDesc->pPipelineStateSubobjectStream ||
      pDesc->SizeInBytes == 0) {
    return false;
  }
  const uint8_t *ptr =
      reinterpret_cast<const uint8_t *>(pDesc->pPipelineStateSubobjectStream);
  const uint8_t *end = ptr + pDesc->SizeInBytes;
  while (ptr < end) {
    uint32_t type = *reinterpret_cast<const uint32_t *>(ptr);
    if (type == D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS) {
      const auto *bytecode =
          reinterpret_cast<const D3D12_SHADER_BYTECODE *>(ptr + sizeof(void *));
      uint8_t minor = GetShaderMinorVersion(*bytecode);
      if (minor >= 3)
        return true;
    }
    
    size_t size = GetStreamElementSize(type);
    if (size == 0) return false;
    ptr += size;
  }
  return false;
}'''

# 3. MutateStreamCS
old3 = '''static D3D12_PIPELINE_STATE_STREAM_DESC
MutateStreamCS(const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc,
               std::vector<uint8_t> &outBuffer) {
  outBuffer.clear();
  if (!pDesc || !pDesc->pPipelineStateSubobjectStream ||
      pDesc->SizeInBytes == 0) {
    return *pDesc;
  }

  outBuffer.resize(pDesc->SizeInBytes);
  std::memcpy(outBuffer.data(), pDesc->pPipelineStateSubobjectStream,
              pDesc->SizeInBytes);

  uint8_t *ptr = outBuffer.data();
  uint8_t *end = ptr + pDesc->SizeInBytes;

  while (ptr < end) {
    uint32_t type = *reinterpret_cast<uint32_t *>(ptr);

    if (type == 8) { // CS (Compute Shader)
      auto *bytecode =
          reinterpret_cast<D3D12_SHADER_BYTECODE *>(ptr + sizeof(void *));
      bytecode->pShaderBytecode = kEmptyComputeShader;
      bytecode->BytecodeLength = sizeof(kEmptyComputeShader);

      ptr += sizeof(void *) + sizeof(D3D12_SHADER_BYTECODE);
    } else if (type == 2 || type == 3) { // VS or PS
      ptr += sizeof(void *) + sizeof(D3D12_SHADER_BYTECODE);
    } else {
      size_t size = 8;
      switch (type) {
      case 0:
        size = 16;
        break;
      case 1:
        size = 16;
        break;
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 8:
      case 13:
      case 20:
        size = 24;
        break;
      case 7:
        size = 48;
        break;
      case 9:
        size = 328;
        break;
      case 10:
      case 14:
      case 15:
      case 17:
      case 18:
      case 19:
        size = 16;
        break;
      case 11:
        size = 56;
        break;
      case 12:
        size = 64;
        break;
      case 16:
        size = 48;
        break;
      case 21:
        size = 32;
        break;
      default:
        D3D12_PIPELINE_STATE_STREAM_DESC mutatedDesc = *pDesc;
        mutatedDesc.pPipelineStateSubobjectStream = outBuffer.data();
        return mutatedDesc;
      }
      ptr += size;
    }
  }

  D3D12_PIPELINE_STATE_STREAM_DESC mutatedDesc = *pDesc;
  mutatedDesc.pPipelineStateSubobjectStream = outBuffer.data();
  return mutatedDesc;
}'''

new3 = '''static D3D12_PIPELINE_STATE_STREAM_DESC
MutateStreamCS(const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc,
               std::vector<uint8_t> &outBuffer) {
  outBuffer.clear();
  if (!pDesc || !pDesc->pPipelineStateSubobjectStream ||
      pDesc->SizeInBytes == 0) {
    return *pDesc;
  }

  outBuffer.resize(pDesc->SizeInBytes);
  std::memcpy(outBuffer.data(), pDesc->pPipelineStateSubobjectStream,
              pDesc->SizeInBytes);

  uint8_t *ptr = outBuffer.data();
  uint8_t *end = ptr + pDesc->SizeInBytes;

  while (ptr < end) {
    uint32_t type = *reinterpret_cast<uint32_t *>(ptr);

    if (type == D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS) {
      auto *bytecode =
          reinterpret_cast<D3D12_SHADER_BYTECODE *>(ptr + sizeof(void *));
      bytecode->pShaderBytecode = kEmptyComputeShader;
      bytecode->BytecodeLength = sizeof(kEmptyComputeShader);
    }
    
    size_t size = GetStreamElementSize(type);
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
}'''

code = code.replace(old1, new1)
code = code.replace(old2, new2)
code = code.replace(old3, new3)

with open('src/D3D12Proxy.cpp', 'w') as f:
    f.write(code)

print("Replacement done")
