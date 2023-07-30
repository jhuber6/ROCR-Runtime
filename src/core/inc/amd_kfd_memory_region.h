////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2023, Advanced Micro Devices, Inc. All rights reserved.
//
// Developed by:
//
//                 AMD Research and AMD HSA Software Development
//
//                 Advanced Micro Devices, Inc.
//
//                 www.amd.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
//  - Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimers.
//  - Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimers in
//    the documentation and/or other materials provided with the distribution.
//  - Neither the names of Advanced Micro Devices, Inc,
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this Software without specific prior written
//    permission.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

// AMD specific HSA backend.

#ifndef HSA_RUNTIME_CORE_INC_KFD_AMD_MEMORY_REGION_H_
#define HSA_RUNTIME_CORE_INC_KFD_AMD_MEMORY_REGION_H_

#include "hsakmt/hsakmt.h"

#include "core/inc/runtime.h"
#include "core/inc/amd_memory_region.h"
#include "core/util/simple_heap.h"
#include "core/util/locks.h"

#include "inc/hsa_ext_amd.h"

namespace rocr {

namespace core {
class Agent;
} // namespace core

namespace AMD {

class KfdMemoryRegion : public MemoryRegion {
 public:
  /// @brief Convert this object into hsa_region_t.
  static __forceinline hsa_region_t Convert(MemoryRegion* region) {
    const hsa_region_t region_handle = {
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(region))};
    return region_handle;
  }

  static __forceinline const hsa_region_t Convert(const MemoryRegion* region) {
    const hsa_region_t region_handle = {
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(region))};
    return region_handle;
  }

  /// @brief  Convert hsa_region_t into AMD::MemoryRegion *.
  static __forceinline MemoryRegion* Convert(hsa_region_t region) {
    return reinterpret_cast<MemoryRegion*>(region.handle);
  }

  /// @brief Allocate agent accessible memory (system / local memory).
  static void* AllocateKfdMemory(const HsaMemFlags& flag, HSAuint32 node_id,
                                 size_t size);

  /// @brief Free agent accessible memory (system / local memory).
  static void FreeKfdMemory(void* ptr, size_t size);

  /// @brief Pin memory.
  static bool MakeKfdMemoryResident(size_t num_node, const uint32_t* nodes, const void* ptr,
                                    size_t size, uint64_t* alternate_va, HsaMemMapFlags map_flag);

  /// @brief Unpin memory.
  static void MakeKfdMemoryUnresident(const void* ptr);

  KfdMemoryRegion(bool fine_grain, bool kernarg, bool full_profile,
                  core::Agent* owner, const HsaMemoryProperties& mem_props);

  ~KfdMemoryRegion();

  hsa_status_t Allocate(size_t& size, AllocateFlags alloc_flags,
                        void** address) const override;

  hsa_status_t Free(void* address, size_t size) const override;

  hsa_status_t IPCFragmentExport(void* address) const override;

  hsa_status_t GetInfo(hsa_region_info_t attribute, void* value) const override;

  hsa_status_t GetPoolInfo(hsa_amd_memory_pool_info_t attribute,
                           void* value) const override;

  hsa_status_t GetAgentPoolInfo(const core::Agent& agent,
                                hsa_amd_agent_memory_pool_info_t attribute,
                                void* value) const override;

  hsa_status_t AllowAccess(uint32_t num_agents, const hsa_agent_t* agents,
                           const void* ptr, size_t size) const override;

  hsa_status_t CanMigrate(const MemoryRegion& dst, bool& result) const override;

  hsa_status_t Migrate(uint32_t flag, const void* ptr) const override;

  hsa_status_t Lock(uint32_t num_agents, const hsa_agent_t* agents,
                    void* host_ptr, size_t size, void** agent_ptr) const override;

  hsa_status_t Unlock(void* host_ptr) const override;

  HSAuint64 GetBaseAddress() const override { return mem_props_.VirtualBaseAddress; }

  HSAuint64 GetPhysicalSize() const override { return mem_props_.SizeInBytes; }

  HSAuint64 GetVirtualSize() const override { return virtual_size_; }

  hsa_status_t AssignAgent(void* ptr, size_t size, const core::Agent& agent,
                           hsa_access_permission_t access) const override;

  void Trim() const override;

  HSAuint64 GetCacheSize() const override { return fragment_allocator_.cache_size(); }

  __forceinline bool IsLocalMemory() const override {
    return ((mem_props_.HeapType == HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE) ||
            (mem_props_.HeapType == HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC));
  }

  __forceinline bool IsPublic() const override {
    return (mem_props_.HeapType == HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC);
  }

  __forceinline bool IsSystem() const override {
    return mem_props_.HeapType == HSA_HEAPTYPE_SYSTEM;
  }

  __forceinline bool IsLDS() const override {
    return mem_props_.HeapType == HSA_HEAPTYPE_GPU_LDS;
  }

  __forceinline bool IsGDS() const override {
    return mem_props_.HeapType == HSA_HEAPTYPE_GPU_GDS;
  }

  __forceinline bool IsScratch() const override {
    return mem_props_.HeapType == HSA_HEAPTYPE_GPU_SCRATCH;
  }

  __forceinline uint32_t BusWidth() const {
    return static_cast<uint32_t>(mem_props_.Width);
  }

  __forceinline uint32_t MaxMemCloc() const {
    return static_cast<uint32_t>(mem_props_.MemoryClockMax);
  }

 private:
  const HsaMemoryProperties mem_props_;

  HsaMemFlags mem_flag_;

  HsaMemMapFlags map_flag_;

  size_t max_single_alloc_size_;

  // Used to collect total system memory
  static size_t max_sysmem_alloc_size_;

  HSAuint64 virtual_size_;

  // Protects against concurrent allow_access calls to fragments of the same block by virtue of all
  // fragments of the block routing to the same MemoryRegion.
  mutable KernelMutex access_lock_;

  static const size_t kPageSize_ = 4096;

  // Determine access type allowed to requesting device
  hsa_amd_memory_pool_access_t GetAccessInfo(const core::Agent& agent,
                                             const core::Runtime::LinkInfo& link_info) const;

  // Operational body for Allocate.  Recursive.
  hsa_status_t AllocateImpl(size_t& size, AllocateFlags alloc_flags,
                            void** address) const;

  // Operational body for Free.  Recursive.
  hsa_status_t FreeImpl(void* address, size_t size) const;

  static bool RegisterMemory(void* ptr, size_t size, const HsaMemFlags& MemFlags);
  static void DeregisterMemory(void* ptr);

  class BlockAllocator {
   private:
    KfdMemoryRegion& region_;
    static const size_t block_size_ = 2 * 1024 * 1024;  // 2MB blocks.
   public:
    explicit BlockAllocator(KfdMemoryRegion& region) : region_(region) {}
    void* alloc(size_t request_size, size_t& allocated_size) const;
    void free(void* ptr, size_t length) const { region_.FreeImpl(ptr, length); }
    size_t block_size() const { return block_size_; }
  };

  mutable SimpleHeap<BlockAllocator> fragment_allocator_;
};

}  // namespace amd
}  // namespace rocr

#endif  // header guard
