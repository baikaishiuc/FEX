/*
$info$
tags: glue|block-database
desc: Stores information about blocks, and provides C++ implementations to lookup the blocks
$end_info$
*/

#include <FEXCore/Utils/Allocator.h>
#include <FEXCore/Utils/LogManager.h>

#include "Interface/Context/Context.h"
#include "Interface/Core/LookupCache.h"

#include <sys/mman.h>

namespace FEXCore {
LookupCache::LookupCache(FEXCore::Context::Context *CTX)
  : ctx {CTX} {

  TotalCacheSize = ctx->Config.VirtualMemSize / 4096 * 8 + CODE_SIZE + L1_SIZE;
  // Setup our PMR map.
  BlockLinks = BlockLinks_pma.new_object<BlockLinksMapType>();

  // Block cache ends up looking like this
  // PageMemoryMap[VirtualMemoryRegion >> 12]
  //       |
  //       v
  // PageMemory[Memory & (VIRTUAL_PAGE_SIZE - 1)]
  //       |
  //       v
  // Pointer to Code
  //
  // Allocate a region of memory that we can use to back our block pointers
  // We need one pointer per page of virtual memory
  // At 64GB of virtual memory this will allocate 128MB of virtual memory space
  PagePointer = reinterpret_cast<uintptr_t>(FEXCore::Allocator::mmap(nullptr, TotalCacheSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

  // Allocate our memory backing our pages
  // We need 32KB per guest page (One pointer per byte)
  // XXX: We can drop down to 16KB if we store 4byte offsets from the code base
  // We currently limit to 128MB of real memory for caching for the total cache size.
  // Can end up being inefficient if we compile a small number of blocks per page
  PageMemory = PagePointer + ctx->Config.VirtualMemSize / 4096 * 8;
  LOGMAN_THROW_AA_FMT(PageMemory != -1ULL, "Failed to allocate page memory");

  // L1 Cache
  L1Pointer = PageMemory + CODE_SIZE;
  LOGMAN_THROW_AA_FMT(L1Pointer != -1ULL, "Failed to allocate L1Pointer");

  VirtualMemSize = ctx->Config.VirtualMemSize;
}

LookupCache::~LookupCache() {
  const size_t TotalCacheSize = ctx->Config.VirtualMemSize / 4096 * 8 + CODE_SIZE + L1_SIZE;
  FEXCore::Allocator::munmap(reinterpret_cast<void*>(PagePointer), TotalCacheSize);

  // No need to free BlockLinks map.
  // These will get freed when their memory allocators are deallocated.
}

void LookupCache::ClearL2Cache() {
  std::lock_guard<std::recursive_mutex> lk(WriteLock);
  // Clear out the page memory
  // PagePointer and PageMemory are sequential with each other. Clear both at once.
  madvise(reinterpret_cast<void*>(PagePointer), ctx->Config.VirtualMemSize / 4096 * 8 + CODE_SIZE, MADV_DONTNEED);
  AllocateOffset = 0;
}

void LookupCache::ClearCache() {
  std::lock_guard<std::recursive_mutex> lk(WriteLock);

  // Clear L1 and L2 by clearing the full cache.
  madvise(reinterpret_cast<void*>(PagePointer), TotalCacheSize, MADV_DONTNEED);
  // Clear the BlockLinks allocator which frees the BlockLinks map implicitly.
  BlockLinks_mbr.release();
  // Allocate a new pointer from the BlockLinks pma again.
  BlockLinks = BlockLinks_pma.new_object<BlockLinksMapType>();
  // All code is gone, clear the block list
  BlockList.clear();
}

}

