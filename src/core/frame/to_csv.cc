//------------------------------------------------------------------------------
// Copyright 2018-2022 H2O.ai
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
#include "documentation.h"
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "python/_all.h"
#include "python/string.h"
#include "python/xargs.h"
#include "write/csv_writer.h"
#include "options.h"

namespace py {

static void change_to_lowercase(std::string& str) {
  for (size_t i = 0; i < str.size(); ++i) {
    char c = str[i];
    if (c >= 'A' && c <= 'Z') str[i] = c - 'A' + 'a';
  }
}



//------------------------------------------------------------------------------
// Frame::to_csv()
//------------------------------------------------------------------------------

oobj Frame::to_csv(const XArgs& args) {
  const Arg& arg_path     = args[0];
  const Arg& arg_sep      = args[1];
  const Arg& arg_quoting  = args[2];
  const Arg& arg_append   = args[3];
  const Arg& arg_header   = args[4];
  const Arg& arg_bom      = args[5];
  const Arg& arg_hex      = args[6];
  const Arg& arg_compress = args[7];
  const Arg& arg_verbose  = args[8];
  const Arg& arg_strategy = args[9];

  // path
  oobj path = arg_path.to<oobj>(ostring(""));
  if (!path.is_string()) {
    throw TypeError() << "Parameter `path` in `Frame.to_csv()` should be a "
        "string, instead got " << path.typeobj();
  }
  path = oobj::import("os", "path", "expanduser").call(path);
  std::string filename = path.to_string();

  // sep
  std::string sep = arg_sep.to<std::string>(",");
  if (sep.size() != 1) {
    throw ValueError() << "Parameter `sep` in `Frame.to_csv()` should be a "
        "single-character string, instead its length is " << sep.size();
  }

  // quoting
  int quoting;
  if (arg_quoting.is_string()) {
    auto quoting_str = arg_quoting.to_string();
    change_to_lowercase(quoting_str);
    if (quoting_str == "minimal") quoting = 0;
    else if (quoting_str == "all") quoting = 1;
    else if (quoting_str == "nonnumeric") quoting = 2;
    else if (quoting_str == "none") quoting = 3;
    else {
      throw ValueError() << "Invalid value of the `quoting` parameter in "
          "`Frame.to_csv()`: '" << quoting_str << "'";
    }
  } else {
    quoting = arg_quoting.to<int>(0);
    if (quoting < 0 || quoting > 3) {
      throw ValueError() << "Invalid value of the `quoting` parameter in "
          "`Frame.to_csv()`: " << quoting;
    }
  }

  // append
  bool append = arg_append.to<bool>(false);
  if (append && filename.empty()) {
    throw ValueError() << "`append` parameter is set to True, but the "
        "output file is not specified";
  }

  // header
  bool header;
  if (arg_header.is_none_or_undefined() || arg_header.is_auto() ||
      arg_header.is_ellipsis())
  {
    header = !(append && File::nonempty(filename));
  } else {
    header = arg_header.to<bool>(true);
  }

  // bom
  bool bom = arg_bom.to<bool>(false);
  if (bom && append && File::nonempty(filename)) {
    bom = false;  // turn off when appending to an existing file
  }

  // hex
  bool hex = arg_hex.to<bool>(false);

  // compress
  auto compress_str = arg_compress.to<std::string>("auto");
  bool compress = false;  // eventually this will be an Enum
  if (compress_str == "auto" || compress_str == "infer") {
    size_t n = filename.size();
    compress = !append && (n > 3 && filename[n-3] == '.' &&
                                    filename[n-2] == 'g' &&
                                    filename[n-1] == 'z');
  } else if (compress_str == "gzip") {
    compress = true;
    if (append) {
      throw ValueError() << "Compression cannot be used in the 'append' mode";
    }
  } else {
    throw ValueError() << "Unsupported compression method '"
        << compress_str << "' in `Frame.to_csv()`";
  }

  // verbose
  bool verbose = arg_verbose.to<bool>(false);

  auto strategy = arg_strategy.to<std::string>("");
  auto sstrategy = (strategy == "mmap")  ? WritableBuffer::Strategy::Mmap :
                   (strategy == "write") ? WritableBuffer::Strategy::Write :
                                           WritableBuffer::Strategy::Auto;

  // Create the CsvWriter object
  dt::write::csv_writer writer(dt, filename);
  writer.set_append(append);
  writer.set_header(header);
  writer.set_strategy(sstrategy);
  writer.set_usehex(hex);
  writer.set_bom(bom);
  writer.set_sep(sep[0]);
  writer.set_verbose(verbose);
  writer.set_quoting(quoting);
  writer.set_compression(compress);
  writer.write_main();
  return writer.get_result();
}


DECLARE_METHOD(&Frame::to_csv)
    ->name("to_csv")
    ->docs(dt::doc_Frame_to_csv)
    ->n_positional_or_keyword_args(1)
    ->n_keyword_args(9)
    ->arg_names({"path", "sep", "quoting", "append", "header", "bom",
                 "hex", "compression", "verbose", "method"})
    ->add_synonym_arg("_strategy", "method");



}  // namespace py
