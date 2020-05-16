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
#include "csv/reader.h"
#include "csv/reader_parsers.h"
#include "python/string.h"
#include "read/output_column.h"
#include "read/precolumn.h"
#include "utils/temporary_file.h"
#include "column.h"
namespace dt {
namespace read {



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

PreColumn::PreColumn()
  : parse_type_(PT::Mu),
    requested_type_(RT::RAuto),
    type_bumped_(false),
    present_in_output_(true),
    present_in_buffer_(true) {}

PreColumn::PreColumn(PreColumn&& o) noexcept
  : name_(std::move(o.name_)),
    parse_type_(o.parse_type_),
    requested_type_(o.requested_type_),
    type_bumped_(o.type_bumped_),
    present_in_output_(o.present_in_output_),
    present_in_buffer_(o.present_in_buffer_),
    outcol_(std::move(o.outcol_)) {}




//------------------------------------------------------------------------------
// Column's data
//------------------------------------------------------------------------------

void PreColumn::archive_data(size_t nrows_written,
                             std::shared_ptr<TemporaryFile>& tempfile)
{
  if (nrows_written == outcol_.nrows_in_chunks_) return;
  if (type_bumped_ || !present_in_buffer_) return;
  xassert(nrows_written > outcol_.nrows_in_chunks_);

  bool col_is_string = is_string();
  size_t nrows_chunk = nrows_written - outcol_.nrows_in_chunks_;
  size_t data_size = elemsize() * (nrows_chunk + col_is_string);
  Buffer stored_databuf, stored_strbuf;
  if (tempfile) {
    auto writebuf = tempfile->data_w();
    {
      Buffer tmpbuf;
      tmpbuf.swap(outcol_.databuf_);
      size_t offset = writebuf->write(data_size, tmpbuf.rptr());
      stored_databuf = Buffer::tmp(tempfile, offset, data_size);
    }
    if (col_is_string) {
      outcol_.strbuf_->finalize();
      Buffer tmpbuf = outcol_.strbuf_->get_mbuf();
      size_t offset = writebuf->write(tmpbuf.size(), tmpbuf.rptr());
      stored_strbuf = Buffer::tmp(tempfile, offset, tmpbuf.size());
      outcol_.strbuf_ = nullptr;
    }
  }
  else {
    stored_databuf.swap(outcol_.databuf_);
    stored_databuf.resize(data_size);
    if (col_is_string) {
      outcol_.strbuf_->finalize();
      stored_strbuf = outcol_.strbuf_->get_mbuf();
      outcol_.strbuf_ = nullptr;
    }
  }

  outcol_.chunks_.push_back(
    col_is_string? Column::new_string_column(nrows_chunk,
                                             std::move(stored_databuf),
                                             std::move(stored_strbuf))
                 : Column::new_mbuf_column(nrows_chunk, get_stype(),
                                           std::move(stored_databuf))
  );
  outcol_.nrows_in_chunks_ = nrows_written;
  xassert(!outcol_.databuf_ && !outcol_.strbuf_);
}


void PreColumn::allocate(size_t new_nrows) {
  if (type_bumped_ || !present_in_buffer_) return;
  xassert(new_nrows >= outcol_.nrows_in_chunks_);

  size_t new_nrows_allocated = new_nrows - outcol_.nrows_in_chunks_;
  size_t allocsize = (new_nrows_allocated + is_string()) * elemsize();
  outcol_.databuf_.resize(allocsize);

  if (is_string()) {
    size_t zero = 0;
    std::memcpy(outcol_.databuf_.xptr(), &zero, elemsize());
    if (!outcol_.strbuf_) {
      outcol_.strbuf_ = std::unique_ptr<MemoryWritableBuffer>(
                    new MemoryWritableBuffer(allocsize));
    }
  }
}


OutputColumn& PreColumn::outcol() {
  return outcol_;
}





//---- Column's name -----------------------------------------------------------

const std::string& PreColumn::get_name() const noexcept {
  return name_;
}

void PreColumn::set_name(std::string&& newname) noexcept {
  name_ = std::move(newname);
}

void PreColumn::swap_names(PreColumn& other) noexcept {
  name_.swap(other.name_);
}

const char* PreColumn::repr_name(const GenericReader& g) const {
  const char* start = name_.c_str();
  const char* end = start + name_.size();
  return g.repr_binary(start, end, 25);
}



//---- Column's type -----------------------------------------------------------

PT PreColumn::get_ptype() const noexcept {
  return parse_type_;
}

RT PreColumn::get_rtype() const noexcept {
  return requested_type_;
}

SType PreColumn::get_stype() const {
  return ParserLibrary::info(parse_type_).stype;
}

PreColumn::ptype_iterator
PreColumn::get_ptype_iterator(int8_t* qr_ptr) const {
  return PreColumn::ptype_iterator(parse_type_, requested_type_, qr_ptr);
}

void PreColumn::set_ptype(const PreColumn::ptype_iterator& it) {
  xassert(requested_type_ == it.get_rtype());
  type_bumped_ = true;
  parse_type_ = *it;
  outcol_.set_stype(get_stype());
}

// Set .parse_type_ to the provided value, disregarding the restrictions imposed
// by the .requested_type_ field.
void PreColumn::force_ptype(PT new_ptype) {
  parse_type_ = new_ptype;
  outcol_.set_stype(get_stype());
}

void PreColumn::set_rtype(int64_t it) {
  requested_type_ = static_cast<RT>(it);
  // Temporary
  switch (requested_type_) {
    case RDrop:
      parse_type_ = PT::Str32;
      present_in_output_ = false;
      present_in_buffer_ = false;
      break;
    case RAuto:    break;
    case RBool:    parse_type_ = PT::Bool01; break;
    case RInt:     parse_type_ = PT::Int32; break;
    case RInt32:   parse_type_ = PT::Int32; break;
    case RInt64:   parse_type_ = PT::Int64; break;
    case RFloat:   parse_type_ = PT::Float32Hex; break;
    case RFloat32: parse_type_ = PT::Float32Hex; break;
    case RFloat64: parse_type_ = PT::Float64Plain; break;
    case RStr:     parse_type_ = PT::Str32; break;
    case RStr32:   parse_type_ = PT::Str32; break;
    case RStr64:   parse_type_ = PT::Str64; break;
  }
  outcol_.set_stype(get_stype());
}

const char* PreColumn::typeName() const {
  return ParserLibrary::info(parse_type_).name.data();
}



//---- Column info -------------------------------------------------------------

bool PreColumn::is_string() const {
  return ParserLibrary::info(parse_type_).isstring();
}

bool PreColumn::is_dropped() const noexcept {
  return requested_type_ == RT::RDrop;
}

bool PreColumn::is_type_bumped() const noexcept {
  return type_bumped_;
}

bool PreColumn::is_in_output() const noexcept {
  return present_in_output_;
}

bool PreColumn::is_in_buffer() const noexcept {
  return present_in_buffer_;
}

size_t PreColumn::elemsize() const {
  return static_cast<size_t>(ParserLibrary::info(parse_type_).elemsize);
}

void PreColumn::reset_type_bumped() {
  type_bumped_ = false;
}

void PreColumn::set_in_buffer(bool f) {
  present_in_buffer_ = f;
}

size_t PreColumn::nrows_archived() const noexcept {
  return outcol_.nrows_in_chunks_;
}



//---- Misc --------------------------------------------------------------------

static PyTypeObject* init_nametypepytuple() {
  static const char* tuple_name = "column_descriptor";
  static const char* field0 = "name";
  static const char* field1 = "type";
  PyStructSequence_Desc desc;
  PyStructSequence_Field fields[3];
  fields[0].name = const_cast<char*>(field0);
  fields[1].name = const_cast<char*>(field1);
  fields[2].name = nullptr;
  fields[0].doc = nullptr;
  fields[1].doc = nullptr;
  fields[2].doc = nullptr;
  desc.name = const_cast<char*>(tuple_name);
  desc.doc = nullptr;
  desc.fields = fields;
  desc.n_in_sequence = 2;
  // Do not use PyStructSequence_NewType, because it is buggy
  // (see https://lists.gt.net/python/bugs/1320383)
  // The memory must also be cleared because https://bugs.python.org/issue33742
  auto res = new PyTypeObject();
  PyStructSequence_InitType(res, &desc);
  return res;
}


py::oobj PreColumn::py_descriptor() const {
  static PyTypeObject* name_type_pytuple = init_nametypepytuple();
  PyObject* nt_tuple = PyStructSequence_New(name_type_pytuple);  // new ref
  if (!nt_tuple) throw PyError();
  PyObject* stype = info(ParserLibrary::info(parse_type_).stype).py_stype().release();
  PyObject* cname = py::ostring(name_).release();
  PyStructSequence_SetItem(nt_tuple, 0, cname);
  PyStructSequence_SetItem(nt_tuple, 1, stype);
  return py::oobj::from_new_reference(nt_tuple);
}


size_t PreColumn::memory_footprint() const {
  size_t sz = archived_size();
  sz += outcol_.databuf_.memory_footprint();
  sz += outcol_.strbuf_? outcol_.strbuf_->size() : 0;
  sz += name_.size() + sizeof(*this);
  return sz;
}


size_t PreColumn::archived_size() const {
  size_t sz = 0;
  for (const auto& col : outcol_.chunks_) {
    size_t k = col.get_num_data_buffers();
    for (size_t i = 0; i < k; ++i) {
      sz += col.get_data_size(i);
    }
  }
  return sz;
}


void PreColumn::prepare_for_rereading() {
  if (type_bumped_ && present_in_output_) {
    present_in_buffer_ = true;
    type_bumped_ = false;
    outcol_.chunks_.clear();
    outcol_.nrows_in_chunks_ = 0;
    outcol_.strbuf_ = nullptr;
  }
  else {
    present_in_buffer_ = false;
  }
}



//---- ptype_iterator ----------------------------------------------------------

PreColumn::ptype_iterator::ptype_iterator(PT pt, RT rt, int8_t* qr_ptr)
  : pqr(qr_ptr), rtype(rt), orig_ptype(pt), curr_ptype(pt) {}

PT PreColumn::ptype_iterator::operator*() const {
  return curr_ptype;
}

RT PreColumn::ptype_iterator::get_rtype() const {
  return rtype;
}

PreColumn::ptype_iterator& PreColumn::ptype_iterator::operator++() {
  if (curr_ptype < PT::Str32) {
    curr_ptype = static_cast<PT>(curr_ptype + 1);
  } else {
    *pqr = *pqr + 1;
  }
  return *this;
}

bool PreColumn::ptype_iterator::has_incremented() const {
  return curr_ptype != orig_ptype;
}





}}  // namespace dt::read
