//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <string>
#include <cstring>              // std::memcmp
#include "jay/jay.capnp.h"
#include "datatable.h"




DataTable* DataTable::open_jay(const std::string& path,
                               std::vector<std::string>& colnames)
{
  xassert(colnames.empty());
  MemoryRange mbuf = MemoryRange::mmap(path);

  const char*  ptr = static_cast<const char*>(mbuf.rptr());
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

  auto meta_ptr = static_cast<const void*>(ptr + len - 16 - meta_size);
  auto meta_len_words = meta_size / sizeof(capnp::word);
  auto meta_arr = kj::ArrayPtr<const capnp::word>(
                      static_cast<const capnp::word*>(meta_ptr), meta_len_words);
  capnp::FlatArrayMessageReader reader(meta_arr);

  auto frame = reader.getRoot<jay::Frame>();
  int64_t ncols = frame.getNcols();
  int64_t nrows = frame.getNrows();
  Column** columns = dt::malloc<Column*>(sizeof(Column*) * size_t(ncols + 1));
  columns[ncols] = nullptr;
  int64_t i = 0;
  for (auto col : frame.getColumns()) {
    columns[i++] = Column::from_jay(col, nrows, mbuf);
    colnames.push_back(col.getName());
  }

  return new DataTable(columns);
}



static MemoryRange extract_buffer(MemoryRange& srcbuf,
                                  jay::Buffer::Reader coords)
{
  size_t offset = coords.getOffset();
  size_t length = coords.getLength();
  return MemoryRange::view(srcbuf, length, offset + 8);
}

template <typename T, typename A>
static void initStats(NumericalStats<T, A>* stats, size_t nacnt, T min, T max)
{
  stats->set_countna(static_cast<int64_t>(nacnt));
  stats->set_min(min);
  stats->set_max(max);
}


Column* Column::from_jay(jay::Column::Reader jaycol, int64_t nrows,
                         MemoryRange& jaybuf)
{
  using JayType = jay::Column::Type::Which;
  using StatsBool8   = NumericalStats<int8_t, int64_t>;
  using StatsInt8    = NumericalStats<int8_t, int64_t>;
  using StatsInt16   = NumericalStats<int16_t, int64_t>;
  using StatsInt32   = NumericalStats<int32_t, int64_t>;
  using StatsInt64   = NumericalStats<int64_t, int64_t>;
  using StatsFloat32 = NumericalStats<float, double>;
  using StatsFloat64 = NumericalStats<double, double>;

  size_t nacnt = jaycol.getNullcount();
  auto jtype = jaycol.getType();
  Column* res = nullptr;

  switch (jtype.which()) {
    case JayType::MU:
      throw RuntimeError() << "Invalid column type in Jay";

    case JayType::BOOL8: {
      auto jt = jtype.getBool8();
      MemoryRange mbuf = extract_buffer(jaybuf, jt.getData());
      res = new BoolColumn(nrows, std::move(mbuf));
      initStats(dynamic_cast<StatsBool8*>(res->get_stats()),
                nacnt, jt.getMin(), jt.getMax());
      break;
    }
    case JayType::INT8: {
      auto jt = jtype.getInt8();
      MemoryRange mbuf = extract_buffer(jaybuf, jt.getData());
      res = new IntColumn<int8_t>(nrows, std::move(mbuf));
      initStats(dynamic_cast<StatsInt8*>(res->get_stats()),
                nacnt, jt.getMin(), jt.getMax());
      break;
    }
    case JayType::INT16: {
      auto jt = jtype.getInt16();
      MemoryRange mbuf = extract_buffer(jaybuf, jt.getData());
      res = new IntColumn<int16_t>(nrows, std::move(mbuf));
      initStats(dynamic_cast<StatsInt16*>(res->get_stats()),
                nacnt, jt.getMin(), jt.getMax());
      break;
    }
    case JayType::INT32: {
      auto jt = jtype.getInt32();
      MemoryRange mbuf = extract_buffer(jaybuf, jt.getData());
      res = new IntColumn<int32_t>(nrows, std::move(mbuf));
      initStats(dynamic_cast<StatsInt32*>(res->get_stats()),
                nacnt, jt.getMin(), jt.getMax());
      break;
    }
    case JayType::INT64: {
      auto jt = jtype.getInt64();
      MemoryRange mbuf = extract_buffer(jaybuf, jt.getData());
      res = new IntColumn<int64_t>(nrows, std::move(mbuf));
      initStats(dynamic_cast<StatsInt64*>(res->get_stats()),
                nacnt, jt.getMin(), jt.getMax());
      break;
    }
    case JayType::FLOAT32: {
      auto jt = jtype.getFloat32();
      MemoryRange mbuf = extract_buffer(jaybuf, jt.getData());
      res = new RealColumn<float>(nrows, std::move(mbuf));
      initStats(dynamic_cast<StatsFloat32*>(res->get_stats()),
                nacnt, jt.getMin(), jt.getMax());
      break;
    }
    case JayType::FLOAT64: {
      auto jt = jtype.getFloat64();
      MemoryRange mbuf = extract_buffer(jaybuf, jt.getData());
      res = new RealColumn<double>(nrows, std::move(mbuf));
      initStats(dynamic_cast<StatsFloat64*>(res->get_stats()),
                nacnt, jt.getMin(), jt.getMax());
      break;
    }
    case JayType::STR32: {
      auto jt = jtype.getStr32();
      MemoryRange obuf = extract_buffer(jaybuf, jt.getOffsets());
      MemoryRange sbuf = extract_buffer(jaybuf, jt.getStrdata());
      res = new StringColumn<uint32_t>(nrows, std::move(obuf), std::move(sbuf));
      break;
    }
    case JayType::STR64: {
      auto jt = jtype.getStr64();
      MemoryRange obuf = extract_buffer(jaybuf, jt.getOffsets());
      MemoryRange sbuf = extract_buffer(jaybuf, jt.getStrdata());
      res = new StringColumn<uint64_t>(nrows, std::move(obuf), std::move(sbuf));
      break;
    }
  }
  return res;
}
