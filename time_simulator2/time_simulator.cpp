#include <dlfcn.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

typedef int (*old_gettimeofday_t) (struct timeval*, void*);
typedef unsigned int (*old_sleep_t) (unsigned int);

static char* make_time_path();

static struct timeval previous_read = {.tv_sec = 0, .tv_usec = 0};
static struct timeval previous_real_wall = {.tv_sec = 0, .tv_usec = 0};

extern "C" int
#if defined(__linux__)
gettimeofday (struct timeval *__restrict tp, __timezone_ptr_t __tz)
#elif defined(__APPLE__)
gettimeofday(struct timeval *tp, void *tzp)    
#endif
{
    int socket_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("unable to create socket");
        return -1;
    }

    char *path = make_time_path();
    if (!path) {
        perror("unable to get path");
        close(socket_fd);
        return -1;
    }

    struct sockaddr_un unix_addr;
    strcpy(unix_addr.sun_path, path);
    unix_addr.sun_family = PF_LOCAL;
    if (connect(socket_fd, (struct sockaddr*) &unix_addr, sizeof(struct sockaddr_un))) {
        perror("unable to connect");
        goto error_cleanup;
    }

    if (read(socket_fd, tp, sizeof(struct timeval)) != sizeof(struct timeval)) {
        perror("unable to read properly");
        goto error_cleanup;
    }

    close(socket_fd);
    free(path);

    return 0;

 error_cleanup:
    close(socket_fd);
    free(path);
    return -1;    
}

extern "C" int
clock_gettime(clockid_t clock_id, struct timespec *tp)
{
    struct timeval tv;
    if (gettimeofday(&tv, nullptr)) {
        return -1;
    }

    tp->tv_sec = tv.tv_sec;
    tp->tv_nsec = tv.tv_usec * 1000;
    return 0;
}

static char*
make_time_path()
{
    char *result;
    char *home_dir = getenv("HOME");
    if (!home_dir) {
        perror("unable to find user's home directory");
        return NULL;
    }

    if (asprintf(&result, "%s/.gazebo_time", home_dir) < 0) {
        perror("unable to make path for unix socket");
        return NULL;
    }

    return result;
}
