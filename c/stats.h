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
#include "types.h"

class Column;


enum class Stat : uint8_t {
  NaCount,
  Sum,
  Mean,
  StDev,
  Skew,
  Kurt,
  Min,
  Qt25,
  Median,
  Qt75,
  Max,
  Mode,
  NModal,
  NUnique
};
constexpr uint8_t NSTATS = 14;



//------------------------------------------------------------------------------
// Stats class
//------------------------------------------------------------------------------

/**
 * Base class in the hierarchy of Statistics Containers:
 *
 *                           +---------+
 *                  _________|  Stats  |____________
 *                 /         +---------+            \
 *                /               |                  \
 *  +---------------+    +--------------------+    +-------------+
 *  | PyObjectStats |    |   NumericalStats   |    | StringStats |
 *  +---------------+    +--------------------+    +-------------+
 *                      /          |           \
 *       +--------------+   +--------------+   +-----------+
 *       | BooleanStats |   | IntegerStats |   | RealStats |
 *       +--------------+   +--------------+   +-----------+
 *
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
    std::bitset<NSTATS> _computed;
    int64_t _countna;
    int64_t _nunique;
    int64_t _nmodal;

  public:
    Stats() = default;
    virtual ~Stats() {}
    Stats(const Stats&) = delete;
    void operator=(const Stats&) = delete;

    int64_t countna(const Column*);
    int64_t nunique(const Column*);
    int64_t nmodal(const Column*);

    bool is_computed(Stat s) const;
    void reset();
    void set_countna(int64_t n);
    virtual void merge_stats(const Stats*);

    virtual size_t memory_footprint() const = 0;
    virtual void verify_integrity(const Column*) const;

  protected:
    virtual Stats* make() const = 0;
    template <typename T, typename F> void verify_stat(Stat, T, F) const;
    virtual void verify_more(Stats*, const Column*) const;

    virtual void compute_countna(const Column*) = 0;
    virtual void compute_sorted_stats(const Column*) = 0;
    void set_computed(Stat s);
    void set_computed(Stat s, bool flag);
};



//------------------------------------------------------------------------------
// NumericalStats class
//------------------------------------------------------------------------------

/**
 * Base class for all numerical STypes. The class is parametrized by:
 *   T - The type of "min"/"max" statistics. This should be the same as the
 *       type of a column's element.
 *   A - The type of "sum" statistic. This is either `int64_t` for integral
 *       types, or `double` for real-valued columns.
 *
 * This class itself is not compatible with any SType; one of its children
 * should be used instead (see the inheritance diagram above).
 */
template <typename T, typename A>
class NumericalStats : public Stats {
  protected:
    double _mean;
    double _sd;
    double _skew;
    double _kurt;
    A _sum;
    T _min;
    T _max;
    T _mode;
    int64_t : (sizeof(A) + sizeof(T) * 3) * 56 % 64;

  public:
    size_t memory_footprint() const override { return sizeof(*this); }

    double mean(const Column*);
    double stdev(const Column*);
    double skew(const Column*);
    double kurt(const Column*);
    T min(const Column*);
    T max(const Column*);
    T mode(const Column*);
    A sum(const Column*);

    void set_min(T value);
    void set_max(T value);

    // void verify_integrity(const Column*) const override;

  protected:
    void verify_more(Stats*, const Column*) const override;

    // Helper method that computes min, max, sum, mean, sd, and countna
    virtual void compute_numerical_stats(const Column*);
    virtual void compute_sorted_stats(const Column*) override;
    virtual void compute_countna(const Column*) override;
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
    virtual RealStats<T>* make() const override;
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
  protected:
    virtual IntegerStats<T>* make() const override;
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
    virtual BooleanStats* make() const override;
    void compute_numerical_stats(const Column *col) override;
    void compute_sorted_stats(const Column*) override;
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
  private:
    CString _mode;

  public:
    virtual size_t memory_footprint() const override { return sizeof(*this); }

    CString mode(const Column*);

  protected:
    StringStats<T>* make() const override;
    void compute_countna(const Column*) override;
    void compute_sorted_stats(const Column*) override;
};

extern template class StringStats<uint32_t>;
extern template class StringStats<uint64_t>;



//------------------------------------------------------------------------------
// PyObjectStats class
//------------------------------------------------------------------------------

class PyObjectStats : public Stats {
  public:
    virtual size_t memory_footprint() const override { return sizeof(*this); }

  protected:
    PyObjectStats* make() const override;
    void compute_countna(const Column*) override;
    void compute_sorted_stats(const Column*) override;
};


#endif /* dt_STATS_h */
