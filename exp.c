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
    const int offset = 100;
    const int select = atoi(argv[1]);
    char buf[100000];

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open " FIB_DEV ".");
        exit(1);
    }
    for (int i = 0; i <= offset; i++) {
        int64_t data[2][TEST_NUM];
        char name[128];
        switch (select) {
        case 0:
            snprintf(name, 128, "data/seq%d.dat", i);
            break;
        case 1:
            snprintf(name, 128, "data/fast%d.dat", i);
        }
        FILE *f = fopen(name, "w");
        for (int j = 0; j < TEST_NUM; j++) {
            lseek(fd, i, SEEK_SET);
            read(fd, buf, sizeof(buf) / sizeof(char));
            sscanf(buf, "%ld %ld", &data[0][j], &data[1][j]);
            fprintf(f, "%ld,%ld\n", data[0][j], data[1][j]);
        }
        fclose(f);
        uint64_t convert = average(data[0], TEST_NUM),
                 both = average(data[1], TEST_NUM);
        printf("%d,%lu,%lu,%lu\n", i, both - convert, convert, both);
    }
    close(fd);
    return 0;
}
