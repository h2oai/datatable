//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <cerrno>              // errno
#include <cstdlib>             // std::realloc, std::free
#include <unordered_map>
#include "utils/alloc.h"
#include "utils/exceptions.h"  // MemoryError
#include "datatablemodule.h"
#include "mmm.h"               // MemoryMapManager

namespace dt
{


void* _realloc(void* ptr, size_t n) {
  if (n == 0) {
    dt::free(ptr);
    return nullptr;
  }
  int attempts = 3;
  while (true) {
    // The documentation for `void* realloc(void* ptr, size_t new_size);` says
    // the following:
    // | If there is not enough memory, the old memory block is not freed and
    // | null pointer is returned.
    // | If new_size is zero, the behavior is implementation defined (null
    // | pointer may be returned (in which case the old memory block may or may
    // | not be freed), or some non-null pointer may be returned that may not be
    // | used to access storage). Support for zero size is deprecated as of
    // | C11 DR 400.
    void* newptr = std::realloc(ptr, n);
    if (newptr) {
      // tracking/untracking should be done atomically in multi-threaded mode,
      // together with std::realloc(). Otherwise, it is possible to get a
      // situation where one thread reallocs freeing up an address, and then
      // another thread picks up that address emitting an error that the address
      // is in use (because the first thread hadn't had the chance to untrack it
      // yet).
      return newptr;
    }
    if (errno == 12 && attempts--) {
      // Occasionally, `std::realloc()` may fail if the system ran out of
      // memmap resources. It's unclear why, but freeing up some of memmap
      // handles sometime allows `std::realloc()` to succeed.
      MemoryMapManager::get()->freeup_memory();
      errno = 0;
    } else {
      throw MemoryError()
        << "Unable to allocate memory of size " << n << Errno;
    }
  }
}


void free(void* ptr) {
  if (!ptr) return;
  std::free(ptr);
  // UNTRACK(ptr);
}


}; // namespace dt
