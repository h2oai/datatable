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
#include "frame/py_frame.h"
#include "python/obj.h"
#include "read2/multisource.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace read2 {

using SourceVec = std::vector<std::unique_ptr<Source>>;

static SourceVec fromAny(const py::robj);
static SourceVec fromFile(const py::robj);
static SourceVec fromText(const py::robj);
static SourceVec fromCmd(const py::robj);
static SourceVec fromUrl(const py::robj);



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

MultiSource::MultiSource(
    const char* fnName,
    const py::robj arg0,
    const py::robj argFile,
    const py::robj argText,
    const py::robj argCmd,
    const py::robj argUrl)
{
  const bool arg0_defined    = arg0 && !arg0.is_none();
  const bool argFile_defined = argFile && !argFile.is_none();
  const bool argText_defined = argText && !argText.is_none();
  const bool argCmd_defined  = argCmd && !argCmd.is_none();
  const bool argUrl_defined  = argUrl && !argUrl.is_none();
  const int total = arg0_defined +
                    argFile_defined +
                    argText_defined +
                    argCmd_defined +
                    argUrl_defined;
  if (total == 1) {
    if (arg0_defined)    sources_ = fromAny(arg0);
    if (argFile_defined) sources_ = fromFile(argFile);
    if (argText_defined) sources_ = fromText(argText);
    if (argCmd_defined)  sources_ = fromCmd(argCmd);
    if (argUrl_defined)  sources_ = fromUrl(argUrl);
  }
  else if (total == 0) {
    throw TypeError() << "No input source for " << fnName << " was given";
  }
  else {
    std::vector<const char*> extraArgs;
    if (argFile_defined) extraArgs.push_back("file");
    if (argText_defined) extraArgs.push_back("text");
    if (argCmd_defined)  extraArgs.push_back("cmd");
    if (argUrl_defined)  extraArgs.push_back("url");
    if (arg0_defined) {
      throw TypeError()
          << "When an unnamed argument is passed to " << fnName
          << ", it is invalid to also provide the `" << extraArgs[0]
          << "` parameter";
    } else {
      xassert(extraArgs.size() >= 2);
      throw TypeError()
          << "Both parameters `" << extraArgs[0] << "` and `" << extraArgs[1]
          << "` cannot be passed to " << fnName << " simultaneously";
    }
  }
}


static SourceVec fromAny(const py::robj src) {
  (void) src;
  return SourceVec();
}


static SourceVec fromFile(const py::robj src) {
  (void) src;
  return SourceVec();
}


static SourceVec fromText(const py::robj src) {
  (void) src;
  return SourceVec();
}


static SourceVec fromCmd(const py::robj src) {
  (void) src;
  return SourceVec();
}


static SourceVec fromUrl(const py::robj src) {
  (void) src;
  return SourceVec();
}




//------------------------------------------------------------------------------
// Process sources, and return the results
//------------------------------------------------------------------------------

py::oobj MultiSource::readSingle() {
  xassert(iterationIndex_ == 0);
  if (sources_.empty()) {
    return py::Frame::oframe(new DataTable);
  }

  // bool err = (reader_.multisource_strategy == FreadMultiSourceStrategy::Error);
  // bool warn = (reader_.multisource_strategy == FreadMultiSourceStrategy::Warn);
  // if (sources_.size() > 1 && err) throw _multisrc_error();

  // py::oobj res = read_next();
  // if (iterationIndex_ < sources_.size()) {
  //   if (err) throw _multisrc_error();
  //   if (warn) emit_multisrc_warning();
  // }
  // return res;
  return py::None();
}




}}  // namespace dt::read2
