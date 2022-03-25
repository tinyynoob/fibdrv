#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    const int offset = 100;
    char buf[10000];
    char write_buf[] = "testing writing";

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        long long t = write(fd, write_buf, 0);
        printf("Writing to " FIB_DEV ", returned the time %lld (ns)\n", t);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        long long sz = read(fd, buf, sizeof(buf));
        if (!sz)
            continue;
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    // for (int i = offset; i >= 0; i--) {
    //     lseek(fd, i, SEEK_SET);
    //     long long sz = read(fd, buf, sizeof(buf));
    //     if (!sz)
    //         continue;
    //     printf("Reading from " FIB_DEV
    //            " at offset %d, returned the sequence "
    //            "%s.\n",
    //            i, buf);
    // }

    close(fd);
    return 0;
}
