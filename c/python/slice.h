//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#ifndef dt_PYTHON_SLICE_h
#define dt_PYTHON_SLICE_h
#include "python/obj.h"

namespace py {


/**
 * Wrapper around python `slice` object.
 *
 * The constructor `oslice(start, stop, step)` reflects the standard python
 * notation `start:stop:step`. The arguments in this constructor are `int64_t`.
 * Also, some of the parameters can be declared as empty (None) by passing the
 * constant `oslice::NA`.
 *
 * For regular (numeric) slices the properties `.start`, `.stop` and `.step`
 * return the corresponding slice fields as `int64_t`s. If the corresponding
 * field is None, then `oslice::NA` constant will be returned. If the user
 * passes a slice where some of the fields are big integers, then those will be
 * truncated to fit into `int64_t`.
 *
 * The `normalize(n, &start, &count, &step)` function will adapt the slice as
 * if applied to an array of length `n`.
 *
 * This object also supports string slices such as ["A":"Z"] (without the step).
 * For this use-case the `.start` and `.stop` properties can be returned as
 * `py::oobj`s.
 */
class oslice : public oobj {
  public:
    static constexpr int64_t MAX = 9223372036854775807LL;
    static constexpr int64_t NA = -9223372036854775807LL - 1;

    oslice() = default;
    oslice(const oslice&) = default;
    oslice(oslice&&) = default;
    oslice& operator=(const oslice&) = default;
    oslice& operator=(oslice&&) = default;

    oslice(int64_t start, int64_t stop, int64_t step);

    // Return true if all components of the slice are None
    bool is_trivial() const;

    bool is_numeric() const;
    int64_t start() const;
    int64_t stop() const;
    int64_t step() const;

    bool is_string() const;
    oobj start_obj() const;
    oobj stop_obj() const;

    void normalize(
        size_t len, size_t* pstart, size_t* pcount, size_t* pstep) const;

    static void normalize(
        size_t len,
        int64_t istart, int64_t istop, int64_t istep,
        size_t* ostart, size_t* ocount, size_t* ostep);

  private:
    // Private constructor, used from `_obj`. If you need to construct
    // `oslice` from `oobj`, use `oobj.to_oslice()` instead.
    oslice(const robj&);
    friend class _obj;
};



}
#endif
