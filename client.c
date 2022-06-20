#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    const int offset = 1000;
    char buf[100000];
    // char write_buf[] = "testing writing";

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        long long t = write(fd, buf, 0);  // fib sequence
        printf("Writing to " FIB_DEV
               " at offset %d, returned the time %lld (ns)\n",
               i, t);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        long long sz = read(fd, buf, sizeof(buf) / sizeof(char));
        if (!sz)
            break;
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    close(fd);
    return 0;
}
