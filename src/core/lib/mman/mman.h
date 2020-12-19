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
//
// This library implements a wrapper for mmap functions around
// the memory mapping Windows API. The code below is a modified
// version of the original:
// - added an operating system check, so that this code only builds on Windows;
// - added MAP_NORESERVE;
// - added MAP_PRIVATE handling to page and file protection routines;
// - modified `dwMaxSizeLow` and `dwMaxSizeHigh` calculation;
// - minor changes to code format.
//
//------------------------------------------------------------------------------
#include "utils/macros.h"


#if DT_OS_WINDOWS

	#ifndef _SYS_MMAN_H_
	#define _SYS_MMAN_H_

	#ifndef _WIN32_WINNT		    // Allow use of features specific to Windows XP or later.
	#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
	#endif

	/* All the headers include this file. */
	#ifndef _MSC_VER
	#include <_mingw.h>
	#endif

	#include <sys/types.h>

	#ifdef __cplusplus
	extern "C" {
	#endif

	#define PROT_NONE       0
	#define PROT_READ       1
	#define PROT_WRITE      2
	#define PROT_EXEC       4

	#define MAP_FILE        0
	#define MAP_SHARED      1
	#define MAP_PRIVATE     2
	#define MAP_TYPE        0xf
	#define MAP_FIXED       0x10
	#define MAP_ANONYMOUS   0x20
	#define MAP_NORESERVE   0x00
	#define MAP_ANON        MAP_ANONYMOUS

	#define MAP_FAILED      ((void *)-1)

	/* Flags for msync. */
	#define MS_ASYNC        1
	#define MS_SYNC         2
	#define MS_INVALIDATE   4

	void*   mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
	int     munmap(void *addr, size_t len);
	int     mprotect(void *addr, size_t len, int prot);
	int     msync(void *addr, size_t len, int flags);
	int     mlock(const void *addr, size_t len);
	int     munlock(const void *addr, size_t len);

	#ifdef __cplusplus
	};
	#endif

	#endif /*  _SYS_MMAN_H_ */

#endif
