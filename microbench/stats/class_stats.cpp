#include "stats.h"
#include <math.h>
#include <stdio.h>
#include <string>


void StatsClass::compute_class_stats(const Column* col) {
    data = col->data;
    loop_over_ridx(col);
    data = NULL;
}

StatsClass::StatsClass() {
    count_na = -1;
    nrows = -1;
    data = NULL;
}

StatsClass::~StatsClass() {
}

void StatsClass::print_stats() {}
inline void StatsClass::loop_prologue() {
    count_na = 0;
}
inline void StatsClass::loop_body(const int64_t& i) {}
inline void StatsClass::loop_epilogue() {}

void StatsClass::loop_over_ridx(const Column* col) {
    loop_prologue();
    nrows = col->nrows;
    for (int64_t i = 0; i < nrows; ++i) {
        loop_body(i);
    }
    loop_epilogue();
}

template<typename T>
class NumStatsClass : public StatsClass {
public:
    virtual ~NumStatsClass() {}
    void print_stats() override {
        printf("nrows:    %lld\n"
               "min:      %lld\n"
               "max:      %lld\n"
               "sum:      %lld\n"
               "mean:     %f\n"
               "sd:       %f\n"
               "na count: %lld\n",
               nrows, min, max, sum, mean, sd, count_na);
    }
protected:
    NumStatsClass() : StatsClass() {
        sum = -1;
        min = max = 0;
        mean = 0;
        sd = -1;
    }
    virtual inline void loop_prologue() override {
        StatsClass::loop_prologue();
        sum = 0;
    }
    virtual inline void loop_epilogue() override {}
    T sum;
    T min;
    T max;
    double mean;
    double sd;
};

class BooleanStatsClass : public NumStatsClass<int64_t> {
public:
    BooleanStatsClass() : NumStatsClass<int64_t>() {
        min = -127;
        max = -127;
        sum = -1;
        count_na = -1;
    }
protected:
    inline void loop_prologue() override {
        NumStatsClass<int64_t>::loop_prologue();
    }

    inline void loop_body(const int64_t& i) override {
        switch (((int8_t*) data)[i]) {
            case 0 :
                break;
            case 1 :
                ++sum;
                break;
            default :
                ++count_na;
                break;
        };
    }

    inline void loop_epilogue() override {
        int64_t count = nrows - count_na;
        mean = count > 0 ? ((double) sum) / count : -127;
        sd = count > 1 ? sqrt((double)(count - sum)/count * sum/(count - 1)) :
                    count == 1 ? 0 : -127;
        min = -127;
        if (count - sum > 0) min = 0;
        else if (sum > 0) min = 1;
        max = this->min != -127 ? sum > 0 : -127;
    }
};

StatsClass* make_class_stats() {
    return new BooleanStatsClass();
}
