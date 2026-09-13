#ifndef FT_PING_H
# define FT_PING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#include <getopt.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <arpa/inet.h>

#define DATA_SIZE          56
#define ICMP_HDR_SIZE      sizeof(struct icmphdr)
#define TIMESTAMP_SIZE      sizeof(struct timespec)
#define PACKET_SIZE         (ICMP_HDR_SIZE + DATA_SIZE)
#define RECV_BUF_SIZE       4096
#define DEFAULT_LINGER      10
#define EXIT_USAGE          64
#define RETURN_ERR(val, ...) return (fprintf(stderr, __VA_ARGS__), val)

typedef struct s_ping_stats {
    unsigned int    ntransmitted;
    unsigned int    nreceived;
    unsigned int    nrepeats;
    double          rtt_min;
    double          rtt_max;
    double          rtt_sum;
    double          rtt_sum_sq;
} t_ping_stats;

typedef struct s_ping {
    int             sockfd;
    bool            verbose;
    bool            numeric;
    bool            dontroute;
    int             linger;
    int             timeout;
    int             ttl;
    unsigned int    count;
    char            *target;
    char            hostname[NI_MAXHOST];
    char            ip_str[INET_ADDRSTRLEN];
    struct sockaddr_in dest_addr;
    uint16_t        pid;
    uint8_t         seq_table[65536 / 8];
    t_ping_stats    stats;
} t_ping;

extern volatile sig_atomic_t g_stop;

uint16_t    icmp_checksum(const void *data, size_t len);
int         resolve_destination(t_ping *ping);
int         create_raw_socket(void);
int         setup_signal_handler(void);
int         send_echo(t_ping *ping);
int         process_packet(t_ping *ping, const char *buf, ssize_t len,
                           const struct sockaddr_in *from);
int         parse_args(t_ping *ping, int argc, char **argv);
void        print_stats(t_ping *ping);
double      compute_rtt_ms(const struct timespec *send_ts,
                           const struct timespec *now);
void        stats_update(t_ping_stats *stats, double rtt_ms);
int         event_loop(t_ping *ping);

#endif
