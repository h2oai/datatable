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

namespace py {


//------------------------------------------------------------------------------
// User-facing API
//------------------------------------------------------------------------------

oobj Frame::get_names() const {
  if (names == nullptr) _init_names();
  return oobj(names);
}


void Frame::set_names(obj arg)
{
  if (arg.is_undefined() || arg.is_none()) {
    _fill_default_names();
  }
  else if (arg.is_list() || arg.is_tuple()) {
    _dedup_and_save_names(arg.to_pylist());
  }
  else if (arg.is_dict() && !dt->names.empty()) {
    _replace_names_from_map(arg.to_pydict());
  }
  else {
    throw TypeError() << "Expected a list of strings, got " << arg.typeobj();
  }
}


oobj Frame::colindex(PKArgs& args)
{
  auto col = args[0];

  if (col.is_string()) {
    if (!inames) _init_inames();
    PyObject* colname = col.to_borrowed_ref();
    // If key is not in the dict, PyDict_GetItem(dict, key) returns NULL
    // without setting an exception.
    PyObject* index = PyDict_GetItem(inames, colname);  // borrowed ref
    if (index) {
      return oobj(index);
    }
    throw ValueError()
        << "Column `" << PyUnicode_AsUTF8(colname) << "` does not exist in "
           "Frame";  // TODO: add frame repr here
  }
  if (col.is_int()) {
    int64_t colidx = col.to_int64_strict();
    if (colidx < 0 && colidx + dt->ncols >= 0) {
      colidx += dt->ncols;
    }
    if (colidx >= 0 && colidx < dt->ncols) {
      return py::oInt(colidx);
    }
    throw ValueError() << "Column index `" << colidx << "` is invalid for a "
        "Frame with " << dt->ncols << " column" << (dt->ncols==1? "" : "s");
  }
  throw TypeError() << "The argument to Frame.colindex() should be a string "
      "or an integer, not " << col.typeobj();
}




//------------------------------------------------------------------------------
// Private helper methods
//------------------------------------------------------------------------------

// Clear existing memoized names
void Frame::_clear_names() {
  Py_XDECREF(names);
  Py_XDECREF(inames);
  names = nullptr;
  inames = nullptr;
  dt->names.clear();
  dt->names.reserve(static_cast<size_t>(dt->ncols));
}


void Frame::_init_names() const {
  if (names) return;
  xassert(dt->names.size() == static_cast<size_t>(dt->ncols));

  py::otuple onames(dt->ncols);
  for (size_t i = 0; i < onames.size(); ++i) {
    onames.set(i, py::ostring(dt->names[i]));
  }
  names = onames.release();
}


void Frame::_init_inames() const {
  if (inames) return;
  xassert(dt->names.size() == static_cast<size_t>(dt->ncols));
  _init_names();

  // TODO: use py::dict
  PyObject* dict = PyDict_New();
  if (!dict) throw PyError();
  for (int64_t i = 0; i < dt->ncols; ++i) {
    PyObject* name = PyTuple_GET_ITEM(names, i);
    PyObject* index = PyLong_FromLong(i);
    PyDict_SetItem(dict, name, index);
    Py_DECREF(index);
  }
  inames = dict;
}


void Frame::_fill_default_names() {
  auto index0 = static_cast<size_t>(config::frame_names_auto_index);
  auto prefix = config::frame_names_auto_prefix;
  auto ncols  = static_cast<size_t>(dt->ncols);

  _clear_names();
  for (size_t i = 0; i < ncols; ++i) {
    dt->names.push_back(prefix + std::to_string(i + index0));
  }
}


/**
 * This is a main method to assign column names to a Frame. It checks that the
 * names are valid, not duplicate, and if necessary modifies them to enforce
 * such constraints.
 */
void Frame::_dedup_and_save_names(py::olist nameslist) {
  auto ncols = static_cast<size_t>(dt->ncols);
  if (nameslist.size() != ncols) {
    throw ValueError() << "The `names` list has length " << nameslist.size()
        << ", while the Frame has "
        << (ncols < nameslist.size() && ncols? "only " : "")
        << ncols << " column" << (ncols == 1? "" : "s");
  }

  // Prepare the containers for placing the new column names there
  _clear_names();
  py::otuple new_names(dt->ncols);
  py::odict  new_inames;
  std::vector<std::string> duplicates;

  // If any name is empty or None, it will be replaced with the default name
  // in the end. The reason we don't replace immediately upon seeing an empty
  // name is to ensure that the auto-generated names do not clash with the
  // user-specified names somewhere later in the list.
  bool fill_default_names = false;

  for (size_t i = 0; i < ncols; ++i) {
    py::obj name = nameslist[i];
    if (!name.is_string() && !name.is_none()) {
      throw TypeError() << "Invalid `names` list: element " << i
          << " is not a string";
    }
    // Convert to a C-style name object. Note that if `name` is python None,
    // then the resulting `cname` will be `{nullptr, 0}`.
    CString cname = name.to_cstring();
    char* strname = const_cast<char*>(cname.ch);
    size_t namelen = static_cast<size_t>(cname.size);
    if (namelen == 0) {
      fill_default_names = true;
      dt->names.push_back(std::string());
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
    py::oobj newname = name_mangled? oobj(ostring(resname))
                                   : oobj(name);
    // Check for name duplicates. If the name was already seen before, we
    // replace it with a modified name (by incrementing the name's digital
    // suffix if it has one, or otherwise by adding such a suffix).
    if (new_inames.has(newname)) {
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
      while (new_inames.has(newname)) {
        count++;
        resname = basename + std::to_string(count);
        newname = py::ostring(resname);
      }
    }

    // Store the name in all containers
    dt->names.push_back(resname);
    new_inames.set(newname, oobj(oInt(i)));
    new_names.set(i, std::move(newname));
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
    for (size_t i = 0; i < ncols; ++i) {
      size_t namelen = dt->names[i].size();
      const char* nameptr = dt->names[i].data();
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
      if (!dt->names[i].empty()) continue;
      dt->names[i] = prefix + std::to_string(index0);
      oobj newname = py::ostring(dt->names[i]);
      new_inames.set(newname, oobj(oInt(i)));
      new_names.set(i, std::move(newname));
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

  // Store the pythonic tuple / dict of names
  names  = new_names.release();
  inames = new_inames.release();

  xassert(ncols == dt->names.size());
  xassert(ncols == static_cast<size_t>(PyTuple_Size(names)));
  xassert(ncols == static_cast<size_t>(PyDict_Size(inames)));
}



void Frame::_replace_names_from_map(py::odict replacements)
{
  if (!names)  _init_names();
  if (!inames) _init_inames();

  py::odict names_map(inames);
  py::olist names_list(names);
  _clear_names();
  for (auto kv : replacements) {
    obj key = kv.first;
    obj val = kv.second;
    obj idx = names_map.get(key);
    if (idx.is_undefined()) {
      throw ValueError() << "Cannot find column `" << key.str()
        << "` in the Frame";
    }
    if (!val.is_string()) {
      throw TypeError() << "The replacement name for column `" << key.str()
        << "` should be a string, but got " << val.typeobj();
    }
    int64_t i = idx.to_int64_strict();
    names_list.set(i, val);
  }
  _dedup_and_save_names(names_list);
}


} // namespace py
