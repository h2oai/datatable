//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "frame/py_frame.h"
#include "python/dict.h"
#include "python/int.h"
#include "python/string.h"
#include "python/tuple.h"
#include "utils/assert.h"



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
    pylistNP(const py::olist& arg) : names(arg) {}
    virtual size_t size() const override;
    virtual CString item_as_cstring(size_t i) override;
    virtual py::oobj item_as_pyoobj(size_t i) override;
};


class strvecNP : public NameProvider {
  private:
    const std::vector<std::string>& names;

  public:
    strvecNP(const std::vector<std::string>& arg) : names(arg) {}
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
  py::obj name = names[i];
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
}



//------------------------------------------------------------------------------
// Frame API
//------------------------------------------------------------------------------
namespace py {


oobj Frame::get_names() const {
  return dt->get_pynames();
}


void Frame::set_names(obj arg)
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


oobj Frame::colindex(PKArgs& args)
{
  auto col = args[0];

  if (col.is_string()) {
    int64_t index = dt->colindex(col.to_pyobj());
    if (index == -1) {
      throw _name_not_found_error(col.to_string());
    }
    return py::oint(index);
  }
  if (col.is_int()) {
    int64_t colidx = col.to_int64_strict();
    int64_t ncols = dt->ncols;
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



//------------------------------------------------------------------------------
// Private helper methods
//------------------------------------------------------------------------------

/**
 * Compute Levenshtein distance between two strings `a` and `b`, as described in
 * https://en.wikipedia.org/wiki/Levenshtein_distance
 *
 * Use iterative algorithm, single-row version. The temporary storage required
 * for the calculations is passed in array `v`, which must be allocated for at
 * least `min(a.size(), b.size()) + 1` elements.
 */
static double _dlevenshtein(
    const std::string& a, const std::string& b, double* v)
{
  const char* aa = a.data();
  const char* bb = b.data();
  int n = static_cast<int>(a.size());
  int m = static_cast<int>(b.size());
  if (n > m) {
    std::swap(aa, bb);
    std::swap(m, n);
  }
  // Remove common prefix from the strings
  while (n && *aa == *bb) {
    n--; m--;
    aa++; bb++;
  }
  // Remove common suffix from the strings
  while (n && aa[n - 1] == bb[m - 1]) {
    n--; m--;
  }
  if (n == 0) return m;
  xassert(0 < n && n <= m);
  // Compute the Levenshtein distance
  aa--;  // Shift pointers, so that we can use 1-based indexing below
  bb--;
  for (int j = 1; j <= n; ++j) v[j] = j;
  for (int i = 1; i <= m; ++i) {
    double w = i - 1;
    v[0] = i;
    for (int j = 1; j <= n; ++j) {
      char ach = aa[j];
      char bch = bb[i];
      double c = 0.0;
      if (ach != bch) {
        // Use non-trivial cost function to compare character substitution:
        //   * the cost is lowest when 2 characters differ by case only,
        //     or if both are "space-like" (i.e. ' ', '_' or '.')
        //   * medium cost for substituting letters with letters, or digits
        //     with digits
        //   * highest cost for all other substitutions.
        //
        bool a_lower = 'a' <= ach && ach <= 'z';
        bool a_upper = 'A' <= ach && ach <= 'Z';
        bool a_digit = '0' <= ach && ach <= '9';
        bool a_space = ach == ' ' || ach == '_' || ach == '.';
        bool b_lower = 'a' <= bch && bch <= 'z';
        bool b_upper = 'A' <= bch && bch <= 'Z';
        bool b_digit = '0' <= bch && bch <= '9';
        bool b_space = bch == ' ' || bch == '_' || bch == '.';
        c = (a_lower && ach == bch + ('a'-'A'))? 0.2 :
            (a_upper && bch == ach + ('a'-'A'))? 0.2 :
            (a_space && b_space)? 0.2 :
            (a_digit && b_digit)? 0.75 :
            ((a_lower|a_upper) && (b_lower|b_upper))? 0.75 : 1.0;
      }
      double del_cost = v[j] + 1;
      double ins_cost = v[j - 1] + 1;
      double sub_cost = w + c;
      w = v[j];
      v[j] = std::min(del_cost, std::min(ins_cost, sub_cost));
    }
  }
  return v[n];
}


Error Frame::_name_not_found_error(const std::string& name) {
  const std::vector<std::string>& names = dt->get_names();
  auto tmp = std::unique_ptr<double[]>(new double[name.size() + 1]);
  double* vtmp = tmp.get();
  double maxdist = name.size() <= 3? 1 :
                   name.size() <= 6? 2 :
                   name.size() <= 9? 3 :
                   name.size() <= 16? 4 : 5;
  struct scored_column {
    size_t index;
    double score;
  };
  auto col0 = scored_column { 0, 100.0 };
  auto col1 = scored_column { 0, 100.0 };
  auto col2 = scored_column { 0, 100.0 };
  for (size_t i = 0; i < names.size(); ++i) {
    double dist = _dlevenshtein(name, names[i], vtmp);
    if (dist <= maxdist) {
      auto curr = scored_column { i, dist };
      if (curr.score < col0.score) {
        col2 = col1; col1 = col0; col0 = curr;
      } else if (curr.score < col1.score) {
        col2 = col1; col1 = curr;
      } else if (curr.score < col2.score) {
        col2 = curr;
      }
    }
  }

  auto err = ValueError();
  err << "Column `" << name << "` does not exist in the Frame";
  if (col0.score < 10) {
    err << "; did you mean `" << names[col0.index] << "`";
    if (col1.score < 10) {
      err << (col2.score < 10? ", " : " or ");
      err << "`" << names[col1.index] << "`";
      if (col2.score < 10) {
        err << " or `" << names[col2.index] << "`";
      }
    }
    err << "?";
  }
  return err;
}



#ifdef DTTEST
  void cover_py_FrameNameProviders() {
    pylistNP* t1 = new pylistNP(py::olist(0));
    delete t1;

    strvec src2 = {"\xFF__", "foo"};
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
#endif

} // namespace py



//------------------------------------------------------------------------------
// DataTable methods
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
  if (!py_inames) _init_pynames();
  py::obj pyindex = py_inames.get(pyname);
  return pyindex? pyindex.to_int64_strict() : -1;
}


/**
 * Copy names without checking for validity, since we know they were already
 * verified in DataTable `other`.
 */
void DataTable::copy_names_from(const DataTable* other) {
  names = other->names;
  py_names = other->py_names;
  py_inames = other->py_inames;
}


/**
 * Initialize DataTable's column names to the default "C0", "C1", "C2", ...
 */
void DataTable::set_names_to_default() {
  auto index0 = static_cast<size_t>(config::frame_names_auto_index);
  auto prefix = config::frame_names_auto_prefix;
  auto zcols  = static_cast<size_t>(ncols);
  py_names  = py::otuple();
  py_inames = py::odict(nullptr);
  names.clear();
  names.reserve(zcols);
  for (size_t i = 0; i < zcols; ++i) {
    names.push_back(prefix + std::to_string(i + index0));
  }
}


void DataTable::set_names(py::olist names_list) {
  pylistNP np(names_list);
  _set_names_impl(&np);
}


void DataTable::set_names(const std::vector<std::string>& names_list) {
  strvecNP np(names_list);
  _set_names_impl(&np);
}


void DataTable::replace_names(py::odict replacements) {
  py::olist newnames(ncols);

  for (int64_t i = 0; i < ncols; ++i) {
    newnames.set(i, py_names[i]);
  }
  for (auto kv : replacements) {
    py::obj key = kv.first;
    py::obj val = kv.second;
    py::obj idx = py_inames.get(key);
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





//------------------------------------------------------------------------------
// DataTable private helpers
//------------------------------------------------------------------------------

void DataTable::_init_pynames() const {
  if (py_names) return;
  size_t zcols = static_cast<size_t>(ncols);
  xassert(names.size() == zcols);

  py_names = py::otuple(zcols);
  py_inames = py::odict();
  for (size_t i = 0; i < zcols; ++i) {
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
  size_t zcols = static_cast<size_t>(ncols);
  if (nameslist->size() != zcols) {
    throw ValueError() << "The `names` list has length " << nameslist->size()
        << ", while the Frame has "
        << (zcols < nameslist->size() && zcols? "only " : "")
        << zcols << " column" << (zcols == 1? "" : "s");
  }

  // Prepare the containers for placing the new column names there
  py_names  = py::otuple(zcols);
  py_inames = py::odict();
  names.clear();
  names.reserve(zcols);
  std::vector<std::string> duplicates;

  // If any name is empty or None, it will be replaced with the default name
  // in the end. The reason we don't replace immediately upon seeing an empty
  // name is to ensure that the auto-generated names do not clash with the
  // user-specified names somewhere later in the list.
  bool fill_default_names = false;

  for (size_t i = 0; i < zcols; ++i) {
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
    int64_t index0 = config::frame_names_auto_index;
    std::string prefix = config::frame_names_auto_prefix;
    const char* prefixptr = prefix.data();
    size_t prefixlen = prefix.size();

    // Within the existing names, find ones with the pattern "{prefix}<num>".
    // If such names exist, we'll start autonaming with 1 + max(<num>), where
    // the maximum is taken among all such names.
    for (size_t i = 0; i < zcols; ++i) {
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
    for (size_t i = 0; i < zcols; ++i) {
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
    Warning w;
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

  xassert(zcols == names.size());
  xassert(zcols == py_names.size());
  xassert(zcols == py_inames.size());
}



void DataTable::_integrity_check_names() const {
  if (!py_names && !py_inames) return;
  if (!py_names || !py_inames) {
    throw AssertionError() << "One of DataTable.py_names or DataTable.py_inames"
      " is not properly computed";
  }
  if (!py_names.is_tuple()) {
    throw AssertionError() << "DataTable.py_names is not a tuple";
  }
  if (!py_inames.is_dict()) {
    throw AssertionError() << "DataTable.py_inames is not a dict";
  }
  size_t zcols = static_cast<size_t>(ncols);
  if (py_names.size() != zcols) {
    throw AssertionError() << "len(.py_names) is " << py_names.size()
        << ", whereas .ncols = " << zcols;
  }
  if (py_inames.size() != zcols) {
    throw AssertionError() << ".inames has " << py_inames.size()
      << " elements, while the Frame has " << zcols << " columns";
  }
  for (size_t i = 0; i < zcols; ++i) {
    py::obj elem = py_names[i];
    if (!elem.is_string()) {
      throw AssertionError() << "Element " << i << " of .py_names is not "
          "a string but " << elem.typeobj();
    }
    std::string sname = elem.to_string();
    if (sname != names[i]) {
      throw AssertionError() << "Element " << i << " of .py_names is '"
          << sname << "', but the actual column name is '" << names[i] << "'";
    }
    py::obj res = py_inames.get(elem);
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
