//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#ifndef dt_STATS_h
#define dt_STATS_h
#include <bitset>
#include <vector>
#include "types.h"

class Column;
class OColumn;


enum class Stat : uint8_t {
  NaCount = 0,
  Sum     = 1,
  Mean    = 2,
  StDev   = 3,
  Skew    = 4,
  Kurt    = 5,
  Min     = 6,
  Qt25    = 7,
  Median  = 8,
  Qt75    = 9,
  Max     = 10,
  Mode    = 11,
  NModal  = 12,
  NUnique = 13
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
 *  | PyObjectStats |    |    NumericStats    |    | StringStats |
 *  +---------------+    +--------------------+    +-------------+
 *                      /          |           \
 *       +--------------+   +--------------+   +-----------+
 *       | BooleanStats |   | IntegerStats |   | RealStats |
 *       +--------------+   +--------------+   +-----------+
 *
 *
 * `NumericStats` acts as a base class for all numeric STypes.
 * `IntegerStats` are used with `IntegerColumn<T>`s.
 * `BooleanStats` are used for `BooleanColumn`.
 * `RealStats` are used for `RealColumn<T>` classes.
 * `StringStats` are used with `StringColumn<T>`s.
 *
 * Each stat can be in one of the 3 states: not computed, computed&invalid,
 * and computed&valid. When a stat is not computed, its value is not known.
 * The stored value of the stat in this state can be arbitrary (however, there
 * are no public methods for retrieving this value without computing it).
 *
 * Once a value was computed, it can be either valid or NA (invalid). Some
 * stats (such as NaCount or Sum) are always valid. In the various stat-getter
 * functions the validity of a stat is either returned from the function, or
 * stored in the provided `&isvalid` argument. When a stat is invalid, its
 * stored value can be arbitrary.
 *
 * The type of each stat can be either `int64_t`, or `double`, or `CString`.
 * More types may be added in the future. The table below shows the type
 * of each stat depending on the parent column's stype:
 *
 *     +-------+-----+-----+-----+-----+-----+-----+-----+-----+
 *     |       |     | MIN |     | MEAN| SKEW| QT* |     | NUNQ|
 *     | SType |NACNT| MAX | SUM |  SD | KURT| MED | MODE| NMOD|
 *     +-------+-----+-----+-----+-----+-----+-----+-----+-----+
 *     | BOOL  | i64 | i64 | f64 | f64 | f64 | f64 | i64 | i64 |
 *     | INT8  | i64 | i64 | f64 | f64 | f64 | f64 | i64 | i64 |
 *     | INT16 | i64 | i64 | f64 | f64 | f64 | f64 | i64 | i64 |
 *     | INT32 | i64 | i64 | f64 | f64 | f64 | f64 | i64 | i64 |
 *     | INT64 | i64 | i64 | f64 | f64 | f64 | f64 | i64 | i64 |
 *     | FLT32 | i64 | f64 | f64 | f64 | f64 | f64 | f64 | i64 |
 *     | FLT64 | i64 | f64 | f64 | f64 | f64 | f64 | f64 | i64 |
 *     | STR32 | i64 |  -- |  -- |  -- |  -- |  -- | str | i64 |
 *     | STR64 | i64 |  -- |  -- |  -- |  -- |  -- | str | i64 |
 *     | OBJ   | i64 |  -- |  -- |  -- |  -- |  -- |  -- |  -- |
 *     +-------+-----+-----+-----+-----+-----+-----+-----+-----+
 *
 * There are 3 different ways to access a value of each particular stat. They
 * are listed below. In each case a stat will be computed if it hasn't been
 * computed yet:
 *
 * bool get_stat(Stat stat, [int64_t|double|CString]* out)
 *   If the stat is valid, returns true and stores the value of the stat into
 *   the `&out` variable. If the stat is NA, returns false. The type of the
 *   `&out` variable must be appropriate for the stat/column. If not, the
 *   function will return false without raising an error.
 *
 * [int64_t|double|CString] get_stat_<R>(Stat stat, bool* isvalid)
 *   Similar to the previous, but the value of the stat is returned, while its
 *   validity is stored in the `isvalid` output variable (unless `isvalid` is
 *   nullptr, in which case the validity flag is not returned).
 *
 * size_t nacount()
 * double sum()
 * double mean(bool* isvalid)
 * ...
 * int64_t min_int(bool* isvalid)
 * double min_dbl(bool* isvalid)
 *   Access to each individual stat. If a stat can be NA, the `isvalid` flag
 *   will be the argument. If a stat is a non-negative integer, it will be
 *   returned as size_t for convenience.
 *
 */
class Stats
{
  protected:
    const OColumn& column;
    std::bitset<NSTATS> _computed;
    std::bitset<NSTATS> _valid;
    size_t _countna;
    size_t _nunique;
    size_t _nmodal;

  //---- Generic properties ------------
  public:
    explicit Stats(const OColumn& col);
    Stats(const Stats&) = delete;
    Stats(Stats&&) = delete;
    virtual ~Stats();

    void reset();
    bool is_computed(Stat s) const;
    bool is_valid(Stat s) const;

  protected:
    void set_computed(Stat, bool flag = true);
    void set_valid(Stat, bool flag = true);

  //---- Stat getters ------------------
  public:
    bool get_stat(Stat, int64_t*);
    bool get_stat(Stat, double*);
    bool get_stat(Stat, CString*);

    int64_t get_stat_int(Stat, bool* isvalid);
    double  get_stat_dbl(Stat, bool* isvalid);
    CString get_stat_str(Stat, bool* isvalid);

    virtual size_t  nacount ();
    virtual size_t  nunique ();
    virtual size_t  nmodal  ();
    virtual double  sum     ();
    virtual double  mean    (bool* isvalid);
    virtual double  stdev   (bool* isvalid);
    virtual double  skew    (bool* isvalid);
    virtual double  kurt    (bool* isvalid);
    virtual int64_t min_int (bool* isvalid);
    virtual double  min_dbl (bool* isvalid);
    virtual int64_t max_int (bool* isvalid);
    virtual double  max_dbl (bool* isvalid);
    virtual int64_t mode_int(bool* isvalid);
    virtual double  mode_dbl(bool* isvalid);
    virtual CString mode_str(bool* isvalid);

  //---- Stat setters ------------------
  public:
    void set_stat(Stat, int64_t, bool isvalid = true);
    void set_stat(Stat, double, bool isvalid = true);
    void set_stat(Stat, const CString&, bool isvalid = true);

  //---- Computing stats ---------------
  protected:
    virtual void compute_countna();
    virtual void compute_minmax();


  //--- Old API ---
  public:

    virtual void merge_stats(const Stats*);

    virtual size_t memory_footprint() const = 0;
    virtual void verify_integrity(const Column*) const;

  protected:
    template <typename T, typename F> void verify_stat(Stat, T, F) const;
    virtual void verify_more(Stats*, const Column*) const;

    virtual void compute_nunique(const Column*);
    virtual void compute_sorted_stats(const Column*) = 0;
    void set_isna(Stat s, bool flag);
};



//------------------------------------------------------------------------------
// NumericStats class
//------------------------------------------------------------------------------

/**
 * Base class for all numerical STypes. The class is parametrized by T - the
 * type of "min"/"max"/"mode" statistics, which can be either int64_t or double.
 * All lower integer or floating-point types should be promoted to these.
 *
 * This class itself is not compatible with any SType; one of its children
 * should be used instead (see the inheritance diagram above).
 */
template <typename T>
class NumericStats : public Stats {
  static_assert(std::is_same<T, int64_t>::value ||
                std::is_same<T, double>::value, "Wrong type in NumericStats");
  protected:
    double _sum;
    double _mean;
    double _sd;
    double _skew;
    double _kurt;
    T _min;
    T _max;
    T _mode;

  public:
    using Stats::Stats;

    bool get_nacount(int64_t* out) override;
    bool get_nunique(int64_t* out) override;
    bool get_nmodal (int64_t* out) override;
    bool get_sum    (double* out) override;
    bool get_mean   (double* out) override;
    bool get_stdev  (double* out) override;
    bool get_skew   (double* out) override;
    bool get_kurt   (double* out) override;
    bool get_min    (int64_t* out) override;
    bool get_min    (double* out) override;
    bool get_max    (int64_t* out) override;
    bool get_max    (double* out) override;
    bool get_mode   (int64_t* out) override;
    bool get_mode   (double* out) override;

  //---- OLD ---
    size_t memory_footprint() const override { return sizeof(*this); }

  protected:
    void verify_more(Stats*, const Column*) const override;

    // Helper method that computes min, max, sum, mean, sd, and countna
    virtual void compute_numerical_stats(const Column*);
    virtual void compute_sorted_stats(const Column*) override;
    virtual void compute_countna(const Column*) override;
    void compute_minmax();
};


extern template class NumericStats<int64_t>;
extern template class NumericStats<double>;



//------------------------------------------------------------------------------
// RealStats class
//------------------------------------------------------------------------------

/**
 * Child of NumericalStats that represents real-valued columns.
 */
template <typename T>
class RealStats : public NumericStats<double> {
  public:
    using NumericStats<double>::NumericStats;
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
class IntegerStats : public NumericStats<int64_t> {
  public:
    using NumericStats<int64_t>::NumericStats;
};

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
class BooleanStats : public NumericStats<int64_t> {
  public:
    using NumericStats<int64_t>::NumericStats;
  protected:
    void compute_numerical_stats(const Column* col) override;
    void compute_sorted_stats(const Column*) override;
};



//------------------------------------------------------------------------------
// StringStats class
//------------------------------------------------------------------------------

/**
 * Stats for variable string columns. Uses a template type `T` to define the
 * integer type that is used to represent its offsets.
 */
class StringStats : public Stats {
  private:
    CString _mode;

  public:
    using Stats::Stats;
    virtual size_t memory_footprint() const override { return sizeof(*this); }

    CString mode(const Column*);

  protected:
    void compute_countna(const Column*) override;
    void compute_nunique(const Column*) override;
    void compute_sorted_stats(const Column*) override;
};




//------------------------------------------------------------------------------
// PyObjectStats class
//------------------------------------------------------------------------------

class PyObjectStats : public Stats {
  public:
    using Stats::Stats;
    virtual size_t memory_footprint() const override { return sizeof(*this); }

  protected:
    void compute_countna(const Column*) override;
    void compute_sorted_stats(const Column*) override;
};


#endif /* dt_STATS_h */
