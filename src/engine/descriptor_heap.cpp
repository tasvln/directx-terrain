#include "descriptor_heap.h"

DescriptorHeap::DescriptorHeap(
    ComPtr<ID3D12Device2> device, 
    D3D12_DESCRIPTOR_HEAP_TYPE type, 
    UINT numDescriptors, 
    bool shaderVisible
) : type(type), capacity(numDescriptors)  
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = numDescriptors;
    desc.Type = type;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    throwFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));
    descriptorSize = device->GetDescriptorHandleIncrementSize(type);

    LOG_INFO(L"DescriptorHeap Initialized: type=%d, numDescriptors=%d", type, numDescriptors);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::getCPUHandle(UINT index) const {
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(heap->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(index, descriptorSize);
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::getGPUHandle(UINT index) const {
    auto handle = heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * descriptorSize;
    return handle;
}

