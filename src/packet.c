#include "ft_ping.h"

#define SEQ_BIT(seq)    (1U << ((seq) & 7))
#define SEQ_BYTE(p, s)  ((p)->seq_table[(s) >> 3])

int create_raw_socket(void)
{
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

    if (sockfd < 0)
    {
        if (errno == EPERM || errno == EACCES)
            RETURN_ERR(-1, "ft_ping: Lacking privilege for icmp socket.\n"
                           "ft_ping: run as root or: sudo setcap cap_net_raw+ep ft_ping\n");
        RETURN_ERR(-1, "ft_ping: socket: %s\n", strerror(errno));
    }
    return sockfd;
}

int send_echo(t_ping *ping)
{
    char packet[PACKET_SIZE];
    struct icmphdr *icmp = (struct icmphdr *)packet;
    uint16_t seq = ping->stats.ntransmitted;
    struct timespec ts;

    memset(packet, 0, sizeof(packet));
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->un.echo.id = htons(ping->pid);
    icmp->un.echo.sequence = htons(seq);

    /* Payload: send timestamp, then filler bytes 0,1,2... */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    memcpy(packet + ICMP_HDR_SIZE, &ts, TIMESTAMP_SIZE);
    for (size_t i = 0; i < DATA_SIZE - TIMESTAMP_SIZE; i++)
        packet[ICMP_HDR_SIZE + TIMESTAMP_SIZE + i] = i;

    icmp->checksum = icmp_checksum(packet, sizeof(packet));

    if (sendto(ping->sockfd, packet, sizeof(packet), 0,
               (struct sockaddr *)&ping->dest_addr,
               sizeof(ping->dest_addr)) < 0)
        RETURN_ERR(-1, "ft_ping: sending packet: %s\n", strerror(errno));

    SEQ_BYTE(ping, seq) &= ~SEQ_BIT(seq);
    ping->stats.ntransmitted++;
    return 0;
}

double compute_rtt_ms(const struct timespec *send_ts,
                      const struct timespec *now)
{
    double sec = now->tv_sec - send_ts->tv_sec;
    double nsec = now->tv_nsec - send_ts->tv_nsec;
    return sec * 1000.0 + nsec / 1000000.0;
}

static int handle_echo_reply(t_ping *ping, const struct iphdr *ip,
                             const struct icmphdr *icmp, ssize_t icmp_len,
                             const struct sockaddr_in *from)
{
    uint16_t seq = ntohs(icmp->un.echo.sequence);
    char src_ip[INET_ADDRSTRLEN];
    bool dup;

    if (ntohs(icmp->un.echo.id) != ping->pid)
        return -1;

    inet_ntop(AF_INET, &from->sin_addr, src_ip, sizeof(src_ip));
    if (icmp_checksum(icmp, icmp_len) != 0)
        fprintf(stderr, "checksum mismatch from %s\n", src_ip);

    dup = SEQ_BYTE(ping, seq) & SEQ_BIT(seq);
    if (dup)
        ping->stats.nrepeats++;
    else
    {
        SEQ_BYTE(ping, seq) |= SEQ_BIT(seq);
        ping->stats.nreceived++;
    }

    printf("%zd bytes from %s: icmp_seq=%u ttl=%d",
           icmp_len, src_ip, seq, ip->ttl);
    if (icmp_len >= (ssize_t)(ICMP_HDR_SIZE + TIMESTAMP_SIZE))
    {
        struct timespec send_ts;
        struct timespec now;

        memcpy(&send_ts, (const char *)icmp + ICMP_HDR_SIZE, TIMESTAMP_SIZE);
        clock_gettime(CLOCK_MONOTONIC, &now);
        double rtt_ms = compute_rtt_ms(&send_ts, &now);
        stats_update(&ping->stats, rtt_ms);
        printf(" time=%.3f ms", rtt_ms);
    }
    if (dup)
        printf(" (DUP!)");
    printf("\n");
    return 0;
}

#define NITEMS(a) (sizeof(a) / sizeof((a)[0]))

static void print_code_text(int type, int code, const char *prefix)
{
    static const struct {
        int         type;
        int         code;
        const char  *text;
    } code_text[] = {
        {ICMP_DEST_UNREACH,  ICMP_NET_UNREACH,    "Destination Net Unreachable"},
        {ICMP_DEST_UNREACH,  ICMP_HOST_UNREACH,   "Destination Host Unreachable"},
        {ICMP_DEST_UNREACH,  ICMP_PROT_UNREACH,   "Destination Protocol Unreachable"},
        {ICMP_DEST_UNREACH,  ICMP_PORT_UNREACH,   "Destination Port Unreachable"},
        {ICMP_DEST_UNREACH,  ICMP_FRAG_NEEDED,    "Fragmentation needed and DF set"},
        {ICMP_DEST_UNREACH,  ICMP_SR_FAILED,      "Source Route Failed"},
        {ICMP_DEST_UNREACH,  ICMP_NET_UNKNOWN,    "Network Unknown"},
        {ICMP_DEST_UNREACH,  ICMP_HOST_UNKNOWN,   "Host Unknown"},
        {ICMP_DEST_UNREACH,  ICMP_HOST_ISOLATED,  "Host Isolated"},
        {ICMP_DEST_UNREACH,  ICMP_NET_UNR_TOS,    "Destination Network Unreachable At This TOS"},
        {ICMP_DEST_UNREACH,  ICMP_HOST_UNR_TOS,   "Destination Host Unreachable At This TOS"},
        {ICMP_DEST_UNREACH,  ICMP_PKT_FILTERED,   "Packet Filtered"},
        {ICMP_DEST_UNREACH,  ICMP_PREC_VIOLATION, "Precedence Violation"},
        {ICMP_DEST_UNREACH,  ICMP_PREC_CUTOFF,    "Precedence Cutoff"},
        {ICMP_REDIRECT,      ICMP_REDIR_NET,      "Redirect Network"},
        {ICMP_REDIRECT,      ICMP_REDIR_HOST,     "Redirect Host"},
        {ICMP_REDIRECT,      ICMP_REDIR_NETTOS,   "Redirect Type of Service and Network"},
        {ICMP_REDIRECT,      ICMP_REDIR_HOSTTOS,  "Redirect Type of Service and Host"},
        {ICMP_TIME_EXCEEDED, ICMP_EXC_TTL,        "Time to live exceeded"},
        {ICMP_TIME_EXCEEDED, ICMP_EXC_FRAGTIME,   "Frag reassembly time exceeded"},
    };

    for (size_t i = 0; i < NITEMS(code_text); i++)
        if (code_text[i].type == type && code_text[i].code == code)
        {
            printf("%s\n", code_text[i].text);
            return;
        }
    printf("%s, Unknown Code: %d\n", prefix, code);
}

static void print_type_text(int type)
{
    static const struct {
        int         type;
        const char  *text;
    } type_text[] = {
        {ICMP_ROUTERADVERT,   "Router Advertisement"},
        {ICMP_ROUTERSOLICIT,  "Router Discovery"},
        {ICMP_TIMESTAMP,      "Timestamp"},
        {ICMP_TIMESTAMPREPLY, "Timestamp Reply"},
        {ICMP_INFO_REQUEST,   "Information Request"},
        {ICMP_INFO_REPLY,     "Information Reply"},
        {ICMP_ADDRESS,        "Address Mask Request"},
        {ICMP_ADDRESSREPLY,   "Address Mask Reply"},
    };

    for (size_t i = 0; i < NITEMS(type_text); i++)
        if (type_text[i].type == type)
        {
            printf("%s\n", type_text[i].text);
            return;
        }
    printf("Bad ICMP type: %d\n", type);
}

/*
 * Prints the IP header we sent, as quoted back inside the ICMP error.
 * Layout matches the reference ping -v output, field by field.
 */
static void print_ip_data(const struct iphdr *ip, bool verbose)
{
    const unsigned char *raw = (const unsigned char *)ip;
    const unsigned char *cp;
    size_t hlen = ip->ihl * 4;
    uint16_t len = ip->tot_len;
    char src[INET_ADDRSTRLEN];
    char dst[INET_ADDRSTRLEN];

    if (verbose)
    {
        printf("IP Hdr Dump:\n ");
        for (size_t j = 0; j < sizeof(*ip); j++)
            printf("%02x%s", raw[j], (j % 2) ? " " : "");
        printf("\n");
    }

    inet_ntop(AF_INET, &ip->saddr, src, sizeof(src));
    inet_ntop(AF_INET, &ip->daddr, dst, sizeof(dst));

    printf("Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src\tDst\tData\n");
    printf(" %1x  %1x  %02x", ip->version, ip->ihl, ip->tos);
    /* Quoted length may be in either byte order; print the sane one. */
    printf(" %04x %04x", len > 0x2000 ? ntohs(len) : len, ntohs(ip->id));
    printf("   %1x %04x", (ntohs(ip->frag_off) & 0xe000) >> 13,
           ntohs(ip->frag_off) & 0x1fff);
    printf("  %02x  %02x %04x", ip->ttl, ip->protocol, ntohs(ip->check));
    printf(" %s ", src);
    printf(" %s ", dst);
    for (cp = raw + sizeof(*ip); cp < raw + hlen; cp++)
        printf("%02x", *cp);
    printf("\n");

    cp = raw + hlen;
    if (ip->protocol == IPPROTO_ICMP)
    {
        printf("ICMP: type %u, code %u, size %u", cp[0], cp[1],
               (unsigned)(ntohs(ip->tot_len) - hlen));
        if (cp[0] == ICMP_ECHOREPLY || cp[0] == ICMP_ECHO)
            printf(", id 0x%04x, seq 0x%04x",
                   cp[4] * 256 + cp[5], cp[6] * 256 + cp[7]);
        printf("\n");
    }
}

/*
 * ICMP error (Time Exceeded, Unreachable, ...). Only handled when the
 * quoted packet is one of our own echo requests to our destination.
 */
static int handle_icmp_error(t_ping *ping, const struct icmphdr *icmp,
                             ssize_t icmp_len, const struct sockaddr_in *from)
{
    const struct iphdr *orig_ip = (const struct iphdr *)(icmp + 1);
    const struct icmphdr *orig_icmp;
    size_t orig_hlen;
    char src_ip[INET_ADDRSTRLEN];

    if (icmp_len < (ssize_t)(ICMP_HDR_SIZE + sizeof(struct iphdr)))
        return -1;
    orig_hlen = orig_ip->ihl * 4;
    if (orig_hlen < sizeof(struct iphdr)
        || icmp_len < (ssize_t)(ICMP_HDR_SIZE + orig_hlen + ICMP_HDR_SIZE))
        return -1;
    orig_icmp = (const struct icmphdr *)((const char *)orig_ip + orig_hlen);

    if (orig_ip->daddr != ping->dest_addr.sin_addr.s_addr
        || orig_ip->protocol != IPPROTO_ICMP
        || orig_icmp->type != ICMP_ECHO
        || ntohs(orig_icmp->un.echo.id) != ping->pid)
        return -1;

    inet_ntop(AF_INET, &from->sin_addr, src_ip, sizeof(src_ip));
    printf("%zd bytes from %s: ", icmp_len, src_ip);

    switch (icmp->type)
    {
    case ICMP_DEST_UNREACH:
        print_code_text(icmp->type, icmp->code, "Dest Unreachable");
        break;
    case ICMP_REDIRECT:
        print_code_text(icmp->type, icmp->code, "Redirect");
        break;
    case ICMP_TIME_EXCEEDED:
        print_code_text(icmp->type, icmp->code, "Time exceeded");
        break;
    case ICMP_PARAMETERPROB:
    {
        char gw[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &icmp->un.gateway, gw, sizeof(gw));
        printf("Parameter problem: IP address = %s\n", gw);
        print_ip_data(orig_ip, ping->verbose);
        return 0;
    }
    case ICMP_SOURCE_QUENCH:
        printf("Source Quench\n");
        print_ip_data(orig_ip, ping->verbose);
        return 0;
    default:
        print_type_text(icmp->type);
        return 0;
    }
    if (ping->verbose)
        print_ip_data(orig_ip, true);
    return 0;
}

/* Returns 0 when the packet is a response to us (reply, dup or error). */
int process_packet(t_ping *ping, const char *buf, ssize_t len,
                   const struct sockaddr_in *from)
{
    const struct iphdr *ip = (const struct iphdr *)buf;
    const struct icmphdr *icmp;
    ssize_t hlen;

    if (len < (ssize_t)sizeof(struct iphdr))
        return -1;
    hlen = ip->ihl * 4;
    if (hlen < 20 || len < hlen + (ssize_t)ICMP_HDR_SIZE)
        return -1;
    if (ip->protocol != IPPROTO_ICMP)
        return -1;

    icmp = (const struct icmphdr *)(buf + hlen);
    switch (icmp->type)
    {
    case ICMP_ECHOREPLY:
        return handle_echo_reply(ping, ip, icmp, len - hlen, from);
    case ICMP_ECHO:
    case ICMP_TIMESTAMP:
    case ICMP_ADDRESS:
        return -1;
    default:
        return handle_icmp_error(ping, icmp, len - hlen, from);
    }
}
