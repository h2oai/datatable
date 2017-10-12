#ifndef STATS_H
#define STATS_H

#include <stddef.h>
#include <stdint.h>
#include "types.h"
#include "datatable.h"
#include "rowindex.h"

class DataTable;
class Column;
class RowIndex;

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
public:
    static Stats* construct(Column*);
    static void destruct(Stats*);

    virtual void reset();
    size_t alloc_size() const;
    void merge_stats(const Stats*); // TODO: virtual when implemented
    static Stats* void_ptr();
    bool is_void() const;

    //===================== Get Stat Value ======================
    template <typename T> T min();
    template <typename T> T max();
    template <typename A> A sum();
    virtual double          mean();
    virtual double          sd();
    virtual int64_t         countna();
    //===========================================================

    //================ Get Stats for a DataTable ================
    static DataTable* mean_datatable   ( const DataTable*);
    static DataTable* sd_datatable     ( const DataTable*);
    static DataTable* countna_datatable( const DataTable*);
    static DataTable* min_datatable    ( const DataTable*);
    static DataTable* max_datatable    ( const DataTable*);
    static DataTable* sum_datatable    ( const DataTable*);
    //===========================================================
protected:
    double _mean;
    double _sd;
    int64_t _countna;
    const Column* _ref_col;

    Stats(Column*);
    virtual ~Stats();

    //================== Get Stat as a Column ===================
    Column*         mean_column();
    Column*         sd_column();
    Column*         countna_column();
    virtual Column* min_column();
    virtual Column* max_column();
    virtual Column* sum_column();
    //===========================================================

    // Helper functions to deal with the template stats
    virtual void* min_raw();
    virtual void* max_raw();
    virtual void* sum_raw();

    friend DataTable;
};

void init_stats(void);

#endif /* STATS_H */

