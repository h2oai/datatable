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
#include "column/column_impl.h"
#include "column/rbound.h"
#include "column/sentinel.h"
#include "column/virtual.h"
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
static jay::Buffer saveMemoryRange(const void*, size_t, WritableBuffer*);
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
Buffer DataTable::save_jay() {
  auto wb = std::unique_ptr<MemoryWritableBuffer>(
                new MemoryWritableBuffer(memory_footprint()));
  save_jay_impl(wb.get());
  return wb->get_mbuf();
}


void DataTable::save_jay_impl(WritableBuffer* wb) {
  wb->write(8, "JAY1\0\0\0\0");

  flatbuffers::FlatBufferBuilder fbb(1024);

  std::vector<flatbuffers::Offset<jay::Column>> msg_columns;
  for (size_t i = 0; i < ncols_; ++i) {
    Column& col = get_column(i);
    if (col.stype() == SType::OBJ) {
      auto w = DatatableWarning();
      w << "Column `" << names_[i] << "` of type obj64 was not saved";
      w.emit();
    } else {
      auto saved_col = col.write_to_jay(names_[i], fbb, wb);
      msg_columns.push_back(saved_col);
    }
  }
  xassert((wb->size() & 7) == 0);

  auto frame = jay::CreateFrameDirect(fbb,
                  nrows_,
                  msg_columns.size(),
                  static_cast<int>(nkeys_),
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

flatbuffers::Offset<jay::Column> Column::write_to_jay(
        const std::string& name,
        flatbuffers::FlatBufferBuilder& fbb,
        WritableBuffer* wb)
{
  jay::Stats jsttype = jay::Stats_NONE;
  flatbuffers::Offset<void> jsto;
  Stats* colstats = get_stats_if_exist();
  switch (stype()) {
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
  cbb.add_type(stype_to_jaytype[static_cast<int>(stype())]);
  cbb.add_name(sname);
  cbb.add_nullcount(na_count());
  write_data_to_jay(cbb, wb);

  if (jsttype != jay::Stats_NONE) {
    cbb.add_stats_type(jsttype);
    cbb.add_stats(jsto);
  }

  return cbb.Finish();
}


void Column::write_data_to_jay(jay::ColumnBuilder& cbb, WritableBuffer* wb) {
  impl_->write_data_to_jay(*this, cbb, wb);
}


void dt::Sentinel_ColumnImpl::write_data_to_jay(
    Column&, jay::ColumnBuilder& cbb, WritableBuffer* wb) const
{
  size_t nbufs = get_num_data_buffers();
  for (size_t i = 0; i < nbufs; ++i) {
    const void* data = get_data_readonly(i);
    size_t size = get_data_size(i);
    jay::Buffer saved_buf = saveMemoryRange(data, size, wb);
    if (i == 0) cbb.add_data(&saved_buf);
    if (i == 1) cbb.add_strdata(&saved_buf);
  }
}


void dt::Virtual_ColumnImpl::write_data_to_jay(
    Column& thiscol, jay::ColumnBuilder& cbb, WritableBuffer* wb) const
{
  thiscol.materialize(false);
  thiscol.write_data_to_jay(cbb, wb);
}


void dt::Rbound_ColumnImpl::write_data_to_jay(
    Column&, jay::ColumnBuilder& cbb, WritableBuffer* wb) const
{
  for (const Column& col : chunks_) {
    const_cast<Column*>(&col)->materialize();
  }
  size_t nbufs = chunks_[0].get_num_data_buffers();
  for (size_t i = 0; i < nbufs; ++i) {
    size_t pos0 = 0;
    size_t size0 = 0;
    for (const Column& col : chunks_) {
      const void* data = col.get_data_readonly(i);
      size_t size = col.get_data_size(i);
      size_t pos = wb->write(size, data);
      xassert(pos >= 8);
      if (!pos0) pos0 = pos;
      size0 += size;
    }
    if (size0 & 7) {
      size_t zero = 0;
      wb->write(8 - (size0 & 7), &zero);
    }
    jay::Buffer saved_buf(pos0 - 8, size0);
    if (i == 0) cbb.add_data(&saved_buf);
    if (i == 1) cbb.add_strdata(&saved_buf);
  }
}




//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

static jay::Buffer saveMemoryRange(
    const void* data, size_t len, WritableBuffer* wb)
{
  size_t pos = wb->prepare_write(len, data);
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
  static_assert(std::is_integral<T>::value || std::is_floating_point<T>::value,
                "Only integer / floating values are supporteds");
  using R = typename std::conditional<std::is_integral<T>::value, int64_t, double>::type;
  if (!(stats && stats->is_computed(Stat::Min) && stats->is_computed(Stat::Max)))
    return 0;
  R min, max;
  bool min_valid = stats->get_stat(Stat::Min, &min);
  bool max_valid = stats->get_stat(Stat::Max, &max);
  StatBuilder ss(min_valid? static_cast<T>(min) : GETNA<T>(),
                 max_valid? static_cast<T>(max) : GETNA<T>());
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
    Buffer mr = dt->save_jay();
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
