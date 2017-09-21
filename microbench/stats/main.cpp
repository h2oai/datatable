#include "stats.h"
#include "utils.h"
#include <iostream>
#include <time.h>
#include <assert.h>
#include <stdlib.h>

class StatsWrap {
public:
    virtual void print_stats() {}
    virtual void compute_stats(const Column*) {}
    virtual ~StatsWrap() {}
};

class StatsMacroWrap : public StatsWrap {
public:
    StatsMacroWrap() {
        stats = make_macro_stats();
    }
    ~StatsMacroWrap() { free_macro_stats(stats); }
    void compute_stats(const Column* col) override {
        compute_macro_stats(stats, col);
    }
    void print_stats() override {
        print_macro_stats(stats);
    }
private:
    StatsMacro* stats;
};

class StatsClassWrap : public StatsWrap {
public:
    StatsClassWrap() {
        stats = make_class_stats();
    }
    ~StatsClassWrap() { delete stats; }
    void compute_stats(const Column* col) override {
        stats->compute_class_stats(col);
    }
    void print_stats() override {
        stats->print_stats();
    }
private:
    StatsClass* stats;
};

int test(const char *algoname, StatsWrap* stats, int S, int N, int K, int B, int R) {
    printf("STARTING %s...\n", algoname);
    assert(K <= (S << 3));
    size_t arr_size = (size_t) N * S;
    Column *col = (Column*) malloc(sizeof(Column));
    col->data = malloc(arr_size);
    if (!col) return 1;
    col->nrows = N;
    double* times = (double*) malloc(B * sizeof(double));
    double tsum = 0;
    printf("STARTING LOOP...\n");
    for (int i = 0; i < B; ++i) {
        srand(R);
        R = rand();
        if (S == 1) {
            uint8_t mask = (uint8_t)((1 << K) - 1);
            uint8_t *xx = (uint8_t*) col->data;
            for (int ii = 0; ii < N; ++ii) {
                xx[ii] = (uint8_t)(rand() & mask);
            }
        } else assert(0);
        printf("ARRAY MADE\n");
        double t0 = now();
        stats->compute_stats(col);
        times[i] = now() - t0;
        tsum += times[i];
        stats->print_stats();
    }
    double tmean = tsum / B;
    printf("@%s:  %.3f ns\n", algoname, tmean * 1e9);
    free(col->data);
    free(col);
    delete stats;
    return 0;
}

int main(int argc, char**argv) {
    /**
     * A - Type of algorithm: 1 (macro vs class))
     * B - Number of columns to try (default 100)
     * K - Range specifier. Column values will be the range of [0, 1<<K)
     * N - Array size
     * R - Random seed
     */
    int A = getCmdArgInt(argc, argv, "algo", 1);
    int B = getCmdArgInt(argc, argv, "batches", 5);
    int N = getCmdArgInt(argc, argv, "n", 10000);
    int K = getCmdArgInt(argc, argv, "k", 1);
    int R = getCmdArgInt(argc, argv, "r", (int) now());
    printf("Array size  = %d\n", N);
    printf("N sig bits  = %d\n", K);
    printf("N batches   = %d\n", B);
    printf("Random seed = %d\n", R);
    printf("\n");

    switch(A) {
    case 1:
        printf("CASE 1\n");
        test("class (bool)", new StatsClassWrap(), 1, N, K < 2 ? K : 1, B, R);
        test("macro (bool)", new StatsMacroWrap(), 1, N, K < 2 ? K : 1, B, R);
        break;
    default:
        printf("A = %d is not supported\n", A);
    };

    return 0;
}
