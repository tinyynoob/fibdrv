#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

#define TEST_NUM 1000

int main()
{
    char write_buf[] = "testing writing";
    int offset = 1000;

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }
    for (int i = 0; i < offset; i++) {
        lseek(fd, i, SEEK_SET);
        unsigned long long total = 0;
        for (int j = 0; j < TEST_NUM; j++)
            total += write(fd, write_buf, strlen(write_buf));
        printf("%d,%llu\n", i, total / TEST_NUM);
    }
    close(fd);
    return 0;
}
