#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2020 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------
#
#  python ci/headers.py src/core
#
#-------------------------------------------------------------------------------
import glob
import os
import re

c_extensions = [".c", ".cc", ".cxx", ".cpp", ".c++"]
h_extensions = [".h", ".hh", ".hxx", ".hpp", ".h++"]
all_extensions = c_extensions + h_extensions

std_symbols_library = {
    "std::abort":                              ["cstdlib"],
    "std::abs":                                ["cstdlib", "cmath", "complex", "valarray"],
    "std::accumulate":                         ["numeric"],
    "std::acos":                               ["cmath"],
    "std::acosh":                              ["cmath"],
    "std::add_const":                          ["type_traits"],
    "std::add_cv":                             ["type_traits"],
    "std::add_lvalue_reference":               ["type_traits"],
    "std::add_pointer":                        ["type_traits"],
    "std::add_rvalue_reference":               ["type_traits"],
    "std::add_volatile":                       ["type_traits"],
    "std::addressof":                          ["memory"],
    "std::align":                              ["memory"],
    "std::aligned_storage":                    ["type_traits"],
    "std::allocator":                          ["memory"],
    "std::allocator_traits":                   ["memory"],
    "std::array":                              ["array"],
    "std::asin":                               ["cmath"],
    "std::asinh":                              ["cmath"],
    "std::atan":                               ["cmath"],
    "std::atan2":                              ["cmath"],
    "std::atanh":                              ["cmath"],
    "std::atomic":                             ["atomic"],
    "std::atomic_flag":                        ["atomic"],
    "std::bad_alloc":                          ["new"],
    "std::bad_function_call":                  ["functional"],
    "std::bad_optional_access":                ["optional"],
    "std::begin":                              ["array", "deque", "forward_list", "iterator", "list", "map", "regex", "set", "span", "string", "string_view", "unordered_map", "unordered_set", "vector"],
    "std::bidirectional_iterator_tag":         ["iterator"],
    "std::bitset":                             ["bitset"],
    "std::bsearch":                            ["cstdlib"],
    "std::cbrt":                               ["cmath"],
    "std::ceil":                               ["cmath"],
    "std::ceilf":                              ["cmath"],
    "std::ceill":                              ["cmath"],
    "std::cerr":                               ["iostream"],
    "std::chrono":                             ["chrono"],
    "std::common_reference":                   ["type_traits"],
    "std::common_type":                        ["type_traits"],
    "std::condition_variable":                 ["condition_variable"],
    "std::condition_variable_any":             ["condition_variable"],
    "std::conditional":                        ["type_traits"],
    "std::conditional_t":                      ["type_traits"],
    "std::contiguous_iterator_tag":            ["iterator"],
    "std::copysign":                           ["cmath"],
    "std::copysignf":                          ["cmath"],
    "std::copysignl":                          ["cmath"],
    "std::cos":                                ["cmath"],
    "std::cosf":                               ["cmath"],
    "std::cosh":                               ["cmath"],
    "std::cosl":                               ["cmath"],
    "std::count":                              ["algorithm"],
    "std::cout":                               ["iostream"],
    "std::current_exception":                  ["exception"],
    "std::dec":                              ["ios"],
    "std::decay":                              ["type_traits"],
    "std::declval":                            ["utility"],
    "std::default_random_engine":              ["random"],
    "std::distance":                           ["iterator"],
    "std::domain_error":                       ["stdexcept"],
    "std::dynamic_extent":                     ["span"],
    "std::enable_if":                          ["type_traits"],
    "std::end":                                ["array", "deque", "forward_list", "iterator", "list", "map", "regex", "set", "span", "string", "string_view", "unordered_map", "unordered_set", "vector"],
    "std::equal":                              ["algorithm"],
    "std::equal_to":                           ["functional"],
    "std::erf":                                ["cmath"],
    "std::erfc":                               ["cmath"],
    "std::exception":                          ["exception"],
    "std::exception_ptr":                      ["exception"],
    "std::exp":                                ["cmath"],
    "std::exp2":                               ["cmath"],
    "std::expm1":                              ["cmath"],
    "std::fabs":                               ["cmath", "cstdlib"],
    "std::fabsf":                              ["cmath", "cstdlib"],
    "std::fabsl":                              ["cmath", "cstdlib"],
    "std::false_type":                         ["type_traits"],
    "std::FILE":                               ["cstdio"],
    "std::fill":                               ["algorithm"],
    "std::fixed":                              ["ios"],
    "std::floor":                              ["cmath"],
    "std::floorf":                             ["cmath"],
    "std::floorl":                             ["cmath"],
    "std::flush":                              ["ostream"],
    "std::fmod":                               ["cmath"],
    "std::fmodf":                              ["cmath"],
    "std::fmodl":                              ["cmath"],
    "std::fclose":                             ["cstdio"],
    "std::perror":                             ["cstdio"],
    "std::remove":                             ["cstdio"],
    "std::fopen":                              ["cstdio"],
    "std::forward":                            ["utility"],
    "std::forward_as_tuple":                   ["tuple"],
    "std::forward_iterator_tag":               ["iterator"],
    "std::fprintf":                            ["cstdio"],
    "std::free":                               ["cstdlib"],
    "std::function":                           ["functional"],
    "std::future":                             ["future"],
    "std::get":                                ["array", "tuple"],
    "std::hash":                               ["functional"],
    "std::hex":                                ["ios"],
    "std::hypot":                              ["cmath"],
    "std::in_place":                           ["utility"],
    "std::in_place_index":                     ["utility"],
    "std::in_place_index_t":                   ["utility"],
    "std::in_place_t":                         ["utility"],
    "std::in_place_type":                      ["utility"],
    "std::in_place_type_t":                    ["utility"],
    "std::initializer_list":                   ["initializer_list"],
    "std::input_iterator_tag":                 ["iterator"],
    "std::integral_constant":                  ["type_traits"],
    "std::invalid_argument":                   ["stdexcept"],
    "std::iota":                               ["numeric"],
    "std::is_array":                           ["type_traits"],
    "std::is_assignable":                      ["type_traits"],
    "std::is_base_of":                         ["type_traits"],
    "std::is_class":                           ["type_traits"],
    "std::is_const":                           ["type_traits"],
    "std::is_constructible":                   ["type_traits"],
    "std::is_convertible":                     ["type_traits"],
    "std::is_copy_constructible":              ["type_traits"],
    "std::is_default_constructible":           ["type_traits"],
    "std::is_destructible":                    ["type_traits"],
    "std::is_empty":                           ["type_traits"],
    "std::is_enum":                            ["type_traits"],
    "std::is_floating_point":                  ["type_traits"],
    "std::is_function":                        ["type_traits"],
    "std::is_integral":                        ["type_traits"],
    "std::is_lvalue_reference":                ["type_traits"],
    "std::is_move_constructible":              ["type_traits"],
    "std::is_nothrow_copy_constructible":      ["type_traits"],
    "std::is_nothrow_default_constructible":   ["type_traits"],
    "std::is_nothrow_destructible":            ["type_traits"],
    "std::is_nothrow_move_assignable":         ["type_traits"],
    "std::is_nothrow_move_constructible":      ["type_traits"],
    "std::is_null_pointer":                    ["type_traits"],
    "std::is_object":                          ["type_traits"],
    "std::is_place_t":                         ["utility"],
    "std::is_pointer":                         ["type_traits"],
    "std::is_reference":                       ["type_traits"],
    "std::is_rvalue_reference":                ["type_traits"],
    "std::is_same":                            ["type_traits"],
    "std::is_scalar":                          ["type_traits"],
    "std::is_signed":                          ["type_traits"],
    "std::is_standard_layout":                 ["type_traits"],
    "std::is_trivial":                         ["type_traits"],
    "std::is_trivially_copy_assignable":       ["type_traits"],
    "std::is_trivially_copy_constructible":    ["type_traits"],
    "std::is_trivially_default_constructible": ["type_traits"],
    "std::is_trivially_destructible":          ["type_traits"],
    "std::is_union":                           ["type_traits"],
    "std::is_unsigned":                        ["type_traits"],
    "std::is_void":                            ["type_traits"],
    "std::is_volatile":                        ["type_traits"],
    "std::isfinite":                           ["cmath"],
    "std::isinf":                              ["cmath"],
    "std::isnan":                              ["cmath"],
    "std::ldexp":                              ["cmath"],
    "std::ldexpf":                             ["cmath"],
    "std::ldexpl":                             ["cmath"],
    "std::length_error":                       ["stdexcept"],
    "std::lexicographical_compare":            ["algorithm"],
    "std::lgamma":                             ["cmath"],
    "std::localtime":                          ["ctime"],
    "std::lock":                               ["mutex"],
    "std::lock_guard":                         ["mutex"],
    "std::log":                                ["cmath"],
    "std::log10":                              ["cmath"],
    "std::log1p":                              ["cmath"],
    "std::log2":                               ["cmath"],
    "std::logic_error":                        ["stdexcept"],
    "std::lrint":                              ["cmath"],
    "std::lrintf":                             ["cmath"],
    "std::lrintl":                             ["cmath"],
    "std::make_optional":                      ["optional"],
    "std::make_pair":                          ["utility"],
    "std::make_shared":                        ["memory"],
    "std::make_signed":                        ["type_traits"],
    "std::make_tuple":                         ["tuple"],
    "std::make_unique":                        ["memory"],
    "std::make_unsigned":                      ["type_traits"],
    "std::map":                                ["map"],
    "std::max":                                ["algorithm"],
    "std::max_element":                        ["algorithm"],
    "std::memcmp":                             ["cstring"],
    "std::memcpy":                             ["cstring"],
    "std::memmove":                            ["cstring"],
    "std::memory_order":                       ["atomic"],
    "std::memory_order_acq_rel":               ["atomic"],
    "std::memory_order_acquire":               ["atomic"],
    "std::memory_order_consume":               ["atomic"],
    "std::memory_order_relaxed":               ["atomic"],
    "std::memory_order_release":               ["atomic"],
    "std::memory_order_seq_cst":               ["atomic"],
    "std::memset":                             ["cstring"],
    "std::min":                                ["algorithm"],
    "std::min_element":                        ["algorithm"],
    "std::move":                               ["utility"],
    "std::mt19937":                            ["random"],
    "std::mutex":                              ["mutex"],
    "std::nan":                                ["cmath"],
    "std::nanf":                               ["cmath"],
    "std::nanl":                               ["cmath"],
    "std::next":                               ["iterator"],
    "std::nextafter":                          ["cmath"],
    "std::nextafterf":                         ["cmath"],
    "std::nextafterl":                         ["cmath"],
    "std::nexttoward":                         ["cmath"],
    "std::nexttowardf":                        ["cmath"],
    "std::nexttowardl":                        ["cmath"],
    "std::normal_distribution":                ["random"],
    "std::nullopt":                            ["optional"],
    "std::nullopt_t":                          ["optional"],
    "std::nullptr_t":                          ["cstddef"],
    "std::numeric_limits":                     ["limits"],
    "std::optional":                           ["optional"],
    "std::ostringstream":                      ["sstream"],
    "std::out_of_range":                       ["stdexcept"],
    "std::output_iterator_tag":                ["iterator"],
    "std::overflow_error":                     ["stdexcept"],
    "std::packaged_task":                      ["future"],
    "std::pair":                               ["utility"],
    "std::piecewise_construct":                ["utility"],
    "std::piecewise_construct_t":              ["utility"],
    "std::pow":                                ["cmath"],
    "std::printf":                             ["cstdio"],
    "std::ptrdiff_t":                          ["cstddef"],
    "std::rand":                               ["cstdlib"],
    "std::random_access_iterator_tag":         ["iterator"],
    "std::random_device":                      ["random"],
    "std::range_error":                        ["stdexcept"],
    "std::realloc":                            ["cstdlib"],
    "std::recursive_mutex":                    ["mutex"],
    "std::regex":                              ["regex"],
    "std::regex_constants":                    ["regex"],
    "std::regex_error":                        ["regex"],
    "std::regex_match":                        ["regex"],
    "std::remove_all_extents":                 ["type_traits"],
    "std::remove_const":                       ["type_traits"],
    "std::remove_cv":                          ["type_traits"],
    "std::remove_extent":                      ["type_traits"],
    "std::remove_pointer":                     ["type_traits"],
    "std::remove_reference":                   ["type_traits"],
    "std::remove_volatile":                    ["type_traits"],
    "std::result_of":                          ["type_traits"],
    "std::rethrow_exception":                  ["exception"],
    "std::reverse_iterator":                   ["iterator"],
    "std::rint":                               ["cmath"],
    "std::rintf":                              ["cmath"],
    "std::rintl":                              ["cmath"],
    "std::rotate":                             ["algorithm"],
    "std::runtime_error":                      ["stdexcept"],
    "std::search":                             ["algorithm"],
    "std::set":                                ["set"],
    "std::setbase":                            ["iomanip"],
    "std::setfill":                            ["iomanip"],
    "std::setprecision":                       ["iomanip"],
    "std::setw":                               ["iomanip"],
    "std::shared_lock":                        ["shared_mutex"],
    "std::shared_mutex":                       ["shared_mutex"],
    "std::shared_ptr":                         ["memory"],
    "std::sig_atomic_t":                       ["csignal"],
    "std::signal":                             ["csignal"],
    "std::signbit":                            ["cmath"],
    "std::sin":                                ["cmath"],
    "std::sinf":                               ["cmath"],
    "std::sinh":                               ["cmath"],
    "std::sinl":                               ["cmath"],
    "std::size_t":                             ["cstddef", "cstdio", "cstdlib", "cstring", "ctime"],
    "std::snprintf":                           ["cstdio"],
    "std::sort":                               ["algorithm"],
    "std::span":                               ["span"],
    "std::sprintf":                            ["cstdio"],
    "std::sqrt":                               ["cmath"],
    "std::sqrtf":                              ["cmath"],
    "std::sqrtl":                              ["cmath"],
    "std::srand":                              ["cstdlib"],
    "std::stable_sort":                        ["algorithm"],
    "std::stack":                              ["stack"],
    "std::strcmp":                             ["cstring"],
    "std::strerror":                           ["cstring"],
    "std::string":                             ["string"],
    "std::string_view":                        ["string_view"],
    "std::stringstream":                       ["sstream"],
    "std::strlen":                             ["cstring"],
    "std::strncmp":                            ["cstring"],
    "std::strncpy":                            ["cstring"],
    "std::strrchr":                            ["cstring"],
    "std::swap":                               ["algorithm", "utility", "string_view"],
    "std::tan":                                ["cmath"],
    "std::tanf":                               ["cmath"],
    "std::tanh":                               ["cmath"],
    "std::tanhf":                              ["cmath"],
    "std::tanhl":                              ["cmath"],
    "std::tanl":                               ["cmath"],
    "std::tgamma":                             ["cmath"],
    "std::this_thread":                        ["thread"],
    "std::thread":                             ["thread"],
    "std::tie":                                ["tuple"],
    "std::time":                               ["ctime"],
    "std::time_t":                             ["ctime"],
    "std::tm":                                 ["ctime"],
    "std::to_address":                         ["memory"],
    "std::to_string":                          ["string"],
    "std::tr1":                                [],
    "std::transform":                          ["algorithm"],
    "std::true_type":                          ["type_traits"],
    "std::trunc":                              ["cmath"],
    "std::tuple":                              ["tuple"],
    "std::tuple_element":                      ["tuple", "array", "utility"],
    "std::tuple_size":                         ["tuple", "array", "utility"],
    "std::uncaught_exception":                 ["exception"],
    "std::underflow_error":                    ["stdexcept"],
    "std::underlying_type":                    ["type_traits"],
    "std::uniform_int_distribution":           ["random"],
    "std::unique_lock":                        ["mutex"],
    "std::unique_ptr":                         ["memory"],
    "std::unordered_map":                      ["unordered_map"],
    "std::unordered_set":                      ["unordered_set"],
    "std::vector":                             ["vector"],
    "std::weak_ptr":                           ["memory"],
}



class Source:

    def __init__(self, srcpath, basepath):
        self._path = os.path.relpath(srcpath, basepath)
        self._lines = None
        self._sys_includes_base = []
        self._src_includes_base = []
        self._sys_includes_resolved = None
        self._std_symbols = None
        self.read_source(srcpath)
        self.remove_comments()
        self.find_includes()

    @property
    def path(self):
        return self._path

    @property
    def src_includes(self):
        return self._src_includes_base

    @property
    def sys_includes(self):
        return self._sys_includes_base



    def read_source(self, filename):
        with open(filename, "rt") as inp:
            self._lines = [line.strip() for line in inp]


    def remove_comments(self):
        def process_line(line, status):
            if status is None:        return process_linestart(line)
            elif status == "string":  return process_string("", line, '"')
            elif status == "rstring": return process_rstring("", line)
            elif status == "comment": return process_comment("", line)
            else:                     raise RuntimeError(status)

        def process_linestart(line):
            if line.startswith("#"):
                mm = re.match(r'#\s*include\s*(".*?"|<.*?>)', line)
                if mm:
                    assert mm.start() == 0
                    i = mm.end()
                    return process_normal(line[:i], line[i:])
            return process_normal('', line)

        def process_normal(prefix, line):
            for i, ch in enumerate(line):
                if ch == '"':
                    return process_string(prefix + line[:i+1], line[i+1:], ch)
                if ch == "'":
                    return process_string(prefix + line[:i+1], line[i+1:], ch)
                if ch == '/':
                    nextch = line[i+1:i+2]
                    if nextch == '/':
                        return (prefix + line[:i], None)
                    if nextch == '*':
                        return process_comment(prefix + line[:i], line[i+2:])
                if ch == 'R' and line[i:i+3] == 'R"(':
                    return process_rstring(prefix + line[:i] + '"', line[i+3:])
            return prefix + line, None

        def process_string(prefix, line, quote):
            skip_next = False
            for i, ch in enumerate(line):
                if skip_next:
                    skip_next = False
                    continue
                if ch == quote:  # end of string
                    return process_normal(prefix + ch, line[i+1:])
                if ch == '\\':
                    skip_next = True
            return (prefix, "string")

        def process_rstring(prefix, line):
            for i, ch in enumerate(line):
                if ch == ')' and line[i:i+2] == ')"':
                    return process_normal(prefix + '"', line[i+2:])
            return (prefix, "rstring")

        def process_comment(prefix, line):
            for i, ch in enumerate(line):
                if ch == '*' and line[i:i+2] == '*/':
                    return process_normal(prefix, line[i+2:])
            return (prefix, "comment")


        out = []
        status = None
        for line in self._lines:
            line, status = process_line(line, status)
            out.append(line)
        assert status is None, "status=%r when parsing file %s" % (status, self.path)
        self._lines = out


    def find_includes(self):
        for line in self._lines:
            if line.startswith("#"):
                mm = re.match(r'#\s*include\s*(".*?"|<.*?>)', line)
                if mm:
                    quoted = mm.group(1)
                    if quoted[0] == '<':
                        self._sys_includes_base.append(quoted[1:-1])
                    else:
                        self._src_includes_base.append(quoted[1:-1])

    def resolve_includes(self, all_sources):
        assert isinstance(all_sources, dict)
        assert self.path in all_sources
        if self._sys_includes_resolved is None:
            self._sys_includes_resolved = set(self._sys_includes_base)
            for include_path in self._src_includes_base:
                if include_path == "../datatable/include/datatable.h":
                    continue
                if include_path not in all_sources:
                    raise ValueError('Path "%s" (#include\'d from %s) is not in '
                                     'the list of all sources'
                                     % (include_path, self.path))
                incl = all_sources[include_path]
                incl_sys_includes = incl.resolve_includes(all_sources)
                self._sys_includes_resolved |= incl_sys_includes
        return self._sys_includes_resolved


    def find_std_symbols(self):
        symbols = set()
        for line in self._lines:
            matches = re.findall(r'std::\w+', line)
            symbols |= set(matches)
        self._std_symbols = symbols


    def check_std_symbols(self):
        self.find_std_symbols()
        includes = self._sys_includes_resolved
        errors_found = 0
        for std_symbol in self._std_symbols:
            if std_symbol == "std::experimental": continue
            headers = std_symbols_library.get(std_symbol)
            if headers is None:
                print("Unknown symbol %s in file %s"
                      % (std_symbol, self._path))
            elif not headers or includes.intersection(headers):
                pass
            else:
                errors_found += 1
                print("Missing header <%s> for symbol %s in file %s"
                      % (headers[0], std_symbol, self._path))
        return errors_found



def analyze(path):
    all_sources = {}
    files = glob.glob(path + "/**", recursive=True)
    for i, entry in enumerate(files):
        if not os.path.isfile(entry):
            continue
        ext = os.path.splitext(entry)[1]
        if ext.lower() not in all_extensions:
            continue

        src = Source(entry, path)
        all_sources[src.path] = src

    n_errors = 0
    for i, src in enumerate(all_sources.values()):
        src.resolve_includes(all_sources)
        n_errors += src.check_std_symbols()
    if n_errors:
        print("-----------\n%d errors found" % n_errors)
    else:
        print("ok")




if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description='Analyze C/C++ source files to detect missing standard '
                    'headers')
    parser.add_argument("path", help="Path to the directory with C/C++ source "
                                     "files")

    args = parser.parse_args()
    analyze(args.path)
