//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#include "read2/read_director.h"
#include "frame/py_frame.h"
#include "python/obj.h"
namespace dt {
namespace read2 {


ReadDirector::ReadDirector(SourceIterator&& sources, ReadOptions&& options)
    : sources_(std::move(sources)),
      options_(std::move(options)) {}


py::oobj ReadDirector::readSingle() {
  return readNext();
}


py::oobj ReadDirector::readNext() {
  Source* src = sources_.next();
  if (!src) {
    return py::oobj();
  }

  auto srcName = py::ostring(src->getName());
  auto result = src->readWith(this);
  // py::Frame::cast_from(result)->setSource(srcName);
  return result;

  // py::oobj res;
  // GenericReader new_reader(reader_);
  // auto& src = sources_[iteration_index_];
  // if (reader_.errors_strategy == IreadErrorHandlingStrategy::Error) {
  //   res = src->read(new_reader);
  //   py::Frame::cast_from(res)->set_source(src->name());
  // }
  // else {
  //   try {
  //     res = src->read(new_reader);
  //     py::Frame::cast_from(res)->set_source(src->name());
  //   }
  //   catch (const Error& e) {
  //     if (reader_.errors_strategy == IreadErrorHandlingStrategy::Warn) {
  //       emit_badsrc_warning(src->name(), e);
  //     }
  //     if (reader_.errors_strategy == IreadErrorHandlingStrategy::Store) {
  //       exception_to_python(e);
  //       PyObject* etype = nullptr;
  //       PyObject* evalue = nullptr;
  //       PyObject* etraceback = nullptr;
  //       PyErr_Fetch(&etype, &evalue, &etraceback);
  //       PyErr_NormalizeException(&etype, &evalue, &etraceback);
  //       if (etraceback) PyException_SetTraceback(evalue, etraceback);
  //       Py_XDECREF(etype);
  //       Py_XDECREF(etraceback);
  //       res = py::oobj::from_new_reference(evalue);
  //     }
  //   }
  // }
  // SourcePtr next = src->continuation();
  // if (next) {
  //   sources_[iteration_index_] = std::move(next);
  // } else {
  //   iteration_index_++;
  // }
  // if (!res) goto start;
  // return res;

}


py::oobj ReadDirector::readBuffer(Buffer buf) {
  return readStream(BufferedStream::fromBuffer(buf));
}


py::oobj ReadDirector::readStream(std::unique_ptr<BufferedStream>&& stream) {
  // [Temporary]: return the content of the input buffer as a bytes object
  Buffer all = stream->getChunk(0, (1ull << 60));
  PyObject* out = PyBytes_FromStringAndSize(
                      static_cast<const char*>(all.rptr()),
                      static_cast<Py_ssize_t>(all.size())
  );
  return py::oobj::from_new_reference(out);
}




}}  // namespace dt::read2
