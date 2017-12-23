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
#ifndef dt_CSV_WORKER_H
#define dt_CSV_WORKER_H
#include <memory>       // std::unique_ptr
#include <string>       // std::string
#include <vector>       // std::vector
#include "memorybuf.h"  // MemoryBuffer
#include "writebuf.h"   // WritableBuffer


//------------------------------------------------------------------------------

class GReaderOutputColumn {
  public:
    std::string name;
    MemoryBuffer* data;
    int64_t valid_from_row;
    int8_t type;
    int64_t : 56;

  public:
    GReaderOutputColumn();
    virtual ~GReaderOutputColumn();
};


class GReaderOutputStringColumn : public GReaderOutputColumn {
  public:
    WritableBuffer* strdata;

  public:
    GReaderOutputStringColumn();
    virtual ~GReaderOutputStringColumn();
};



//------------------------------------------------------------------------------

class ThreadContext {
  public:
    void* wbuf;
    // std::vector<StrBuf2> strbufs;
    size_t rowsize;
    size_t wbuf_nrows;
    size_t used_nrows;
    size_t row0;
    int ithread;
    int : 32;

  public:
    ThreadContext(int ithread, size_t nrows, size_t ncols);
    virtual ~ThreadContext();
    virtual void* next_row();
    virtual void push_buffers() = 0;
    virtual const char* read_chunk(const char* start, const char* end) = 0;
    virtual void order(size_t r0) { row0 = r0; }
    virtual size_t get_nrows() { return used_nrows; }
    virtual void set_nrows(size_t n) { used_nrows = n; }
};

typedef std::unique_ptr<ThreadContext> ThreadContextPtr;



//------------------------------------------------------------------------------

class ChunkedDataReader
{
  private:
    // the data is read from here:
    const char* inputptr;
    size_t inputsize;
    int64_t inputline;

    // and saved into here, via the intermediate buffers TThreadContext, that
    // are instantiated within the read_all() method:
    std::vector<GReaderOutputColumn> cols;

    // Additional parameters
    size_t max_nrows;
    size_t alloc_nrows;

    // Runtime parameters:
    size_t chunksize;
    size_t nchunks;
    int nthreads;
    bool chunks_contiguous;
    int : 24;

public:
  ChunkedDataReader();
  virtual ~ChunkedDataReader();
  void set_input(const char* ptr, size_t size, int64_t line);

  virtual ThreadContextPtr init_thread_context() = 0;
  virtual void compute_chunking_strategy();
  virtual const char* adjust_chunk_start(const char* ch, const char* eof);
  void read_all();
};



#endif
