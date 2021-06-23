//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include "documentation.h"
#include "frame/py_frame.h"
#include "ltype.h"
#include "python/_all.h"
#include "python/string.h"
#include "python/xargs.h"
#include "stype.h"
#include "types/py_type.h"
namespace py {

PyObject* Frame_Type = nullptr;



//------------------------------------------------------------------------------
// head() & tail()
//------------------------------------------------------------------------------

oobj Frame::head(const XArgs& args) {
  size_t n = std::min(args[0].to<size_t>(10),
                      dt->nrows());
  return m__getitem__(otuple(oslice(0, static_cast<int64_t>(n), 1),
                             None()));
}

DECLARE_METHOD(&Frame::head)
    ->name("head")
    ->docs(dt::doc_Frame_head)
    ->n_positional_args(1)
    ->arg_names({"n"});



oobj Frame::tail(const XArgs& args) {
  size_t n = std::min(args[0].to<size_t>(10),
                      dt->nrows());
  // Note: usual slice `-n::` doesn't work as expected when `n = 0`
  int64_t start = static_cast<int64_t>(dt->nrows() - n);
  return m__getitem__(otuple(oslice(start, oslice::NA, 1),
                             None()));
}

DECLARE_METHOD(&Frame::tail)
    ->name("tail")
    ->docs(dt::doc_Frame_tail)
    ->n_positional_args(1)
    ->arg_names({"n"});




//------------------------------------------------------------------------------
// copy()
//------------------------------------------------------------------------------

oobj Frame::copy(const XArgs& args) {
  bool deepcopy = args[0].to<bool>(false);

  oobj res = Frame::oframe(deepcopy? new DataTable(*dt, DataTable::deep_copy)
                                   : new DataTable(*dt));
  Frame* newframe = static_cast<Frame*>(res.to_borrowed_ref());
  newframe->meta_ = meta_;
  newframe->source_ = source_;
  return res;
}

DECLARE_METHOD(&Frame::copy)
    ->name("copy")
    ->n_keyword_args(1)
    ->arg_names({"deep"})
    ->docs(dt::doc_Frame_copy);



oobj Frame::m__deepcopy__(const XArgs&) {
  return robj(this).get_attr("copy")
          .call({}, {py::ostring("deep"), py::True()});
}

DECLARE_METHOD(&Frame::m__deepcopy__)
    ->name("__deepcopy__")
    ->n_positional_or_keyword_args(1)
    ->arg_names({"memo"});

oobj Frame::m__copy__() {
  return robj(this).invoke("copy");
}




//------------------------------------------------------------------------------
// __len__()
//------------------------------------------------------------------------------

size_t Frame::m__len__() const {
  return dt->ncols();
}




//------------------------------------------------------------------------------
// export_names()
//------------------------------------------------------------------------------

oobj Frame::export_names(const XArgs&) {
  py::oobj f = py::oobj::import("datatable", "f");
  py::otuple names = dt->get_pynames();
  py::otuple out_vars(names.size());
  for (size_t i = 0; i < dt->ncols(); ++i) {
    out_vars.set(i, f.get_item(names[i]));
  }
  return std::move(out_vars);
}

DECLARE_METHOD(&Frame::export_names)
    ->name("export_names")
    ->docs(dt::doc_Frame_export_names);




//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------
bool Frame::internal_construction = false;


oobj Frame::oframe(DataTable* dt) {
  xassert(Frame_Type);
  Frame::internal_construction = true;
  PyObject* res = PyObject_CallObject(Frame_Type, nullptr);
  Frame::internal_construction = false;
  if (!res) throw PyError();

  Frame* frame = reinterpret_cast<Frame*>(res);
  frame->dt = dt;
  return oobj::from_new_reference(frame);
}

oobj Frame::oframe(DataTable&& dt) {
  return oframe(new DataTable(std::move(dt)));
}


oobj Frame::oframe(robj src) {
  return robj(Frame_Type).call(otuple{src});
}


void Frame::m__dealloc__() {
  delete dt;
  dt = nullptr;
  source_ = nullptr;
}


void Frame::_clear_types() {
  source_ = nullptr;
}




//------------------------------------------------------------------------------
// Materialize
//------------------------------------------------------------------------------

void Frame::materialize(const XArgs& args) {
  bool to_memory = args[0].to<bool>(false);
  dt->materialize(to_memory);
}

DECLARE_METHODv(&Frame::materialize)
    ->name("materialize")
    ->n_positional_or_keyword_args(1)
    ->arg_names({"to_memory"})
    ->docs(dt::doc_Frame_materialize);




//------------------------------------------------------------------------------
// .meta
//------------------------------------------------------------------------------

static GSArgs args_meta("meta", dt::doc_Frame_meta);

oobj Frame::get_meta() const {
  return meta_? meta_ : py::None();
}


void Frame::set_meta(const Arg& meta) {
  if (!meta.is_dict() && !meta.is_none()) {
    throw TypeError() << "`.meta` must be a dictionary or `None`, "
      << "instead got: " << meta.typeobj();
  }
  meta_ = meta.is_none()? py::None() : meta.to_pydict();
}



//------------------------------------------------------------------------------
// .ncols
//------------------------------------------------------------------------------

static GSArgs args_ncols("ncols", dt::doc_Frame_ncols);

oobj Frame::get_ncols() const {
  return py::oint(dt->ncols());
}



//------------------------------------------------------------------------------
// .nrows
//------------------------------------------------------------------------------

static GSArgs args_nrows("nrows", dt::doc_Frame_nrows);

oobj Frame::get_nrows() const {
  return py::oint(dt->nrows());
}

void Frame::set_nrows(const Arg& nr) {
  if (!nr.is_int()) {
    throw TypeError() << "Number of rows must be an integer, not "
        << nr.typeobj();
  }
  int64_t new_nrows = nr.to_int64_strict();
  if (new_nrows < 0) {
    throw ValueError() << "Number of rows cannot be negative";
  }
  dt->resize_rows(static_cast<size_t>(new_nrows));
}



//------------------------------------------------------------------------------
// .shape
//------------------------------------------------------------------------------

static GSArgs args_shape("shape", dt::doc_Frame_shape);

oobj Frame::get_shape() const {
  return otuple(get_nrows(), get_ncols());
}


static GSArgs args_ndims(
  "ndims",
  "Number of dimensions in the Frame, always 2\n");

oobj Frame::get_ndims() const {
  return oint(2);
}



//------------------------------------------------------------------------------
// .source
//------------------------------------------------------------------------------

static GSArgs args_source("source", dt::doc_Frame_source);

oobj Frame::get_source() const {
  return source_? source_ : py::None();
}


void Frame::set_source(const std::string& src) {
  source_ = src.empty()? py::None() : py::ostring(src);
}



//------------------------------------------------------------------------------
// .type
//------------------------------------------------------------------------------

static GSArgs args_type("type", dt::doc_Frame_type);

oobj Frame::get_type() const {
  if (dt->ncols() == 0) return None();
  auto type = dt->get_column(0).type();
  for (size_t i = 1; i < dt->ncols(); ++i) {
    auto colType = dt->get_column(i).type();
    if (!(colType == type)) {
      throw InvalidOperationError()
          << "The type of column '" << dt->get_names()[i]
          << "' is `" << colType << "`, which is different from the "
          "type of the previous column" << (i>1? "s" : "");
    }
  }
  return dt::PyType::make(type);
}



//------------------------------------------------------------------------------
// .types
//------------------------------------------------------------------------------

static GSArgs args_types("types", dt::doc_Frame_types);

oobj Frame::get_types() const {
  py::olist result(dt->ncols());
  for (size_t i = 0; i < dt->ncols(); i++) {
    result.set(i, dt::PyType::make(dt->get_column(i).type()));
  }
  return std::move(result);
}



//------------------------------------------------------------------------------
// .stypes
//------------------------------------------------------------------------------

static GSArgs args_stypes("stypes", dt::doc_Frame_stypes);

oobj Frame::get_stypes() const {
  py::otuple stypes(dt->ncols());
  for (size_t i = 0; i < stypes.size(); ++i) {
    dt::SType st = dt->get_column(i).stype();
    stypes.set(i, dt::stype_to_pyobj(st));
  }
  return std::move(stypes);
}



//------------------------------------------------------------------------------
// .stype
//------------------------------------------------------------------------------

static GSArgs args_stype("stype", dt::doc_Frame_stype);

oobj Frame::get_stype() const {
  if (dt->ncols() == 0) return None();
  dt::SType stype = dt->get_column(0).stype();
  for (size_t i = 1; i < dt->ncols(); ++i) {
    dt::SType col_stype = dt->get_column(i).stype();
    if (col_stype != stype) {
      throw InvalidOperationError()
          << "The stype of column '" << dt->get_names()[i]
          << "' is `" << col_stype << "`, which is different from the "
          "stype of the previous column" << (i>1? "s" : "");
    }
  }
  return stype_to_pyobj(stype);
}



//------------------------------------------------------------------------------
// .ltypes
//------------------------------------------------------------------------------

static GSArgs args_ltypes("ltypes", dt::doc_Frame_ltypes);

oobj Frame::get_ltypes() const {
  py::otuple ltypes(dt->ncols());
  for (size_t i = 0; i < ltypes.size(); ++i) {
    dt::SType st = dt->get_column(i).stype();
    ltypes.set(i, dt::ltype_to_pyobj(stype_to_ltype(st)));
  }
  return std::move(ltypes);
}




//------------------------------------------------------------------------------
// Declare Frame's API
//------------------------------------------------------------------------------

static PKArgs args___init__(1, 0, 3, false, true,
                            {"_data", "names", "types", "type"},
                            "__init__", dt::doc_Frame___init__);

void Frame::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.Frame");
  xt.set_class_doc(dt::doc_Frame);
  xt.set_subclassable(true);
  xt.add(CONSTRUCTOR(&Frame::m__init__, args___init__));
  xt.add(DESTRUCTOR(&Frame::m__dealloc__));
  xt.add(METHOD__LEN__(&Frame::m__len__));
  xt.add(METHOD__GETITEM__(&Frame::m__getitem__));
  xt.add(METHOD__SETITEM__(&Frame::m__setitem__));
  xt.add(METHOD__GETBUFFER__(&Frame::m__getbuffer__, &Frame::m__releasebuffer__));
  Frame_Type = xt.get_type_object();
  args___init__.add_synonym_arg("stypes", "types");
  args___init__.add_synonym_arg("stype", "type");

  _init_key(xt);
  _init_init(xt);
  _init_iter(xt);
  _init_jay(xt);
  _init_names(xt);
  _init_rbind(xt);
  _init_replace(xt);
  _init_repr(xt);
  _init_sizeof(xt);
  _init_stats(xt);
  _init_sort(xt);
  _init_newsort(xt);
  _init_tonumpy(xt);
  _init_topython(xt);

  xt.add(GETTER(&Frame::get_ltypes, args_ltypes));
  xt.add(GETSET(&Frame::get_meta, &Frame::set_meta, args_meta));
  xt.add(GETTER(&Frame::get_ncols, args_ncols));
  xt.add(GETTER(&Frame::get_ndims, args_ndims));
  xt.add(GETSET(&Frame::get_nrows, &Frame::set_nrows, args_nrows));
  xt.add(GETTER(&Frame::get_shape, args_shape));
  xt.add(GETTER(&Frame::get_source, args_source));
  xt.add(GETTER(&Frame::get_stype,  args_stype));
  xt.add(GETTER(&Frame::get_stypes, args_stypes));
  xt.add(GETTER(&Frame::get_type, args_type));
  xt.add(GETTER(&Frame::get_types, args_types));

  xt.add(METHOD0(&Frame::get_names, "keys"));
  xt.add(METHOD0(&Frame::m__copy__, "__copy__"));

  INIT_METHODS_FOR_CLASS(Frame);
}




}  // namespace py
