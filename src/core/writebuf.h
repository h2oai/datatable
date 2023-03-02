//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#ifndef dt_WRITEBUF_h
#define dt_WRITEBUF_h
#include <memory>                     // std::unique_ptr
#include <string>                     // std::string
#include "cstring.h"                  // dt::CString
#include "parallel/shared_mutex.h"
#include "utils/file.h"

class Buffer;


//==============================================================================

class WritableBuffer
{
  protected:
    size_t bytes_written_;

  public:
    enum class Strategy : int8_t { Unknown, Auto, Mmap, Write };

    WritableBuffer();
    virtual ~WritableBuffer();

    /**
     * Factory method for instantiating one of the `WritableBuffer` derived
     * classes based on the strategy and the current platform.
     *
     * @param path
     *     Name of the file to write to. If empty, then we will be writing into
     *     the memory, and MemoryWritableBuffer is returned.
     *
     * @param size
     *     Expected size of the data to be written. This doesn't have to be the
     *     exact amount, however having a good estimate may improve efficiency.
     *     This may also have effect on the choice of the writing strategy.
     *
     * @param strategy
     *     Which subclass of `WritableBuffer` to construct. This could be `Auto`
     *     (choose the best subclass automatically), while all other values
     *     force the choice of a particular subclass.
     *
     * @param append
     *     If true, the file will be opened in append mode ("a"); if false, the
     *     file will be opened in overwrite mode ("w").
     */
    static std::unique_ptr<WritableBuffer> create_target(
      const std::string& path, size_t size, Strategy strategy = Strategy::Auto,
      bool append = false
    );

    // Return the current size of the writable buffer -- i.e. the
    // number of bytes written into it. Note that this is different
    // from the current buffer's "capacity" (i.e. the size it was
    // pre-allocated for).
    //
    size_t size() const;

    /**
     * Prepare to write buffer `src` of size `n`. This method is expected to be
     * called by no more than one thread at a time (for example from the OMP
     * "ordered" section). The value returned by this method should be passed to
     * the subsequent `write_at()` call.
     *
     * Implementations are encouraged to perform as little work as possible within
     * this method, and instead defer most writing to the `write_at()` method.
     * However in case when this is not possible, an implementation may actually
     * write out the provided buffer `src`.
     */
    virtual size_t prepare_write(size_t n, const void* src) = 0;

    /**
     * Write buffer `src` of size `n` at the position `pos` (this position should
     * have previously been returned from `pre_write()`, which also ensured that
     * there is enough space to perform the writing).
     *
     * This call is safe to invoke from multiple threads simultaneously. It is
     * also allowed to call this method when another thread is performing
     * `prepare_write()`.
     */
    virtual void write_at(size_t pos, size_t n, const void* src) = 0;

    /**
     * This method should be called when you're done writing to the buffer. It is
     * distinct from the destructor in that it is not expected to free any
     * resources, but rather make the object read-only.
     */
    virtual void finalize() = 0;

    /**
      * Simple helper method for writing into the buffer in single-
      * threaded context. The return value is the position within the
      * buffer where the data was written.
      */
    size_t write(size_t n, const void* src) {
      size_t writepos = prepare_write(n, src);
      write_at(writepos, n, src);
      return writepos;
    }
    size_t write(const dt::CString& src) {
      size_t writepos = prepare_write(src.size(), src.data());
      write_at(writepos, src.size(), src.data());
      return writepos;
    }

    // Prevent copying / assignment for these objects
    WritableBuffer(const WritableBuffer&) = delete;
    WritableBuffer& operator=(const WritableBuffer&) = delete;
};



//==============================================================================

class FileWritableBuffer : public WritableBuffer
{
  private:
    File* file_;  // owned

  public:
    FileWritableBuffer(const std::string& path, bool append = false);
    virtual ~FileWritableBuffer() override;

    virtual size_t prepare_write(size_t n, const void* src) override;
    virtual void write_at(size_t pos, size_t n, const void* src) override;
    virtual void finalize() override;
};



//==============================================================================

class ThreadsafeWritableBuffer : public WritableBuffer
{
  protected:
    void*  data_;     // owned, managed by child classes
    size_t allocsize_;
    dt::shared_mutex shmutex_;

    virtual void realloc(size_t newsize) = 0;

  public:
    ThreadsafeWritableBuffer();

    virtual size_t prepare_write(size_t n, const void* src) override;
    virtual void write_at(size_t pos, size_t n, const void* src) override;
    virtual void finalize() override;
};



//==============================================================================



class MemoryWritableBuffer : public ThreadsafeWritableBuffer
{
  class Writer {
    MemoryWritableBuffer* mbuf_;
    size_t pos_start_;
    size_t pos_end_;

    public:
      Writer(MemoryWritableBuffer* parent, size_t start, size_t end);
      Writer(const Writer&) = delete;
      Writer(Writer&&);
      ~Writer();

      void write_at(size_t offset, const char* content, size_t len);
  };

  public:
    MemoryWritableBuffer(size_t size = 0);
    ~MemoryWritableBuffer() override;

    // Return memory buffer that was written. This method may only be
    // called after `finalize()`.
    Buffer get_mbuf();
    std::string get_string();

    // Pretend that the buffer is empty, so that any subsequent writes
    // will start from the beginning. This method does not actually
    // erase any data, nor does any reallocation.
    void clear();

    void* data() const;

    Writer writer(size_t start, size_t end);

  private:
    void realloc(size_t newsize) override;
};



//==============================================================================

class MmapWritableBuffer : public ThreadsafeWritableBuffer
{
  private:
    std::string filename_;

  public:
    MmapWritableBuffer(const std::string& path, size_t size, bool append = false);
    ~MmapWritableBuffer() override;

    void finalize() override;

  private:
    void realloc(size_t newsize) override;
    void map(int fd, size_t size);
    void unmap();
};




#endif
