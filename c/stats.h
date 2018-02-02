//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_STATS_h
#define dt_STATS_h
#include <stddef.h>
#include <stdint.h>
#include <vector>
#include "datatable_check.h"
#include "types.h"

class Column;


//------------------------------------------------------------------------------
// Stats class
//------------------------------------------------------------------------------

/**
 * Base class in the hierarchy of Statistics Containers:
 *
 *                                +-------+
 *                                | Stats |
 *                                +-------+
 *                                 /     \
 *                 +----------------+   +-------------+
 *                 | NumericalStats |   | StringStats |
 *                 +----------------+   +-------------+
 *                   /           \
 *         +--------------+   +-----------+
 *         | IntegerStats |   | RealStats |
 *         +--------------+   +-----------+
 *              /
 *     +--------------+
 *     | BooleanStats |
 *     +--------------+
 *
 * `NumericalStats` acts as a base class for all numeric STypes.
 * `IntegerStats` are used with `IntegerColumn<T>`s.
 * `BooleanStats` are used for `BooleanColumn`.
 * `RealStats` are used for `RealColumn<T>` classes.
 * `StringStats` are used with `StringColumn<T>`s.
 *
 * Each class supports methods to compute/retrieve statistics supported by its
 * corresponding column. To be more specific, every available stat <S> must have
 * public methods
 *   <S>_computed() - check whether the statistic was already computed
 *   <S>_compute(Column*) - force calculation of the statistic base on the data
 *       from the provided column.
 *   <S>_get() - retrieve the value of computed statistic (but the user should
 *       check its availability first).
 *
 */
class Stats {
  protected:
    uint64_t compute_mask;
    int64_t _countna;

  public:
    Stats();
    virtual ~Stats() {}
    Stats(const Stats&) = delete;
    void operator=(const Stats&) = delete;
    virtual size_t memory_footprint() const { return sizeof(*this); }

    virtual void countna_compute(const Column*) = 0;
    bool         countna_computed() const;
    int64_t      countna_get() const { return _countna; }

    virtual void reset();
    virtual void merge_stats(const Stats*);

    bool verify_integrity(IntegrityCheckContext&,
                          const std::string& name = "Stats") const;
};



//------------------------------------------------------------------------------
// NumericalStats class
//------------------------------------------------------------------------------

/**
 * Base class for all numerical STypes. The class is parametrized by:
 *   T - The type of "min"/"max" statistics. This is should be the same as the
 *       type of a column's element.
 *   A - The type "sum" statistic. This is either `int64_t` for integral types,
 *       or `double` for real-valued columns.
 *
 * This class itself is not compatible with any SType; one of its children
 * should be used instead (see the inheritance diagram above).
 */
template <typename T, typename A>
class NumericalStats : public Stats {
  protected:
    double _mean;
    double _sd;
    A _sum;
    T _min;
    T _max;
    int64_t : (sizeof(A) + sizeof(T) + sizeof(T)) * 56 % 64;

  public:
    NumericalStats();
    void reset() override;
    size_t memory_footprint() const override { return sizeof(*this); }

    // Mean value
    void   mean_compute(const Column*);
    bool   mean_computed() const;
    double mean_get() const { return _mean; }

    // Standard deviation
    void   sd_compute(const Column*);
    bool   sd_computed() const;
    double sd_get() const { return _sd; }

    // Minimum
    void min_compute(const Column*);
    bool min_computed() const;
    T    min_get() const { return _min; }

    // Maximum
    void max_compute(const Column*);
    bool max_computed() const;
    T    max_get() const { return _max; }

    // Sum
    void sum_compute(const Column*);
    bool sum_computed() const;
    A    sum_get() const { return _sum; }

    // Count NA
    void countna_compute(const Column*) override;

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



//------------------------------------------------------------------------------
// RealStats class
//------------------------------------------------------------------------------

/**
 * Child of NumericalStats that represents real-valued columns.
 */
template <typename T>
class RealStats : public NumericalStats<T, double> {
  protected:
    void compute_numerical_stats(const Column*) override;
};

extern template class RealStats<float>;
extern template class RealStats<double>;



//------------------------------------------------------------------------------
// IntegerStats class
//------------------------------------------------------------------------------

/**
 * Child of NumericalStats that represents integer-valued columns.
 */
template <typename T>
class IntegerStats : public NumericalStats<T, int64_t> {
};

extern template class IntegerStats<int8_t>;
extern template class IntegerStats<int16_t>;
extern template class IntegerStats<int32_t>;
extern template class IntegerStats<int64_t>;



//------------------------------------------------------------------------------
// BooleanStats class
//------------------------------------------------------------------------------

/**
 * Child of IntegerStats that represents boolean columns. Although Boolean
 * stype is treated as if it was `int8_t`, some optimizations can be achieved
 * if we know that the set of possible element values is {0, 1, NA}.
 */
class BooleanStats : public NumericalStats<int8_t, int64_t> {
  protected:
    void compute_numerical_stats(const Column *col) override;
};



//------------------------------------------------------------------------------
// StringStats class
//------------------------------------------------------------------------------

/**
 * Stats for variable string columns. Uses a template type `T` to define the
 * integer type that is used to represent its offsets.
 */
template <typename T>
class StringStats : public Stats {
  public:
    void countna_compute(const Column*) override;
    virtual size_t memory_footprint() const override { return sizeof(*this); }
};

extern template class StringStats<int32_t>;
extern template class StringStats<int64_t>;



#endif /* dt_STATS_h */
