#include "ft_ping.h"

void stats_update(t_ping_stats *stats, double rtt_ms)
{
    if (rtt_ms < stats->rtt_min)
        stats->rtt_min = rtt_ms;
    if (rtt_ms > stats->rtt_max)
        stats->rtt_max = rtt_ms;
    stats->rtt_sum += rtt_ms;
    stats->rtt_sum_sq += rtt_ms * rtt_ms;
}

/* Format matches the reference ping statistics block. */
void print_stats(t_ping *ping)
{
    t_ping_stats *s = &ping->stats;

    fflush(stdout);
    printf("--- %s ping statistics ---\n", ping->hostname);
    printf("%u packets transmitted, ", s->ntransmitted);
    printf("%u packets received, ", s->nreceived);
    if (s->nrepeats)
        printf("+%u duplicates, ", s->nrepeats);
    if (s->ntransmitted)
    {
        if (s->nreceived > s->ntransmitted)
            printf("-- somebody is printing forged packets!");
        else
            printf("%u%% packet loss",
                   (s->ntransmitted - s->nreceived) * 100 / s->ntransmitted);
    }
    printf("\n");

    if (s->nreceived)
    {
        double total = s->nreceived + s->nrepeats;
        double avg = s->rtt_sum / total;
        double variance = s->rtt_sum_sq / total - avg * avg;

        printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
               s->rtt_min, avg, s->rtt_max,
               variance > 0 ? sqrt(variance) : 0.0);
    }
}
