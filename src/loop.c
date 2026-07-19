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
        RETURN_ERR(-1, "ft_ping: sigaction: %s\n", strerror(errno));
    if (sigaction(SIGQUIT, &sa, NULL) < 0)
        RETURN_ERR(-1, "ft_ping: sigaction: %s\n", strerror(errno));
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

static void timespec_add_sec(struct timespec *ts, time_t sec)
{
    ts->tv_sec += sec;
}

void event_loop(t_ping *ping)
{
    struct timespec start;
    struct timespec deadline_ts;
    char buf[RECV_BUF_SIZE];
    struct sockaddr_in from;
    socklen_t fromlen;
    int deadline_hit = 0;

    clock_gettime(CLOCK_MONOTONIC, &start);
    if (ping->deadline > 0)
    {
        deadline_ts = start;
        timespec_add_sec(&deadline_ts, ping->deadline);
    }
    send_echo(ping);
    ping->sequence++;

    while (!g_stop)
    {
        if (ping->count > 0 && ping->stats.ntransmitted >= ping->count)
            break;

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        {
            struct timespec next_expected = start;
            timespec_add_sec(&next_expected, ping->stats.ntransmitted);
            if (timespec_ge(&now, &next_expected))
            {
                send_echo(ping);
                ping->sequence++;
                continue;
            }
        }

        if (ping->deadline > 0 && timespec_ge(&now, &deadline_ts))
        {
            deadline_hit = 1;
            break;
        }

        fromlen = sizeof(from);
        ssize_t n = recvfrom(ping->sockfd, buf, sizeof(buf), 0,
                             (struct sockaddr *)&from, &fromlen);
        if (n >= 0)
        {
            process_packet(ping, buf, n, &from);
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            continue;
        }
        else if (errno == EINTR)
        {
            continue;
        }
        else
        {
            fprintf(stderr, "ft_ping: recvfrom: %s\n", strerror(errno));
            break;
        }
    }

    if (!deadline_hit)
        drain_socket(ping);
}
