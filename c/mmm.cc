//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "mmm.h"
#include <algorithm>
#include "utils/assert.h"
#include "utils/exceptions.h"



MemoryMapManager::MemoryMapManager(size_t nelems) {
  entries.reserve(nelems);
  entries.push_back(MmmEntry());
}


MemoryMapManager* MemoryMapManager::get() {
  static MemoryMapManager* mmapmanager = new MemoryMapManager(65536);
  return mmapmanager;
}


void MemoryMapManager::add_entry(MemoryMapWorker* obj, size_t mmapsize) {
  xassert(!entries.empty());
  entries.push_back(MmmEntry(mmapsize, obj));
  obj->save_entry_index(entries.size() - 1);
}


void MemoryMapManager::del_entry(size_t i, MemoryMapWorker* obj) {
  xassert(i > 0);
  xassert(i < entries.size());
  if (entries[i].obj != obj) {
    throw RuntimeError() << "Data corruption in MemoryMapManager: "
       << "entry " << i << " is {.size=" << entries[i].size
       << ", .obj=" << entries[i].obj << "}, but referred from object " << obj;
  }
  // Move the last entry into the now-empty slot <i>
  std::swap(entries[i], entries[entries.size() - 1]);
  entries[i].obj->save_entry_index(i);
  entries.pop_back();
  xassert(!entries.empty());
}


void MemoryMapManager::freeup_memory() {
  // Sort the entries by size in descending order
  sort_entries();
  // Evict the entries at the top of the array
  for (size_t j = 0; j < n_entries_to_purge; ++j) {
    if (entries.size() <= 1) break;
    entries.back().obj->evict();
    entries.pop_back();
  }
  xassert(!entries.empty());
}


void MemoryMapManager::sort_entries() {
  auto start = entries.begin() + 1;
  auto end = entries.end();  //start + static_cast<ptrdiff_t>(count);
  std::sort(start, end);
  for (size_t i = 1; i < entries.size(); ++i) {
    entries[i].obj->save_entry_index(i);
  }
}


MemoryMapWorker::~MemoryMapWorker() {}
