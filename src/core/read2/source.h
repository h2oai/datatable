//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#ifndef dt_READ2_SOURCE_h
#define dt_READ2_SOURCE_h
#include <memory>            // std::unique_ptr
#include <string>            // std::string
#include "python/_all.h"     // py::oobj
#include "read2/_declarations.h"
namespace dt {
namespace read2 {



/**
  * Single input source for ?read functions. This is a base abstract
  * class, with different implementations.
  *
  * The objects of this class are used by the SourceIterator class only.
  */
class Source {
  protected:
    std::string name_;

  public:
    Source(const std::string& name);
    virtual ~Source();

    // Each source has a name (the names need not be unique) which
    // attempts to identify the origin of the object. This name will
    // be carried in the `.source` attribute of the frame produced.
    const std::string& getName() const;

    // Primary Source function, it will read the data from the
    // current source, and return it as a python Frame object.
    virtual py::oobj readWith(ReadDirector*) = 0;
};




//------------------------------------------------------------------------------
// Implementations
//------------------------------------------------------------------------------

/**
  * Source is an object that represents a contiguous piece of data that
  * already resides in memory. Typically, this is created from a string
  * or bytes object.
  */
class Source_Memory : public Source {
  private:
    Buffer buffer_;

  public:
    Source_Memory(py::robj textsrc);
    py::oobj readWith(ReadDirector*) override;
};



/**
  * Source is a regular file residing on disk, the name of the file is
  * provided by the user. This source memory-maps the file and then
  * reads the resulting buffer.
  */
class Source_File : public Source {
  private:
    std::string filename_;

  public:
    Source_File(std::string&& filename);
    py::oobj readWith(ReadDirector*) override;
};



/**
  * Source is a python "file-like" object, which is any object that
  * has `.read()` method. This source will read the file using the
  * stream interface.
  */
class Source_Filelike : public Source {
  private:
    py::oobj fileObject_;

  public:
    Source_Filelike(py::robj file);
    py::oobj readWith(ReadDirector*) override;
};




class Source_Url : public Source {
  private:
    std::string url_;
    py::oobj fileObject_;

  public:
    Source_Url(const std::string& url);
    ~Source_Url() override;
    py::oobj readWith(ReadDirector*) override;
};




}}  // namespace dt::read
#endif