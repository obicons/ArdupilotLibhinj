#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <gazebo/transport/transport.hh>
#include <gazebo/msgs/msgs.hh>
#include <gazebo/gazebo_client.hh>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

static int socket_fd;
static pthread_mutex_t inner_mutex;
static bool running = true;

static struct timeval current_time;

static char* make_time_path();

void
handle_signals(int sig)
{
    running = false;
}

void
cb(ConstWorldStatisticsPtr &_msg)
{
    pthread_mutex_lock(&inner_mutex);
    current_time.tv_sec = _msg->sim_time().sec();
    current_time.tv_usec = _msg->sim_time().nsec() / 1000;
    pthread_mutex_unlock(&inner_mutex);
}

static char*
make_time_path()
{
    char *result;
    char *home_dir = getenv("HOME");
    if (!home_dir) {
        perror("unable to find user's home directory");
        exit(1);
    }

    if (asprintf(&result, "%s/.gazebo_time", home_dir) < 0) {
        perror("unable to make path for unix socket");
        exit(1);
    }

    return result;
}

int
main()
{
    gazebo::transport::NodePtr node;
    gazebo::transport::SubscriberPtr sub;

    if (pthread_mutex_init(&inner_mutex, nullptr)) {
        perror("unable to initialize mutex");
        exit(1);
    }

    signal(SIGINT, handle_signals);
    signal(SIGTERM, handle_signals);

    // initialize the socket
    char *path = make_time_path();
    unlink(path);

    if ((socket_fd = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) {
        perror("unable to create socket");
        exit(1);
    }

    struct sockaddr_un unixaddr;
    strcpy(unixaddr.sun_path, path);
    unixaddr.sun_family = PF_LOCAL;
    if (bind(socket_fd, (const struct sockaddr*) &unixaddr, sizeof(struct sockaddr_un))) {
        perror("unable to bind socket");
        close(socket_fd);
        exit(1);
    }

    // subscribe to the gazebo time topics
    gazebo::client::setup(0, nullptr);
    node = gazebo::transport::NodePtr(new gazebo::transport::Node());
    node->Init();

    sub = node->Subscribe("~/world_stats", cb);

    // listen on the socket
    if (listen(socket_fd, 50)) {
        perror("unable to listen on socket");
        close(socket_fd);
        exit(1);
    }

    struct timeval timeout = {.tv_sec = 0, .tv_usec = 10000};
    fd_set single_fd_set;
    while (running) {
        // get ready to select 
        FD_ZERO(&single_fd_set);
        FD_SET(socket_fd, &single_fd_set);

        int ready = select(socket_fd + 1, &single_fd_set, nullptr,
                           nullptr, &timeout);
        if (ready <= 0)
            continue;

        int cfd = accept(socket_fd, nullptr, nullptr);
        if (cfd < 0) {
            perror("unable to accept on socket");
            continue;
        }

        // get the local time 
        struct timeval local_currentime;
        pthread_mutex_lock(&inner_mutex);
        memcpy(&local_currentime, &current_time, sizeof(struct timeval));
        pthread_mutex_unlock(&inner_mutex);

        // write the local time as a response
        write(cfd, &local_currentime, sizeof(struct timeval));
        close(cfd);
    }
}
