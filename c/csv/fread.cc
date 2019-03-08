//------------------------------------------------------------------------------
// Original version of this file contributed by Matt Dowle, from R data.table
// project (https://github.com/Rdatatable/data.table)
//------------------------------------------------------------------------------
#include <ctype.h>     // isspace
#include <stdarg.h>    // va_list, va_start
#include <stdio.h>     // vsnprintf
#include <algorithm>
#include <cmath>       // std::sqrt, std::ceil
#include <cstdio>      // std::snprintf
#include <string>      // std::string
#include "csv/fread.h"
#include "csv/freadLookups.h"
#include "csv/reader.h"
#include "csv/reader_fread.h"
#include "csv/reader_parsers.h"
#include "utils/assert.h"
#include "datatable.h"



//=================================================================================================
//
// Main fread() function that does all the job of reading a text/csv file.
//
// Returns 1 if it finishes successfully, and 0 otherwise.
//
//=================================================================================================
std::unique_ptr<DataTable> FreadReader::read_all()
{
  detect_lf();
  skip_preamble();

  if (verbose) fo.t_initialized = wallclock();

  detect_sep_and_qr();    // [2]
  detect_column_types();  // [3]

  //*********************************************************************************************
  // [4] Parse column names (if present)
  //
  //     This section also moves the `sof` pointer to point at the first row
  //     of data ("removing" the column names).
  //*********************************************************************************************
  if (header == 1) {
    trace("[4] Assign column names");
    dt::read::field64 tmp;
    FreadTokenizer fctx = makeTokenizer(&tmp, /* anchor= */ sof);
    fctx.ch = sof;
    parse_column_names(fctx);
    sof = fctx.ch;  // Update sof to point to the first line after the columns
    line++;
  }
  if (verbose) fo.t_column_types_detected = wallclock();


  //*********************************************************************************************
  // [5] Allow user to override column types; then allocate the DataTable
  //*********************************************************************************************
  {
    if (verbose) trace("[5] Apply user overrides on column types");
    std::unique_ptr<PT[]> oldtypes = columns.getTypes();

    report_columns_to_python();

    size_t ncols = columns.size();
    size_t ndropped = 0;
    int nUserBumped = 0;
    for (size_t i = 0; i < ncols; i++) {
      dt::read::Column& col = columns[i];
      col.reset_type_bumped();
      if (col.is_dropped()) {
        ndropped++;
        continue;
      } else {
        if (col.get_ptype() < oldtypes[i]) {
          // FIXME: if the user wants to override the type, let them
          throw RuntimeError()
              << "Attempt to override column " << i + 1 << " \"" << col.repr_name(*this)
              << "\" with detected type '" << ParserLibrary::info(oldtypes[i]).cname()
              << "' down to '" << col.typeName() << "' which will lose accuracy. "
                 "If this was intended, please coerce to the lower type afterwards. Only "
                 "overrides to a higher type are permitted.";
        }
        nUserBumped += (col.get_ptype() != oldtypes[i]);
      }
    }
    if (verbose) {
      if (nUserBumped || ndropped) {
        trace("After %d type and %d drop user overrides : %s",
              nUserBumped, ndropped, columns.printTypes());
      }
      trace("Allocating %d column slots with %zd rows",
            ncols - ndropped, allocnrow);
    }

    columns.set_nrows(allocnrow);

    if (verbose) {
      fo.t_frame_allocated = wallclock();
      fo.n_rows_allocated = allocnrow;
      fo.n_cols_allocated = ncols - ndropped;
      fo.allocation_size = columns.totalAllocSize();
    }
  }


  //*********************************************************************************************
  // [6] Read the data
  //*********************************************************************************************
  bool firstTime = true;

  std::unique_ptr<PT[]> typesPtr = columns.getTypes();
  PT* types = typesPtr.get();  // This pointer is valid until `typesPtr` goes out of scope

  trace("[6] Read the data");
  read:  // we'll return here to reread any columns with out-of-sample type exceptions
  {
    dt::read::FreadParallelReader scr(*this, types);
    scr.read_all();

    if (firstTime) {
      fo.t_data_read = fo.t_data_reread = wallclock();
    } else {
      fo.t_data_reread = wallclock();
    }
    size_t ncols = columns.size();
    size_t ncols_to_reread = columns.nColumnsToReread();
    if (ncols_to_reread) {
      fo.n_cols_reread += ncols_to_reread;
      size_t n_type_bump_cols = 0;
      for (size_t j = 0; j < ncols; j++) {
        dt::read::Column& col = columns[j];
        if (!col.is_in_output()) continue;
        bool bumped = col.is_type_bumped();
        col.reset_type_bumped();
        col.set_in_buffer(bumped);
        n_type_bump_cols += bumped;
      }
      firstTime = false;
      if (verbose) {
        trace(n_type_bump_cols == 1
              ? "%zu column needs to be re-read because its type has changed"
              : "%zu columns need to be re-read because their types have changed",
              n_type_bump_cols);
      }
      goto read;
    }

    fo.n_rows_read = columns.get_nrows();
    fo.n_cols_read = columns.nColumnsInOutput();
  }


  trace("[7] Finalize the datatable");
  auto res = makeDatatable();
  if (verbose) fo.report();
  return res;
}
