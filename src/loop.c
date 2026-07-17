#include "ft_ping.h"

volatile sig_atomic_t g_stop = 0;

static void handle_signal(int signum)
{
    (void)signum;
    g_stop = 1;
}

int setup_signal_handler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) < 0)
    {
        fprintf(stderr, "ft_ping: sigaction: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

static void drain_socket(t_ping *ping)
{
    char buf[RECV_BUF_SIZE];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    while (1)
    {
        ssize_t n = recvfrom(ping->sockfd, buf, sizeof(buf),
                             MSG_DONTWAIT,
                             (struct sockaddr *)&from, &fromlen);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            if (errno == EINTR)
                continue;
            fprintf(stderr, "ft_ping: recvfrom: %s\n", strerror(errno));
            break;
        }
        process_packet(ping, buf, n, &from);
    }
}

static int timespec_ge(const struct timespec *a, const struct timespec *b)
{
    return a->tv_sec > b->tv_sec
        || (a->tv_sec == b->tv_sec && a->tv_nsec >= b->tv_nsec);
}

static double timespec_diff_ms(const struct timespec *a, const struct timespec *b)
{
    return (a->tv_sec - b->tv_sec) * 1000.0
         + (a->tv_nsec - b->tv_nsec) / 1000000.0;
}

static void timespec_add_sec(struct timespec *ts, time_t sec)
{
    ts->tv_sec += sec;
}

void event_loop(t_ping *ping)
{
    struct pollfd pfd;
    struct timespec next_send;
    bool first = true;

    pfd.fd = ping->sockfd;
    pfd.events = POLLIN;

    while (!g_stop)
    {
        if (first)
        {
            clock_gettime(CLOCK_MONOTONIC, &next_send);
            timespec_add_sec(&next_send, 1);
            send_echo(ping);
            ping->sequence++;
            first = false;
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        if (timespec_ge(&now, &next_send))
        {
            clock_gettime(CLOCK_MONOTONIC, &next_send);
            timespec_add_sec(&next_send, 1);
            send_echo(ping);
            ping->sequence++;
            continue;
        }

        double timeout_ms = timespec_diff_ms(&next_send, &now);
        if (timeout_ms < 0)
            timeout_ms = 0;

        int ret = poll(&pfd, 1, (int)(timeout_ms + 1));
        if (ret < 0)
        {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "ft_ping: poll: %s\n", strerror(errno));
            break;
        }

        if (ret > 0 && (pfd.revents & POLLIN))
            drain_socket(ping);
    }

    drain_socket(ping);
}
