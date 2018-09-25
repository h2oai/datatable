//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <vector>
#include <string>
#include "rowindex.h"
#include "types.h"
#include "datatable.h"
#include <random>
#include "py_datatable.h"
#include "utils.h"


/**
 * Shared mutex implementation with busy `while` loops instead of `std::mutex`.
 */
class shared_mutex {
  private:
    size_t state;
    static constexpr size_t EXCLMASK = 1ULL << (sizeof(size_t) * 8 - 1);

  public:
    shared_mutex() : state(0) {}

    void lock_shared() {
      size_t state_old;

      while (true) {
        #pragma omp atomic read
        state_old = state;
        // This check is required to prevent deadlocks, so that exclusive `lock()`
        // takes priority over the `lock_shared()` when competing. Note, that
        // the `state` should be "omp atomic read", otherwise behavior is unpredictable
        // on some systems.
        if (state_old & EXCLMASK) continue;

        #pragma omp atomic capture
        {state_old = state; ++state;}

        if (state_old & EXCLMASK) {
          #pragma omp atomic update
          --state;
        } else break;
      }
    }

    void unlock_shared() {
      #pragma omp atomic update
      --state;
    }

    void lock() {
      size_t state_old;
      bool exclusive_request = false;

      while (true) {
        #pragma omp atomic capture
        {state_old = state; state |= EXCLMASK;}

        if ((state_old & EXCLMASK) && !exclusive_request) continue;
        if (state_old == EXCLMASK) break;
        exclusive_request = true;
      }
    }

    void unlock() {
      #pragma omp atomic update
      state &= ~EXCLMASK;
    }
};


class shared_lock {
  private:
    shared_mutex& mutex;
    bool exclusive;
    uint64_t : 56;

  public:
    shared_lock(shared_mutex& m, bool excl = false)
      : mutex(m), exclusive(excl)
    {
      if (exclusive) {
        mutex.lock();
      } else {
        mutex.lock_shared();
      }
    }

    ~shared_lock() {
      if (exclusive) {
        mutex.unlock();
      } else {
        mutex.unlock_shared();
      }
    }

    void exclusive_start() {
      if (!exclusive) {
        mutex.unlock_shared();
        mutex.lock();
        exclusive = true;
      }
    }

    void exclusive_end() {
      if (exclusive) {
        mutex.unlock();
        mutex.lock_shared();
        exclusive = false;
      }
    }
};


typedef std::unique_ptr<double[]> DoublePtr;
struct ex {
  int64_t id;
  DoublePtr coords;
};
typedef std::unique_ptr<ex> ExPtr;

#define PBSTR "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 60
#define PBSTEPS 100

class Aggregator {
  public:
    Aggregator(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
               unsigned int, PyObject*, unsigned int);
    DataTablePtr aggregate(DataTable*);
    static constexpr double epsilon = 1.0e-15;
    static void set_norm_coeffs(double&, double&, double, double, int32_t);
    static void print_progress(double, int);

  private:
    int32_t min_rows;
    int32_t n_bins;
    int32_t nx_bins;
    int32_t ny_bins;
    int32_t nd_max_bins;
    int32_t max_dimensions;
    unsigned int seed;
    unsigned int nthreads;
    PyObject* progress_fn;

    // Grouping and aggregating functions
    void group_1d(DataTablePtr&, DataTablePtr&);
    void group_2d(DataTablePtr&, DataTablePtr&);
    void group_nd(DataTablePtr&, DataTablePtr&);
    void test(DataTablePtr&, DataTablePtr&);
    void group_1d_continuous(DataTablePtr&, DataTablePtr&);
    void group_2d_continuous(DataTablePtr&, DataTablePtr&);
    void group_1d_categorical(DataTablePtr&, DataTablePtr&);
    void group_2d_categorical(DataTablePtr&, DataTablePtr&);
    void group_2d_mixed(bool, DataTablePtr&, DataTablePtr&);
    void aggregate_exemplars(DataTable*, DataTablePtr&);

    // Helper functions
    int32_t get_nthreads(DataTablePtr&);
    DoublePtr generate_pmatrix(DataTablePtr&);
    void normalize_row(DataTablePtr&, DoublePtr&, int32_t);
    void project_row(DataTablePtr&, DoublePtr&, int32_t, DoublePtr&);
    double calculate_distance(DoublePtr&, DoublePtr&, int64_t, double,
                              bool early_exit=true);
    void adjust_delta(double&, std::vector<ExPtr>&, std::vector<int64_t>&,
                      int64_t);
    void adjust_members(std::vector<int64_t>&, DataTablePtr&);
    size_t calculate_map(std::vector<int64_t>&, size_t);
    void progress(double, int status_code=0);
};


DECLARE_FUNCTION(
  aggregate,
  "aggregate(self, min_rows=500, n_bins=500, nx_bins=50, ny_bins=50, nd_bins = 500, max_dimensions=50, seed=0, progress_fn=None, nthreads=0)\n\n",
  dt_EXTRAS_AGGREGATOR_cc)
