//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <string>
#include <cstring>              // std::memcmp
#include "jay/jay_generated.h"
#include "datatable.h"


// Helper functions
static Column* column_from_jay(const jay::Column* jaycol, MemoryRange& jaybuf);



//------------------------------------------------------------------------------
// Open DataTable
//------------------------------------------------------------------------------

DataTable* DataTable::open_jay(const std::string& path,
                               std::vector<std::string>& colnames)
{
  xassert(colnames.empty());
  MemoryRange mbuf = MemoryRange::mmap(path);

  const uint8_t* ptr = static_cast<const uint8_t*>(mbuf.rptr());
  const size_t len = mbuf.size();
  if (len < 24) {
    throw IOError() << "Invalid Jay file of size " << len;
  }
  if (std::memcmp(ptr, "JAY1\0\0\0\0", 8) != 0 ||
      std::memcmp(ptr + len - 8, "\0\0\0\0" "1JAY", 8) *
      std::memcmp(ptr + len - 8, "\0\0\0\0" "JAY1", 8))
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

  Column** columns = dt::malloc<Column*>(sizeof(Column*) * size_t(ncols + 1));
  columns[ncols] = nullptr;
  size_t i = 0;
  for (const jay::Column* jcol : *msg_columns) {
    Column* col = column_from_jay(jcol, mbuf);
    if (col->nrows != static_cast<int64_t>(nrows)) {
      throw IOError() << "Length of column " << i << " is " << col->nrows
          << ", however the Frame contains " << nrows << " rows";
    }
    columns[i++] = col;
    colnames.push_back(jcol->name()->str());
  }

  auto dt = new DataTable(columns);
  dt->nkeys = frame->nkeys();
  return dt;
}



//------------------------------------------------------------------------------
// Open an individual column
//------------------------------------------------------------------------------

static MemoryRange extract_buffer(MemoryRange& src, const jay::Buffer* jbuf) {
  size_t offset = jbuf->offset();
  size_t length = jbuf->length();
  return MemoryRange::view(src, length, offset + 8);
}


template <typename T, typename A, typename JStats>
static void initStats(Stats* stats, const jay::Column* jcol) {
  auto tstats = static_cast<NumericalStats<T, A>*>(stats);
  auto jstats = static_cast<const JStats*>(jcol->stats());
  if (jstats) {
    tstats->set_countna(static_cast<int64_t>(jcol->nullcount()));
    tstats->set_min(jstats->min());
    tstats->set_max(jstats->max());
  }
}


static Column* column_from_jay(const jay::Column* jcol, MemoryRange& jaybuf) {
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
    case jay::Type_Bool8:   initStats<int8_t,  int64_t, jay::StatsBool>(stats, jcol); break;
    case jay::Type_Int8:    initStats<int8_t,  int64_t, jay::StatsInt8>(stats, jcol); break;
    case jay::Type_Int16:   initStats<int16_t, int64_t, jay::StatsInt16>(stats, jcol); break;
    case jay::Type_Int32:   initStats<int32_t, int64_t, jay::StatsInt32>(stats, jcol); break;
    case jay::Type_Int64:   initStats<int64_t, int64_t, jay::StatsInt64>(stats, jcol); break;
    case jay::Type_Float32: initStats<float,   double,  jay::StatsFloat32>(stats, jcol); break;
    case jay::Type_Float64: initStats<double,  double,  jay::StatsFloat64>(stats, jcol); break;
    default: break;
  }

  return col;
}
