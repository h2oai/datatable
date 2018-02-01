//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
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
  if (i < count) {
    // Move the last entry into the now-empty slot <i>
    entries[i].size = entries[count].size;
    entries[i].obj = entries[count].obj;
    entries[count].obj->save_entry_index(i);
  }
  entries[count].size = 0;
  entries[count].obj = nullptr;
  count--;
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
