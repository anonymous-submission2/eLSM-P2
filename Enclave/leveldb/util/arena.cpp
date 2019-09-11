// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/arena.h"
#include <assert.h>
#include "Enclave.h"
#include "Enclave_t.h"
#define OUTSIDE 0
#define PINGPONG 0
namespace leveldb {

static const int kBlockSize = 4096;
//flag==1 means readbuffer
Arena::Arena(int f) : memory_usage_(0), flag(f) {
#if OUTSIDE
#if PINGPONG
  static int i = 0; 
  if (i==0)
      ocall_allocate3(&_backmem1,&_backmem2,&_backmem3);
  else {
      if (i%3==0)
	_backmem = _backmem1; 
      else if (i%3==1)
	_backmem = _backmem2; 
      else
	_backmem = _backmem3; 
  }
  i++;
#else
  ocall_allocate(&_backmem, flag);
#endif
  current_block = 0;
#endif
  alloc_ptr_ = NULL;  // First allocation will allocate a block
  alloc_bytes_remaining_ = 0;
}

Arena::~Arena() {
#if OUTSIDE
  current_block=0;
#else
  for (size_t i = 0; i < blocks_.size(); i++) {
    delete[] blocks_[i];
  }
#endif
}

char* Arena::AllocateFallback(size_t bytes) {
  if (bytes > kBlockSize / 4) {
    // Object is more than a quarter of our block size.  Allocate it separately
    // to avoid wasting too much space in leftover bytes.
    char* result = AllocateNewBlock(bytes);
    return result;
  }

  // We waste the remaining space in the current block.
  alloc_ptr_ = AllocateNewBlock(kBlockSize);
  alloc_bytes_remaining_ = kBlockSize;

  char* result = alloc_ptr_;
  alloc_ptr_ += bytes;
  alloc_bytes_remaining_ -= bytes;
  return result;
}

char* Arena::AllocateAligned(size_t bytes) {
  const int align = (sizeof(void*) > 8) ? sizeof(void*) : 8;
  assert((align & (align-1)) == 0);   // Pointer size should be a power of 2
  size_t current_mod = reinterpret_cast<uintptr_t>(alloc_ptr_) & (align-1);
  size_t slop = (current_mod == 0 ? 0 : align - current_mod);
  size_t needed = bytes + slop;
  char* result;
  if (needed <= alloc_bytes_remaining_) {
    result = alloc_ptr_ + slop;
    alloc_ptr_ += needed;
    alloc_bytes_remaining_ -= needed;
  } else {
    // AllocateFallback always returned aligned memory
    result = AllocateFallback(bytes);
  }
  // Ju remove assertion
  //assert((reinterpret_cast<uintptr_t>(result) & (align-1)) == 0);
  return result;
}

char* Arena::AllocateNewBlock(size_t block_bytes) {
#if OUTSIDE
  char* result = (char*)_backmem + current_block; 
  current_block += block_bytes;
  if (current_block >= 1024*1024*1024) {
    ocall_allocate(&_backmem, flag);
    current_block = 0;
  }
#else
  char* result = new char[block_bytes];
#endif
  blocks_.push_back(result);
  memory_usage_.NoBarrier_Store(
      reinterpret_cast<void*>(MemoryUsage() + block_bytes + sizeof(char*)));
//  if (flag)
 //     printf("memory_usage is %d\n",memory_usage_);
  return result;
}

}  // namespace leveldb
