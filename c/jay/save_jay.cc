//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <capnp/message.h>
#include <capnp/serialize.h>
#include "jay/jay.capnp.h"
#include "jay/jay_generated.h"
#include "datatable.h"
#include "utils/assert.h"
#include "writebuf.h"

using WritableBufferPtr = std::unique_ptr<WritableBuffer>;
static fbjay::Type stype_to_jaytype[DT_STYPES_COUNT];


static void saveMemoryRange(
    const MemoryRange& mbuf, WritableBufferPtr& wb, jay::Buffer::Builder buffer)
{
  size_t len = mbuf.size();
  const void* data = mbuf.rptr();
  size_t pos = wb->prep_write(len, data);
  wb->write_at(pos, len, data);
  xassert(pos >= 8);
  if (len & 7) {  // Align the buffer to 8-byte boundary
    uint64_t zero = 0;
    wb->write(8 - (len & 7), &zero);
  }

  buffer.setOffset(pos - 8);
  buffer.setLength(len);
}

static fbjay::Buffer saveMemoryRange(
    const MemoryRange* mbuf, WritableBufferPtr& wb)
{
  if (!mbuf) return fbjay::Buffer();
  size_t len = mbuf->size();
  const void* data = mbuf->rptr();
  size_t pos = wb->prep_write(len, data);
  wb->write_at(pos, len, data);
  xassert(pos >= 8);
  if (len & 7) {  // Align the buffer to 8-byte boundary
    uint64_t zero = 0;
    wb->write(8 - (len & 7), &zero);
  }

  return fbjay::Buffer(pos - 8, len);
}



template <typename T, typename A, typename TBuilder>
void saveMinMax(NumericalStats<T, A>* stats, TBuilder builder) {
  if (!stats) return;
  if (stats->is_computed(Stat::Min)) {
    builder.setMin(stats->min(nullptr));
  }
  if (stats->is_computed(Stat::Max)) {
    builder.setMax(stats->max(nullptr));
  }
}



template <typename T, typename A, typename StatBuilder>
flatbuffers::Offset<void> saveStats(
    Stats* stats, flatbuffers::FlatBufferBuilder& fbb)
{
  static_assert(std::is_constructible<StatBuilder, T, T>::value,
                "Invalid StatBuilder class");
  if (!stats ||
      !(stats->is_computed(Stat::Min) && stats->is_computed(Stat::Max)))
    return 0;
  auto nstat = static_cast<NumericalStats<T, A>*>(stats);
  StatBuilder ss(nstat->min(nullptr), nstat->max(nullptr));
  flatbuffers::Offset<void> o = fbb.CreateStruct(ss).Union();
  return o;
}


void DataTable::save_jay(const std::string& path,
                         const std::vector<std::string>& colnames)
{
  reify();
  int64_t n_obj_cols = 0;
  for (int64_t i = 0; i < ncols; ++i) {
    n_obj_cols += (columns[i]->stype() == ST_OBJECT_PYPTR);
  }

  auto wb = WritableBuffer::create_target(path, memory_footprint(),
                                          WritableBuffer::Strategy::Auto);
  wb->write(8, "JAY1\0\0\0\0");

  capnp::MallocMessageBuilder message;

  auto frame = message.initRoot<jay::Frame>();
  frame.setNrows(nrows);
  frame.setNcols(ncols - n_obj_cols);

  auto zcols = static_cast<unsigned int>(ncols - n_obj_cols);
  auto msg_columns = frame.initColumns(zcols);
  unsigned int j = 0;
  for (size_t i = 0; i < static_cast<size_t>(ncols); ++i) {
    if (columns[i]->stype() == ST_OBJECT_PYPTR) {
      Warning() << "Column '" << colnames[i] << "' of type obj64 was not saved";
    } else {
      columns[i]->save_jay(colnames[i], msg_columns[j++], wb);
    }
  }
  xassert((wb->size() & 7) == 0);

  auto meta = capnp::messageToFlatArray(message);
  auto metaBytes = meta.asBytes();
  auto metaSize = static_cast<size_t>(metaBytes.end() - metaBytes.begin());
  wb->write(metaSize, metaBytes.begin());
  if (metaSize & 7) {
    wb->write(8 - (metaSize & 7), "\0\0\0\0\0\0\0");
    metaSize += 8 - (metaSize & 7);
  }

  wb->write(8, &metaSize);
  wb->write(8, "\0\0\0\0JAY1");
  wb->finalize();
}



void DataTable::save_jay_fb(const std::string& path,
                         const std::vector<std::string>& colnames)
{
  reify();

  auto wb = WritableBuffer::create_target(path, memory_footprint(),
                                          WritableBuffer::Strategy::Auto);
  wb->write(8, "JAY1\0\0\0\0");

  flatbuffers::FlatBufferBuilder fbb(1024);

  std::vector<flatbuffers::Offset<fbjay::Column>> msg_columns;
  for (size_t i = 0; i < static_cast<size_t>(ncols); ++i) {
    if (columns[i]->stype() == ST_OBJECT_PYPTR) {
      Warning() << "Column '" << colnames[i] << "' of type obj64 was not saved";
    } else {
      msg_columns.push_back(columns[i]->save_jay_fb(colnames[i], fbb, wb));
    }
  }
  xassert((wb->size() & 7) == 0);

  auto frame = fbjay::CreateFrameDirect(fbb,
                  static_cast<size_t>(nrows),
                  msg_columns.size(),
                  &msg_columns);
  fbb.Finish(frame);

  uint8_t* metaBytes = fbb.GetBufferPointer();
  size_t   metaSize = fbb.GetSize();
  wb->write(metaSize, metaBytes);
  if (metaSize & 7) {
    wb->write(8 - (metaSize & 7), "\0\0\0\0\0\0\0");
    metaSize += 8 - (metaSize & 7);
  }

  wb->write(8, &metaSize);
  wb->write(8, "\0\0\0\0JAY1");
  wb->finalize();
}



void Column::save_jay(
    const std::string& name, jay::Column::Builder msg_col,
    WritableBufferPtr& wb)
{
  using StatsBool8   = NumericalStats<int8_t, int64_t>;
  using StatsInt8    = NumericalStats<int8_t, int64_t>;
  using StatsInt16   = NumericalStats<int16_t, int64_t>;
  using StatsInt32   = NumericalStats<int32_t, int64_t>;
  using StatsInt64   = NumericalStats<int64_t, int64_t>;
  using StatsFloat32 = NumericalStats<float, double>;
  using StatsFloat64 = NumericalStats<double, double>;

  msg_col.setName(name);
  msg_col.setNullcount(static_cast<uint64_t>(countna()));
  auto msg_col_type = msg_col.initType();

  switch (stype()) {
    case ST_BOOLEAN_I1: {
      auto mctype = msg_col_type.initBool8();
      saveMemoryRange(mbuf, wb, mctype.initData());
      saveMinMax(dynamic_cast<StatsBool8*>(stats), mctype);
      break;
    }
    case ST_INTEGER_I1: {
      auto mctype = msg_col_type.initInt8();
      saveMemoryRange(mbuf, wb, mctype.initData());
      saveMinMax(dynamic_cast<StatsInt8*>(stats), mctype);
      break;
    }
    case ST_INTEGER_I2: {
      auto mctype = msg_col_type.initInt16();
      saveMemoryRange(mbuf, wb, mctype.initData());
      saveMinMax(dynamic_cast<StatsInt16*>(stats), mctype);
      break;
    }
    case ST_INTEGER_I4: {
      auto mctype = msg_col_type.initInt32();
      saveMemoryRange(mbuf, wb, mctype.initData());
      saveMinMax(dynamic_cast<StatsInt32*>(stats), mctype);
      break;
    }
    case ST_INTEGER_I8: {
      auto mctype = msg_col_type.initInt64();
      saveMemoryRange(mbuf, wb, mctype.initData());
      saveMinMax(dynamic_cast<StatsInt64*>(stats), mctype);
      break;
    }
    case ST_REAL_F4: {
      auto mctype = msg_col_type.initFloat32();
      saveMemoryRange(mbuf, wb, mctype.initData());
      saveMinMax(dynamic_cast<StatsFloat32*>(stats), mctype);
      break;
    }
    case ST_REAL_F8: {
      auto mctype = msg_col_type.initFloat64();
      saveMemoryRange(mbuf, wb, mctype.initData());
      saveMinMax(dynamic_cast<StatsFloat64*>(stats), mctype);
      break;
    }
    case ST_STRING_I4_VCHAR: {
      auto mctype = msg_col_type.initStr32();
      saveMemoryRange(mbuf, wb, mctype.initOffsets());
      saveMemoryRange(dynamic_cast<StringColumn<uint32_t>*>(this)->strbuf,
                      wb, mctype.initStrdata());
      break;
    }
    case ST_STRING_I8_VCHAR: {
      auto mctype = msg_col_type.initStr64();
      saveMemoryRange(mbuf, wb, mctype.initOffsets());
      saveMemoryRange(dynamic_cast<StringColumn<uint64_t>*>(this)->strbuf,
                      wb, mctype.initStrdata());
      break;
    }
    default: {
      throw NotImplError() << "Cannot save column of type " << stype();
    }
  }
}




flatbuffers::Offset<fbjay::Column>
Column::save_jay_fb(
    const std::string& name, flatbuffers::FlatBufferBuilder& fbb,
    WritableBufferPtr& wb)
{
  fbjay::Stats jsttype = fbjay::Stats_NONE;
  flatbuffers::Offset<void> jsto;
  switch (stype()) {
    case ST_BOOLEAN_I1: jsto = saveStats<int8_t,  int64_t, fbjay::StatsBool>(stats, fbb);    jsttype = fbjay::Stats_Bool; break;
    case ST_INTEGER_I1: jsto = saveStats<int8_t,  int64_t, fbjay::StatsInt8>(stats, fbb);    jsttype = fbjay::Stats_Int8; break;
    case ST_INTEGER_I2: jsto = saveStats<int16_t, int64_t, fbjay::StatsInt16>(stats, fbb);   jsttype = fbjay::Stats_Int16; break;
    case ST_INTEGER_I4: jsto = saveStats<int32_t, int64_t, fbjay::StatsInt32>(stats, fbb);   jsttype = fbjay::Stats_Int32; break;
    case ST_INTEGER_I8: jsto = saveStats<int64_t, int64_t, fbjay::StatsInt64>(stats, fbb);   jsttype = fbjay::Stats_Int64; break;
    case ST_REAL_F4:    jsto = saveStats<float,   double,  fbjay::StatsFloat32>(stats, fbb); jsttype = fbjay::Stats_Float32; break;
    case ST_REAL_F8:    jsto = saveStats<double,  double,  fbjay::StatsFloat64>(stats, fbb); jsttype = fbjay::Stats_Float64; break;
    default: break;
  }

  auto sname = fbb.CreateString(name.c_str());
  fbjay::ColumnBuilder cbb(fbb);
  cbb.add_type(stype_to_jaytype[stype()]);
  cbb.add_name(sname);
  cbb.add_nullcount(static_cast<uint64_t>(countna()));

  fbjay::Buffer saved_mbuf = saveMemoryRange(&mbuf, wb);
  cbb.add_data(&saved_mbuf);
  if (jsttype != fbjay::Stats_NONE) {
    cbb.add_stats_type(jsttype);
    cbb.add_stats(jsto);
  }

  if (stype() == ST_STRING_I4_VCHAR) {
    auto scol = static_cast<StringColumn<uint32_t>*>(this);
    fbjay::Buffer saved_strbuf = saveMemoryRange(&(scol->strbuf), wb);
    cbb.add_strdata(&saved_strbuf);
  }
  if (stype() == ST_STRING_I8_VCHAR) {
    auto scol = static_cast<StringColumn<uint64_t>*>(this);
    fbjay::Buffer saved_strbuf = saveMemoryRange(&(scol->strbuf), wb);
    cbb.add_strdata(&saved_strbuf);
  }

  return cbb.Finish();
}


/* Called once from datatablemodule.c */
void init_jay();  // prevent -Wmissing-prototypes warning
void init_jay() {
  stype_to_jaytype[ST_BOOLEAN_I1]      = fbjay::Type_Bool8;
  stype_to_jaytype[ST_INTEGER_I1]      = fbjay::Type_Int8;
  stype_to_jaytype[ST_INTEGER_I2]      = fbjay::Type_Int16;
  stype_to_jaytype[ST_INTEGER_I4]      = fbjay::Type_Int32;
  stype_to_jaytype[ST_INTEGER_I8]      = fbjay::Type_Int64;
  stype_to_jaytype[ST_REAL_F4]         = fbjay::Type_Float32;
  stype_to_jaytype[ST_REAL_F8]         = fbjay::Type_Float64;
  stype_to_jaytype[ST_STRING_I4_VCHAR] = fbjay::Type_Str32;
  stype_to_jaytype[ST_STRING_I8_VCHAR] = fbjay::Type_Str64;
}
