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
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "python/_all.h"
#include "python/string.h"
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

static const char* doc_to_csv =
R"(to_csv(self, path=None, *, quoting="minimal", append=False,
       header="auto", bom=False, hex=False, compression=None,
       verbose=False, method="auto")
--

Write the contents of the Frame into a CSV file.

This method uses multiple threads to serialize the Frame's data. The
number of threads is can be configured using the global option
``dt.options.nthreads``.

The method supports simple writing to file, appending to an existing
file, or creating a python string if no filename was provided.
Optionally, the output could be gzip-compressed.

Parameters
----------
path: str
    Path to the output CSV file that will be created. If the file
    already exists, it will be overwritten. If no path is given,
    then the Frame will be serialized into a string, and that string
    will be returned.

quoting: csv.QUOTE_* | "minimal" | "all" | "nonnumeric" | "none"
    `"minimal"` | `csv.QUOTE_MINIMAL`
        quote the string fields only as necessary, i.e. if the string
        starts or ends with the whitespace, or contains quote
        characters, separator, or any of the C0 control characters
        (including newlines, etc).

    `"all"` | `csv.QUOTE_ALL`
        all fields will be quoted, both string, numeric, and boolean.

    `"nonnumeric"` | `csv.QUOTE_NONNUMERIC`
        all string fields will be quoted.

    `"none"` | `csv.QUOTE_NONE`
        none of the fields will be quoted. This option must be used
        at user's own risk: the file produced may not be valid CSV.

append: bool
    If True, the file given in the `path` parameter will be opened
    for appending (i.e. mode="a"), or created if it doesn't exist.
    If False (default), the file given in the `path` will be
    overwritten if it already exists.

header: bool | "auto"
    This option controls whether or not to write headers into the
    output file. If this option is not given (or equal to ...), then
    the headers will be written unless the option `append` is True
    and the file `path` already exists. Thus, by default the headers
    will be written in all cases except when appending content into
    an existing file.

bom: bool
    If True, then insert the byte-order mark into the output file
    (the option is False by default). Even if the option is True,
    the BOM will not be written when appending data to an existing
    file.

    According to Unicode standard, including BOM into text files is
    "neither required nor recommended". However, some programs (e.g.
    Excel) may not be able to recognize file encoding without this
    mark.

hex: bool
    If True, then all floating-point values will be printed in hex
    format (equivalent to %a format in C `printf`). This format is
    around 3 times faster to write/read compared to usual decimal
    representation, so its use is recommended if you need maximum
    speed.

compression: None | "gzip" | "auto"
    Which compression method to use for the output stream. The default
    is "auto", which tries to infer the compression method from the
    output file's name. The only compression format currently supported
    is "gzip". Compression may not be used when `append` is True.

verbose: bool
    If True, some extra information will be printed to the console,
    which may help to debug the inner workings of the algorithm.

method: "mmap" | "write" | "auto"
    Which method to use for writing to disk. On certain systems 'mmap'
    gives a better performance; on other OSes 'mmap' may not work at
    all.

(return): None | str | bytes
    None if `path` is non-empty. This is the most common case: the
    output is written to the file provided.

    String containing the CSV text as if it would have been written
    to a file, if the path is empty or None. If the compression is
    turned on, a bytes object will be returned instead.
)";

static PKArgs args_to_csv(
    0, 1, 8, false, false,
    {"path", "quoting", "append", "header", "bom", "hex", "compression",
     "verbose", "method"},
    "to_csv", doc_to_csv);


oobj Frame::to_csv(const PKArgs& args)
{
  const Arg& arg_path     = args[0];
  const Arg& arg_quoting  = args[1];
  const Arg& arg_append   = args[2];
  const Arg& arg_header   = args[3];
  const Arg& arg_bom      = args[4];
  const Arg& arg_hex      = args[5];
  const Arg& arg_compress = args[6];
  const Arg& arg_verbose  = args[7];
  const Arg& arg_strategy = args[8];

  // path
  oobj path = arg_path.to<oobj>(ostring(""));
  if (!path.is_string()) {
    throw TypeError() << "Parameter `path` in Frame.to_csv() should be a "
        "string, instead got " << path.typeobj();
  }
  path = oobj::import("os", "path", "expanduser").call(path);
  std::string filename = path.to_string();

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
          "Frame.to_csv(): '" << quoting_str << "'";
    }
  } else {
    quoting = arg_quoting.to<int>(0);
    if (quoting < 0 || quoting > 3) {
      throw ValueError() << "Invalid value of the `quoting` parameter in "
          "Frame.to_csv(): " << quoting;
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
        << compress_str << "' in Frame.to_csv()";
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
  writer.set_verbose(verbose);
  writer.set_quoting(quoting);
  writer.set_compression(compress);
  writer.write_main();
  return writer.get_result();
}




//------------------------------------------------------------------------------
// Declare Frame methods
//------------------------------------------------------------------------------

void Frame::_init_tocsv(XTypeMaker& xt) {
  args_to_csv.add_synonym_arg("_strategy", "method");
  xt.add(METHOD(&Frame::to_csv, args_to_csv));
}


}  // namespace py
