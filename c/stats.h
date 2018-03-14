//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_STATS_h
#define dt_STATS_h
#include <bitset>
#include <vector>
#include "datatable_check.h"
#include "types.h"

class Column;


enum Stat {
  Min,
  Max,
  Sum,
  Mean,
  StDev,
  NaCnt,
  NUniq,
};



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
 */
class Stats {
  protected:
    std::bitset<7> _computed;
    int64_t _countna;
    int64_t _nuniq;

  public:
    Stats() = default;
    virtual ~Stats() {}
    Stats(const Stats&) = delete;
    void operator=(const Stats&) = delete;

    int64_t countna_get(const Column*);

    bool is_computed(Stat s) const;
    bool countna_computed() const;
    bool mean_computed() const;
    bool sd_computed() const;
    bool min_computed() const;
    bool max_computed() const;
    bool sum_computed() const;

    void reset();
    virtual void merge_stats(const Stats*);

    virtual size_t memory_footprint() const;
    bool verify_integrity(IntegrityCheckContext&,
                          const std::string& name = "Stats") const;

  protected:
    virtual void countna_compute(const Column*) = 0;
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
    size_t memory_footprint() const override { return sizeof(*this); }

    double mean_get(const Column*);
    double sd_get(const Column*);
    T min_get(const Column*);
    T max_get(const Column*);
    A sum_get(const Column*);

  protected:
    // Helper method that computes min, max, sum, mean, sd, and countna
    virtual void compute_numerical_stats(const Column*);
    virtual void compute_sorted_stats(const Column*);
    void countna_compute(const Column*) override;
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
