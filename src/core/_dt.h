//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#ifndef dt__DT_h
#define dt__DT_h
#include <cstddef>      // std::size_t
#include <cstdint>      // uint8_t
#include <exception>    // std::exception_ptr
#include <memory>       // std::unique_ptr
#include <string>       // std::string
#include <type_traits>  //
#include <utility>      // std::move, std::pair
#include <vector>       // std::vector

class Buffer;
class Column;
class ColumnJayData;
class DataTable;
class Groupby;
class MemoryWritableBuffer;
class RowIndex;
class Stats;
class TemporaryFile;
class WritableBuffer;

namespace dt {
  class CString;
  enum class LType : uint8_t;
  enum class SType : uint8_t;

  struct ArrowArray;
  struct ArrowSchema;
  class OArrowArray;
  class OArrowSchema;
  class Type;
}

extern std::exception_ptr getbuffer_exception; // Declared in __getbuffer__.cc


using std::size_t;
using dtptr = std::unique_ptr<DataTable>;
using colvec = std::vector<Column>;
using strvec = std::vector<std::string>;
using sztvec = std::vector<size_t>;
using int32vec = std::vector<int32_t>;
using dblvec = std::vector<double>;
using RiGb = std::pair<RowIndex, Groupby>;


namespace py {
  class Arg;
  class Frame;
  class GSArgs;
  class PKArgs;
  class XArgs;
  class buffer;  // in pybuffer.h
  class obool;
  class oby;
  class odate;
  class odatetime;
  class odict;
  class ofloat;
  class oint;
  class oiter;
  class ojoin;
  class olist;
  class onamedtuple;
  class oobj;
  class orange;
  class oset;
  class oslice;
  class osort;
  class ostring;
  class otuple;
  class oupdate;
  class rdict;
  class robj;
  class rtuple;
  class _obj;
}


namespace flatbuffers {
  class FlatBufferBuilder;
  template <typename T> struct Offset;
}


namespace jay {
  struct Frame;
  struct Column;
  struct Buffer;
  struct ColumnBuilder;
}


namespace dt {
namespace read {
  enum PT : uint8_t;
  enum RT : uint8_t;

  struct ParseContext;
  union field64;

  class ChunkCoordinates;
  class GenericReader;
  class OutputColumn;
  class InputColumn;
  class PreFrame;
  class ThreadContext;

  void parse_string(const dt::read::ParseContext&);
}}
class FreadReader;


#endif
