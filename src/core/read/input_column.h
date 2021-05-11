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
#ifndef dt_READ_INPUT_COLUMN_h
#define dt_READ_INPUT_COLUMN_h
#include "read/output_column.h"
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
  * "dropped" columns (they have `requested_type_ = RT::RDrop`).
  */
class InputColumn
{
  private:
    std::string name_;
    PT parse_type_;
    RT requested_type_;
    size_t : 48;

    // TODO: make OutputColumn completely separate from InputColumn
    OutputColumn outcol_;

  public:
    InputColumn();
    InputColumn(InputColumn&&) noexcept = default;
    InputColumn(const InputColumn&) = delete;

    // Column's data
    OutputColumn& outcol();

    // Column's name
    const std::string& get_name() const noexcept;
    void set_name(std::string&& newname) noexcept;
    void swap_names(InputColumn& other) noexcept;
    const char* repr_name(const GenericReader& g) const;  // static ptr

    // Column's type(s)
    PT get_ptype() const noexcept;
    RT get_rtype() const noexcept;
    SType get_stype() const;
    void set_rtype(int64_t it);
    void set_ptype(PT new_ptype);
    const char* typeName() const;

    // Column info
    bool is_string() const;
    bool is_dropped() const noexcept;
    size_t elemsize() const;

    // Misc
    py::oobj py_descriptor() const;
    size_t memory_footprint() const;
    size_t archived_size() const;
};




}} // namespace dt::read
#endif
