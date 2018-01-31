//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_MMM_H
#define dt_MMM_H
#include <vector>


class MemoryMapWorker {
public:
  virtual ~MemoryMapWorker();
  virtual void save_entry_index(size_t i) = 0;
  virtual void evict() = 0;
};


class MmmEntry {
public:
  size_t size;
  MemoryMapWorker* obj;
};


class MemoryMapManager {
  std::vector<MmmEntry> entries;  // 0th entry always remains empty.
  size_t count;  // Number of items currently in the `entries` array.
                 // The entries are at indices 1 .. count

public:
  static MemoryMapManager* get();
  void add_entry(MemoryMapWorker* obj, size_t size);
  void del_entry(size_t i);
  void freeup_memory();

private:
  static const size_t n_entries_to_purge = 128;
  MemoryMapManager(size_t nelems);
  void sort_entries();
};


#endif
