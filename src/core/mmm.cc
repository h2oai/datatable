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
  entries.push_back(MmmEntry(mmapsize, obj));
  obj->save_entry_index(entries.size() - 1);
}


// Careful not to throw any exceptions here: this method is called from
// MmapMRI's destructor, and an uncaught exception will cause SIGABRT.
void MemoryMapManager::del_entry(size_t i) {
  // Move the last entry into the now-empty slot <i>
  std::swap(entries[i], entries.back());
  entries[i].obj->save_entry_index(i);
  entries.pop_back();
}


bool MemoryMapManager::check_entry(size_t i, const MemoryMapWorker* obj) {
  return (i > 0 && i < entries.size() && entries[i].obj == obj);
}


void MemoryMapManager::freeup_memory() {
  #ifndef NDEBUG
    size_t size0 = entries.size();
  #endif
  // Sort the entries by size in descending order
  sort_entries();
  // Evict the entries at the top of the array
  for (size_t j = 0; j < n_entries_to_purge; ++j) {
    if (entries.size() <= 1) break;
    entries.back().obj->evict();
    xassert(entries.size() == size0 - j);
    entries.pop_back();
  }
}


void MemoryMapManager::sort_entries() {
  std::sort(entries.begin() + 1, entries.end());
  for (size_t i = 1; i < entries.size(); ++i) {
    entries[i].obj->save_entry_index(i);
  }
}


MemoryMapWorker::~MemoryMapWorker() {}
