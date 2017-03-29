// *****************************************************************************
//
//  This file is shared across `R data.table` and `Py datatable`.
//  R:  https://github.com/Rdatatable/data.table
//  Py: https://github.com/h2oai/datatable
//
// *****************************************************************************
#include "fread.h"
#include "fread_impl.h"
#define STOP(...) fread_STOP(__VA_ARGS__)
#define WARN(...) fread_WARN(__VA_ARGS__)
#define VLOG(...) fread_VLOG(__VA_ARGS__)


int fread_main(FReadArgs *self, void *out)
{
  return 1;
}

