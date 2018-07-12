//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "jay/jay_generated.h"
#include "datatable.h"
#include "utils/assert.h"
#include "writebuf.h"

void init_jay();  // called once from datatablemodule.c

using WritableBufferPtr = std::unique_ptr<WritableBuffer>;
static jay::Type stype_to_jaytype[DT_STYPES_COUNT];
static flatbuffers::Offset<jay::Column> column_to_jay(
    Column* col, const std::string& name, flatbuffers::FlatBufferBuilder& fbb,
    WritableBufferPtr& wb);
static jay::Buffer saveMemoryRange(const MemoryRange*, WritableBufferPtr&);
template <typename T, typename A, typename StatBuilder>
static flatbuffers::Offset<void> saveStats(
    Stats* stats, flatbuffers::FlatBufferBuilder& fbb);


//------------------------------------------------------------------------------
// Save DataTable
//------------------------------------------------------------------------------

void DataTable::save_jay(const std::string& path,
                         const std::vector<std::string>& colnames,
                         WritableBuffer::Strategy wstrategy)
{
  // Cannot store a view frame, so materialize first.
  reify();

  size_t sizehint = (wstrategy == WritableBuffer::Strategy::Auto)
                    ? memory_footprint() : 0;
  auto wb = WritableBuffer::create_target(path, sizehint, wstrategy);
  wb->write(8, "JAY1\0\0\0\0");

  flatbuffers::FlatBufferBuilder fbb(1024);

  std::vector<flatbuffers::Offset<jay::Column>> msg_columns;
  for (size_t i = 0; i < static_cast<size_t>(ncols); ++i) {
    Column* col = columns[i];
    if (col->stype() == SType::OBJ) {
      Warning() << "Column '" << colnames[i] << "' of type obj64 was not saved";
    } else {
      auto saved_col = column_to_jay(col, colnames[i], fbb, wb);
      msg_columns.push_back(saved_col);
    }
  }
  xassert((wb->size() & 7) == 0);

  auto frame = jay::CreateFrameDirect(fbb,
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



//------------------------------------------------------------------------------
// Save a column
//------------------------------------------------------------------------------

static flatbuffers::Offset<jay::Column> column_to_jay(
    Column* col, const std::string& name, flatbuffers::FlatBufferBuilder& fbb,
    WritableBufferPtr& wb)
{
  jay::Stats jsttype = jay::Stats_NONE;
  flatbuffers::Offset<void> jsto;
  Stats* colstats = col->get_stats_if_exist();
  switch (col->stype()) {
    case SType::BOOL:
      jsto = saveStats<int8_t,  int64_t, jay::StatsBool>(colstats, fbb);
      jsttype = jay::Stats_Bool;
      break;
    case SType::INT8:
      jsto = saveStats<int8_t,  int64_t, jay::StatsInt8>(colstats, fbb);
      jsttype = jay::Stats_Int8;
      break;
    case SType::INT16:
      jsto = saveStats<int16_t, int64_t, jay::StatsInt16>(colstats, fbb);
      jsttype = jay::Stats_Int16;
      break;
    case SType::INT32:
      jsto = saveStats<int32_t, int64_t, jay::StatsInt32>(colstats, fbb);
      jsttype = jay::Stats_Int32;
      break;
    case SType::INT64:
      jsto = saveStats<int64_t, int64_t, jay::StatsInt64>(colstats, fbb);
      jsttype = jay::Stats_Int64;
      break;
    case SType::FLOAT32:
      jsto = saveStats<float,   double,  jay::StatsFloat32>(colstats, fbb);
      jsttype = jay::Stats_Float32;
      break;
    case SType::FLOAT64:
      jsto = saveStats<double,  double,  jay::StatsFloat64>(colstats, fbb);
      jsttype = jay::Stats_Float64;
      break;
    default: break;
  }

  auto sname = fbb.CreateString(name.c_str());

  jay::ColumnBuilder cbb(fbb);
  cbb.add_type(stype_to_jaytype[static_cast<int>(col->stype())]);
  cbb.add_name(sname);
  cbb.add_nullcount(static_cast<uint64_t>(col->countna()));

  MemoryRange mbuf = col->data_buf();  // shallow copt of col's `mbuf`
  jay::Buffer saved_mbuf = saveMemoryRange(&mbuf, wb);
  cbb.add_data(&saved_mbuf);
  if (jsttype != jay::Stats_NONE) {
    cbb.add_stats_type(jsttype);
    cbb.add_stats(jsto);
  }

  if (col->stype() == SType::STR32) {
    auto scol = static_cast<StringColumn<uint32_t>*>(col);
    MemoryRange sbuf = scol->str_buf();
    jay::Buffer saved_strbuf = saveMemoryRange(&sbuf, wb);
    cbb.add_strdata(&saved_strbuf);
  }
  if (col->stype() == SType::STR64) {
    auto scol = static_cast<StringColumn<uint64_t>*>(col);
    MemoryRange sbuf = scol->str_buf();
    jay::Buffer saved_strbuf = saveMemoryRange(&sbuf, wb);
    cbb.add_strdata(&saved_strbuf);
  }

  return cbb.Finish();
}



//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

void init_jay() {
  stype_to_jaytype[int(SType::BOOL)]    = jay::Type_Bool8;
  stype_to_jaytype[int(SType::INT8)]    = jay::Type_Int8;
  stype_to_jaytype[int(SType::INT16)]   = jay::Type_Int16;
  stype_to_jaytype[int(SType::INT32)]   = jay::Type_Int32;
  stype_to_jaytype[int(SType::INT64)]   = jay::Type_Int64;
  stype_to_jaytype[int(SType::FLOAT32)] = jay::Type_Float32;
  stype_to_jaytype[int(SType::FLOAT64)] = jay::Type_Float64;
  stype_to_jaytype[int(SType::STR32)]   = jay::Type_Str32;
  stype_to_jaytype[int(SType::STR64)]   = jay::Type_Str64;
}


static jay::Buffer saveMemoryRange(
    const MemoryRange* mbuf, WritableBufferPtr& wb)
{
  if (!mbuf) return jay::Buffer();
  size_t len = mbuf->size();
  const void* data = mbuf->rptr();
  size_t pos = wb->prep_write(len, data);
  wb->write_at(pos, len, data);
  xassert(pos >= 8);
  if (len & 7) {  // Align the buffer to 8-byte boundary
    uint64_t zero = 0;
    wb->write(8 - (len & 7), &zero);
  }

  return jay::Buffer(pos - 8, len);
}


template <typename T, typename A, typename StatBuilder>
static flatbuffers::Offset<void> saveStats(
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
