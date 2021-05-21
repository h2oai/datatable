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
  py::Frame::cast_from(result)->setSource(srcName);
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
  (void) stream;
  return py::None();
}


#if 0


void detectCSVSettings() {
  Buffer chunk = stream->getChunk(0, 65536);
  const char* sof = static_cast<const char*>(chunk.rptr());
  const char* eof = sof + chunk.size();
  const char* ch = sof;
  char quote = '\0';
  int characterCounts[128];
  while (ch < eof) {
    char c = *ch;
    if (c >= 0) {  // ASCII
      characterCounts[c]++;
      if (c == '\'' || c == '"') {
        quote = c;
        break;
      }
      // maybe also break if newline count becomes > 100
    }
    ch++;
  }
  if (quote) {
    if (ch == sof) {
      quote_at_line_start = true;
    } else {
      char prevCh = ch[-1];
      if (prevCh == ' ') {
        whitespace_before_quote = true;
        for (i = 1; i <= ch - sof && (prevCh = ch[-i]) == ' '; i++);
      }
      if (prevCh < 0) {
        not_a_valid_quote
      }
      else if (prevCh == '\n' || prevCh == '\r') {
        quote_at_line_start = true;
      }
      else if (prevCh in illegalQuotes) {
        not_a_valid_quote
      }
      else {
        sep_before_quote = prevCh;
      }
    }
  }
}






void detectCSVSettings(NewlineKind newline) {
  Buffer chunk = stream->getChunk(0, 65536);
  const char* sof = static_cast<const char*>(chunk.rptr());
  const char* eof = sof + chunk.size();
  const char* ch = sof;
  char quote = '\0';
  int characterCounts[128];
  ScanOptions opts;

  int ret = scanLine(opts, &ch, eof, characterCounts);
}






#endif


}}  // namespace dt::read2
