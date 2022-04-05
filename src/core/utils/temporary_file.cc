//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include <cstdio>                   // std::remove, std::perror
#include <cstdlib>                  // std::rand
#include <ctime>
#include "buffer.h"
#include "parallel/api.h"
#include "python/obj.h"
#include "utils/temporary_file.h"
#include "utils/macros.h"
#include "writebuf.h"


static std::string get_temp_dir() {
  dt::PythonLock lock;
  auto gettempdirfn = py::oobj::import("tempfile", "gettempdir");
  auto tempdir = gettempdirfn.call();
  return tempdir.to_string();
}

// Create a temporary file name such that:
//   - the file is in the `tempdir` directory
//   - the file didn't exist previously
//   - the file is physically created before this function returns
//   - if the file cannot be created for any reason (for example, the
//     provided temporary directory does not exist, or is readonly),
//     then an IOError will be thrown.
//
static std::string get_temp_file(const std::string& tempdir) {
  #if DT_OS_WINDOWS
    constexpr char pathsep = '\\';
  #else
    constexpr char pathsep = '/';
  #endif
  constexpr size_t NAMELEN = 50;
  std::srand(static_cast<unsigned int>(std::time(nullptr)));
  static const char* LETTERS = "abcdefghijklmnopqrstuvwxyz0123456789";

  while (true) {
    std::string filename(NAMELEN, '\0');
    for (size_t i = 0; i < NAMELEN; ++i) {
      filename[i] = LETTERS[std::rand() % 36];
    }
    std::string fullname = tempdir + pathsep + filename;
    std::FILE* fp = std::fopen(fullname.c_str(), "r");
    if (fp) {  // file already exists, try another one
      std::fclose(fp);
    }
    else {     // file doesn't exist, try creating it
      fp = std::fopen(fullname.c_str(), "w");
      if (fp) {
        std::fclose(fp);
        return fullname;
      }
      else {
        throw IOError() << "Cannot create temporary file " << fullname << Errno;
      }
    }
  }
}


//------------------------------------------------------------------------------
// TemporaryFile
//------------------------------------------------------------------------------

TemporaryFile::TemporaryFile(const std::string& tempdir_in /* = "" */)
  : bufferptr_(nullptr),
    writebufptr_(nullptr)
{
  auto tempdir = (tempdir_in == "")? get_temp_dir()
                                   : tempdir_in;
  filename_ = get_temp_file(tempdir);
}


TemporaryFile::~TemporaryFile() {
  // Buffer must be deleted before removing the file, on some OSes
  // it may not be possible to remove a file while it is memory-
  // mapped.
  if (writebufptr_) close_write_buffer();
  if (bufferptr_) close_read_buffer();

  int ret = std::remove(filename_.c_str());
  if (ret) {
    std::string msg = "Cannot remove temporary file " + filename_;
    std::perror(msg.c_str());
  }
}


const std::string& TemporaryFile::name() const {
  return filename_;
}


WritableBuffer* TemporaryFile::data_w() {
  init_write_buffer();
  return writebufptr_;
}

const void* TemporaryFile::data_r() {
  init_read_buffer();
  return bufferptr_->rptr();
}

Buffer TemporaryFile::buffer_r() {
  init_read_buffer();
  return *bufferptr_;
}


void TemporaryFile::init_read_buffer() {
  if (bufferptr_) return;
  if (writebufptr_) close_write_buffer();
  bufferptr_ = new Buffer(Buffer::mmap(filename_));
}

void TemporaryFile::close_read_buffer() noexcept {
  delete bufferptr_;
  bufferptr_ = nullptr;
}


void TemporaryFile::init_write_buffer() {
  if (writebufptr_) return;
  xassert(!bufferptr_);
  writebufptr_ = new FileWritableBuffer(filename_);
}

void TemporaryFile::close_write_buffer() noexcept {
  try {
    writebufptr_->finalize();
  } catch (...) {}
  delete writebufptr_;
  writebufptr_ = nullptr;
}
