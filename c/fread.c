// *****************************************************************************
//
//  This file is shared across `R data.table` and `Py datatable`.
//  R:  https://github.com/Rdatatable/data.table
//  Py: https://github.com/h2oai/datatable
//
// *****************************************************************************
#include "fread.h"
#include "fread_impl.h"

static void fread_onexit(void)
{

}


int fread_main(FReadArgs *args, void *out)
{
    _Bool verbose = args->verbose;
    VLOG("hi");
    WARN("careful");
    STOP("error");
    // return 1;
}
