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
#ifndef dt_READ_PRECOLUMN_h
#define dt_READ_PRECOLUMN_h
#include <string>
#include "buffer.h"       // Buffer
#include "python/obj.h"   // py::oobj
#include "writebuf.h"     // WritableBuffer
#include "_dt.h"
namespace dt {
namespace read {



/**
  * Information about a single input column in a GenericReader. An
  * "input column" means a collection of fields at the same index on
  * every line in the input. All these fields are assumed to have a
  * common underlying type.
  *
  * An input column usually translates into an output column in a
  * DataTable returned to the user. The exception to this are
  * "dropped" columns. They are marked with `presentInOutput = false`
  * flag (and have rtype RT::RDrop).
  */
class PreColumn
{
  private:
    std::string name_;
    Buffer databuf_;
    std::unique_ptr<MemoryWritableBuffer> strbuf_;
    PT ptype_;
    RT rtype_;
    bool type_bumped_;
    bool present_in_output_;
    bool present_in_buffer_;
    int32_t : 24;

    class ptype_iterator {
      private:
        int8_t* pqr;
        RT rtype;
        PT orig_ptype;
        PT curr_ptype;
        int64_t : 40;
      public:
        ptype_iterator(PT pt, RT rt, int8_t* qr_ptr);
        PT operator*() const;
        ptype_iterator& operator++();
        bool has_incremented() const;
        RT get_rtype() const;
    };

  public:
    PreColumn();
    PreColumn(const PreColumn&) = delete;
    PreColumn(PreColumn&&);
    virtual ~PreColumn();

    // Column's data
    void allocate(size_t nrows);
    void* data_w();
    WritableBuffer* strdata_w();
    Buffer extract_databuf();
    Buffer extract_strbuf();

    // Column's name
    const std::string& get_name() const noexcept;
    void set_name(std::string&& newname) noexcept;
    void swap_names(PreColumn& other) noexcept;
    const char* repr_name(const GenericReader& g) const;  // static ptr

    // Column's type(s)
    PT get_ptype() const noexcept;
    RT get_rtype() const noexcept;
    SType get_stype() const;
    ptype_iterator get_ptype_iterator(int8_t* qr_ptr) const;
    void set_rtype(int64_t it);
    void set_ptype(const ptype_iterator& it);
    void force_ptype(PT new_ptype);
    const char* typeName() const;

    // Column info
    bool is_string() const;
    bool is_dropped() const;
    bool is_type_bumped() const;
    bool is_in_output() const;
    bool is_in_buffer() const;
    size_t elemsize() const;
    void reset_type_bumped();
    void set_in_buffer(bool f);

    // Misc
    py::oobj py_descriptor() const;
    size_t memory_footprint() const noexcept;
};




}} // namespace dt::read
#endif
