//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#include "frame/py_frame.h"
#include <unordered_set>
#include "python/dict.h"
#include "python/int.h"
#include "python/string.h"
#include "python/tuple.h"
#include "utils/assert.h"
#include "utils/fuzzy_match.h"
#include "options.h"
#include "ztest.h"

static Error _name_not_found_error(const DataTable* dt, const std::string& name)
{
  Error err = ValueError();
  err << "Column `" << name << "` does not exist in the Frame";
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
    virtual ~NameProvider();  // LCOV_EXCL_LINE
    virtual size_t size() const = 0;
    virtual CString item_as_cstring(size_t i) = 0;
    virtual py::oobj item_as_pyoobj(size_t i) = 0;
};


class pylistNP : public NameProvider {
  private:
    const py::olist& names;

  public:
    explicit pylistNP(const py::olist& arg) : names(arg) {}
    virtual size_t size() const override;
    virtual CString item_as_cstring(size_t i) override;
    virtual py::oobj item_as_pyoobj(size_t i) override;
};


class strvecNP : public NameProvider {
  private:
    const strvec& names;

  public:
    explicit strvecNP(const std::vector<std::string>& arg) : names(arg) {}
    virtual size_t size() const override;
    virtual CString item_as_cstring(size_t i) override;
    virtual py::oobj item_as_pyoobj(size_t i) override;
};


//------------------------------------------------------------------------------

NameProvider::~NameProvider() {}

size_t pylistNP::size() const {
  return names.size();
}

CString pylistNP::item_as_cstring(size_t i) {
  py::robj name = names[i];
  if (!name.is_string() && !name.is_none()) {
    throw TypeError() << "Invalid `names` list: element " << i
        << " is not a string";
  }
  return name.to_cstring();
}

py::oobj pylistNP::item_as_pyoobj(size_t i) {
  return py::oobj(names[i]);
}


size_t strvecNP::size() const {
  return names.size();
}

CString strvecNP::item_as_cstring(size_t i) {
  const std::string& name = names[i];
  return CString { name.data(), static_cast<int64_t>(name.size()) };
}

py::oobj strvecNP::item_as_pyoobj(size_t i) {
  return py::ostring(names[i]);
}  // LCOV_EXCL_LINE




//------------------------------------------------------------------------------
// Frame names API
//------------------------------------------------------------------------------
namespace py {

static PKArgs args_colindex(
    1, 0, 0, false, false,
    {"name"}, "colindex",

R"(colindex(self, name)
--

Return index of the column ``name``, or raises a `ValueError` if the requested
column does not exist.

Parameters
----------
name: str
    The name of the column for which the index is sought. This can also
    be an index of a column, in which case the index is checked that
    it doesn't go out-of-bounds, and negative index is converted into
    positive.
)");


oobj Frame::colindex(const PKArgs& args) {
  auto col = args[0];

  if (col.is_string()) {
    int64_t index = dt->colindex(col.to_pyobj());
    if (index == -1) {
      throw _name_not_found_error(dt, col.to_string());
    }
    return py::oint(index);
  }
  if (col.is_int()) {
    int64_t colidx = col.to_int64_strict();
    int64_t ncols = static_cast<int64_t>(dt->ncols);
    if (colidx < 0 && colidx + ncols >= 0) {
      colidx += ncols;
    }
    if (colidx >= 0 && colidx < ncols) {
      return py::oint(colidx);
    }
    throw ValueError() << "Column index `" << colidx << "` is invalid for a "
        "Frame with " << ncols << " column" << (ncols==1? "" : "s");
  }
  throw TypeError() << "The argument to Frame.colindex() should be a string "
      "or an integer, not " << col.typeobj();
}



static GSArgs args_names(
  "names",
R"(Tuple of column names.

You can rename the Frame's columns by assigning a new list/tuple of
names to this property. The length of the new list of names must be
the same as the number of columns in the Frame.

It is also possible to rename just a few columns by assigning a
dictionary ``{oldname: newname, ...}``. Any column not listed in the
dictionary will retain its name.

Examples
--------
>>> d0 = dt.Frame([[1], [2], [3]])
>>> d0.names = ['A', 'B', 'C']
>>> d0.names
('A', 'B', 'C')
>>> d0.names = {'B': 'middle'}
>>> d0.names
('A', 'middle', 'C')
>>> del d0.names
>>> d0.names
('C0', 'C1', 'C2)
)");


oobj Frame::get_names() const {
  return dt->get_pynames();
}  // LCOV_EXCL_LINE


void Frame::set_names(robj arg)
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



void Frame::Type::_init_names(Methods& mm, GetSetters& gs) {
  ADD_METHOD(mm, &Frame::colindex, args_colindex);
  ADD_GETSET(gs, &Frame::get_names, &Frame::set_names, args_names);
}


} // namespace py




//------------------------------------------------------------------------------
// Options
//------------------------------------------------------------------------------

static int64_t     names_auto_index = 0;
static std::string names_auto_prefix = "C";

void py::Frame::init_names_options() {
  config::register_option(
    "frame.names_auto_index",
    []{ return py::oint(names_auto_index); },
    [](py::oobj value){ names_auto_index = value.to_int64_strict(); },
    "When Frame needs to auto-name columns, they will be assigned\n"
    "names C0, C1, C2, ... by default. This option allows you to\n"
    "control the starting index in this sequence. For example, setting\n"
    "options.frame.names_auto_index=1 will cause the columns to be\n"
    "named C1, C2, C3, ...");

  config::register_option(
    "frame.names_auto_prefix",
    []{ return py::ostring(names_auto_prefix); },
    [](py::oobj value){ names_auto_prefix = value.to_string(); },
    "When Frame needs to auto-name columns, they will be assigned\n"
    "names C0, C1, C2, ... by default. This option allows you to\n"
    "control the prefix used in this sequence. For example, setting\n"
    "options.frame.names_auto_prefix='Z' will cause the columns to be\n"
    "named Z0, Z1, Z2, ...");
}



//------------------------------------------------------------------------------
// DataTable names API
//------------------------------------------------------------------------------

/**
 * Return DataTable column names as a C++ vector of strings.
 */
const std::vector<std::string>& DataTable::get_names() const {
  return names;
}


/**
 * Return DataTable column names as a python tuple.
 */
py::otuple DataTable::get_pynames() const {
  if (!py_names) _init_pynames();
  return py_names;
}


/**
 * Return the index of a column given its name; or -1 if such column does not
 * exist in the DataTable.
 */
int64_t DataTable::colindex(const py::_obj& pyname) const {
  if (!py_names) _init_pynames();
  py::robj pyindex = py_inames.get(pyname);
  return pyindex? pyindex.to_int64_strict() : -1;
}


/**
 * Return the index of a column given its name; throw an exception if the
 * column does not exist in the DataTable.
 */
size_t DataTable::xcolindex(const py::_obj& pyname) const {
  if (!py_names) _init_pynames();
  py::robj pyindex = py_inames.get(pyname);
  if (!pyindex) {
    throw _name_not_found_error(this, pyname.to_string());
  }
  return pyindex.to_size_t();
}


/**
 * Copy names without checking for validity, since we know they were already
 * verified in DataTable `other`.
 */
void DataTable::copy_names_from(const DataTable* other) {
  xassert(other);
  names = other->names;
  py_names = other->py_names;
  py_inames = other->py_inames;
}


/**
 * Initialize DataTable's column names to the default "C0", "C1", "C2", ...
 */
void DataTable::set_names_to_default() {
  auto index0 = static_cast<size_t>(names_auto_index);
  py_names  = py::otuple();
  py_inames = py::odict();
  names.clear();
  names.reserve(ncols);
  for (size_t i = 0; i < ncols; ++i) {
    names.push_back(names_auto_prefix + std::to_string(i + index0));
  }
}


void DataTable::set_names(const py::olist& names_list) {
  if (!names_list) return set_names_to_default();
  pylistNP np(names_list);
  _set_names_impl(&np);
}


void DataTable::set_names(const strvec& names_list) {
  strvecNP np(names_list);
  _set_names_impl(&np);
}


void DataTable::replace_names(py::odict replacements) {
  py::olist newnames(ncols);

  for (size_t i = 0; i < ncols; ++i) {
    newnames.set(i, py_names[i]);
  }
  for (auto kv : replacements) {
    py::robj key = kv.first;
    py::robj val = kv.second;
    py::robj idx = py_inames.get(key);
    if (idx.is_undefined()) {
      throw ValueError() << "Cannot find column `" << key.str()
        << "` in the Frame";
    }
    if (!val.is_string()) {
      throw TypeError() << "The replacement name for column `" << key.str()
        << "` should be a string, but got " << val.typeobj();
    }
    int64_t i = idx.to_int64_strict();
    newnames.set(i, val);
  }
  set_names(newnames);
}


void DataTable::reorder_names(const std::vector<size_t>& col_indices) {
  xassert(col_indices.size() == ncols);
  strvec newnames;
  newnames.reserve(ncols);
  for (size_t i = 0; i < ncols; ++i) {
    newnames.push_back(std::move(names[col_indices[i]]));
  }
  names = std::move(newnames);
  if (py_names) {
    py::otuple new_py_names(ncols);
    for (size_t i = 0; i < ncols; ++i) {
      py::robj pyname = py_names[col_indices[i]];
      new_py_names.set(i, pyname);
      py_inames.set(pyname, py::oint(i));
    }
    py_names = std::move(new_py_names);
  }
}




//------------------------------------------------------------------------------
// DataTable private helpers
//------------------------------------------------------------------------------

void DataTable::_init_pynames() const {
  if (py_names) return;
  xassert(names.size() == ncols);

  py_names = py::otuple(ncols);
  py_inames = py::odict();
  for (size_t i = 0; i < ncols; ++i) {
    py::ostring pyname(names[i]);
    py_inames.set(pyname, py::oint(i));
    py_names.set(i, std::move(pyname));
  }
}


/**
 * This is a main method to assign column names to a Frame. It checks that the
 * names are valid, not duplicate, and if necessary modifies them to enforce
 * such constraints.
 */
void DataTable::_set_names_impl(NameProvider* nameslist) {
  if (nameslist->size() != ncols) {
    throw ValueError() << "The `names` list has length " << nameslist->size()
        << ", while the Frame has "
        << (ncols < nameslist->size() && ncols? "only " : "")
        << ncols << " column" << (ncols == 1? "" : "s");
  }

  // Prepare the containers for placing the new column names there
  py_names  = py::otuple(ncols);
  py_inames = py::odict();
  names.clear();
  names.reserve(ncols);
  std::vector<std::string> duplicates;

  // If any name is empty or None, it will be replaced with the default name
  // in the end. The reason we don't replace immediately upon seeing an empty
  // name is to ensure that the auto-generated names do not clash with the
  // user-specified names somewhere later in the list.
  bool fill_default_names = false;

  for (size_t i = 0; i < ncols; ++i) {
    // Convert to a C-style name object. Note that if `name` is python None,
    // then the resulting `cname` will be `{nullptr, 0}`.
    CString cname = nameslist->item_as_cstring(i);
    char* strname = const_cast<char*>(cname.ch);
    size_t namelen = static_cast<size_t>(cname.size);
    if (namelen == 0) {
      fill_default_names = true;
      names.push_back(std::string());
      continue;
    }
    // Ensure there are no invalid characters in the column's name. Invalid
    // characters are considered those with ASCII codes \x00 - \x1F. If any
    // such characters found, we perform substitution s/[\x00-\x1F]+/./g.
    std::string resname;
    bool name_mangled = false;
    for (size_t j = 0; j < namelen; ++j) {
      if (static_cast<uint8_t>(strname[j]) < 0x20) {
        name_mangled = true;
        resname = std::string(strname, j) + ".";
        bool written_dot = true;
        for (; j < namelen; ++j) {
          char ch = strname[j];
          if (static_cast<uint8_t>(ch) < 0x20) {
            if (!written_dot) {
              resname += ".";
              written_dot = true;
            }
          } else {
            resname += ch;
            written_dot = false;
          }
        }
      }
    }
    if (!name_mangled) {
      resname = std::string(strname, namelen);
    }
    py::oobj newname = name_mangled? py::ostring(resname)
                                   : nameslist->item_as_pyoobj(i);
    // Check for name duplicates. If the name was already seen before, we
    // replace it with a modified name (by incrementing the name's digital
    // suffix if it has one, or otherwise by adding such a suffix).
    if (py_inames.has(newname)) {
      duplicates.push_back(resname);
      size_t j = namelen;
      for (; j > 0; --j) {
        char ch = strname[j - 1];
        if (ch < '0' || ch > '9') break;
      }
      std::string basename(resname, 0, j);
      int64_t count = 0;
      if (j < namelen) {
        for (; j < namelen; ++j) {
          char ch = strname[j];
          count = count * 10 + static_cast<int64_t>(ch - '0');
        }
      } else {
        basename += ".";
      }
      while (py_inames.has(newname)) {
        count++;
        resname = basename + std::to_string(count);
        newname = py::ostring(resname);
      }
    }

    // Store the name in all containers
    names.push_back(resname);
    py_inames.set(newname, py::oint(i));
    py_names.set(i, std::move(newname));
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
    for (size_t i = 0; i < ncols; ++i) {
      size_t namelen = names[i].size();
      const char* nameptr = names[i].data();
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
    for (size_t i = 0; i < ncols; ++i) {
      if (!names[i].empty()) continue;
      names[i] = prefix + std::to_string(index0);
      py::oobj newname = py::ostring(names[i]);
      py_inames.set(newname, py::oint(i));
      py_names.set(i, std::move(newname));
      index0++;
    }
  }

  // If there were any duplicate names, issue a warning
  size_t ndup = duplicates.size();
  if (ndup) {
    Warning w = DatatableWarning();
    if (ndup == 1) {
      w << "Duplicate column name '" << duplicates[0] << "' found, and was "
           "assigned a unique name";
    } else {
      w << "Duplicate column names found: ";
      for (size_t i = 0; i < ndup; ++i) {
        w << (i == 0? "'" :
              i < ndup - 1? ", '" : " and '");
        w << duplicates[i] << "'";
      }
      w << "; they were assigned unique names";
    }
    // as `w` goes out of scope, the warning is sent to Python
  }

  xassert(ncols == names.size());
  xassert(ncols == py_names.size());
  xassert(ncols == py_inames.size());
}



void DataTable::_integrity_check_names() const {
  if (names.size() != ncols) {
    throw AssertionError() << "DataTable.names has size " << names.size()
      << ", however there are " << ncols << " columns in the Frame";
  }
  std::unordered_set<std::string> seen_names;
  for (size_t i = 0; i < names.size(); ++i) {
    if (seen_names.count(names[i]) > 0) {
      throw AssertionError() << "Duplicate name '" << names[i] << "' for "
          "column " << i;
    }
    seen_names.insert(names[i]);
    const char* ch = names[i].data();
    size_t len = names[i].size();
    for (size_t j = 0; j < len; ++j) {
      if (static_cast<uint8_t>(ch[j]) < 0x20) {
        throw AssertionError() << "Invalid character '" << ch[j] << "' in "
          "column " << i << "'s name";
      }
    }
  }
}


void DataTable::_integrity_check_pynames() const {
  if (!py_names && py_inames.size() == 0) return;
  if (!py_names && py_inames.size() > 0) {
    throw AssertionError() << "DataTable.py_names is not properly computed";
  }
  if (!py_names.is_tuple()) {
    throw AssertionError() << "DataTable.py_names is not a tuple";
  }
  if (!py_inames.is_dict()) {
    throw AssertionError() << "DataTable.py_inames is not a dict";
  }
  if (py_names.size() != ncols) {
    throw AssertionError() << "len(.py_names) is " << py_names.size()
        << ", whereas .ncols = " << ncols;
  }
  if (py_inames.size() != ncols) {
    throw AssertionError() << ".py_inames has " << py_inames.size()
      << " elements, while the Frame has " << ncols << " columns";
  }
  for (size_t i = 0; i < ncols; ++i) {
    py::robj elem = py_names[i];
    if (!elem.is_string()) {
      throw AssertionError() << "Element " << i << " of .py_names is a "
          << elem.typeobj();
    }
    std::string sname = elem.to_string();
    if (sname != names[i]) {
      throw AssertionError() << "Element " << i << " of .py_names is '"
          << sname << "', but the actual column name is '" << names[i] << "'";
    }
    py::robj res = py_inames.get(elem);
    if (!res) {
      throw AssertionError() << "Column " << i << " '" << names[i] << "' is "
          "absent from the .py_inames dictionary";
    }
    int64_t v = res.to_int64_strict();
    if (v != static_cast<int64_t>(i)) {
      throw AssertionError() << "Column " << i << " '" << names[i] << "' maps "
          "to " << v << " in the .py_inames dictionary";
    }
  }
}




#ifdef DTTEST
namespace dttest {

  void cover_names_FrameNameProviders() {
    pylistNP* t1 = new pylistNP(py::olist(0));
    delete t1;

    std::vector<std::string> src2 = {"\xFF__", "foo"};
    strvecNP* t2 = new strvecNP(src2);
    bool test_ok = false;
    try {
      // This should throw, since the name is not valid UTF8
      auto r = t2->item_as_pyoobj(0);
    } catch (const std::exception&) {
      test_ok = true;
    }
    xassert(test_ok);
    delete t2;
  }


  void cover_names_integrity_checks() {
    DataTable* dt = new DataTable({
                        Column::new_data_column(SType::INT32, 1),
                        Column::new_data_column(SType::FLOAT64, 1)
                    });

    auto check1 = [dt]() { dt->_integrity_check_names(); };
    dt->names.clear();
    test_assert(check1, "DataTable.names has size 0, however there are 2 "
                        "columns in the Frame");
    dt->names = { "foo", "foo" };
    test_assert(check1, "Duplicate name 'foo' for column 1");
    dt->names = { "foo", "f\x0A\x0D" };
    xassert(dt->names.size() == 2);  // silence warning about unused dt->names
    test_assert(check1, "Invalid character '\\x0a' in column 1's name");
    dt->names = { "one", "two" };

    auto check2 = [dt]() { dt->_integrity_check_pynames(); };
    py::oobj q = py::None();
    dt->py_inames.set(q, q);
    test_assert(check2, "DataTable.py_names is not properly computed");
    dt->py_inames.del(q);

    dt->py_names = *reinterpret_cast<const py::otuple*>(&q);
    test_assert(check2, "DataTable.py_names is not a tuple");
    dt->py_inames = *reinterpret_cast<const py::odict*>(&q);
    dt->py_names = py::otuple(1);
    test_assert(check2, "DataTable.py_inames is not a dict");
    dt->py_inames = py::odict();
    test_assert(check2, "len(.py_names) is 1, whereas .ncols = 2");
    dt->py_names = py::otuple(2);
    test_assert(check2, ".py_inames has 0 elements, while the Frame has "
                        "2 columns");
    dt->py_inames.set(py::ostring("one"), py::oint(0));
    dt->py_inames.set(py::ostring("TWO"), py::oint(2));
    dt->py_names.set(0, py::oint(1));
    dt->py_names.set(1, py::ostring("two"));
    test_assert(check2, "Element 0 of .py_names is a <class 'int'>");
    dt->py_names.set(0, py::ostring("1"));
    test_assert(check2, "Element 0 of .py_names is '1', but the actual "
                        "column name is 'one'");
    dt->py_names.set(0, py::ostring("one"));
    test_assert(check2, "Column 1 'two' is absent from the .py_inames "
                        "dictionary");
    dt->py_inames.del(py::ostring("TWO"));
    dt->py_inames.set(py::ostring("two"), py::oint(2));
    test_assert(check2, "Column 1 'two' maps to 2 in the .py_inames "
                        "dictionary");
    dt->py_inames.set(py::ostring("two"), py::oint(1));
    dt->verify_integrity();
  }

}
#endif
