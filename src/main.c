#include "ft_ping.h"

static void set_socket_options(t_ping *ping)
{
    int one = 1;

    setsockopt(ping->sockfd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
    if (ping->dontroute)
        setsockopt(ping->sockfd, SOL_SOCKET, SO_DONTROUTE, &one, sizeof(one));
    if (ping->ttl > 0
        && setsockopt(ping->sockfd, IPPROTO_IP, IP_TTL,
                      &ping->ttl, sizeof(ping->ttl)) < 0)
        fprintf(stderr, "ft_ping: setsockopt(IP_TTL): %s\n", strerror(errno));
}

int main(int argc, char **argv)
{
    t_ping ping;
    int status;

    memset(&ping, 0, sizeof(ping));
    ping.sockfd = -1;
    ping.linger = DEFAULT_LINGER;
    ping.pid = getpid() & 0xFFFF;
    ping.stats.rtt_min = INFINITY;
    /* Line buffered even when piped, so stdout and stderr stay in order. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    status = parse_args(&ping, argc, argv);
    if (status >= 0)
        return status;

    status = EXIT_FAILURE;
    /* Socket first: the privilege check comes before DNS. */
    ping.sockfd = create_raw_socket();
    if (ping.sockfd < 0)
        goto cleanup;
    set_socket_options(&ping);

    if (resolve_destination(&ping) != 0)
        goto cleanup;
    if (setup_signal_handler() != 0)
        goto cleanup;

    printf("PING %s (%s): %d data bytes", ping.hostname, ping.ip_str, DATA_SIZE);
    if (ping.verbose)
        printf(", id 0x%04x = %u", ping.pid, ping.pid);
    printf("\n");

    if (event_loop(&ping) != 0)
        goto cleanup;
    print_stats(&ping);
    status = ping.stats.nreceived == 0 ? EXIT_FAILURE : EXIT_SUCCESS;

cleanup:
    if (ping.sockfd >= 0)
        close(ping.sockfd);
    return status;
}
