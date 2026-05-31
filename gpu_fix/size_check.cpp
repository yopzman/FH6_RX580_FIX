#include <iostream>
#include <d3d12.h>

int main() {
    std::cout << "D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS = " << D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS << "\n";
    std::cout << "D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS = " << D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS << "\n";
    std::cout << "sizeof(D3D12_SHADER_BYTECODE) = " << sizeof(D3D12_SHADER_BYTECODE) << "\n";
    std::cout << "sizeof(D3D12_BLEND_DESC) = " << sizeof(D3D12_BLEND_DESC) << "\n";
    std::cout << "sizeof(D3D12_RASTERIZER_DESC) = " << sizeof(D3D12_RASTERIZER_DESC) << "\n";
    return 0;
}
