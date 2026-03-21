#pragma once

#include "utils/pch.h"

class DescriptorHeap {
public:
    DescriptorHeap(
        ComPtr<ID3D12Device2> device, 
        D3D12_DESCRIPTOR_HEAP_TYPE type, 
        UINT numDescriptors, 
        bool shaderVisible = false
    );

    CD3DX12_CPU_DESCRIPTOR_HANDLE getCPUHandle(UINT index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE   getGPUHandle(UINT index) const;

    // Allocate the next free descriptor slot, returns its index
    UINT allocate() {
        assert(currentIndex < capacity && "DescriptorHeap is full!");
        return currentIndex++;
    }

    UINT getCount()          const { return currentIndex; }
    UINT getCapacity()       const { return capacity; }
    bool isFull()            const { return currentIndex >= capacity; }

    ComPtr<ID3D12DescriptorHeap> getHeap()           const { return heap; }
    UINT                         getDescriptorSize()  const { return descriptorSize; }

private:
    ComPtr<ID3D12DescriptorHeap> heap;
    UINT descriptorSize  = 0;
    UINT currentIndex    = 0;  // next free slot
    UINT capacity        = 0;  // total slots
    D3D12_DESCRIPTOR_HEAP_TYPE type;
};