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
  std::vector<MmmEntry> entries;
  size_t count;  // Number of items currently in the `entries` array.

  static const size_t n_entries_to_purge = 128;

  MemoryMapManager(size_t nelems);
public:
  static MemoryMapManager* get();

  void add_entry(MemoryMapWorker* obj, size_t size);
  void del_entry(size_t i);
  void freeup_memory();

private:
  void sort_entries();
};


extern MemoryMapManager mmapmanager;


#endif
