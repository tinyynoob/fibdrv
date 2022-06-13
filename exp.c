#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

#define TEST_NUM 200

int main(int argc, char *argv[])
{
    const int offset = 100;
    const int select = atoi(argv[1]);

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }
    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        unsigned long long ucons = 0, kcons = 0;
        for (int j = 0; j < TEST_NUM; j++) {
            struct timespec t1, t2;
            clock_gettime(CLOCK_MONOTONIC, &t1);
            kcons += write(fd, NULL, select);
            clock_gettime(CLOCK_MONOTONIC, &t2);
            ucons += (unsigned long long) (t2.tv_sec * 1e9 + t2.tv_nsec) -
                     (t1.tv_sec * 1e9 + t1.tv_nsec);
        }
        printf("%d,%llu,%llu,%llu\n", i, kcons / TEST_NUM, ucons / TEST_NUM,
               (ucons - kcons) / TEST_NUM);
    }
    close(fd);
    return 0;
}
