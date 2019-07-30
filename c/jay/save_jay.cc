//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "frame/py_frame.h"
#include "jay/jay_generated.h"
#include "python/_all.h"
#include "python/args.h"
#include "python/string.h"
#include "utils/assert.h"
#include "datatable.h"
#include "writebuf.h"

using WritableBufferPtr = std::unique_ptr<WritableBuffer>;
static jay::Type stype_to_jaytype[DT_STYPES_COUNT];
static flatbuffers::Offset<jay::Column> column_to_jay(
    const OColumn& col, const std::string& name,
    flatbuffers::FlatBufferBuilder& fbb, WritableBuffer* wb);
static jay::Buffer saveMemoryRange(const MemoryRange*, WritableBuffer*);
template <typename T, typename StatBuilder>
static flatbuffers::Offset<void> saveStats(
    Stats* stats, flatbuffers::FlatBufferBuilder& fbb);


//------------------------------------------------------------------------------
// Save DataTable
//------------------------------------------------------------------------------

/**
 * Save Frame in Jay format to the provided file.
 */
void DataTable::save_jay(const std::string& path,
                         WritableBuffer::Strategy wstrategy)
{
  size_t sizehint = (wstrategy == WritableBuffer::Strategy::Auto)
                    ? memory_footprint() : 0;
  auto wb = WritableBuffer::create_target(path, sizehint, wstrategy);
  save_jay_impl(wb.get());
}


/**
 * Save Frame in Jay format to memory,
 */
MemoryRange DataTable::save_jay() {
  auto wb = std::unique_ptr<MemoryWritableBuffer>(
                new MemoryWritableBuffer(memory_footprint()));
  save_jay_impl(wb.get());
  return wb->get_mbuf();
}


void DataTable::save_jay_impl(WritableBuffer* wb) {
  // Cannot store a view frame, so materialize first.
  materialize();

  wb->write(8, "JAY1\0\0\0\0");

  flatbuffers::FlatBufferBuilder fbb(1024);

  std::vector<flatbuffers::Offset<jay::Column>> msg_columns;
  for (size_t i = 0; i < ncols; ++i) {
    const OColumn& col = get_ocolumn(i);
    if (col.stype() == SType::OBJ) {
      DatatableWarning() << "Column `" << names[i]
          << "` of type obj64 was not saved";
    } else {
      auto saved_col = column_to_jay(col, names[i], fbb, wb);
      msg_columns.push_back(saved_col);
    }
  }
  xassert((wb->size() & 7) == 0);

  auto frame = jay::CreateFrameDirect(fbb,
                  nrows,
                  msg_columns.size(),
                  static_cast<int>(nkeys),
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
  wb->write(8, "\0\0\0\0" "1JAY");
  wb->finalize();
}



//------------------------------------------------------------------------------
// Save a column
//------------------------------------------------------------------------------

static flatbuffers::Offset<jay::Column> column_to_jay(
    const OColumn& col, const std::string& name, flatbuffers::FlatBufferBuilder& fbb,
    WritableBuffer* wb)
{
  jay::Stats jsttype = jay::Stats_NONE;
  flatbuffers::Offset<void> jsto;
  Stats* colstats = col->get_stats_if_exist();
  switch (col.stype()) {
    case SType::BOOL:
      jsto = saveStats<int8_t,  jay::StatsBool>(colstats, fbb);
      jsttype = jay::Stats_Bool;
      break;
    case SType::INT8:
      jsto = saveStats<int8_t,  jay::StatsInt8>(colstats, fbb);
      jsttype = jay::Stats_Int8;
      break;
    case SType::INT16:
      jsto = saveStats<int16_t, jay::StatsInt16>(colstats, fbb);
      jsttype = jay::Stats_Int16;
      break;
    case SType::INT32:
      jsto = saveStats<int32_t, jay::StatsInt32>(colstats, fbb);
      jsttype = jay::Stats_Int32;
      break;
    case SType::INT64:
      jsto = saveStats<int64_t, jay::StatsInt64>(colstats, fbb);
      jsttype = jay::Stats_Int64;
      break;
    case SType::FLOAT32:
      jsto = saveStats<float,   jay::StatsFloat32>(colstats, fbb);
      jsttype = jay::Stats_Float32;
      break;
    case SType::FLOAT64:
      jsto = saveStats<double,  jay::StatsFloat64>(colstats, fbb);
      jsttype = jay::Stats_Float64;
      break;
    default: break;
  }

  auto sname = fbb.CreateString(name.c_str());

  jay::ColumnBuilder cbb(fbb);
  cbb.add_type(stype_to_jaytype[static_cast<int>(col.stype())]);
  cbb.add_name(sname);
  cbb.add_nullcount(col.na_count());

  MemoryRange mbuf = col->data_buf();  // shallow copt of col's `mbuf`
  jay::Buffer saved_mbuf = saveMemoryRange(&mbuf, wb);
  cbb.add_data(&saved_mbuf);
  if (jsttype != jay::Stats_NONE) {
    cbb.add_stats_type(jsttype);
    cbb.add_stats(jsto);
  }

  if (col.stype() == SType::STR32) {
    auto scol = static_cast<const StringColumn<uint32_t>*>(col.get());
    MemoryRange sbuf = scol->str_buf();
    jay::Buffer saved_strbuf = saveMemoryRange(&sbuf, wb);
    cbb.add_strdata(&saved_strbuf);
  }
  if (col.stype() == SType::STR64) {
    auto scol = static_cast<const StringColumn<uint64_t>*>(col.get());
    MemoryRange sbuf = scol->str_buf();
    jay::Buffer saved_strbuf = saveMemoryRange(&sbuf, wb);
    cbb.add_strdata(&saved_strbuf);
  }

  return cbb.Finish();
}



//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

static jay::Buffer saveMemoryRange(
    const MemoryRange* mbuf, WritableBuffer* wb)
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


template <typename T, typename StatBuilder>
static flatbuffers::Offset<void> saveStats(
    Stats* stats, flatbuffers::FlatBufferBuilder& fbb)
{
  static_assert(std::is_constructible<StatBuilder, T, T>::value,
                "Invalid StatBuilder class");
  if (!stats ||
      !(stats->is_computed(Stat::Min) && stats->is_computed(Stat::Max)))
    return 0;
  auto nstat = static_cast<NumericalStats<T>*>(stats);
  StatBuilder ss(nstat->min(nullptr), nstat->max(nullptr));
  flatbuffers::Offset<void> o = fbb.CreateStruct(ss).Union();
  return o;
}



//------------------------------------------------------------------------------
// py::Frame interface
//------------------------------------------------------------------------------
namespace py {


static PKArgs args_to_jay(
  1, 0, 1, false, false, {"path", "_strategy"}, "to_jay",

R"(to_jay(self, path, _strategy='auto')
--

Save this frame to a binary file on disk, in .jay format.

Parameters
----------
path: str
    The destination file name. Although not necessary, we recommend
    using extension ".jay" for the file. If the file exists, it will
    be overwritten.
    If this argument is omitted, the file will be created in memory
    instead, and returned as a `bytes` object.

_strategy: 'mmap' | 'write' | 'auto'
    Which method to use for writing the file to disk. The "write"
    method is more portable across different operating systems, but
    may be slower. This parameter has no effect when `path` is
    omitted.
)");


oobj Frame::to_jay(const PKArgs& args) {
  // path
  oobj path = args[0].to<oobj>(ostring(""));
  if (!path.is_string()) {
    throw TypeError() << "Parameter `path` in Frame.to_jay() should be a "
        "string, instead got " << path.typeobj();
  }
  path = oobj::import("os", "path", "expanduser").call({path});
  std::string filename = path.to_string();

  // _strategy
  auto strategy = args[1].to<std::string>("auto");
  auto sstrategy = (strategy == "mmap")  ? WritableBuffer::Strategy::Mmap :
                   (strategy == "write") ? WritableBuffer::Strategy::Write :
                   (strategy == "auto")  ? WritableBuffer::Strategy::Auto :
                                           WritableBuffer::Strategy::Unknown;
  if (sstrategy == WritableBuffer::Strategy::Unknown) {
    throw TypeError() << "Parameter `_strategy` in Frame.to_jay() should be "
        "one of 'mmap', 'write' or 'auto'; instead got '" << strategy << "'";
  }

  if (filename.empty()) {
    MemoryRange mr = dt->save_jay();
    auto data = static_cast<const char*>(mr.xptr());
    auto size = static_cast<Py_ssize_t>(mr.size());
    return oobj::from_new_reference(PyBytes_FromStringAndSize(data, size));
  }
  else {
    dt->save_jay(filename, sstrategy);
    return None();
  }
}



void Frame::_init_jay(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::to_jay, args_to_jay));

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


} // namespace py
