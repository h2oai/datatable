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
#ifndef dt__DT_h
#define dt__DT_h
#include <cstdint>
#include <string>
#include <vector>


class Buffer;
class Column;
class DataTable;
class Groupby;
class RowIndex;
class WritableBuffer;

struct CString;
enum class LType : uint8_t;
enum class SType : uint8_t;

using std::size_t;
using colvec = std::vector<Column>;
using strvec = std::vector<std::string>;
using sztvec = std::vector<size_t>;


namespace py {
  class Arg;
  class GSArgs;
  class PKArgs;
  class obool;
  class odict;
  class ofloat;
  class oint;
  class oiter;
  class olist;
  class onamedtuple;
  class oobj;
  class orange;
  class oset;
  class oslice;
  class ostring;
  class otuple;
  class rdict;
  class robj;
}


namespace flatbuffers {
  class FlatBufferBuilder;
  template <typename T> struct Offset;
}


namespace jay {
  struct Frame;
  struct Column;
  struct Buffer;
}


// dt::read support
namespace dt {
namespace read {
  enum PT : uint8_t;
  enum RT : uint8_t;
  enum BT : uint8_t;

}}


#endif
