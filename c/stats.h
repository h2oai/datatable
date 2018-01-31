//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------

#ifndef dt_STATS_H
#define dt_STATS_H
#include <stddef.h>
#include <stdint.h>
#include <vector>
#include "types.h"
#include "datatable_check.h"

class Column;

/**
 * Statistics Container
 * Gone is the era of CStats. The Stats class contains a method for each
 * retrievable statistic. To be more specific, every available stat must have
 * the public methods `stat()` and `stat_datatable()`. In the case for a stat
 * whose type may vary by column, it is the user's responsibility to determine
 * what is the appropriate return type.
 *
 * A Stats class is associated with a Column and Rowindex (the latter may be
 * NULL). This class does NOT contain a reference counter. Create a copy
 * of the class instead of having more than one reference to it (Not currently
 * available).
 *
 * A reference to Stats should never be NULL; it should instead point to the
 * Stats instance returned by `void_ptr()`. This is to prevent the need for NULL
 * checks before calling a method. A "void" Stats will return a NA value for any
 * statistic.
 */
class Stats {
protected:
  uint64_t compute_mask;
public:
  int64_t _countna;

  Stats();
  virtual ~Stats();
  virtual void compute_countna(const Column*) = 0;
  bool countna_computed() const;

  virtual void reset();
  size_t alloc_size() const;
  void merge_stats(const Stats*); // TODO: virtual when implemented

  bool verify_integrity(IntegrityCheckContext&,
                        const std::string& name = "Stats") const;
};


/**
 * Base class for all numerical STypes. Two template types are used:
 *     T - The type for all statistics containing a value from `_ref_col`
 *         (e.g. min, max). Naturally, `T` should be compatible with `_ref_col`s
 *         SType.
 *     A - The type for all aggregate statistics whose type is dependent on `T`
 *         (e.g. sum). `T` should be able to cast into `A`
 * This class itself is not compatible with any SType; one of its children
 * should be used instead.
 */
template <typename T, typename A>
class NumericalStats : public Stats {
public:
  double _mean;
  double _sd;
  A _sum;
  T _min;
  T _max;
  int64_t : ((sizeof(A) + (sizeof(T) * 2) * 7) % 8) * 8;
  NumericalStats();
  void reset() override;

  /**
   * The `compute_stat` methods will compute the value of a statistic
   * regardless if it has been flagged as computed or not.
   */
  void compute_mean(const Column*);
  void compute_sd(const Column*);
  void compute_min(const Column*);
  void compute_max(const Column*);
  void compute_sum(const Column*);
  void compute_countna(const Column*) override;

  bool mean_computed() const;
  bool sd_computed() const;
  bool min_computed() const;
  bool max_computed() const;
  bool sum_computed() const;

protected:
  // Helper method that computes min, max, sum, mean, sd, and countna
  virtual void compute_numerical_stats(const Column*);
};

extern template class NumericalStats<int8_t, int64_t>;
extern template class NumericalStats<int16_t, int64_t>;
extern template class NumericalStats<int32_t, int64_t>;
extern template class NumericalStats<int64_t, int64_t>;
extern template class NumericalStats<float, double>;
extern template class NumericalStats<double, double>;

/**
 * Child of NumericalStats that defaults `A` to a double. `T` should be a float
 * or double.
 */
template <typename T>
class RealStats : public NumericalStats<T, double> {
public:
    RealStats() : NumericalStats<T, double>() {}
protected:
    void compute_numerical_stats(const Column*) override;
};

extern template class RealStats<float>;
extern template class RealStats<double>;


/**
 * Child of NumericalStats that defaults `A` to int64_t. `T` should be an
 * integer type.
 */
template <typename T>
class IntegerStats : public NumericalStats<T, int64_t> {
public:
  IntegerStats() : NumericalStats<T, int64_t>() {}
};

extern template class IntegerStats<int8_t>;
extern template class IntegerStats<int16_t>;
extern template class IntegerStats<int32_t>;
extern template class IntegerStats<int64_t>;

/**
 * Child of NumericalStats that defaults `T` to int8_t and `A` to int64_t.
 * `compute_numerical_stats()` is optimized since a boolean SType should only
 * have 3 values (true, false, NA).
 */
class BooleanStats : public NumericalStats<int8_t, int64_t> {
public:
  BooleanStats() : NumericalStats<int8_t, int64_t>() {}
protected:
  void compute_numerical_stats(const Column *col) override;
};


/**
 * Stats for variable string columns. Uses a template type `T` to define the
 * integer type that is used to represent its offsets.
 */
template <typename T>
class StringStats : public Stats {
public:
  StringStats();
  void reset() override;
  void compute_countna(const Column*) override;
};

extern template class StringStats<int32_t>;
extern template class StringStats<int64_t>;

void init_stats(void);

#endif /* STATS_H */

