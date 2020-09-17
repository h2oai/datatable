//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include <algorithm>          // std::min
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include "cstring.h"
#include "expr/declarations.h"
#include "expr/fexpr_column.h"
#include "frame/py_frame.h"
#include "python/dict.h"
#include "python/int.h"
#include "python/string.h"
#include "python/tuple.h"
#include "utils/assert.h"
#include "utils/fuzzy_match.h"
#include "utils/tests.h"
#include "options.h"
#include "stype.h"

static Error _name_not_found_error(const DataTable* dt, const std::string& name)
{
  Error err = KeyError();
  // TODO: sanitize column's name: limit the display size, escape
  //       special characters, handle the empty string
  err << "Column `" << escape_backticks(name)
      << "` does not exist in the Frame";
  std::string suggested = dt::suggest_similar_strings(dt->get_names(), name);
  if (!suggested.empty()) {
    err << "; did you mean " << suggested << "?";
  }
  return err;
}



//------------------------------------------------------------------------------
// "Names provider" helper classes
//------------------------------------------------------------------------------

class NameProvider {
  public:
    virtual ~NameProvider() {}  // LCOV_EXCL_LINE
    virtual size_t size() const = 0;
    virtual dt::CString item_as_cstring(size_t i) = 0;
    virtual py::oobj item_as_pyoobj(size_t i) = 0;
};



class pylistNP : public NameProvider {
  private:
    const py::olist& names;

  public:
    explicit pylistNP(const py::olist& arg)
      : names(arg) {}

    virtual size_t size() const override {
      return names.size();
    }

    virtual dt::CString item_as_cstring(size_t i) override {
      py::robj name = names[i];
      if (!name.is_string() && !name.is_none()) {
        throw TypeError() << "Invalid `names` list: element " << i
            << " is not a string";
      }
      return name.to_cstring();
    }

    virtual py::oobj item_as_pyoobj(size_t i) override {
      return py::oobj(names[i]);
    }
};



class strvecNP : public NameProvider {
  private:
    const strvec& names;

  public:
    explicit strvecNP(const strvec& arg)
      : names(arg) {}

    virtual size_t size() const override {
      return names.size();
    }

    virtual dt::CString item_as_cstring(size_t i) override {
      const std::string& name = names[i];
      return dt::CString { name.data(), name.size() };
    }

    virtual py::oobj item_as_pyoobj(size_t i) override {
      return py::ostring(names[i]);
    }
};




//------------------------------------------------------------------------------
// colindex()
//------------------------------------------------------------------------------
namespace py {


static const char* doc_colindex = R"(colindex(self, column)
--

Return the position of the `column` in the Frame.

The index of the first column is `0`, just as with regular python
lists.


Parameters
----------
column: str | int | FExpr
    If string, then this is the name of the column whose index you
    want to find.

    If integer, then this represents a column's index. The return
    value is thus the same as the input argument `column`, provided
    that it is in the correct range. If the `column` argument is
    negative, then it is interpreted as counting from the end of the
    frame. In this case the positive value `column + ncols` is
    returned.

    Lastly, `column` argument may also be an
    :ref:`f-expression <f-expressions>` such as `f.A` or `f[3]`. This
    case is treated as if the argument was simply `"A"` or `3`. More
    complicated f-expressions are not allowed and will result in a
    `TypeError`.

(return): int
    The numeric index of the provided `column`. This will be an
    integer between `0` and `self.ncols - 1`.

(except): KeyError | IndexError
    If the `column` argument is a string, and the column with such
    name does not exist in the frame, then a :exc:`KeyError` is raised.
    When this exception is thrown, the error message may contain
    suggestions for up to 3 similarly looking column names that
    actually exist in the Frame.

    If the `column` argument is an integer that is either greater
    than or equal to :attr:`ncols <Frame.ncols>` or less than
    `-ncols`, then an :exc:`IndexError` is raised.


Examples
--------

>>> df = dt.Frame(A=[3, 14, 15], B=["enas", "duo", "treis"],
...               C=[0, 0, 0])
>>> df.colindex("B")
1
>>> df.colindex(-1)
2

>>> from datatable import f
>>> df.colindex(f.A)
0
)";

static PKArgs args_colindex(
    1, 0, 0, false, false, {"column"}, "colindex", doc_colindex
);


oobj Frame::colindex(const PKArgs& args) {
  auto arg_col = args[0];
  if (!arg_col) {
    throw TypeError() << "Frame.colindex() is missing the required "
                         "positional argument `column`";
  }
  oobj col = arg_col.to_oobj();

  if (col.is_dtexpr()) {
    auto op = col.get_attr("_op").to_size_t();
    if (op == 1) {
      col = col.get_attr("_args").to_otuple()[0];
    }
    // fall-through
  }
  else if (col.is_fexpr()) {
    auto fexpr = dt::expr::as_fexpr(col);
    auto fexpr_col1 = dynamic_cast<dt::expr::FExpr_ColumnAsArg*>(fexpr.get());
    if (fexpr_col1) {
      auto arg = fexpr_col1->get_arg();
      if (arg->get_expr_kind() == dt::expr::Kind::Int) {
        auto i = arg->evaluate_int();
        return py::oint(dt->xcolindex(i));
      }
      if (arg->get_expr_kind() == dt::expr::Kind::Str) {
        col = arg->evaluate_pystr();
      }
    }
    auto fexpr_col2 = dynamic_cast<dt::expr::FExpr_ColumnAsAttr*>(fexpr.get());
    if (fexpr_col2) {
      col = fexpr_col2->get_pyname();
    }
  }
  if (col.is_string()) {
    size_t index = dt->xcolindex(col);
    return py::oint(index);
  }
  if (col.is_int()) {
    // dt->xcolindex() throws an exception if column index is out of bounds
    size_t index = dt->xcolindex(col.to_int64_strict());
    return py::oint(index);
  }
  throw TypeError() << "The argument to Frame.colindex() should be a string "
      "or an integer, not " << col.typeobj();
}


} // namespace py



//------------------------------------------------------------------------------
// .names
//------------------------------------------------------------------------------
namespace py {

static const char* doc_names =
R"(
The tuple of names of all columns in the frame.

Each name is a non-empty string not containing any ASCII control
characters, and jointly the names are unique within the frame.

This property is also assignable: setting ``DT.names`` has the effect
of renaming the frame's columns without changing their order. When
renaming, the length of the new list of names must be the same as the
number of columns in the frame. It is also possible to rename just a
few of the columns by assigning a dictionary ``{oldname: newname}``.
Any column not listed in the dictionary will keep its old name.

When setting new column names, we will verify whether they satisfy
the requirements mentioned above. If not, a warning will be emitted
and the names will be automatically :ref:`mangled <name-mangling>`.

Parameters
----------
return: Tuple[str, ...]
    When used in getter form, this property returns the names of all
    frame's columns, as a tuple. The length of the tuple is equal to
    the number of columns in the frame, :attr:`ncols <Frame.ncols>`.

newnames: List[str?] | Tuple[str?, ...] | Dict[str, str?] | None
    The most common form is to assign the list or tuple of new
    column names. The length of the new list must be equal to the
    number of columns in the frame. Some (or all) elements in the list
    may be ``None``'s, indicating that that column should have
    an auto-generated name.

    If ``newnames`` is a dictionary, then it provides a mapping from
    old to new column names. The dictionary may contain less entries
    than the number of columns in the frame: the columns not mentioned
    in the dictionary will retain their names.

    Setting the ``.names`` to ``None`` is equivalent to using the
    ``del`` keyword: the names will be set to their default values,
    which are usually ``C0, C1, ...``.

except: ValueError
    If the length of the list/tuple `newnames` does not match the
    number of columns in the frame.

except: KeyError
    If `newnames` is a dictionary containing entries that do not
    match any of the existing columns in the frame.

Examples
--------
>>> DT = dt.Frame([[1], [2], [3]])
>>> DT.names = ['A', 'B', 'C']
>>> DT.names
('A', 'B', 'C')
>>> DT.names = {'B': 'middle'}
>>> DT.names
('A', 'middle', 'C')
>>> del DT.names
>>> DT.names
('C0', 'C1', 'C2)
)";

static GSArgs args_names("names", doc_names);


oobj Frame::get_names() const {
  return dt->get_pynames();
}


void Frame::set_names(const Arg& arg)
{
  if (arg.is_undefined() || arg.is_none()) {
    dt->set_names_to_default();
  }
  else if (arg.is_list() || arg.is_tuple()) {
    dt->set_names(arg.to_pylist());
  }
  else if (arg.is_dict()) {
    dt->replace_names(arg.to_pydict());
  }
  else {
    throw TypeError() << "Expected a list of strings, got " << arg.typeobj();
  }
}



void Frame::_init_names(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::colindex, args_colindex));
  xt.add(GETSET(&Frame::get_names, &Frame::set_names, args_names));
}


} // namespace py




//------------------------------------------------------------------------------
// Options
//------------------------------------------------------------------------------

static const char * doc_options_frame_names_auto_index =
R"(
When Frame needs to auto-name columns, they will be assigned
names `C0`, `C1`, `C2`, etc. by default. This option allows you to
control the starting index in this sequence. For example, setting
`dt.options.frame.names_auto_index=1` will cause the columns to be
named `C1`, `C2`, `C3`, etc.

See Also
--------
- :ref:`name-mangling` -- tutorial on name mangling.

)";

static const char * doc_options_frame_names_auto_prefix =
R"(
When Frame needs to auto-name columns, they will be assigned
names `C0`, `C1`, `C2`, etc. by default. This option allows you to
control the prefix used in this sequence. For example, setting
`dt.options.frame.names_auto_prefix='Z'` will cause the columns to be
named `Z0`, `Z1`, `Z2`, etc.

See Also
--------
- :ref:`name-mangling` -- tutorial on name mangling.

)";

static int64_t     names_auto_index = 0;
static std::string names_auto_prefix = "C";

void py::Frame::init_names_options() {
  dt::register_option(
    "frame.names_auto_index",
    []{ return py::oint(names_auto_index); },
    [](const py::Arg& value){ names_auto_index = value.to_int64_strict(); },
    doc_options_frame_names_auto_index
  );

  dt::register_option(
    "frame.names_auto_prefix",
    []{ return py::ostring(names_auto_prefix); },
    [](const py::Arg& value){ names_auto_prefix = value.to_string(); },
    doc_options_frame_names_auto_prefix
  );
}



//------------------------------------------------------------------------------
// DataTable names API
//------------------------------------------------------------------------------

/**
 * Return DataTable column names as a C++ vector of strings.
 */
const strvec& DataTable::get_names() const {
  return names_;
}


/**
 * Return DataTable column names as a python tuple.
 */
py::otuple DataTable::get_pynames() const {
  if (!py_names_) _init_pynames();
  return py_names_;
}


/**
 * Return the index of a column given its name; or -1 if such column does not
 * exist in the DataTable.
 */
int64_t DataTable::colindex(const py::_obj& pyname) const {
  if (!py_names_) _init_pynames();
  py::robj pyindex = py_inames_.get(pyname);
  return pyindex? pyindex.to_int64_strict() : -1;
}


/**
 * Return the index of a column given its name; throw an exception if the
 * column does not exist in the DataTable.
 */
size_t DataTable::xcolindex(const py::_obj& pyname) const {
  if (!py_names_) _init_pynames();
  py::robj pyindex = py_inames_.get(pyname);
  if (!pyindex) {
    throw _name_not_found_error(this, pyname.to_string());
  }
  return pyindex.to_size_t();
}


/**
 * Copy names without checking for validity, since we know they were already
 * verified in DataTable `other`.
 */
void DataTable::copy_names_from(const DataTable& other) {
  names_ = other.names_;
  py_names_ = other.py_names_;
  py_inames_ = other.py_inames_;
}


/**
 * Initialize DataTable's column names to the default "C0", "C1", "C2", ...
 */
void DataTable::set_names_to_default() {
  auto index0 = static_cast<size_t>(names_auto_index);
  py_names_  = py::otuple();
  py_inames_ = py::odict();
  names_.clear();
  names_.reserve(ncols_);
  for (size_t i = 0; i < ncols_; ++i) {
    names_.push_back(names_auto_prefix + std::to_string(i + index0));
  }
}


void DataTable::set_names(const py::olist& names_list, bool warn) {
  if (!names_list) return set_names_to_default();
  pylistNP np(names_list);
  _set_names_impl(&np, warn);
  columns_.resize(names_.size());
}


void DataTable::set_names(const strvec& names_list, bool warn) {
  xassert(names_list.size() == ncols_);
  strvecNP np(names_list);
  _set_names_impl(&np, warn);
}


void DataTable::replace_names(py::odict replacements, bool warn) {
  py::olist newnames(ncols_);

  for (size_t i = 0; i < ncols_; ++i) {
    newnames.set(i, py_names_[i]);
  }
  for (auto kv : replacements) {
    py::robj key = kv.first;
    py::robj val = kv.second;
    py::robj idx = py_inames_.get(key);
    if (idx.is_undefined()) {
      throw KeyError() << "Cannot find column `" << key.str()
        << "` in the Frame";
    }
    if (!val.is_string()) {
      throw TypeError() << "The replacement name for column `" << key.str()
        << "` should be a string, but got " << val.typeobj();
    }
    int64_t i = idx.to_int64_strict();
    newnames.set(i, val);
  }
  set_names(newnames, warn);
}


void DataTable::reorder_names(const std::vector<size_t>& col_indices) {
  xassert(col_indices.size() == ncols_);
  strvec newnames;
  newnames.reserve(ncols_);
  for (size_t i = 0; i < ncols_; ++i) {
    newnames.push_back(std::move(names_[col_indices[i]]));
  }
  names_ = std::move(newnames);
  if (py_names_) {
    py::otuple new_py_names(ncols_);
    for (size_t i = 0; i < ncols_; ++i) {
      py::robj pyname = py_names_[col_indices[i]];
      new_py_names.set(i, pyname);
      py_inames_.set(pyname, py::oint(i));
    }
    py_names_ = std::move(new_py_names);
  }
}




//------------------------------------------------------------------------------
// DataTable private helpers
//------------------------------------------------------------------------------

void DataTable::_init_pynames() const {
  if (py_names_) return;
  xassert(names_.size() == ncols_);

  py_names_ = py::otuple(ncols_);
  py_inames_ = py::odict();
  for (size_t i = 0; i < ncols_; ++i) {
    py::ostring pyname(names_[i]);
    py_inames_.set(pyname, py::oint(i));
    py_names_.set(i, std::move(pyname));
  }
}


// Ensure there are no invalid characters in a column's name. Invalid
// are considered characters with ASCII codes \x00 - \x1F. If any of them
// are found, we perform substitution s/[\x00-\x1F]+/./g.
//
static std::string _mangle_name(const dt::CString& name, bool* was_mangled) {
  auto chars = reinterpret_cast<const uint8_t*>(name.data());
  auto len = name.size();

  size_t j = 0;
  for (; j < len && chars[j] >= 0x20; ++j);
  if (j == len) {
    *was_mangled = false;
    return name.to_string();
  }

  std::ostringstream out;
  bool written_dot = false;
  for (j = 0; j < len; ++j) {
    uint8_t ch = chars[j];
    if (ch < 0x20) {
      if (!written_dot) {
        out << '.';
        written_dot = true;
      }
    } else {
      out << static_cast<char>(ch);
      written_dot = false;
    }
  }
  *was_mangled = true;
  return out.str();
}


static void _deduplicate(std::string* name, py::oobj* pyname,
                         py::odict& seen_names,
                         std::unordered_map<std::string,
                                            std::unordered_set<size_t>>& stems)
{
  size_t n = name->size();
  const char* chars = name->data();
  // Find the "stem" of the name: the part of its name without the trailing
  // digits.
  size_t j = n;
  for (; j > 0; --j) {
    char ch = chars[j - 1];
    if (ch < '0' || ch > '9') break;
  }
  std::string stem(*name, 0, j);

  // Find the "count" part of the name: digits that follow the stem
  size_t count = 0;
  if (j < n) {
    for (; j < n; ++j) {
      char ch = chars[j];
      count = count * 10 + static_cast<size_t>(ch - '0');
    }
    count++;
  } else {
    count = static_cast<size_t>(names_auto_index);
    if (chars[n-1] != '.') {
      stem += '.';
    }
  }

  auto& seen_counts = stems[stem];  // inserts an empty set if needed
  while (true) {
    // Quickly skip those `count` values that were observed previously
    while (seen_counts.count(count)) count++;
    // Now the value of `count` may have not been seen before. Update
    // the name variable to use the new count value
    *name = stem + std::to_string(count);
    *pyname = py::ostring(*name);
    // The name "{stem}{count}" was either seen before, in which case we
    // should add it to the `seen_counts` set; or it wasn't seen before,
    // in which case we'll return this name to the caller, and thus it
    // has to be added to the `seen_counts` set anyways.
    seen_counts.insert(count);
    // If this new name is not in the list of seen names, then
    // we are done: use this new name
    if (!seen_names.has(*pyname)) break;
    // Otherwise, increase the count and try again
    count++;
  }
}


/**
 * This is a main method to assign column names to a Frame. It checks that the
 * names are valid, not duplicate, and if necessary modifies them to enforce
 * such constraints.
 */
void DataTable::_set_names_impl(NameProvider* nameslist, bool warn_duplicates) {
  if (nameslist->size() != ncols_) {
    throw ValueError() << "The `names` list has length " << nameslist->size()
        << ", while the Frame has "
        << (ncols_ < nameslist->size() && ncols_? "only " : "")
        << ncols_ << " column" << (ncols_ == 1? "" : "s");
  }

  // Prepare the containers for placing the new column names there
  py_names_  = py::otuple(ncols_);
  py_inames_ = py::odict();
  names_.clear();
  names_.reserve(ncols_);
  std::unordered_map<std::string, std::unordered_set<size_t>> stems;
  static constexpr size_t MAX_DUPLICATES = 3;
  size_t n_duplicates = 0;
  strvec duplicates(MAX_DUPLICATES);
  strvec replacements(MAX_DUPLICATES);

  // If any name is empty or None, it will be replaced with the default name
  // in the end. The reason we don't replace immediately upon seeing an empty
  // name is to ensure that the auto-generated names do not clash with the
  // user-specified names somewhere later in the list.
  bool fill_default_names = false;

  for (size_t i = 0; i < ncols_; ++i) {
    // Convert to a C-style name object. Note that if `name` is python None,
    // then the resulting `cname` will be `{nullptr, 0}`.
    dt::CString cname = nameslist->item_as_cstring(i);
    if (cname.size() == 0) {
      fill_default_names = true;
      names_.push_back(std::string());
      continue;
    }
    bool name_mangled;
    std::string resname = _mangle_name(cname, &name_mangled);
    py::oobj newname = name_mangled? py::ostring(resname)
                                   : nameslist->item_as_pyoobj(i);
    // Check for name duplicates. If the name was already seen before, we
    // replace it with a modified name (by incrementing the name's digital
    // suffix if it has one, or otherwise by adding such a suffix).
    if (py_inames_.has(newname)) {
      size_t k = std::min(n_duplicates, MAX_DUPLICATES - 1);
      duplicates[k] = resname;
      _deduplicate(&resname, &newname, py_inames_, stems);
      replacements[k] = resname;
      n_duplicates++;
    }

    // Store the name in all containers
    names_.push_back(resname);
    py_inames_.set(newname, py::oint(i));
    py_names_.set(i, std::move(newname));
  }

  // If during the processing we discovered any empty names, they must be
  // replaced with auto-generated ones.
  if (fill_default_names) {
    // Config variables to be used for name auto-generation
    int64_t index0 = names_auto_index;
    std::string prefix = names_auto_prefix;
    const char* prefixptr = prefix.data();
    size_t prefixlen = prefix.size();

    // Within the existing names, find ones with the pattern "{prefix}<num>".
    // If such names exist, we'll start autonaming with 1 + max(<num>), where
    // the maximum is taken among all such names.
    for (size_t i = 0; i < ncols_; ++i) {
      size_t namelen = names_[i].size();
      const char* nameptr = names_[i].data();
      if (namelen <= prefixlen) continue;
      if (std::strncmp(nameptr, prefixptr, prefixlen) != 0) continue;
      int64_t value = 0;
      for (size_t j = prefixlen; j < namelen; ++j) {
        char ch = nameptr[j];
        if (ch < '0' || ch > '9') goto next_name;
        value = value * 10 + static_cast<int64_t>(ch - '0');
      }
      if (value >= index0) {
        index0 = value + 1;
      }
      next_name:;
    }

    // Now actually fill the empty names
    for (size_t i = 0; i < ncols_; ++i) {
      if (!names_[i].empty()) continue;
      names_[i] = prefix + std::to_string(index0);
      py::oobj newname = py::ostring(names_[i]);
      py_inames_.set(newname, py::oint(i));
      py_names_.set(i, std::move(newname));
      index0++;
    }
  }

  // If there were any duplicate names, issue a warning
  if (n_duplicates > 0 && warn_duplicates) {
    auto w = DatatableWarning();
    if (n_duplicates == 1) {
      w << "Duplicate column name found, and was assigned a unique name: "
        << "'" << duplicates[0] << "' -> '" << replacements[0] << "'";
    } else {
      w << "Duplicate column names found, and were assigned unique names: ";
      size_t n = std::min(n_duplicates, MAX_DUPLICATES);
      for (size_t i = 0; i < n; ++i) {
        w << ((i == 0)? "'" :
              (i < n - 1 || i == n_duplicates - 1) ? ", '" : ", ..., '")
          << duplicates[i] << "' -> '"
          << replacements[i] << "'";
      }
    }
    w.emit_warning();
  }

  xassert(ncols_ == names_.size());
  xassert(ncols_ == py_names_.size());
  xassert(ncols_ == py_inames_.size());
}



void DataTable::_integrity_check_names() const {
  if (names_.size() != ncols_) {
    throw AssertionError() << "DataTable.names has size " << names_.size()
      << ", however there are " << ncols_ << " columns in the Frame";
  }
  std::unordered_set<std::string> seen_names;
  for (size_t i = 0; i < names_.size(); ++i) {
    if (seen_names.count(names_[i]) > 0) {
      throw AssertionError() << "Duplicate name '" << names_[i] << "' for "
          "column " << i;
    }
    seen_names.insert(names_[i]);
    const char* ch = names_[i].data();
    size_t len = names_[i].size();
    for (size_t j = 0; j < len; ++j) {
      if (static_cast<uint8_t>(ch[j]) < 0x20) {
        throw AssertionError() << "Invalid character '" << ch[j] << "' in "
          "column " << i << "'s name";
      }
    }
  }
}


void DataTable::_integrity_check_pynames() const {
  if (!py_names_) {
    XAssert(py_inames_.size() == 0);
    return;
  }
  XAssert(py_names_.is_tuple());
  XAssert(py_inames_.is_dict());
  XAssert(py_names_.size() == ncols_);
  XAssert(py_inames_.size() == ncols_);
  for (size_t i = 0; i < ncols_; ++i) {
    py::robj elem = py_names_[i];
    XAssert(elem.is_string());
    XAssert(elem.to_string() == names_[i]);
    py::robj res = py_inames_.get(elem);
    XAssert(bool(res) && "column in py_inames_ dict");
    XAssert(res.to_int64_strict() == static_cast<int64_t>(i));
  }
}



//------------------------------------------------------------------------------
// Tests
//------------------------------------------------------------------------------
#ifdef DTTEST

TEST(coverage, names_FrameNameProviders) {
  pylistNP* t1 = new pylistNP(py::olist(0));
  delete t1;

  std::vector<std::string> src2 = {"\xFF__", "foo"};
  strvecNP* t2 = new strvecNP(src2);

  ASSERT_THROWS(
    [&]{ t2->item_as_pyoobj(0); },
    PyError,
    "'utf-8' codec can't decode byte 0xff in position 0: invalid start byte"
  );
  delete t2;
}


TEST(coverage, names_integrity_checks) {
  DataTable* dt = new DataTable({
                      Column::new_data_column(1, dt::SType::INT32),
                      Column::new_data_column(1, dt::SType::FLOAT64)
                  });

  auto check1 = [=]{ dt->_integrity_check_names(); };
  dt->names_.clear();
  ASSERT_THROWS(check1, AssertionError,
      "DataTable.names has size 0, however there are 2 columns in the Frame");

  dt->names_ = { "foo", "foo" };
  ASSERT_THROWS(check1, AssertionError,
      "Duplicate name 'foo' for column 1");

  dt->names_ = { "foo", "f\x0A\x0D" };
  ASSERT_THROWS(check1, AssertionError,
      "Invalid character '\\n' in column 1's name");

  dt->names_ = { "one", "two" };
  dt->_integrity_check_names();  // should not throw

  auto check2 = [dt]() { dt->_integrity_check_pynames(); };
  py::oobj q = py::None();
  dt->py_inames_.set(q, q);
  ASSERT_THROWS(check2, AssertionError,
      "Assertion 'py_inames_.size() == 0' failed");
  dt->py_inames_.del(q);

  dt->py_names_ = *static_cast<const py::otuple*>(&q);
  ASSERT_THROWS(check2, AssertionError,
      "Assertion 'py_names_.is_tuple()' failed");

  dt->py_inames_ = *static_cast<const py::odict*>(&q);
  dt->py_names_ = py::otuple(1);
  ASSERT_THROWS(check2, AssertionError,
      "Assertion 'py_inames_.is_dict()' failed");

  dt->py_inames_ = py::odict();
  ASSERT_THROWS(check2, AssertionError,
      "Assertion 'py_names_.size() == ncols_' failed");

  dt->py_names_ = py::otuple(2);
  ASSERT_THROWS(check2, AssertionError,
      "Assertion 'py_inames_.size() == ncols_' failed");

  dt->py_inames_.set(py::ostring("one"), py::oint(0));
  dt->py_inames_.set(py::ostring("TWO"), py::oint(2));
  dt->py_names_.set(0, py::oint(1));
  dt->py_names_.set(1, py::ostring("two"));
  ASSERT_THROWS(check2, AssertionError,
      "Assertion 'elem.is_string()' failed");

  dt->py_names_.set(0, py::ostring("1"));
  ASSERT_THROWS(check2, AssertionError,
      "Assertion 'elem.to_string() == names_[i]' failed");

  dt->py_names_.set(0, py::ostring("one"));
  ASSERT_THROWS(check2, AssertionError,
      "Assertion 'bool(res) && \"column in py_inames_ dict\"' failed");

  dt->py_inames_.del(py::ostring("TWO"));
  dt->py_inames_.set(py::ostring("two"), py::oint(2));
  ASSERT_THROWS(check2, AssertionError,
      "Assertion 'res.to_int64_strict() == static_cast<int64_t>(i)' failed");

  dt->py_inames_.set(py::ostring("two"), py::oint(1));
  dt->verify_integrity();
  delete dt;
}



#endif
