#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

int
main()
{
    struct timeval tv;
    for (int i = 0; i < 100; i++) {
        gettimeofday(&tv, NULL);
        printf("%.2ld.%d\n", tv.tv_sec, tv.tv_usec);
    }
}
