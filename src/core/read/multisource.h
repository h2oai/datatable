//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#ifndef dt_READ_MULTISOURCE_h
#define dt_READ_MULTISOURCE_h
#include "_dt.h"
#include "read/source.h"      // Source
namespace dt {
namespace read {



/**
  * This class encapsulates various input sources for ?read family
  * of functions.
  *
  * Consider that the input for fread may come in a number of
  * different shapes: a string, a file, a list of files, a glob
  * pattern, a URL, an archive, a multi-sheet XLS file, etc. This
  * class encapsulates all that variety under a single interface.
  *
  * Internally, this class contains a vector of `Source_*` objects,
  * each representing an input source that produces a single Frame
  * object in the output.
  *
  * Occasionally, there could be inputs that look as single sources,
  * while in fact containing several Frames inside. Examples are:
  * CSV files that are concatenation of tables with different number
  * of columns; Excel files where a single sheet contains several
  * disjoint tables; etc. Normally, in such cases the Source_* object
  * returns a single Frame and emits a warning about additional
  * Frames being present. However, if a special input option is
  * given, then we would want all those frames to be returned. In
  * such a case, the input source must return a single frame, and
  * then set an internal attribute indicating that more frames are
  * pending. This class will then query that attribute, and collect
  * all frames, one at a time.
  */
class MultiSource
{
  private:
    using SourcePtr = std::unique_ptr<Source>;
    using SourceVec = std::vector<SourcePtr>;

    GenericReader reader_;
    SourceVec     sources_;
    size_t        iteration_index_;

  public:
    MultiSource(const py::XArgs&, GenericReader&&);
    MultiSource(const MultiSource&) = delete;
    MultiSource(MultiSource&&) = delete;

    py::oobj read_single();
    py::oobj read_next();
};




}}  // namespace dt::read
#endif
