//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_READ_COLUMN_h
#define dt_READ_COLUMN_h
#include <string>
#include "buffer.h"     // Buffer
#include "python/obj.h"   // py::oobj
#include "writebuf.h"     // WritableBuffer

// forward-declare
enum PT : uint8_t;
enum RT : uint8_t;
enum class SType : uint8_t;
class GenericReader;

namespace dt {
namespace read {


/**
 * Information about a single input column in a GenericReader. An "input column"
 * means a collection of fields at the same index on every line in the input.
 * All these fields are assumed to have a common underlying type.
 *
 * An input column usually translates into an output column in a DataTable
 * returned to the user. The exception to this are "dropped" columns. They are
 * marked with `presentInOutput = false` flag (and have rtype RT::RDrop).
 */
class Column {
  private:
    std::string name;
    Buffer databuf;
    MemoryWritableBuffer* strbuf;
    PT ptype;
    RT rtype;
    bool typeBumped;
    bool presentInOutput;
    bool presentInBuffer;
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
    Column();
    Column(const Column&) = delete;
    Column(Column&&);
    virtual ~Column();

    // Column's data
    void allocate(size_t nrows);
    void* data_w();
    WritableBuffer* strdata_w();
    Buffer extract_databuf();
    Buffer extract_strbuf();

    // Column's name
    const std::string& get_name() const noexcept;
    void set_name(std::string&& newname) noexcept;
    void swap_names(Column& other) noexcept;
    const char* repr_name(const GenericReader& g) const;  // static ptr

    // Column's type(s)
    PT get_ptype() const;
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


} // namespace read
} // namespace dt

#endif
