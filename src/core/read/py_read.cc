//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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


#if 0
// Return true if `text` has any characters from C0 range.
static bool _has_control_characters(const CString& text) {
  size_t n = static_cast<size_t>(text.size);
  const char* ch = text.ch;
  for (size_t i = 0; i < n; ++i) {
    if (static_cast<unsigned char>(ch[i]) < 0x20) {
      return true;
    }
  }
  return false;
}


static bool _looks_like_url(const CString& text) {
  size_t n = static_cast<size_t>(text.size);
  const char* ch = text.ch;
  if (n >= 8) {
    if (std::memcmp(ch, "https://", 8) == 0) return true;
    if (std::memcmp(ch, "http://", 7) == 0) return true;
    if (std::memcmp(ch, "file://", 7) == 0) return true;
    if (std::memcmp(ch, "ftp://", 6) == 0) return true;
  }
  return false;
}


static bool _looks_like_glob(const CString& text) {
  size_t n = static_cast<size_t>(text.size);
  const char* ch = text.ch;
  for (size_t i = 0; i < n; ++i) {
    char c = ch[i];
    if (c == '*' || c == '?' || c == '[' || c == ']') return true;
  }
  return false;
}


static ReadSource _resolve_source_any(const py::Arg& src, GenericReader& gr) {
  (void)gr;
  xassert(src.is_defined());
  auto srcobj = src.to_oobj();
  if (src.is_string() || src.is_bytes()) {
    CString cstr = src.to_cstring();
    if (cstr.size >= 4096 || _has_control_characters(cstr)) {
      return ReadSource::from_text(srcobj);
    }
    // if (_looks_like_url(cstr)) {
    //   return ReadSource::from_url(srcobj);
    // }
  }
  #if 0
    if isinstance(src, (str, bytes)):
        if len(src) >= 4096 or _has_control_characters(src):
            return FreadSource_Text(src, "<text>", params)
        if isinstance(src, str):
            if _url_regex.match(src):
                return FreadSource_Url(src, src, params)
            if _glob_regex.search(src):
                return FreadSource_Glob(src, src, params)
        p = pathlib.Path(src)
        return FreadSource_Path(p, params)

    if isinstance(src, os.PathLike):
        return FreadSource_Path(src, params)

    if hasattr(src, "read"):
        return FreadSource.from_fileobj(src)

    if isinstance(src, (list, tuple)):
        return FreadSource.from_list(src)

  #endif
  throw TypeError() << "Unknown type for the first argument in fread: "
                    << src.typeobj();
}

#endif
