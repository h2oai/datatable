//------------------------------------------------------------------------------
// The code in this file is a slightly modified version of mman Windows library:
//   https://code.google.com/p/mman-win32/
// (MIT licensed)
//
// Copyright kutuzov.viktor.84@gmail.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//------------------------------------------------------------------------------
#include "lib/mman/mman.h"

#if DT_OS_WINDOWS

  #pragma warning(push)
  #pragma warning(disable : 4100)

  #include <windows.h>
  #include <errno.h>
  #include <io.h>

  #ifndef FILE_MAP_EXECUTE
  #define FILE_MAP_EXECUTE    0x0020
  #endif /* FILE_MAP_EXECUTE */
  static_assert(sizeof(DWORD) == 4, "DWORD should be 4-byte");

  static int __map_mman_error(const DWORD err, const int deferr)
  {
      if (err == 0)
          return 0;
      //TODO: implement
      return err;
  }

  static DWORD __map_mmap_prot_page(const int prot, const int flags = 0)
  {
      DWORD protect = 0;

      if (prot == PROT_NONE)
          return protect;

      bool is_private = (flags & MAP_PRIVATE) == MAP_PRIVATE;
      if ((prot & PROT_EXEC) != 0)
      {
          DWORD page_execute_write = is_private? PAGE_EXECUTE_WRITECOPY :
                                                 PAGE_EXECUTE_READWRITE;

          protect = ((prot & PROT_WRITE) != 0) ? page_execute_write :
                                                 PAGE_EXECUTE_READ;
      }
      else
      {
          DWORD page_write = is_private? PAGE_WRITECOPY :
                                         PAGE_READWRITE;

          protect = ((prot & PROT_WRITE) != 0) ? page_write :
                                                 PAGE_READONLY;
      }

      return protect;
  }

  static DWORD __map_mmap_prot_file(const int prot, const int flags = 0)
  {
      DWORD desiredAccess = 0;
      bool is_private = (flags & MAP_PRIVATE) == MAP_PRIVATE;
      DWORD file_map_write = is_private? FILE_MAP_COPY :
                                         FILE_MAP_WRITE;


      if (prot == PROT_NONE)
          return desiredAccess;

      if ((prot & PROT_READ) != 0)
          desiredAccess |= FILE_MAP_READ;
      if ((prot & PROT_WRITE) != 0)
          desiredAccess |= file_map_write;
      if ((prot & PROT_EXEC) != 0)
          desiredAccess |= FILE_MAP_EXECUTE;

      return desiredAccess;
  }

  void* mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
  {
      HANDLE fm, h;

      void * map = MAP_FAILED;

  #ifdef _MSC_VER
  #pragma warning(push)
  #pragma warning(disable: 4293)
  #endif

      const DWORD dwFileOffsetLow = (sizeof(off_t) <= sizeof(DWORD)) ?
                      (DWORD)off : (DWORD)(off & 0xFFFFFFFFL);
      const DWORD dwFileOffsetHigh = (sizeof(off_t) <= sizeof(DWORD)) ?
                      (DWORD)0 : (DWORD)((off >> 32) & 0xFFFFFFFFL);
      const DWORD protect = __map_mmap_prot_page(prot, flags);
      const DWORD desiredAccess = __map_mmap_prot_file(prot, flags);

      const size_t maxSize = (size_t)off + len;
      const DWORD dwMaxSizeLow = (DWORD)(maxSize & 0xFFFFFFFFL);
      const DWORD dwMaxSizeHigh = (DWORD)((maxSize >> 32) & 0xFFFFFFFFL);

  #ifdef _MSC_VER
  #pragma warning(pop)
  #endif

      errno = 0;

      if (len == 0
          /* Unsupported flag combinations */
          || (flags & MAP_FIXED) != 0
          /* Usupported protection combinations */
          || prot == PROT_EXEC)
      {
          errno = EINVAL;
          return MAP_FAILED;
      }

      h = ((flags & MAP_ANONYMOUS) == 0) ?
                      (HANDLE)_get_osfhandle(fildes) : INVALID_HANDLE_VALUE;

      if ((flags & MAP_ANONYMOUS) == 0 && h == INVALID_HANDLE_VALUE)
      {
          errno = EBADF;
          return MAP_FAILED;
      }

      fm = CreateFileMapping(h, NULL, protect, dwMaxSizeHigh, dwMaxSizeLow, NULL);

      if (fm == NULL)
      {
          errno = __map_mman_error(GetLastError(), EPERM);
          return MAP_FAILED;
      }

      map = MapViewOfFile(fm, desiredAccess, dwFileOffsetHigh, dwFileOffsetLow, len);

      CloseHandle(fm);

      if (map == NULL)
      {
          errno = __map_mman_error(GetLastError(), EPERM);
          return MAP_FAILED;
      }

      return map;
  }

  int munmap(void *addr, size_t len)
  {
      if (UnmapViewOfFile(addr))
          return 0;

      errno =  __map_mman_error(GetLastError(), EPERM);

      return -1;
  }

  int mprotect(void *addr, size_t len, int prot)
  {
      DWORD newProtect = __map_mmap_prot_page(prot);
      DWORD oldProtect = 0;

      if (VirtualProtect(addr, len, newProtect, &oldProtect))
          return 0;

      errno =  __map_mman_error(GetLastError(), EPERM);

      return -1;
  }

  int msync(void *addr, size_t len, int flags)
  {
      if (FlushViewOfFile(addr, len))
          return 0;

      errno =  __map_mman_error(GetLastError(), EPERM);

      return -1;
  }

  int mlock(const void *addr, size_t len)
  {
      if (VirtualLock((LPVOID)addr, len))
          return 0;

      errno =  __map_mman_error(GetLastError(), EPERM);

      return -1;
  }

  int munlock(const void *addr, size_t len)
  {
      if (VirtualUnlock((LPVOID)addr, len))
          return 0;

      errno =  __map_mman_error(GetLastError(), EPERM);

      return -1;
  }

  #pragma warning(pop)

#endif
