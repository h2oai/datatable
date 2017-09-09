/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   stats.h
 * Author: nkalonia1
 *
 * Created on August 31, 2017, 2:57 PM
 */

#ifndef STATS_H
#define STATS_H

#include "types.h"
#include "datatable.h"


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
typedef struct Stats {
    int64_t countna;
    union {
        struct {
            int64_t sum;
            int8_t min;
            int8_t max;
            int8_t _padding[6];
        } b; // LT_BOOLEAN
        struct {
            int64_t min;
            int64_t max;
            int64_t sum;
            double mean;
            double sd;
        } i; // LT_INTEGER
        struct {
            double min;
            double max;
            double sum;
            double mean;
            double sd;
        } r; // LT_REAL
    };
    uint64_t isdefined; // Bit mask for determining if a CStat has been computed
    SType stype; // LType is not enough information for create NA values
    int8_t _padding[7];
} Stats;

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

#define DT_CSTATS_COUNT (C_COUNT_NA + 1)

Stats* make_data_stats(const SType);
void stats_dealloc(Stats *self);
Stats* stats_copy(Stats *self);
uint64_t cstat_isdefined(const Stats* self, const CStat);
size_t stats_get_allocsize(const Stats* self);
void compute_datatable_cstat(DataTable*, const CStat);
DataTable* make_cstat_datatable(const DataTable*, const CStat);
void init_stats(void);

#endif /* STATS_H */

