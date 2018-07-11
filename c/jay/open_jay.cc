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
  if (std::memcmp(ptr, "JAY1\0\0\0\0", 8) ||
      std::memcmp(ptr + len - 8, "\0\0\0\0JAY1", 8)) {
    throw IOError() << "Invalid signature for a Jay file";
  }

  size_t meta_size = *reinterpret_cast<const size_t*>(ptr + len - 16);
  if (meta_size > len - 24) {
    throw IOError() << "Invalid Jay file: meta information is expected to be "
        << meta_size << " bytes, however file size is only " << len;
  }

  auto meta_ptr = ptr + len - 16 - meta_size;
  auto frame = fbjay::GetFrame(meta_ptr);
  size_t ncols = frame->ncols();
  size_t nrows = frame->nrows();
  auto msg_columns = frame->columns();

  Column** columns = dt::malloc<Column*>(sizeof(Column*) * size_t(ncols + 1));
  columns[ncols] = nullptr;
  size_t i = 0;
  for (const fbjay::Column* jcol : *msg_columns) {
    Column* col = Column::from_jay(jcol, mbuf);
    if (col->nrows != static_cast<int64_t>(nrows)) {
      throw IOError() << "Length of column " << i << " is " << col->nrows
          << ", however the Frame contains " << nrows << " rows";
    }
    columns[i++] = col;
    colnames.push_back(jcol->name()->str());
  }

  return new DataTable(columns);
}


static MemoryRange extract_buffer(MemoryRange& srcbuf,
                                  const fbjay::Buffer* coords)
{
  size_t offset = coords->offset();
  size_t length = coords->length();
  return MemoryRange::view(srcbuf, length, offset + 8);
}

template <typename T, typename A, typename JStats>
static void initStats(Stats* stats, const fbjay::Column* jcol) {
  auto tstats = static_cast<NumericalStats<T, A>*>(stats);
  auto jstats = static_cast<const JStats*>(jcol->stats());
  if (jstats) {
    tstats->set_countna(static_cast<int64_t>(jcol->nullcount()));
    tstats->set_min(jstats->min());
    tstats->set_max(jstats->max());
  }
}


Column* Column::from_jay(const fbjay::Column* jaycol, MemoryRange& jaybuf) {
  fbjay::Type jtype = jaycol->type();

  Column* col = nullptr;
  switch (jtype) {
    case fbjay::Type_Bool8:   col = new BoolColumn(); break;
    case fbjay::Type_Int8:    col = new IntColumn<int8_t>(); break;
    case fbjay::Type_Int16:   col = new IntColumn<int16_t>(); break;
    case fbjay::Type_Int32:   col = new IntColumn<int32_t>(); break;
    case fbjay::Type_Int64:   col = new IntColumn<int64_t>(); break;
    case fbjay::Type_Float32: col = new RealColumn<float>(); break;
    case fbjay::Type_Float64: col = new RealColumn<double>(); break;
    case fbjay::Type_Str32:   col = new StringColumn<uint32_t>(); break;
    case fbjay::Type_Str64:   col = new StringColumn<uint64_t>(); break;
    // default: throw IOError() << "Uknown column type " << jtype;
  }

  MemoryRange databuf = extract_buffer(jaybuf, jaycol->data());
  if (jtype == fbjay::Type_Str32 || jtype == fbjay::Type_Str64) {
    MemoryRange strbuf = extract_buffer(jaybuf, jaycol->strdata());
    col->replace_buffer(std::move(databuf), std::move(strbuf));
  } else {
    col->replace_buffer(std::move(databuf));
  }

  Stats* stats = col->get_stats();
  switch (jtype) {
    case fbjay::Type_Bool8:   initStats<int8_t,  int64_t, fbjay::StatsBool>(stats, jaycol); break;
    case fbjay::Type_Int8:    initStats<int8_t,  int64_t, fbjay::StatsInt8>(stats, jaycol); break;
    case fbjay::Type_Int16:   initStats<int16_t, int64_t, fbjay::StatsInt16>(stats, jaycol); break;
    case fbjay::Type_Int32:   initStats<int32_t, int64_t, fbjay::StatsInt32>(stats, jaycol); break;
    case fbjay::Type_Int64:   initStats<int64_t, int64_t, fbjay::StatsInt64>(stats, jaycol); break;
    case fbjay::Type_Float32: initStats<float,   double,  fbjay::StatsFloat32>(stats, jaycol); break;
    case fbjay::Type_Float64: initStats<double,  double,  fbjay::StatsFloat64>(stats, jaycol); break;
    default: break;
  }

  return col;
}
