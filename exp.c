#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>  // sprintf
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "bitwise/sqrt.h"

#define FIB_DEV "/dev/fibonacci"

#define TEST_NUM 1000

/* Eliminate 5% outliers and compute the average.
 */
uint64_t average(int64_t *nums, int numsSize)
{
    int64_t mu = 0;
    for (int i = 0; i < numsSize; i++)
        mu += nums[i];
    mu /= numsSize;
    int64_t deviation = 0;
    for (int i = 0; i < numsSize; i++)
        deviation += (nums[i] - mu) * (nums[i] - mu) / numsSize;
    deviation = sqrt(deviation);
    for (int i = 0; i < numsSize; i++)
        if (nums[i] >= mu + 2 * deviation || nums[i] <= mu - 2 * deviation)
            nums[i] = -1;
    // printf("%ld, %ld\t", mu, deviation);
    uint64_t ans = 0;
    for (int i = 0; i < numsSize; i++)
        if (nums[i] != -1)
            ans += nums[i];
    return ans / (numsSize / 20 * 19);
}

int main(int argc, char *argv[])
{
    const int offset = 500;
    const int select = atoi(argv[1]);

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }
    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        unsigned long long ucons = 0, kcons = 0;
        int64_t udata[TEST_NUM], kdata[TEST_NUM];
        char name[128];
        snprintf(name, 128, "data/%d.dat", i);
        FILE *f = fopen(name, "w");
        for (int j = 0; j < TEST_NUM; j++) {
            struct timespec t1, t2;
            clock_gettime(CLOCK_MONOTONIC, &t1);
            kdata[j] = write(fd, NULL, select);
            clock_gettime(CLOCK_MONOTONIC, &t2);
            udata[j] =
                (t2.tv_sec * 1e9 + t2.tv_nsec) - (t1.tv_sec * 1e9 + t1.tv_nsec);
            fprintf(f, "%ld\n", kdata[j]);
        }
        fclose(f);
        unsigned long long kt = average(kdata, TEST_NUM);
        unsigned long long ut = average(udata, TEST_NUM);
        printf("%d,%llu,%llu,%llu\n", i, kt, ut, ut - kt);
    }
    close(fd);
    return 0;
}
