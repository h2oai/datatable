//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_MMM_h
#define dt_MMM_h
#include <vector>
using std::size_t;


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

  MmmEntry() : size(0), obj(nullptr) {}
  MmmEntry(MmmEntry&&) = default;
  MmmEntry& operator=(MmmEntry&&) = default;
  MmmEntry(size_t s , MemoryMapWorker* o) : size(s), obj(o) {}
  ~MmmEntry() { size = 0; obj = nullptr; }
  bool operator<(const MmmEntry& rhs) const { return size > rhs.size; }
};


class MemoryMapManager {
  std::vector<MmmEntry> entries;  // 0th entry always remains empty.

public:
  static MemoryMapManager* get();
  void add_entry(MemoryMapWorker* obj, size_t size);
  void del_entry(size_t i);
  void freeup_memory();
  bool check_entry(size_t i, const MemoryMapWorker* obj);

private:
  static const size_t n_entries_to_purge = 128;
  MemoryMapManager(size_t nelems);
  void sort_entries();
};


#endif
