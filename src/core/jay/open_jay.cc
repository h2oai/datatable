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
#include <string>
#include <cstring>              // std::memcmp
#include "frame/py_frame.h"
#include "jay/jay_generated.h"
#include "datatable.h"
#include "datatablemodule.h"
#include "stype.h"


// Helper functions
static Column column_from_jay(size_t nrows,
                              const jay::Column* jaycol,
                              const Buffer& jaybuf);

static void check_jay_signature(const uint8_t* ptr, size_t size);


//------------------------------------------------------------------------------
// Open DataTable
//------------------------------------------------------------------------------

DataTable* open_jay_from_file(const std::string& path) {
  Buffer mbuf = Buffer::mmap(path);
  return open_jay_from_mbuf(mbuf);
}

DataTable* open_jay_from_bytes(const char* ptr, size_t len) {
  // The buffer's lifetime is tied to the lifetime of the bytes object, which
  // could be very short. This is why we have to copy the buffer (even storing
  // a reference will be insufficient: what if the bytes object gets modified?)
  Buffer mbuf = Buffer::mem(len);
  if (len) std::memcpy(mbuf.xptr(), ptr, len);
  return open_jay_from_mbuf(mbuf);
}


DataTable* open_jay_from_mbuf(const Buffer& mbuf)
{
  std::vector<std::string> colnames;

  const uint8_t* ptr = static_cast<const uint8_t*>(mbuf.rptr());
  const size_t len = mbuf.size();
  check_jay_signature(ptr, len);

  size_t meta_size = *reinterpret_cast<const size_t*>(ptr + len - 16);
  if (meta_size > len - 24) {
    throw IOError() << "Invalid Jay file: meta information is expected to be "
        << meta_size << " bytes, however file size is only " << len;
  }

  auto meta_ptr = ptr + len - 16 - meta_size;
  auto frame = jay::GetFrame(meta_ptr);
  flatbuffers::Verifier verifier(meta_ptr, meta_size);
  if (!frame->Verify(verifier)) {
    throw IOError() << "Invalid meta record in a Jay file";
  }

  size_t ncols = frame->ncols();
  size_t nrows = frame->nrows();
  auto msg_columns = frame->columns();

  colvec columns;
  columns.reserve(ncols);
  size_t i = 0;
  for (const jay::Column* jcol : *msg_columns) {
    Column col = column_from_jay(nrows, jcol, mbuf);
    if (col.nrows() != nrows) {
      throw IOError() << "Length of column " << i << " is " << col.nrows()
          << ", however the Frame contains " << nrows << " rows";
    }
    columns.push_back(std::move(col));
    colnames.push_back(jcol->name()->str());
    ++i;
  }

  auto dt = new DataTable(std::move(columns), colnames);
  dt->set_nkeys_unsafe(static_cast<size_t>(frame->nkeys()));
  return dt;
}


static void check_jay_signature(const uint8_t* sof, size_t size) {
  const uint8_t* eof = sof + size;
  if (size < 24) {
    throw IOError() << "Invalid Jay file of size " << size;
  }

  if (std::memcmp(sof, "JAY", 3) != 0) {
    // Note: non-printable chars will be properly escaped by `operator<<`
    throw IOError() << "Invalid signature for a Jay file: first 4 bytes are `"
        << static_cast<char>(sof[0])
        << static_cast<char>(sof[1])
        << static_cast<char>(sof[2])
        << static_cast<char>(sof[3]) << "`";
  }
  if (std::memcmp(eof - 3, "JAY", 3) != 0 &&
      std::memcmp(eof - 4, "JAY1", 4) != 0)
  {
    throw IOError() << "Invalid signature for a Jay file: last 4 bytes are `"
        << static_cast<char>(eof[-4])
        << static_cast<char>(eof[-3])
        << static_cast<char>(eof[-2])
        << static_cast<char>(eof[-1]) << "`";
  }

  if (std::memcmp(sof, "JAY1\0\0\0\0", 8) != 0) {
    std::string version(reinterpret_cast<const char*>(sof) + 3, 5);
    throw IOError() << "Unsupported Jay file version: " << version;
  }
}




//------------------------------------------------------------------------------
// Open an individual column
//------------------------------------------------------------------------------

static Buffer extract_buffer(
    const Buffer& src, const jay::Buffer* jbuf)
{
  size_t offset = jbuf->offset();
  size_t length = jbuf->length();
  return Buffer::view(src, length, offset + 8);
}


template <typename T, typename JStats>
static void initStats(Stats* stats, const jay::Column* jcol) {
  auto jstats = static_cast<const JStats*>(jcol->stats());
  if (jstats) {
    using R = typename std::conditional<std::is_integral<T>::value,
                                        int64_t, double>::type;
    stats->set_nacount(static_cast<size_t>(jcol->nullcount()));
    T min = jstats->min();
    T max = jstats->max();
    stats->set_min(static_cast<R>(min), (min != dt::GETNA<T>()));
    stats->set_max(static_cast<R>(max), (max != dt::GETNA<T>()));
  }
}


static Column column_from_jay(
    size_t nrows, const jay::Column* jcol, const Buffer& jaybuf)
{
  jay::Type jtype = jcol->type();

  auto stype = dt::SType::VOID;
  switch (jtype) {
    case jay::Type_Bool8:   stype = dt::SType::BOOL; break;
    case jay::Type_Int8:    stype = dt::SType::INT8; break;
    case jay::Type_Int16:   stype = dt::SType::INT16; break;
    case jay::Type_Int32:   stype = dt::SType::INT32; break;
    case jay::Type_Int64:   stype = dt::SType::INT64; break;
    case jay::Type_Float32: stype = dt::SType::FLOAT32; break;
    case jay::Type_Float64: stype = dt::SType::FLOAT64; break;
    case jay::Type_Str32:   stype = dt::SType::STR32; break;
    case jay::Type_Str64:   stype = dt::SType::STR64; break;
  }

  Column col;
  Buffer databuf = extract_buffer(jaybuf, jcol->data());
  if (stype == dt::SType::STR32 || stype == dt::SType::STR64) {
    Buffer strbuf = extract_buffer(jaybuf, jcol->strdata());
    col = Column::new_string_column(nrows, std::move(databuf), std::move(strbuf));
  } else {
    col = Column::new_mbuf_column(nrows, stype, std::move(databuf));
  }

  Stats* stats = col.stats();
  switch (jtype) {
    case jay::Type_Bool8:   initStats<int8_t,  jay::StatsBool>(stats, jcol); break;
    case jay::Type_Int8:    initStats<int8_t,  jay::StatsInt8>(stats, jcol); break;
    case jay::Type_Int16:   initStats<int16_t, jay::StatsInt16>(stats, jcol); break;
    case jay::Type_Int32:   initStats<int32_t, jay::StatsInt32>(stats, jcol); break;
    case jay::Type_Int64:   initStats<int64_t, jay::StatsInt64>(stats, jcol); break;
    case jay::Type_Float32: initStats<float,   jay::StatsFloat32>(stats, jcol); break;
    case jay::Type_Float64: initStats<double,  jay::StatsFloat64>(stats, jcol); break;
    default: break;
  }

  return col;
}



//------------------------------------------------------------------------------
// Python open_jay()
//------------------------------------------------------------------------------
namespace py {

static PKArgs args_open_jay(
  1, 0, 0, false, false, {"file"}, "open_jay",
  "open_jay(file)\n--\n\n"
  "Open a Frame from the provided .jay file.\n");


static oobj open_jay(const PKArgs& args)
{
  if (args[0].is_bytes()) {
    // TODO: create & use class obytes
    PyObject* arg1 = args[0].to_borrowed_ref();
    const char* data = PyBytes_AS_STRING(arg1);
    size_t length = static_cast<size_t>(PyBytes_GET_SIZE(arg1));
    auto dt = open_jay_from_bytes(data, length);
    return Frame::oframe(dt);
  }
  else if (args[0].is_string()) {
    std::string filename = args[0].to_string();
    auto dt = open_jay_from_file(filename);
    auto res = Frame::oframe(dt);
    res.to_pyframe()->set_source(filename);
    return res;
  }
  else {
    throw TypeError() << "Invalid type of the argument to open_jay()";
  }
}


void DatatableModule::init_methods_jay() {
  ADD_FN(&open_jay, args_open_jay);
}

} // namespace py
