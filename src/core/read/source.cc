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
#include "csv/reader.h"     // GenericReader
#include "python/string.h"
#include "python/xobject.h"
#include "read/source.h"    // Source
#include "utils/macros.h"
#include "utils/misc.h"
#include "utils/temporary_file.h"
namespace dt {
namespace read {


//------------------------------------------------------------------------------
// Base Source class
//------------------------------------------------------------------------------

Source::Source(const std::string& name) : name_(name) {}

Source::~Source() {}


const std::string& Source::name() const {
  return name_;
}


std::unique_ptr<Source> Source::continuation() {
  return nullptr;
}




//------------------------------------------------------------------------------
// Source_Python  [temporary]
//------------------------------------------------------------------------------

Source_Python::Source_Python(const std::string& name, py::oobj src)
  : Source(name), src_(src) {}


py::oobj Source_Python::read(GenericReader& reader) {
  reader.source_name = &name_;

  auto pysrcs = src_.to_otuple();
  auto src_arg  = pysrcs[0];
  auto file_arg = pysrcs[1];
  int  fileno   = pysrcs[2].to_int32();
  auto text_arg = pysrcs[3];

  double t0 = wallclock();
  Buffer input_mbuf;
  CString text;
  CString filename;
  if (fileno > 0) {
    #if DT_OS_WINDOWS
      throw NotImplError() << "Reading from file-like objects, that involves "
        << "file descriptors, is not supported on Windows";
    #else
      const char* src = src_arg.to_cstring().data();
      input_mbuf = Buffer::mmap(src, 0, fileno, false);
      size_t sz = input_mbuf.size();
      if (reader.verbose) {
        reader.d() << "Using file " << src << " opened at fd=" << fileno
                   <<"; size = " << sz;
      }
    #endif
  } else if (!(text = text_arg.to_cstring()).isna()) {
    input_mbuf = Buffer::unsafe(text.data(), text.size());

  } else if (!(filename = file_arg.to_cstring()).isna()) {
    input_mbuf = Buffer::mmap(filename.to_string());
    size_t sz = input_mbuf.size();
    if (reader.verbose) {
      reader.d() << "File \"" << filename << "\" opened, size: " << sz;
    }

  } else {
    throw IOError() << "No input given to the GenericReader";
  }
  reader.t_open_input = wallclock() - t0;

  auto res = reader.read_buffer(input_mbuf, 0);
  reader.source_name = nullptr;
  return res;
}



//------------------------------------------------------------------------------
// Source_Result  [temporary]
//------------------------------------------------------------------------------

Source_Result::Source_Result(const std::string& name, py::oobj res)
  : Source(name), result_(res) {}


py::oobj Source_Result::read(GenericReader&) {
  return result_;
}



//------------------------------------------------------------------------------
// Source_Text
//------------------------------------------------------------------------------

Source_Text::Source_Text(py::robj textsrc)
  : Source("<text>"), src_(textsrc)
{
  xassert(src_.is_string() || src_.is_bytes());
}


py::oobj Source_Text::read(GenericReader& reader) {
  reader.source_name = &name_;
  auto buf = Buffer::pybytes(src_);
  auto res = reader.read_buffer(buf, 1);
  reader.source_name = nullptr;
  return res;
}




//------------------------------------------------------------------------------
// Source_Url
//------------------------------------------------------------------------------

class ReportHook : public py::XObject<ReportHook>
{
  private:
    dt::progress::work* job_;   // borrowed

  public:
    void m__init__(const py::PKArgs&) {}
    void m__dealloc__() {}

    void m__call__(const py::PKArgs& args) {
      size_t count = args[0].to_size_t();
      size_t block_size = args[1].to_size_t();
      int64_t total_size = args[2].to_int64_strict();
      if (total_size < 0) return;  // TODO: use tentative progress
      size_t zsize = static_cast<size_t>(total_size);

      if (job_->get_work_amount() == 1) {
        job_->add_work_amount(zsize);
      }
      size_t dsize = count * block_size;
      if (dsize >= zsize) {
        // 1 was the original "fake" work amount
        job_->set_done_amount(zsize + 1);
        job_->done();
      } else {
        job_->set_done_amount(dsize + 1);
      }
      xassert(dt::num_threads_in_team() == 0);
      dt::progress::manager->update_view();
    }

    static py::oobj make(dt::progress::work* job) {
      ReportHook::init_type();
      auto res = py::XObject<ReportHook>::make();
      auto reporthook = reinterpret_cast<ReportHook*>(res.to_borrowed_ref());
      reporthook->job_ = job;
      return res;
    }

    static void impl_init_type(py::XTypeMaker& xt) {
      xt.set_class_name("reporthook");
      static py::PKArgs args_init(0, 0, 0, false, false, {}, "__init__", nullptr);
      static py::PKArgs args_call(3, 0, 0, false, false,
          {"count", "blocksize", "totalsize"}, "__call__", nullptr);
      xt.add(CONSTRUCTOR(&ReportHook::m__init__, args_init));
      xt.add(DESTRUCTOR(&ReportHook::m__dealloc__));
      xt.add(METHOD__CALL__(&ReportHook::m__call__, args_call));
    }
};


Source_Url::Source_Url(const std::string& url) : Source(url) {
  // If s3 path is supplied, convert it to the corresponding http URL
  if (url.substr(0, 2) == "s3") {
    auto res = py::oobj::import("urllib.parse", "urlparse")
                 .call({py::ostring(url)});
    url_ = "https://";
    url_ += res.get_attr("netloc").to_string();
    url_ += ".s3.amazonaws.com";
    url_ += res.get_attr("path").to_string();
  } else {
    url_ = url;
  }
}


py::oobj Source_Url::read(GenericReader& reader) {
  reader.source_name = &name_;
  TemporaryFile tmpfile;
{
    dt::progress::work job(1);
    job.set_message("Downloading " + url_);
    // Characters [0-9a-zA-Z._~-] are always considered "safe".
    // Further, "/" is typically used as a path separator, and ":"
    // is in the protocol. Both can be used in URLs safely.
    // The "%" character, on the other hand, is not "safe" -- it is
    // used to indicate an escape character. However, we designate it
    // as "safe" so that the user may be able to pass a URL that is
    // already correctly encoded. The downside is that if the url
    // is not encoded, but happens to have a % character in it, then
    // this function will throw an exception, but I consider such
    // cases to be very rare. If they do occur, then the user could
    // simply pass a correctly encoded URL, and then everything
    // would work.
    auto quoted_url = py::oobj::import("urllib.parse", "quote")
                        .call({py::ostring(url_)},
                              {py::ostring("safe"), py::ostring(":/%")});

    auto retriever = py::oobj::import("urllib.request", "urlretrieve");
    retriever.call({quoted_url,
                    py::ostring(tmpfile.name()),
                    ReportHook::make(&job)});
  }
  auto res = reader.read_buffer(tmpfile.buffer_r(), 0);
  reader.source_name = nullptr;
  return res;
}




}}  // namespace dt::read
