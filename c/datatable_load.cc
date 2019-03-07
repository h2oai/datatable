//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <stdlib.h>  // abs
#include <string>  // memcpy
#include "frame/py_frame.h"
#include "python/args.h"
#include "datatable.h"
#include "datatablemodule.h"
#include "utils.h"



/**
 * Load DataTable stored in NFF format on disk.
 *
 * colspec
 *     A DataTable object containing information about the columns of the
 *     datatable stored on disk. This object should contain 2 columns, with
 *     file names, and stypes of each column in the stored Frame.
 *
 * nrows
 *     Number of rows in the stored Frame.
 */
DataTable* DataTable::load(DataTable* colspec, size_t nrows, const std::string& path,
                           bool recode)
{
    size_t ncols = colspec->nrows;
    std::vector<Column*> columns;
    columns.reserve(ncols);

    if (colspec->ncols != 2 && colspec->ncols != 4) {
        throw ValueError() << "colspec table should have had 2 or 4 columns, "
                           << "but " << colspec->ncols << " were passed";
    }
    SType stypef = colspec->columns[0]->stype();
    SType stypes = colspec->columns[1]->stype();
    if (stypef != SType::STR32 || stypes != SType::STR32) {
        throw ValueError() << "String columns are expected in colspec table, "
                           << "instead got " << stypef << " and "
                           << stypes;
    }

    auto colf = static_cast<StringColumn<uint32_t>*>(colspec->columns[0]);
    auto cols = static_cast<StringColumn<uint32_t>*>(colspec->columns[1]);

    const uint32_t* offf = colf->offsets();
    const uint32_t* offs = cols->offsets();
    constexpr uint32_t NONA = ~GETNA<uint32_t>();

    std::string rootdir(path);
    if (!(rootdir.empty() || rootdir.back() == '/'))
      rootdir += "/";

    for (size_t i = 0; i < ncols; ++i) {
        // Extract filename
        size_t fsta = static_cast<size_t>(offf[i - 1] & NONA);
        size_t fend = static_cast<size_t>(offf[i] & NONA);
        size_t flen = static_cast<size_t>(fend - fsta);

        std::string filename = rootdir;
        filename.append(colf->strdata() + fsta, flen);

        // Extract stype
        size_t ssta = static_cast<size_t>(offs[i - 1] & NONA);
        size_t send = static_cast<size_t>(offs[i] & NONA);
        size_t slen = static_cast<size_t>(send - ssta);
        if (!(slen == 3 || slen==2)) {
            throw ValueError() << "Incorrect stype's length: " << slen;
        }
        std::string stype_str(cols->strdata() + ssta, slen);
        SType stype = stype_from_string(stype_str);
        if (stype == SType::VOID) {
            throw ValueError() << "Unrecognized stype: " << stype_str;
        }

        // Load the column
        Column* newcol = Column::open_mmap_column(stype, nrows, filename, recode);
        columns.push_back(newcol);
    }

    return new DataTable(std::move(columns));
}



namespace py {

static PKArgs args_open_nff(
  5, 0, 0, false, false,
  {"colspec", "nrows", "path", "recode", "names"}, "open_nff", nullptr);

static oobj open_nff(const PKArgs& args) {
  DataTable* colspec = args[0].to_datatable();
  size_t nrows = args[1].to_size_t();
  std::string path = args[2].to_string();
  int recode = args[3].to_bool_strict();
  oobj names = args[4].to_oobj();

  DataTable* dt = DataTable::load(colspec, nrows, path, recode);
  Frame* frame = Frame::from_datatable(dt);
  frame->set_names(robj(names));
  return oobj::from_new_reference(frame);
}


void DatatableModule::init_methods_nff() {
  ADD_FN(&open_nff, args_open_nff);
}

}  // namespace py
