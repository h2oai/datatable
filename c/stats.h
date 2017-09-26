#ifndef STATS2_H
#define STATS2_H

#include "types.h"
#include "datatable.h"

typedef struct DataTable DataTable;
typedef struct Column Column;
typedef struct RowIndex RowIndex;

/**
 * Column Statistic
 * This enum is used to maintain an abstraction between the user and the Stats
 * structure.
 **/
typedef enum CStat {
    C_MIN = 0,
    C_MAX = 1,
    C_SUM = 2,
    C_MEAN = 3,
    C_STD_DEV = 4,
    C_COUNT_NA = 5,
} __attribute__ ((__packed__)) CStat;

/**
 * Statistics container
 * This structure is intended to be used in a DataTable. A DataTable should
 * contain a Stats array of size ncols. Each Stats structure should be uniquely
 * associated with a column inside the DataTable. It is the DataTable's
 * responsibility to maintain this association, and thus it must insure that
 * a Stats structure is properly modified/replaced in the event that its associated
 * column is modified.
 *
 * The format and typing of this structure is subject to change. Use the
 * functions and the CStat enum (see below) when getting/setting values.
 *
 * Note: The Stats structure does NOT contain a reference counter. Create a
 * copy of the structure instead of having more than one reference to it.
 **/
class Stats {
public:
    Stats(const SType);
    virtual ~Stats(); // TODO: Copy constructor?
    virtual void compute_cstat(const Column*, const RowIndex*, const CStat);
    Column* make_column(const CStat) const;
    virtual void reset();
    bool is_computed(const CStat);
    size_t alloc_size() const;
    const SType stype;
    char _padding[7];
protected:
    int64_t countna;
    uint64_t iscomputed;
    void *cstat_to_val[DT_STYPES_COUNT];
private:
    static const uint64_t ALL_COMPUTED = 0xFFFFFFFFFFFFFFFF;
};

#define DT_CSTATS_COUNT (C_COUNT_NA + 1)

Stats* construct_stats(const SType);
DataTable* make_cstat_datatable(const DataTable *self, const CStat s);
void init_stats(void);

#endif /* STATS2_H */

