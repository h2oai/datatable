#ifndef MICROBENCH_UTILS_H
#define MICROBENCH_UTILS_H
#ifdef CLOCK_REALTIME
  #include <time.h>
#else
  #include <sys/time.h>
#endif
#include <stdio.h>
#include <string.h>


int getCmdLineArg(int argc, char **argv, const char *name, char **ret)
{
    for (int i = 1; i < argc; i++) {
        char *ptr = argv[i];
        while (*ptr == '-') ptr++;
        int length = (int) strlen(name);
        int match = 1;
        for (int j = 0; j < length; j++) {
            if (ptr[j] != name[j]) {
                match = 0;
                break;
            }
        }
        if (match && ptr[length] == '=') {
            *ret = ptr + length + 1;
            return 1;
        }
    }
    *ret = NULL;
    return 0;
}



int getCmdArgInt(int argc, char **argv, const char *name, int deflt)
{
    char *arg = NULL;
    if (getCmdLineArg(argc, argv, name, &arg)) {
        return atoi(arg);
    } else {
        return deflt;
    }
}



double now(void) {
    #ifdef CLOCK_REALTIME
        struct timespec time;
        if (clock_gettime(CLOCK_REALTIME, &time) == 0)
            return (double) time.tv_sec + 1e-9 * (double) time.tv_nsec;
    #else
        struct timeval tv;
        if (gettimeofday(&tv, NULL) == 0)
            return (double) tv.tv_sec + 1e-6 * (double) tv.tv_usec;
    #endif
    return 0;
}

static double timer;
void start_timer(void) {
    timer = now();
}
void stop_timer(void) {
    double delta = now() - timer;
    printf("Total time = %g ms\n", delta * 1000);
}
void stop_timeri(int iters) {
    double delta = now() - timer;
    printf("Time per iteration = %g ns\n", delta * 1e9 / iters);
}
double get_timer_iter(int iters) {
    double delta = now() - timer;
    return delta * 1e9 / iters;
}


#endif
