import re

with open('src/D3D12Proxy.cpp', 'r') as f:
    content = f.read()

helper = '''
static size_t GetStreamElementSize(uint32_t type) {
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

content = re.sub(r'static void\nParseStreamAndLogShaders\(const D3D12_PIPELINE_STATE_STREAM_DESC \*pDesc\) \{', helper, content)

parse_stream_body = '''
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
}
'''

content = re.sub(
    r'  if \(\!pDesc \|\| \!pDesc->pPipelineStateSubobjectStream \|\|.*?ptr \+= size;\s*\}\s*\}\s*\}', 
    parse_stream_body.strip(), 
    content, 
    flags=re.DOTALL
)


stream_has_high = '''static bool StreamHasHighSMCS(const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc) {
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

content = re.sub(
    r'static bool StreamHasHighSMCS.*?return false;\s*\}',
    stream_has_high,
    content,
    flags=re.DOTALL
)

mutate_stream = '''static D3D12_PIPELINE_STATE_STREAM_DESC
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

content = re.sub(
    r'static D3D12_PIPELINE_STATE_STREAM_DESC\nMutateStreamCS.*?return mutatedDesc;\s*\}',
    mutate_stream,
    content,
    flags=re.DOTALL
)

with open('src/D3D12Proxy.cpp', 'w') as f:
    f.write(content)

print("Done")
