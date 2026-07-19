#include "ft_ping.h"

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

    printf("--- %s ping statistics ---\n",
           ping->numeric ? ping->ip_str : ping->hostname);
    printf("%u packets transmitted, %u packets received, %u%% packet loss",
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
