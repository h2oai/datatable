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
#include <memory>
#include <vector>
#include "types.h"

namespace dt {
  class ColumnImpl;
}
class Column;


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
 * `RealStats` are used for floating-point-valued classes.
 * `StringStats` are used with `SentinelStr_ColumnImpl<T>`s.
 *
 * Each stat can be in one of the 3 states: not computed, computed&invalid,
 * and computed&valid. When a stat is not computed, its value is not known.
 * The stored value of the stat in this state can be arbitrary (however, there
 * are no public methods for retrieving this value without computing it).
 *
 * Once a value was computed, it can be either valid or NA (invalid). Some
 * stats (such as NaCount) are always valid. In the various stat-getter
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
 *     | BOOL  | u64 | i64 | f64 | f64 | f64 | f64 | i64 | u64 |
 *     | INT8  | u64 | i64 | f64 | f64 | f64 | f64 | i64 | u64 |
 *     | INT16 | u64 | i64 | f64 | f64 | f64 | f64 | i64 | u64 |
 *     | INT32 | u64 | i64 | f64 | f64 | f64 | f64 | i64 | u64 |
 *     | INT64 | u64 | i64 | f64 | f64 | f64 | f64 | i64 | u64 |
 *     | FLT32 | u64 | f64 | f64 | f64 | f64 | f64 | f64 | u64 |
 *     | FLT64 | u64 | f64 | f64 | f64 | f64 | f64 | f64 | u64 |
 *     | STR32 | u64 |  -- |  -- |  -- |  -- |  -- | str | u64 |
 *     | STR64 | u64 |  -- |  -- |  -- |  -- |  -- | str | u64 |
 *     | OBJ   | u64 |  -- |  -- |  -- |  -- |  -- |  -- |  -- |
 *     +-------+-----+-----+-----+-----+-----+-----+-----+-----+
 *
 * There are 3 different ways to access a value of each particular stat. They
 * are listed below. In each case a stat will be computed if it hasn't been
 * computed yet:
 *
 * bool get_stat(Stat stat, [int64_t|size_t|double|CString]* out)
 *   If the stat is valid, returns true and stores the value of the stat into
 *   the `&out` variable. If the stat is NA, returns false. The type of the
 *   `&out` variable must be appropriate for the stat/column. If not, the
 *   function will return false without raising an error.
 *
 * [int64_t|size_t|double|CString] get_stat_<R>(Stat stat, bool* isvalid)
 *   Similar to the previous, but the value of the stat is returned, while its
 *   validity is stored in the `isvalid` output variable (unless `isvalid` is
 *   nullptr, in which case the validity flag is not returned).
 *
 * size_t nacount()
 * double sum()
 * double mean(bool* isvalid)
 * ...
 * int64_t min_int(bool* isvalid)
 * size_t min_uint(bool* isvalid)
 * double min_double(bool* isvalid)
 *   Access to each individual stat. If a stat can be NA, the `isvalid` flag
 *   will be the argument.
 *
 */
class Stats
{
  protected:
    const dt::ColumnImpl* column;
    std::bitset<NSTATS> _computed;
    std::bitset<NSTATS> _valid;
    size_t _countna;
    size_t _nunique;
    size_t _nmodal;

  //---- Generic properties ------------
  public:
    explicit Stats(const dt::ColumnImpl* col);
    Stats(const Stats&) = delete;
    Stats(Stats&&) = delete;
    virtual ~Stats();

    void reset();
    bool is_computed(Stat s) const;
    bool is_valid(Stat s) const;

    virtual size_t memory_footprint() const noexcept = 0;
    virtual std::unique_ptr<Stats> clone() const = 0;
    void verify_integrity(const dt::ColumnImpl*);

  protected:
    // Also sets the `computed` flag
    void set_valid(Stat, bool isvalid = true);


  //---- Stat getters ------------------
  public:
    bool get_stat(Stat, int64_t*);
    bool get_stat(Stat, size_t*);
    bool get_stat(Stat, double*);
    bool get_stat(Stat, CString*);

    int64_t get_stat_int   (Stat, bool* isvalid = nullptr);
    size_t  get_stat_uint  (Stat, bool* isvalid = nullptr);
    double  get_stat_double(Stat, bool* isvalid = nullptr);
    CString get_stat_string(Stat, bool* isvalid = nullptr);

    virtual size_t  nacount    (bool* isvalid = nullptr);
    virtual size_t  nunique    (bool* isvalid = nullptr);
    virtual size_t  nmodal     (bool* isvalid = nullptr);
    virtual double  sum        (bool* isvalid = nullptr);
    virtual double  mean       (bool* isvalid = nullptr);
    virtual double  stdev      (bool* isvalid = nullptr);
    virtual double  skew       (bool* isvalid = nullptr);
    virtual double  kurt       (bool* isvalid = nullptr);
    virtual int64_t min_int    (bool* isvalid = nullptr);
    virtual double  min_double (bool* isvalid = nullptr);
    virtual int64_t max_int    (bool* isvalid = nullptr);
    virtual double  max_double (bool* isvalid = nullptr);
    virtual int64_t mode_int   (bool* isvalid = nullptr);
    virtual double  mode_double(bool* isvalid = nullptr);
    virtual CString mode_string(bool* isvalid = nullptr);

    py::oobj get_stat_as_pyobject(Stat);
    Column get_stat_as_column(Stat);


  //---- Stat setters ------------------
  public:
    void set_stat(Stat, int64_t value, bool isvalid = true);
    void set_stat(Stat, size_t value, bool isvalid = true);
    void set_stat(Stat, double value, bool isvalid = true);
    void set_stat(Stat, const CString& value, bool isvalid = true);

    virtual void set_nacount(size_t value, bool isvalid = true);
    virtual void set_nunique(size_t value, bool isvalid = true);
    virtual void set_nmodal (size_t value, bool isvalid = true);
    virtual void set_sum    (double value, bool isvalid = true);
    virtual void set_mean   (double value, bool isvalid = true);
    virtual void set_stdev  (double value, bool isvalid = true);
    virtual void set_skew   (double value, bool isvalid = true);
    virtual void set_kurt   (double value, bool isvalid = true);
    virtual void set_min    (int64_t value, bool isvalid = true);
    virtual void set_min    (double  value, bool isvalid = true);
    virtual void set_max    (int64_t value, bool isvalid = true);
    virtual void set_max    (double  value, bool isvalid = true);
    virtual void set_mode   (int64_t value, bool isvalid = true);
    virtual void set_mode   (double  value, bool isvalid = true);
    virtual void set_mode   (CString value, bool isvalid = true);


  //---- Computing stats ---------------
  protected:
    virtual void compute_nacount();
    virtual void compute_nunique();
    virtual void compute_minmax();
    virtual void compute_moments12();
    virtual void compute_moments34();
    virtual void compute_sorted_stats();
    void _fill_validity_flag(Stat stat, bool* isvalid);
    template <typename S> std::unique_ptr<Stats> _clone(const S* inp) const;


  private:
    template <typename S> py::oobj pywrap_stat(Stat);
    template <typename S, typename R> Column colwrap_stat(Stat, SType);
    Column strcolwrap_stat(Stat);
    friend class Column;
};



//------------------------------------------------------------------------------
// NumericStats class
//------------------------------------------------------------------------------

/**
 * Base class for all numerical STypes. The class is parametrized by T - the
 * type of element in the Column's API. Thus, T can be only int32_t|int64_t|
 * float|double. The corresponding "min"/"max"/"mode" statistics are stored
 * in upcasted type V, which is either int64_t or double.
 */
template <typename T>
class NumericStats : public Stats {
  using V = typename std::conditional<std::is_integral<T>::value,
                                        int64_t,
                                        double
                                     >::type;
  static_assert(std::is_same<T, int8_t>::value ||
                std::is_same<T, int16_t>::value ||
                std::is_same<T, int32_t>::value ||
                std::is_same<T, int64_t>::value ||
                std::is_same<T, float>::value ||
                std::is_same<T, double>::value, "Wrong type in NumericStats");
  protected:
    double _sum;
    double _mean;
    double _stdev;
    double _skew;
    double _kurt;
    V _min;
    V _max;
    V _mode;

  public:
    using Stats::Stats;
    size_t memory_footprint() const noexcept override;

    double  sum        (bool* isvalid) override;
    double  mean       (bool* isvalid) override;
    double  stdev      (bool* isvalid) override;
    double  skew       (bool* isvalid) override;
    double  kurt       (bool* isvalid) override;
    int64_t min_int    (bool* isvalid) override;
    double  min_double (bool* isvalid) override;
    int64_t max_int    (bool* isvalid) override;
    double  max_double (bool* isvalid) override;
    int64_t mode_int   (bool* isvalid) override;
    double  mode_double(bool* isvalid) override;

    void set_sum  (double value, bool isvalid) override;
    void set_mean (double value, bool isvalid) override;
    void set_stdev(double value, bool isvalid) override;
    void set_skew (double value, bool isvalid) override;
    void set_kurt (double value, bool isvalid) override;
    void set_min  (int64_t value, bool isvalid) override;
    void set_min  (double value, bool isvalid) override;
    void set_max  (int64_t value, bool isvalid) override;
    void set_max  (double value, bool isvalid) override;
    void set_mode (int64_t value, bool isvalid) override;
    void set_mode (double value, bool isvalid) override;

  protected:
    void compute_nacount() override;
    void compute_minmax() override;
    void compute_moments12() override;
    void compute_moments34() override;
    void compute_nunique() override;
    void compute_sorted_stats() override;
};




//------------------------------------------------------------------------------
// RealStats class
//------------------------------------------------------------------------------

/**
 * Child of NumericalStats that represents real-valued columns.
 */
template <typename T>
class RealStats : public NumericStats<T> {
  public:
    using NumericStats<T>::NumericStats;
    std::unique_ptr<Stats> clone() const override;
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
class IntegerStats : public NumericStats<T> {
  public:
    using NumericStats<T>::NumericStats;
    std::unique_ptr<Stats> clone() const override;
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
class BooleanStats : public NumericStats<int64_t> {
  public:
    using NumericStats<int64_t>::NumericStats;

  protected:
    std::unique_ptr<Stats> clone() const override;

    void compute_nacount() override;
    void compute_nunique() override;
    void compute_minmax() override;
    void compute_moments12() override;
    void compute_moments34() override;
    void compute_sorted_stats() override;
    void compute_all_stats();
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
    size_t memory_footprint() const noexcept override;
    std::unique_ptr<Stats> clone() const override;

    CString mode_string(bool* isvalid) override;
    void set_mode(CString value, bool isvalid) override;

  protected:
    void compute_nacount() override;
    void compute_nunique() override;
    void compute_sorted_stats() override;
};




//------------------------------------------------------------------------------
// PyObjectStats class
//------------------------------------------------------------------------------

class PyObjectStats : public Stats {
  public:
    using Stats::Stats;
    size_t memory_footprint() const noexcept override;
    std::unique_ptr<Stats> clone() const override;

  protected:
    void compute_nacount() override;
};




#endif /* dt_STATS_h */
