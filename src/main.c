#include "ft_ping.h"

void print_help(void)
{
    printf("Usage: ft_ping [-v] [-?] <hostname or IP>\n");
}

static void print_startup(t_ping *ping)
{
    printf("PING %s (%s): %d data bytes\n",
           ping->hostname, ping->ip_str, DATA_SIZE);
}

void stats_init(t_ping_stats *stats)
{
    memset(stats, 0, sizeof(*stats));
    stats->rtt_min = INFINITY;
}

void stats_update(t_ping_stats *stats, double rtt_ms)
{
    if (stats->rtt_count == 0)
    {
        stats->rtt_min = rtt_ms;
        stats->rtt_max = rtt_ms;
    }
    else
    {
        if (rtt_ms < stats->rtt_min)
            stats->rtt_min = rtt_ms;
        if (rtt_ms > stats->rtt_max)
            stats->rtt_max = rtt_ms;
    }
    stats->rtt_sum += rtt_ms;
    stats->rtt_sum_sq += rtt_ms * rtt_ms;
    stats->rtt_count++;
}

void print_stats(t_ping *ping)
{
    unsigned int sent = ping->stats.ntransmitted;
    unsigned int received = ping->stats.nreceived;
    unsigned int loss;

    if (sent == 0)
        loss = 0;
    else
        loss = ((sent - received) * 100) / sent;

    printf("--- %s ping statistics ---\n", ping->hostname);
    printf("%u packets transmitted, %u received, %u%% packet loss",
           sent, received, loss);
    if (ping->stats.nerrors > 0 && ping->verbose)
        printf(", +%u errors", ping->stats.nerrors);
    printf("\n");

    if (ping->stats.rtt_count > 0)
    {
        double avg = ping->stats.rtt_sum / ping->stats.rtt_count;
        double variance = (ping->stats.rtt_sum_sq / ping->stats.rtt_count) - (avg * avg);
        if (variance < 0)
            variance = 0;
        double mdev = sqrt(variance);

        printf("round-trip min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n",
               ping->stats.rtt_min, avg,
               ping->stats.rtt_max, mdev);
    }
}

int main(int argc, char **argv)
{
    t_ping ping;
    int opt;

    if (argc < 2)
        RETURN_ERR(1, "error: no arguments. Use --help for usage.\n");
    memset(&ping, 0, sizeof(ping));
    ping.verbose = false;
    ping.pid = getpid() & 0xFFFF;
    ping.sequence = 0;
    stats_init(&ping.stats);

    while ((opt = getopt(argc, argv, "v?")) != -1)
    {
        switch (opt)
        {
        case 'v':
            ping.verbose = true;
            break;
        case '?':
            if (optopt == 0)
            {
                print_help();
                return 0;
            }
            return 2;
        default:
            return 2;
        }
    }

    if (optind >= argc)
    {
        fprintf(stderr, "ft_ping: missing host operand\n");
        fprintf(stderr, "Usage: ft_ping [-v] [-?] <hostname or IP>\n");
        return 2;
    }

    if (optind != argc - 1)
    {
        fprintf(stderr, "ft_ping: too many arguments\n");
        return 2;
    }

    ping.hostname = argv[optind];

    if (resolve_destination(&ping) != 0)
        return 2;

    ping.sockfd = create_raw_socket();
    if (ping.sockfd < 0)
        return 2;

    if (setup_signal_handler() != 0)
    {
        close(ping.sockfd);
        return 2;
    }

    print_startup(&ping);
    event_loop(&ping);
    print_stats(&ping);

    close(ping.sockfd);
    return 0;
}
