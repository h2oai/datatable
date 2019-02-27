//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <string>
#include <cstring>              // std::memcmp
#include "frame/py_frame.h"
#include "jay/jay_generated.h"
#include "datatable.h"
#include "datatablemodule.h"


// Helper functions
static Column* column_from_jay(const jay::Column* jaycol,
                               const MemoryRange& jaybuf);



//------------------------------------------------------------------------------
// Open DataTable
//------------------------------------------------------------------------------

DataTable* open_jay_from_file(const std::string& path) {
  MemoryRange mbuf = MemoryRange::mmap(path);
  return open_jay_from_mbuf(mbuf);
}

DataTable* open_jay_from_bytes(const char* ptr, size_t len) {
  // The buffer's lifetime is tied to the lifetime of the bytes object, which
  // could be very short. This is why we have to copy the buffer (even storing
  // a reference will be insufficient: what if the bytes object gets modified?)
  MemoryRange mbuf = MemoryRange::mem(len);
  std::memcpy(mbuf.xptr(), ptr, len);
  return open_jay_from_mbuf(mbuf);
}


DataTable* open_jay_from_mbuf(const MemoryRange& mbuf)
{
  std::vector<std::string> colnames;

  const uint8_t* ptr = static_cast<const uint8_t*>(mbuf.rptr());
  const size_t len = mbuf.size();
  if (len < 24) {
    throw IOError() << "Invalid Jay file of size " << len;
  }
  if (std::memcmp(ptr, "JAY1\0\0\0\0", 8) != 0 ||
      (std::memcmp(ptr + len - 8, "\0\0\0\0" "1JAY", 8) != 0 &&
       std::memcmp(ptr + len - 8, "\0\0\0\0" "JAY1", 8) != 0))
  {
    throw IOError() << "Invalid signature for a Jay file";
  }

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

  std::vector<Column*> columns;
  columns.reserve(ncols);
  size_t i = 0;
  for (const jay::Column* jcol : *msg_columns) {
    Column* col = column_from_jay(jcol, mbuf);
    if (col->nrows != nrows) {
      throw IOError() << "Length of column " << i << " is " << col->nrows
          << ", however the Frame contains " << nrows << " rows";
    }
    columns.push_back(col);
    colnames.push_back(jcol->name()->str());
    ++i;
  }

  auto dt = new DataTable(std::move(columns), colnames);
  dt->set_nkeys_unsafe(static_cast<size_t>(frame->nkeys()));
  return dt;
}



//------------------------------------------------------------------------------
// Open an individual column
//------------------------------------------------------------------------------

static MemoryRange extract_buffer(
    const MemoryRange& src, const jay::Buffer* jbuf)
{
  size_t offset = jbuf->offset();
  size_t length = jbuf->length();
  return MemoryRange::view(src, length, offset + 8);
}


template <typename T, typename JStats>
static void initStats(Stats* stats, const jay::Column* jcol) {
  auto tstats = static_cast<NumericalStats<T>*>(stats);
  auto jstats = static_cast<const JStats*>(jcol->stats());
  if (jstats) {
    tstats->set_countna(jcol->nullcount());
    tstats->set_min(jstats->min());
    tstats->set_max(jstats->max());
  }
}


static Column* column_from_jay(
    const jay::Column* jcol, const MemoryRange& jaybuf)
{
  jay::Type jtype = jcol->type();

  Column* col = nullptr;
  switch (jtype) {
    case jay::Type_Bool8:   col = new BoolColumn(0); break;
    case jay::Type_Int8:    col = new IntColumn<int8_t>(0); break;
    case jay::Type_Int16:   col = new IntColumn<int16_t>(0); break;
    case jay::Type_Int32:   col = new IntColumn<int32_t>(0); break;
    case jay::Type_Int64:   col = new IntColumn<int64_t>(0); break;
    case jay::Type_Float32: col = new RealColumn<float>(0); break;
    case jay::Type_Float64: col = new RealColumn<double>(0); break;
    case jay::Type_Str32:   col = new StringColumn<uint32_t>(0); break;
    case jay::Type_Str64:   col = new StringColumn<uint64_t>(0); break;
  }

  MemoryRange databuf = extract_buffer(jaybuf, jcol->data());
  if (jtype == jay::Type_Str32 || jtype == jay::Type_Str64) {
    MemoryRange strbuf = extract_buffer(jaybuf, jcol->strdata());
    col->replace_buffer(std::move(databuf), std::move(strbuf));
  } else {
    col->replace_buffer(std::move(databuf));
  }

  Stats* stats = col->get_stats();
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


static oobj open_jay(const PKArgs& args) {
  DataTable* dt = nullptr;
  if (args[0].is_bytes()) {
    // TODO: create & use class obytes
    PyObject* arg1 = args[0].to_borrowed_ref();
    const char* data = PyBytes_AS_STRING(arg1);
    size_t length = static_cast<size_t>(PyBytes_GET_SIZE(arg1));
    dt = open_jay_from_bytes(data, length);
  }
  else if (args[0].is_string()) {
    std::string filename = args[0].to_string();
    dt = open_jay_from_file(filename);
  }
  else {
    throw TypeError() << "Invalid type of the argument to open_jay()";
  }
  Frame* frame = Frame::from_datatable(dt);
  return oobj::from_new_reference(frame);
}


void DatatableModule::init_methods_jay() {
  ADD_FN(&open_jay, args_open_jay);
}

} // namespace py
