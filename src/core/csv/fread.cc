//------------------------------------------------------------------------------
// Original version of this file was contributed by Matt Dowle from R data.table
// project (https://github.com/Rdatatable/data.table)
//------------------------------------------------------------------------------
#include "csv/reader_fread.h"                  // FreadReader
#include "read/fread/fread_parallel_reader.h"  // FreadParallelReader
#include "read/parse_context.h"                // ParseContext
#include "utils/misc.h"                        // wallclock
#include "datatable.h"                         // DataTable

#define D() if (verbose) logger_.info()



//==============================================================================
//
// Main fread() function that does all the job of reading a text/csv file and
// returning std::unique_ptr<DataTable>
//
//==============================================================================
std::unique_ptr<DataTable> FreadReader::read_all()
{
  job->add_work_amount(WORK_PREPARE);
  detect_lf();
  skip_preamble();

  if (verbose) fo.t_initialized = wallclock();

  detect_sep_and_qr();    // [2]
  detect_column_types();  // [3]

  //****************************************************************************
  // [4] Parse column names (if present)
  //
  //     This section also moves the `sof` pointer to point at the first row
  //     of data ("removing" the column names).
  //****************************************************************************
  if (header == 1) {
    auto _ = logger_.section("[4] Assign column names");
    dt::read::field64 tmp;
    dt::read::ParseContext fctx = makeTokenizer();
    fctx.target = &tmp;
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
    auto _ = logger_.section("[5] Apply user overrides on column types");
    auto oldtypes = preframe.get_ptypes();

    report_columns_to_python();

    size_t ncols = preframe.ncols();
    size_t ndropped = 0;
    int nUserBumped = 0;
    for (size_t i = 0; i < ncols; i++) {
      auto& col = preframe.column(i);
      if (col.is_dropped()) {
        ndropped++;
        continue;
      }
      if (col.get_ptype() < oldtypes[i]) {
        // FIXME: if the user wants to override the type, let them
        throw IOError()
            << "Attempt to override column " << i + 1 << " \"" << col.repr_name(*this)
            << "\" with detected type '" << dt::read::parser_infos[oldtypes[i]].name()
            << "' down to '" << col.typeName() << "' which is not supported yet.";
      }
      nUserBumped += (col.get_ptype() != oldtypes[i]);
    }

    if (verbose) {
      if (nUserBumped || ndropped) {
        D() << "After " << nUserBumped << " type and " << ndropped
            << " drop user overrides : " << preframe.print_ptypes();
      }
      D() << "Allocating " << dt::log::plural(ncols - ndropped, "column slot")
          << " with " << dt::log::plural(allocnrow, "row");
    }
    preframe.preallocate(allocnrow);

    if (verbose) {
      fo.t_frame_allocated = wallclock();
      fo.n_rows_allocated = allocnrow;
      fo.n_cols_allocated = ncols - ndropped;
      fo.allocation_size = preframe.total_allocsize();
    }
  }
  job->add_done_amount(WORK_PREPARE);


  //****************************************************************************
  // [6] Read the data
  //****************************************************************************
  auto typesPtr = preframe.get_ptypes();
  dt::read::PT* types = typesPtr.data();  // This pointer is valid until `typesPtr` goes out of scope

  {
    auto _ = logger_.section("[6] Read the data");
    job->set_message("Reading data");
    dt::progress::subtask subwork(*job, WORK_READ);
    dt::read::FreadParallelReader scr(*this, types);
    scr.read_all();
    subwork.done();

    fo.t_data_read = wallclock();
    fo.n_rows_read = preframe.nrows_written();
    fo.n_cols_read = preframe.n_columns_in_output();
  }


  auto _ = logger_.section("[7] Finalizing the frame");
  std::unique_ptr<DataTable> res = std::move(preframe).to_datatable();
  if (verbose) fo.report();
  return res;
}
