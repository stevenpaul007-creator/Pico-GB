#pragma once
#include "common.h"

#include <functional>
#include <stdint.h>
typedef struct {
  void* address; // 内存块的起始地址（使用 void* 来指向任何类型的数据）
  size_t size; // 内存块的大小（以字节为单位）
} MemoryRegion;