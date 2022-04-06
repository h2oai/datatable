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
#ifndef dt_UTILS_TEMPORARY_FILE_h
#define dt_UTILS_TEMPORARY_FILE_h
#include "_dt.h"


/**
  * This class represents a temporary file on disk. The file is
  * created when an object of this class is instantiated, and deleted
  * when the object is destroyed.
  *
  * The user should not attempt to keep the file open while allowing
  * this object to be destroyed: doing so may lead to crashes on some
  * OSes (in particular, Windows).
  *
  * The name of the temporary file will be generated automatically;
  * the file will be created in the directory either specified by the
  * user, or retrieved from Python `tempfile` module.
  */
class TemporaryFile
{
  private:
    std::string filename_;
    Buffer* bufferptr_;
    WritableBuffer* writebufptr_;

  public:
    TemporaryFile(const std::string& tempdir_in = "");
    ~TemporaryFile();

    const std::string& name() const;

    // Open the underlying file for writing and return the
    // corresponding WritableBuffer object. This method may be
    // called multiple times, returning the same object.
    WritableBuffer* data_w();

    // Open the file for reading, returning the pointer to the file's
    // data. If the file was opened for writing previously, its
    // WritableBuffer object will be finalized and closed.
    const void* data_r();
    Buffer buffer_r();

  private:
    void init_read_buffer();
    void close_read_buffer() noexcept;
    void init_write_buffer();
    void close_write_buffer() noexcept;
};



#endif
