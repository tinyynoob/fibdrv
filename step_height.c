#include <stdint.h>
#include <stdio.h>
#include "bitwise/log.h"

#define N 500
#define ITERATION log(N)

int main()
{
    FILE *f = fopen("fast.csv", "r");
    uint64_t data[N + 1], tmp;
    for (int i = 0; i <= N; i++)
        fscanf(f, "%lu,%lu", &tmp, &data[i]);
    fclose(f);
    uint64_t aver[ITERATION];
    for (int group = 1; group <= ITERATION; group++) {
        uint64_t sum = 0;
        int n = 0;
        for (int i = 1 << (group - 1); i < 1 << group && i <= N; i++) {
            n++;
            sum += data[i];
        }
        printf("%llu,", aver[group - 1] = sum / n);
    }
    putchar('\n');
    printf("%lu\n", (aver[ITERATION - 1] - aver[0]) / (ITERATION - 1));
    return 0;
}