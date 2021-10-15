//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include "column/arrow_array.h"
#include "column/arrow_str.h"
#include "datatable.h"
#include "datatablemodule.h"
#include "frame/py_frame.h"
#include "jay/jay_nowarnings.h"
#include "python/xargs.h"
#include "stype.h"


// Helper functions
static Column column_from_jay1(size_t nrows,
                               const jay::Column* jaycol,
                               const Buffer& jaybuf);
static Column column_from_jay2(const jay::Column* jaycol,
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
  // The default value for _max_tables is 1000000, which prevents us
  // from reading frames with 1M columns. See issue #2876.
  flatbuffers::Verifier verifier(
      meta_ptr,
      meta_size,
      64,            // _max_depth (default)
      1 + (1<<30));  // _max_tables
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
    Column col = jcol->type() ? column_from_jay2(jcol, mbuf)
                              : column_from_jay1(nrows, jcol, mbuf);
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


static Column column_from_jay1(
    size_t nrows, const jay::Column* jcol, const Buffer& jaybuf)
{
  xassert(jcol->type() == nullptr);
  xassert(jcol->nrows() == 0);
  xassert(jcol->buffers() == nullptr);
  xassert(jcol->children() == nullptr);
  jay::SType jtype = jcol->stype();

  auto stype = dt::SType::INVALID;
  switch (jtype) {
    case jay::SType_Bool8:   stype = dt::SType::BOOL; break;
    case jay::SType_Int8:    stype = dt::SType::INT8; break;
    case jay::SType_Int16:   stype = dt::SType::INT16; break;
    case jay::SType_Int32:   stype = dt::SType::INT32; break;
    case jay::SType_Int64:   stype = dt::SType::INT64; break;
    case jay::SType_Float32: stype = dt::SType::FLOAT32; break;
    case jay::SType_Float64: stype = dt::SType::FLOAT64; break;
    case jay::SType_Str32:   stype = dt::SType::STR32; break;
    case jay::SType_Str64:   stype = dt::SType::STR64; break;
    case jay::SType_Date32:  stype = dt::SType::DATE32; break;
    case jay::SType_Time64:  stype = dt::SType::TIME64; break;
    case jay::SType_Void0:   stype = dt::SType::VOID; break;
    default: throw NotImplError() << "Unknown column type " << jtype << " in a Jay file";
  }

  Column col;
  if (stype == dt::SType::VOID) {
    col = Column::new_na_column(nrows, stype);
  }
  else {
    Buffer databuf = extract_buffer(jaybuf, jcol->data());
    if (stype == dt::SType::STR32 || stype == dt::SType::STR64) {
      Buffer strbuf = extract_buffer(jaybuf, jcol->strdata());
      col = Column::new_string_column(nrows, std::move(databuf), std::move(strbuf));
    } else {
      col = Column::new_mbuf_column(nrows, stype, std::move(databuf));
    }
  }

  Stats* stats = col.stats();
  switch (jtype) {
    case jay::SType_Bool8:   initStats<int8_t,  jay::StatsBool>(stats, jcol); break;
    case jay::SType_Int8:    initStats<int8_t,  jay::StatsInt8>(stats, jcol); break;
    case jay::SType_Int16:   initStats<int16_t, jay::StatsInt16>(stats, jcol); break;
    case jay::SType_Date32:
    case jay::SType_Int32:   initStats<int32_t, jay::StatsInt32>(stats, jcol); break;
    case jay::SType_Time64:
    case jay::SType_Int64:   initStats<int64_t, jay::StatsInt64>(stats, jcol); break;
    case jay::SType_Float32: initStats<float,   jay::StatsFloat32>(stats, jcol); break;
    case jay::SType_Float64: initStats<double,  jay::StatsFloat64>(stats, jcol); break;
    default: break;
  }

  return col;
}


static dt::Type _resolve_jtype(const jay::Type* jtype) {
  switch (jtype->stype()) {
    case jay::SType_Bool8:   return dt::Type::bool8();
    case jay::SType_Int8:    return dt::Type::int8();
    case jay::SType_Int16:   return dt::Type::int16();
    case jay::SType_Int32:   return dt::Type::int32();
    case jay::SType_Int64:   return dt::Type::int64();
    case jay::SType_Float32: return dt::Type::float32();
    case jay::SType_Float64: return dt::Type::float64();
    case jay::SType_Str32:   return dt::Type::str32();
    case jay::SType_Str64:   return dt::Type::str64();
    case jay::SType_Date32:  return dt::Type::date32();
    case jay::SType_Time64:  return dt::Type::time64();
    case jay::SType_Void0:   return dt::Type::void0();
    case jay::SType_Arr32:   return dt::Type::arr32(_resolve_jtype(jtype->extra_as_child()));
    case jay::SType_Arr64:   return dt::Type::arr64(_resolve_jtype(jtype->extra_as_child()));
  }
  throw NotImplError() << "Unknown column type " << jtype->stype() << " in a Jay file";
}


static Column column_from_jay2(const jay::Column* jcol, const Buffer& jaybuf) {
  xassert(jcol->data() == nullptr);
  xassert(jcol->strdata() == nullptr);

  size_t nrows = jcol->nrows();
  size_t nullcount = jcol->nullcount();
  dt::Type coltype = _resolve_jtype(jcol->type());
  if (coltype.is_void()) {
    return Column::new_na_column(nrows, dt::SType::VOID);
  }
  std::vector<Buffer> buffers;
  auto jbuffers = jcol->buffers();
  if (jbuffers) {
    for (unsigned i = 0; i < jbuffers->size(); i++) {
      buffers.push_back(extract_buffer(jaybuf, (*jbuffers)[i]));
    }
  }
  auto jchildren = jcol->children();
  std::vector<Column> children;
  if (jchildren) {
    for (unsigned i = 0; i < jchildren->size(); i++) {
      children.push_back(column_from_jay2((*jchildren)[i], jaybuf));
    }
  }
  bool has_validity = (*jbuffers).size() > 0 && (*jbuffers)[0]->length() > 0;

  Column col;
  if (coltype.is_string()) {
    xassert(buffers.size() == 3);
    if (has_validity && coltype.stype() == dt::SType::STR32) {
      col = Column(new dt::ArrowStr_ColumnImpl<uint32_t>(
        nrows, coltype.stype(), std::move(buffers[0]), std::move(buffers[1]), std::move(buffers[2])
      ));
    }
    else if (has_validity && coltype.stype() == dt::SType::STR64) {
      col = Column(new dt::ArrowStr_ColumnImpl<uint32_t>(
        nrows, coltype.stype(), std::move(buffers[0]), std::move(buffers[1]), std::move(buffers[2])
      ));
    }
    else {
      col = Column::new_string_column(nrows, std::move(buffers[1]), std::move(buffers[2]));
    }
  }
  else if (coltype.is_array()) {
    xassert(buffers.size() == 2);
    xassert(children.size() == 1);
    if (coltype.stype() == dt::SType::ARR32) {
      col = Column(new dt::ArrowArray_ColumnImpl<uint32_t>(
                nrows, nullcount,
                std::move(buffers[0]), std::move(buffers[1]), std::move(children[0])
            ));
    } else {
      col = Column(new dt::ArrowArray_ColumnImpl<uint64_t>(
                nrows, nullcount,
                std::move(buffers[0]), std::move(buffers[1]), std::move(children[0])
            ));
    }
  }
  else {
    xassert(!has_validity);
    xassert(buffers.size() == 2);
    xassert(children.size() == 0);
    col = Column::new_mbuf_column(nrows, coltype.stype(), std::move(buffers[1]));
  }

  switch (coltype.stype()) {
    case dt::SType::BOOL:    initStats<int8_t,  jay::StatsBool>(col.stats(), jcol); break;
    case dt::SType::INT8:    initStats<int8_t,  jay::StatsInt8>(col.stats(), jcol); break;
    case dt::SType::INT16:   initStats<int16_t, jay::StatsInt16>(col.stats(), jcol); break;
    case dt::SType::DATE32:
    case dt::SType::INT32:   initStats<int32_t, jay::StatsInt32>(col.stats(), jcol); break;
    case dt::SType::TIME64:
    case dt::SType::INT64:   initStats<int64_t, jay::StatsInt64>(col.stats(), jcol); break;
    case dt::SType::FLOAT32: initStats<float,   jay::StatsFloat32>(col.stats(), jcol); break;
    case dt::SType::FLOAT64: initStats<double,  jay::StatsFloat64>(col.stats(), jcol); break;
    default: break;
  }
  return col;
}




//------------------------------------------------------------------------------
// Python open_jay()
//------------------------------------------------------------------------------
namespace py {

static oobj open_jay(const XArgs& args)
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

DECLARE_PYFN(&open_jay)
    ->name("open_jay")
    ->n_positional_args(1)
    ->n_required_args(1)
    ->arg_names({"file"})
    ->docs("open_jay(file)\n--\n\n"
           "Open a Frame from the provided .jay file.\n");




} // namespace py
