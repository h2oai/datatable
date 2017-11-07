//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#include "mmm.h"
#include <algorithm>



MemoryMapManager::MemoryMapManager(size_t nelems) {
  count = 0;
  entries.reserve(nelems);
  for (size_t i = 0; i < nelems; ++i) {
    entries[i].size = 0;
  }
}


MemoryMapManager* MemoryMapManager::get() {
  static MemoryMapManager* mmapmanager = new MemoryMapManager(65536);
  return mmapmanager;
}


void MemoryMapManager::add_entry(MemoryMapWorker* obj, size_t mmapsize) {
  count++;
  if (count == entries.size()) {
    entries.reserve(count * 2);
  }
  entries[count].size = mmapsize;
  entries[count].obj = obj;
  obj->save_entry_index(count);
}


void MemoryMapManager::del_entry(size_t i) {
  if (i == 0) return;
  count--;
  if (i < count) {
    entries[i].size = entries[count].size;
    entries[i].obj = entries[count].obj;
    entries[i].obj->save_entry_index(i);
  }
  entries[count].size = 0;
  entries[count].obj = nullptr;
}


void MemoryMapManager::freeup_memory() {
  // Sort the entries by size in descending order
  sort_entries();
  // Evict the entries at the top of the array
  size_t i = count - 1;
  size_t last_i = (i >= n_entries_to_purge)? i - n_entries_to_purge : 0;
  for (;;) {
    entries[i].obj->evict();
    entries[i].obj = nullptr;
    entries[i].size = 0;
    if (i == last_i) break;
    --i;
  }
  count = last_i;
}


void MemoryMapManager::sort_entries() {
  auto start = entries.begin() + 1;
  auto end = start + static_cast<ptrdiff_t>(count);
  std::sort(start, end, [](const MmmEntry& a, const MmmEntry& b) -> bool {
    return a.size >= b.size;
  });
}


MemoryMapWorker::~MemoryMapWorker() {}
