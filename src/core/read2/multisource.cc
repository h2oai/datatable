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

using SourcePtr = std::unique_ptr<Source>;
using SourceVec = std::vector<SourcePtr>;

static SourceVec _fromAny(const py::robj);
static SourceVec _fromFile(const py::robj);
static SourceVec _fromText(const py::robj);
static SourceVec _fromCmd(const py::robj);
static SourceVec _fromUrl(const py::robj);
static SourceVec _fromGlob(const py::robj);



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
    if (arg0_defined)    sources_ = _fromAny(arg0);
    if (argFile_defined) sources_ = _fromFile(argFile);
    if (argText_defined) sources_ = _fromText(argText);
    if (argCmd_defined)  sources_ = _fromCmd(argCmd);
    if (argUrl_defined)  sources_ = _fromUrl(argUrl);
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



//------------------------------------------------------------------------------
// from Any
//------------------------------------------------------------------------------

// Return true if `text` has any characters from C0 range.
static bool _has_control_characters(const CString& text, char* evidence) {
  size_t n = text.size();
  const char* ch = text.data();
  for (size_t i = 0; i < n; ++i) {
    if (static_cast<unsigned char>(ch[i]) < 0x20) {
      *evidence = ch[i];
      return true;
    }
  }
  return false;
}

static bool _looks_like_url(const CString& text) {
  size_t n = text.size();
  const char* ch = text.data();
  if (n >= 8) {
    if (std::memcmp(ch, "https://", 8) == 0) return true;
    if (std::memcmp(ch, "http://", 7) == 0) return true;
    if (std::memcmp(ch, "file://", 7) == 0) return true;
    if (std::memcmp(ch, "ftp://", 6) == 0) return true;
  }
  return false;
}

static bool _looks_like_glob(const CString& text, char* evidence) {
  size_t n = text.size();
  const char* ch = text.data();
  for (size_t i = 0; i < n; ++i) {
    char c = ch[i];
    if (c == '*' || c == '?' || c == '[' || c == ']') {
      *evidence = c;
      return true;
    }
  }
  return false;
}


static SourceVec _fromAny(const py::robj src) {
  if (src.is_string() || src.is_bytes()) {
    auto cstr = src.to_cstring();
    if (cstr.size() >= 4096) {
      // LOG() << "Input is a string of length " << cstr.size()
      //       << " => assume it's a text source";
      return _fromText(src);
    }
    char c;
    if (_has_control_characters(cstr, &c)) {
      // LOG() << "Input contains character '" << c << "'"
      //       << " => assume it's a text source";
      return _fromText(src);
    }
    if (_looks_like_url(cstr)) {
      // LOG() << "Input looks like a URL";
      return _fromUrl(src);
    }
    if (_looks_like_glob(cstr, &c)) {
      // LOG() << "Input contains character '" << c << "'"
      //       << " => assuming it's a glob pattern";
      return _fromGlob(src);
    }
  }
  return SourceVec();
}



//------------------------------------------------------------------------------
// from File
//------------------------------------------------------------------------------

static SourceVec _fromFile(const py::robj src) {
  // Case 1: src is a filename (str|bytes|PathLike)
  if (src.is_string() || src.is_bytes() || src.is_pathlike()) {
    auto pyFileName = py::oobj::import("os.path", "expanduser").call({src});
    pyFileName = py::oobj::import("os", "fsdecode").call({pyFileName});

    // if file exists
    //     Check the file's extension: if one of the known archive
    //     types (.zip, .tgz, .tar.gz, .gz, .bz2, .xz) then expand
    //     the file into multiple sources using the archive interface.
    //
    //     For certain file types infer format based on extension: .jay,
    //     .xlsx, .tsv (sep=\t), .json, .arff, .html.
    //
    //     All other files treat as plain text files
    //
    // else
    //     Split path into components, and look up parent paths until
    //     found a valid one.
    //     If valid path is a directory:
    //         Throw a FileNotFound error, indicating which part of
    //         the path exists and which doesn't
    //     If valid path is a file:
    //         Assume that it's an archive, and treat the remainder of
    //         the path as the subpath.
    //         Check that archive's extension: process separately for
    //         each known extension.
  }
  // Case 2: src is a file object (has method `.read()`)
  if (src.has_attr("read")) {
    auto fnRead = src.get_attr("read");
    // Create a source which could read data from async stream
    // provided by the `fnRead`.
    // Returned source is stream source.
  }
  (void) src;
  return SourceVec();
}


// Archive interface supports methods:
//   .get_files_list()  // return list of files inside, or null if archive
//                      // format supports single file only
//   .read_file(name)   // read a specific file inside the archive.
//
// Note that reading a file returns a Stream source
//
// Note that streaming from a shell command (either for an Archive
// implementation, or for cmd= parameter) can be done via
// subprocess.Popen() with STDOUT set to a manually opened pipe
// (either os.pipe() or _winapi.CreatePipe()). Then we can simply read
// from the resulting file handle, avoiding python's overhead.
//
//
// Streaming interface basically follows the API of unix' `read(2)`
// (https://linux.die.net/man/2/read):
//
//    ssize_t read(int fd, void *buf, size_t count);
//
// with the exception that the Stream object will probably be returning
// its Buffer, which may be either smaller or larger than the count of
// bytes requested.
//
// Stream objects can be chained one after another into a single pipe.
//

//------------------------------------------------------------------------------
// from Text
//------------------------------------------------------------------------------

static SourceVec _fromText(const py::robj src) {
  if (!(src.is_string() || src.is_bytes())) {
    throw TypeError() << "Invalid parameter `text` in fread: expected "
                         "str or bytes, instead got " << src.typeobj();
  }
  SourceVec out;
  out.emplace_back(new Source_Text(src));
  return out;
}



//------------------------------------------------------------------------------
// from Cmd
//------------------------------------------------------------------------------

static SourceVec _fromCmd(const py::robj src) {
  (void) src;
  return SourceVec();
}



//------------------------------------------------------------------------------
// from Url
//------------------------------------------------------------------------------

static SourceVec _fromUrl(const py::robj src) {
  (void) src;
  return SourceVec();
}



//------------------------------------------------------------------------------
// from Glob
//------------------------------------------------------------------------------

static SourceVec _fromGlob(const py::robj src) {
  auto globFn = py::import("glob", "glob");
  auto filesList = globFn.call({src}).to_pylist();
  auto n = filesList.size();
  SourceVec out;
  if (n == 0) {
    // [TODO]
    // Check whether this is a glob inside an archive:
    // Split the path into components and repeatedly chop off the
    // tail until you find a component that exists. If it's a file,
    // then the remainder must be a glob pattern within the archive.
    // Obtain the list of files within the archive, and then use
    // `fnmatch.fncasematch()` to manually perform glob matching.
    //
    // LOG() << "Glob pattern did not match any files";
  } else {
    // LOG() << "Glob pattern resolved into " << n << " file(s):";
    for (size_t i = 0; i < n; i++) {
      // LOG() << "  " << filesList[i].to_cstring();
      auto resolved = _fromFile(filesList[i]);
      out.insert(out.end(),
          std::make_move_iterator(resolved.begin()),
          std::make_move_iterator(resolved.end())
      );
    }
  }
  return out;
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
