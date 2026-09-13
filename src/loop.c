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
    /* No SA_RESTART: Ctrl+C must wake a blocked recvfrom() with EINTR. */
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) < 0)
        RETURN_ERR(-1, "ft_ping: sigaction: %s\n", strerror(errno));
    return 0;
}

static long long elapsed_us(const struct timespec *from, const struct timespec *to)
{
    return (to->tv_sec - from->tv_sec) * 1000000LL
         + (to->tv_nsec - from->tv_nsec) / 1000;
}

/* -w: whole elapsed seconds are compared against the timeout. */
static bool timeout_reached(t_ping *ping, const struct timespec *start)
{
    struct timespec now;

    if (ping->timeout <= 0)
        return false;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return elapsed_us(start, &now) / 1000000 >= ping->timeout;
}

/*
 * Wait for a packet for at most wait_us. SO_RCVTIMEO turns the blocking
 * recvfrom() into a timed wait, so no I/O multiplexing call is needed.
 * Returns 1 if a packet was read, 0 on timeout or signal, -1 on error.
 */
static int wait_packet(t_ping *ping, long long wait_us, char *buf,
                       ssize_t *len, struct sockaddr_in *from)
{
    struct timeval tv;
    socklen_t fromlen = sizeof(*from);

    if (wait_us < 1)
        wait_us = 1;
    tv.tv_sec = wait_us / 1000000;
    tv.tv_usec = wait_us % 1000000;
    setsockopt(ping->sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    *len = recvfrom(ping->sockfd, buf, RECV_BUF_SIZE, 0,
                    (struct sockaddr *)from, &fromlen);
    if (*len >= 0)
        return 1;
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        return 0;
    RETURN_ERR(-1, "ft_ping: recvfrom: %s\n", strerror(errno));
}

/*
 * Timing rules (checked against the reference ping):
 * - send one packet per interval (1 s)
 * - after the last packet of -c, wait up to -W seconds (linger) for replies
 * - stop early on Ctrl+C, -w timeout, or when -c responses are in
 */
int event_loop(t_ping *ping)
{
    char buf[RECV_BUF_SIZE];
    struct sockaddr_in from;
    struct timespec start;
    struct timespec last;
    struct timespec now;
    long long interval_us = 1000000;
    bool finishing = false;
    unsigned int nresp = 0;
    ssize_t len;
    int ret;

    clock_gettime(CLOCK_MONOTONIC, &start);
    last = start;
    if (send_echo(ping) < 0)
        return -1;

    while (!g_stop)
    {
        clock_gettime(CLOCK_MONOTONIC, &now);
        long long remaining = interval_us - elapsed_us(&last, &now);

        if (remaining > 0)
        {
            ret = wait_packet(ping, remaining, buf, &len, &from);
            if (ret < 0)
                return -1;
            if (ret == 0)
                continue;
            if (process_packet(ping, buf, len, &from) == 0)
                nresp++;
            if (timeout_reached(ping, &start))
                break;
            if (ping->count && nresp >= ping->count)
                break;
            continue;
        }

        if (!ping->count || ping->stats.ntransmitted < ping->count)
        {
            if (send_echo(ping) < 0)
                return -1;
            if (timeout_reached(ping, &start))
                break;
        }
        else if (finishing)
            break;
        else
        {
            finishing = true;
            interval_us = ping->linger * 1000000LL;
        }
        clock_gettime(CLOCK_MONOTONIC, &last);
    }
    return 0;
}
