#ifndef MICROBENCH_STATS_STATS_H_
#define MICROBENCH_STATS_STATS_H_

#include <inttypes.h>

typedef struct Column {
    void *data;
    int64_t nrows;
} Column;

typedef struct StatsMacro {
    int64_t count_na;
    int64_t sum;
    int64_t min;
    int64_t max;
    double mean;
    double sd;
} StatsMacro;

class StatsClass {
public:
    virtual ~StatsClass();
    void compute_class_stats(const Column*);
    virtual void print_stats();
protected:
    StatsClass();
    virtual void loop_over_ridx(const Column*);
    virtual inline void loop_prologue();
    virtual inline void loop_body(const int64_t&);
    virtual inline void loop_epilogue();
    int64_t count_na;
    int64_t nrows;
    void* data;
};

StatsClass* make_class_stats();
StatsMacro* make_macro_stats();
void free_macro_stats(StatsMacro*);

void compute_macro_stats(StatsMacro*, const Column*);
void print_macro_stats(StatsMacro*);


#endif /* MICROBENCH_STATS_STATS_H_ */
