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
#include "column/column_impl.h"
#include "column/rbound.h"
#include "column/sentinel.h"
#include "column/virtual.h"
#include "documentation.h"
#include "frame/py_frame.h"
#include "jay/jay_nowarnings.h"
#include "parallel/api.h"
#include "python/_all.h"
#include "python/args.h"
#include "python/string.h"
#include "utils/assert.h"
#include "datatable.h"
#include "ltype.h"
#include "stype.h"
#include "writebuf.h"

using WritableBufferPtr = std::unique_ptr<WritableBuffer>;
static jay::SType stype_to_jaytype[dt::STYPES_COUNT];
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
    if (col.stype() == dt::SType::OBJ) {
      auto w = DatatableWarning();
      w << "Column `" << names_[i] << "` of type obj64 was not saved";
      w.emit_warning();
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
    case dt::SType::BOOL:
      jsto = saveStats<int8_t,  jay::StatsBool>(colstats, fbb);
      jsttype = jay::Stats_Bool;
      break;
    case dt::SType::INT8:
      jsto = saveStats<int8_t,  jay::StatsInt8>(colstats, fbb);
      jsttype = jay::Stats_Int8;
      break;
    case dt::SType::INT16:
      jsto = saveStats<int16_t, jay::StatsInt16>(colstats, fbb);
      jsttype = jay::Stats_Int16;
      break;
    case dt::SType::DATE32:
    case dt::SType::INT32:
      jsto = saveStats<int32_t, jay::StatsInt32>(colstats, fbb);
      jsttype = jay::Stats_Int32;
      break;
    case dt::SType::TIME64:
    case dt::SType::INT64:
      jsto = saveStats<int64_t, jay::StatsInt64>(colstats, fbb);
      jsttype = jay::Stats_Int64;
      break;
    case dt::SType::FLOAT32:
      jsto = saveStats<float,   jay::StatsFloat32>(colstats, fbb);
      jsttype = jay::Stats_Float32;
      break;
    case dt::SType::FLOAT64:
      jsto = saveStats<double,  jay::StatsFloat64>(colstats, fbb);
      jsttype = jay::Stats_Float64;
      break;
    default: break;
  }

  auto sname = fbb.CreateString(name.c_str());

  jay::ColumnBuilder cbb(fbb);
  cbb.add_stype(stype_to_jaytype[static_cast<int>(stype())]);
  cbb.add_name(sname);
  cbb.add_nullcount(na_count());
  write_data_to_jay(cbb, wb);

  if (!jsto.IsNull()) {
    cbb.add_stats_type(jsttype);
    cbb.add_stats(jsto);
  }

  return cbb.Finish();
}


void Column::write_data_to_jay(jay::ColumnBuilder& cbb, WritableBuffer* wb) {
  const_cast<dt::ColumnImpl*>(impl_)->write_data_to_jay(*this, cbb, wb);
}


void dt::Sentinel_ColumnImpl::write_data_to_jay(
    Column&, jay::ColumnBuilder& cbb, WritableBuffer* wb)
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
    Column& thiscol, jay::ColumnBuilder& cbb, WritableBuffer* wb)
{
  thiscol.materialize(false);
  thiscol.write_data_to_jay(cbb, wb);
}



void dt::Rbound_ColumnImpl::_write_fw_to_jay(
    jay::ColumnBuilder& cbb, WritableBuffer* wb)
{
  size_t pos0 = 0;
  size_t size_total = 0;
  for (const Column& col : chunks_) {
    xassert(col.stype() == stype());
    xassert(col.get_num_data_buffers() == 1);
    const void* data = col.get_data_readonly(0);
    size_t size = col.get_data_size(0);
    size_t pos = wb->write(size, data);
    if (pos0) {
      xassert(pos == pos0 + size_total);
    } else {
      pos0 = pos;
    }
    size_total += size;
  }
  if (size_total & 7) {
    size_t zero = 0;
    wb->write(8 - (size_total & 7), &zero);
  }
  xassert(pos0 >= 8);
  jay::Buffer saved_buf(pos0 - 8, size_total);
  cbb.add_data(&saved_buf);
}


template <typename TIN, typename TOUT>
static Buffer _recode_offsets(const void* data, size_t n, size_t offset0) {
  Buffer outbuf = Buffer::mem(n * sizeof(TOUT));
  auto offsets_in = static_cast<const TIN*>(data);
  auto offsets_out = static_cast<TOUT*>(outbuf.xptr());
  dt::parallel_for_static(n,
    [=](size_t i) {
      offsets_out[i] = static_cast<TOUT>(offsets_in[i]) + static_cast<TOUT>(offset0);
      if (!std::is_same<TIN, TOUT>::value && (offsets_in[i] & dt::GETNA<TIN>())) {
        offsets_out[i] += dt::GETNA<TOUT>() - dt::GETNA<TIN>();
      }
    });
  return outbuf;
}


void dt::Rbound_ColumnImpl::_write_str_offsets_to_jay(
    jay::ColumnBuilder& cbb, WritableBuffer* wb)
{
  // Check whether the column may end up being over the maximum
  // allowed size for the STR32 stype.
  if (stype() == dt::SType::STR32) {
    size_t strdata_size = 0;
    for (const Column& col : chunks_) {
      xassert(!col.is_virtual());
      strdata_size += col.get_data_size(1);
    }
    if (strdata_size > Column::MAX_ARR32_SIZE ||
        nrows_ > Column::MAX_ARR32_SIZE) {
      // [FIXME]
      // stype_ = SType::STR64;
      cbb.add_stype(stype_to_jaytype[static_cast<int>(SType::STR64)]);
    }
  }

  // Write initial zero
  size_t pos0 = 0;
  size_t elemsize0 = stype_elemsize(stype());
  pos0 = wb->write(elemsize0, &pos0);

  size_t offsets_size = elemsize0;  // total size of all offsets written so far
  size_t strdata_size = 0;          // total size of strdata chunks
  for (const Column& col : chunks_) {
    xassert(col.get_num_data_buffers() == 2);
    size_t elemsize = stype_elemsize(col.stype());
    auto data = static_cast<const char*>(col.get_data_readonly(0)) + elemsize;
    auto size = col.get_data_size(0) - elemsize;

    // If no strdata has been written yet, either because this is the first
    // chunk or because the first chunk was empty, then the offsets buffer
    // can be written into the output as-is without the need for additional
    // offsetting. However, this wouldn't apply in a case of a STR32->STR64
    // type bump.
    if (strdata_size == 0 && col.stype() == stype()) {
      wb->write(size, data);
      offsets_size += size;
    }
    // For all subsequent chunks we need to modify the offsets and
    // remove the initial 0 from the `data` array.
    else {
      xassert((col.stype() == stype()) ||
              (col.stype() == SType::STR32 && stype() == SType::STR64));
      size_t n = col.nrows();
      Buffer newbuf = (col.stype() == dt::SType::STR32)
          ? (stype() == dt::SType::STR32
              ? _recode_offsets<uint32_t, uint32_t>(data, n, strdata_size)
              : _recode_offsets<uint32_t, uint64_t>(data, n, strdata_size))
          : _recode_offsets<uint64_t, uint64_t>(data, n, strdata_size);
      wb->write(newbuf.size(), newbuf.rptr());
      offsets_size += newbuf.size();
    }
    strdata_size += col.get_data_size(1);
  }
  if (offsets_size & 7) {
    size_t zero = 0;
    wb->write(8 - (offsets_size & 7), &zero);
  }
  xassert(pos0 >= 8);
  jay::Buffer saved_buf(pos0 - 8, offsets_size);
  cbb.add_data(&saved_buf);
}


void dt::Rbound_ColumnImpl::_write_str_data_to_jay(
    jay::ColumnBuilder& cbb, WritableBuffer* wb)
{
  size_t pos0 = 0;
  size_t size_total = 0;
  for (const Column& col : chunks_) {
    xassert(col.get_num_data_buffers() == 2);
    const void* data = col.get_data_readonly(1);
    size_t size = col.get_data_size(1);
    size_t pos = wb->write(size, data);
    if (pos0) {
      xassert(pos == pos0 + size_total);
    } else {
      pos0 = pos;
    }
    size_total += size;
  }
  if (size_total & 7) {
    size_t zero = 0;
    wb->write(8 - (size_total & 7), &zero);
  }
  xassert(pos0 >= 8);
  jay::Buffer saved_buf(pos0 - 8, size_total);
  cbb.add_strdata(&saved_buf);
}



void dt::Rbound_ColumnImpl::write_data_to_jay(
    Column&, jay::ColumnBuilder& cbb, WritableBuffer* wb)
{
  for (Column& col : chunks_) {
    col.materialize();
  }

  if (stype() == dt::SType::STR32 || stype() == dt::SType::STR64) {
    _write_str_offsets_to_jay(cbb, wb);
    _write_str_data_to_jay(cbb, wb);
  } else {
    _write_fw_to_jay(cbb, wb);
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
  StatBuilder ss(min_valid? static_cast<T>(min) : dt::GETNA<T>(),
                 max_valid? static_cast<T>(max) : dt::GETNA<T>());
  flatbuffers::Offset<void> o = fbb.CreateStruct(ss).Union();
  return o;
}



//------------------------------------------------------------------------------
// py::Frame interface
//------------------------------------------------------------------------------
namespace py {

static PKArgs args_to_jay(
  1, 0, 1, false, false, {"path", "method"}, "to_jay", dt::doc_Frame_to_jay);


oobj Frame::to_jay(const PKArgs& args) {
  // path
  oobj path = args[0].to<oobj>(ostring(""));
  if (!path.is_string()) {
    throw TypeError() << "Parameter `path` in Frame.to_jay() should be a "
        "string, instead got " << path.typeobj();
  }
  path = oobj::import("os", "path", "expanduser").call({path});
  std::string filename = path.to_string();

  // method
  auto str_method = args[1].to<std::string>("auto");
  auto method = (str_method == "mmap") ? WritableBuffer::Strategy::Mmap :
                (str_method == "write")? WritableBuffer::Strategy::Write :
                (str_method == "auto") ? WritableBuffer::Strategy::Auto :
                                         WritableBuffer::Strategy::Unknown;
  if (method == WritableBuffer::Strategy::Unknown) {
    throw TypeError() << "Parameter `method` in Frame.to_jay() should be "
        "one of 'mmap', 'write' or 'auto'; instead got '" << str_method << "'";
  }

  if (filename.empty()) {
    Buffer mr = dt->save_jay();
    auto data = static_cast<const char*>(mr.xptr());
    auto size = static_cast<Py_ssize_t>(mr.size());
    return oobj::from_new_reference(PyBytes_FromStringAndSize(data, size));
  }
  else {
    dt->save_jay(filename, method);
    return None();
  }
}



void Frame::_init_jay(XTypeMaker& xt) {
  args_to_jay.add_synonym_arg("_strategy", "method");
  xt.add(METHOD(&Frame::to_jay, args_to_jay));

  stype_to_jaytype[int(dt::SType::BOOL)]    = jay::SType_Bool8;
  stype_to_jaytype[int(dt::SType::INT8)]    = jay::SType_Int8;
  stype_to_jaytype[int(dt::SType::INT16)]   = jay::SType_Int16;
  stype_to_jaytype[int(dt::SType::INT32)]   = jay::SType_Int32;
  stype_to_jaytype[int(dt::SType::INT64)]   = jay::SType_Int64;
  stype_to_jaytype[int(dt::SType::FLOAT32)] = jay::SType_Float32;
  stype_to_jaytype[int(dt::SType::FLOAT64)] = jay::SType_Float64;
  stype_to_jaytype[int(dt::SType::DATE32)]  = jay::SType_Date32;
  stype_to_jaytype[int(dt::SType::TIME64)]  = jay::SType_Time64;
  stype_to_jaytype[int(dt::SType::STR32)]   = jay::SType_Str32;
  stype_to_jaytype[int(dt::SType::STR64)]   = jay::SType_Str64;
  stype_to_jaytype[int(dt::SType::VOID)]    = jay::SType_Void0;
}


} // namespace py
