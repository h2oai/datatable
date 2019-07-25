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

static PKArgs args_to_csv(
    0, 1, 4, false, false,
    {"path", "quoting", "hex", "verbose", "_strategy"},
    "to_csv",

R"(to_csv(self, path=None, *, quoting="minimal", hex=False,
       verbose=False, _strategy="auto")
--

Write the Frame into the provided file in CSV format.

Parameters
----------
path: str
    Path to the output CSV file that will be created. If the file
    already exists, it will be overwritten. If no path is given,
    then the Frame will be serialized into a string, and that string
    will be returned.

quoting: csv.QUOTE_* | "minimal" | "all" | "nonnumeric" | "none"
    csv.QUOTE_MINIMAL (0) -- quote the string fields only as
        necessary, i.e. if the string starts or ends with the
        whitespace, or contains quote characters, separator, or
        any of the C0 control characters (including newlines, etc).
    csv.QUOTE_ALL (1) -- all fields will be quoted, both string and
        numeric, and even boolean.
    csv.QUOTE_NONNUMERIC (2) -- all string fields will be quoted.
    csv.QUOTE_NONE (3) -- none of the fields will be quoted. This
        option must be used at user's own risk: the file produced
        may not be valid CSV.

hex: bool
    If True, then all floating-point values will be printed in hex
    format (equivalent to %a format in C `printf`). This format is
    around 3 times faster to write/read compared to usual decimal
    representation, so its use is recommended if you need maximum
    speed.

verbose: bool
    If True, some extra information will be printed to the console,
    which may help to debug the inner workings of the algorithm.

_strategy: "mmap" | "write" | "auto"
    Which method to use for writing to disk. On certain systems 'mmap'
    gives a better performance; on other OSes 'mmap' may not work at
    all.
)");


oobj Frame::to_csv(const PKArgs& args)
{
  // path
  oobj path = args[0].to<oobj>(ostring(""));
  if (!path.is_string()) {
    throw TypeError() << "Parameter `path` in Frame.to_csv() should be a "
        "string, instead got " << path.typeobj();
  }
  path = oobj::import("os", "path", "expanduser").call({path});
  std::string filename = path.to_string();

  // quoting
  int quoting;
  if (args[1].is_string()) {
    auto quoting_str = args[1].to_string();
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
    quoting = args[1].to<int>(0);
    if (quoting < 0 || quoting > 3) {
      throw ValueError() << "Invalid value of the `quoting` parameter in "
          "Frame.to_csv(): " << quoting;
    }
  }

  // hex
  bool hex = args[2].to<bool>(false);

  // verbose
  bool verbose = args[3].to<bool>(false);
  oobj logger;
  if (verbose) {
    logger = oobj::import("datatable", "_DefaultLogger").call();
  }

  auto strategy = args[4].to<std::string>("");
  auto sstrategy = (strategy == "mmap")  ? WritableBuffer::Strategy::Mmap :
                   (strategy == "write") ? WritableBuffer::Strategy::Write :
                                           WritableBuffer::Strategy::Auto;

  // Create the CsvWriter object
  dt::write::csv_writer writer(dt, filename);
  writer.set_strategy(sstrategy);
  writer.set_usehex(hex);
  writer.set_logger(logger);
  writer.set_quoting(quoting);
  writer.write_main();
  return writer.get_result();
}




//------------------------------------------------------------------------------
// Declare Frame methods
//------------------------------------------------------------------------------

void Frame::_init_tocsv(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::to_csv, args_to_csv));
}


}  // namespace py
