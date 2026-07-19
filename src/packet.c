#include "ft_ping.h"

int create_raw_socket(void)
{
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0)
        RETURN_ERR(-1, "ft_ping: socket: %s\n%s", strerror(errno), "  (try running with sudo or set cap_net_raw+ep)");

    struct timeval tv = {0, 50000};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    return sockfd;
}

int send_echo(t_ping *ping)
{
    char packet[PACKET_SIZE];
    struct icmphdr *icmp = (struct icmphdr *)packet;
    struct timespec *ts;

    memset(packet, 0, sizeof(packet));
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->un.echo.id = htons(ping->pid);
    icmp->un.echo.sequence = htons(ping->sequence);

    ts = (struct timespec *)(packet + ICMP_HDR_SIZE);
    clock_gettime(CLOCK_MONOTONIC, ts);

    icmp->checksum = icmp_checksum(packet, sizeof(packet));

    ssize_t sent = sendto(ping->sockfd, packet, sizeof(packet), 0,
                          (struct sockaddr *)&ping->dest_addr,
                          sizeof(ping->dest_addr));
    if (sent < 0)
        RETURN_ERR(-1, "ft_ping: sendto: %s\n", strerror(errno));

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

int process_packet(t_ping *ping, const char *buf, ssize_t len,
                   const struct sockaddr_in *from)
{
    const struct iphdr *ip = (const struct iphdr *)buf;
    ssize_t ip_hdr_len;

    if (len < (ssize_t)sizeof(struct iphdr))
        return -1;

    ip_hdr_len = (ip->ihl) * 4;
    if (ip_hdr_len < 20 || ip_hdr_len > 60)
        return -1;

    if (len < ip_hdr_len + (ssize_t)ICMP_HDR_SIZE)
        return -1;

    if (ip->protocol != IPPROTO_ICMP)
        return -1;

    const struct icmphdr *icmp =
        (const struct icmphdr *)(buf + ip_hdr_len);
    ssize_t icmp_len = len - ip_hdr_len;

    switch (icmp->type)
    {
    case ICMP_ECHOREPLY:
    {
        if (ntohs(icmp->un.echo.id) != ping->pid)
            return -1;

        uint16_t recv_seq = ntohs(icmp->un.echo.sequence);
        char src_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from->sin_addr, src_ip, sizeof(src_ip));

        if (icmp_len < (ssize_t)(ICMP_HDR_SIZE + TIMESTAMP_SIZE))
            RETURN_ERR(-1, "ft_ping: short echo reply (%zd bytes)\n",
                       icmp_len);

        const struct timespec *send_ts =
            (const struct timespec *)(buf + ip_hdr_len + ICMP_HDR_SIZE);
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double rtt_ms = compute_rtt_ms(send_ts, &now);

        ping->stats.nreceived++;
        stats_update(&ping->stats, rtt_ms);

        printf("%zd bytes from %s: icmp_seq=%u ttl=%d time=%.3f ms\n",
               icmp_len, src_ip, recv_seq, ip->ttl, rtt_ms);
        return 0;
    }
    case ICMP_TIME_EXCEEDED:
    case ICMP_DEST_UNREACH:
        if (!ping->verbose)
            break;
    {
        const char *err_body = (const char *)(icmp + 1);
        const struct iphdr *orig_ip = (const struct iphdr *)(err_body + 4);
        int orig_ip_hdr_len = (orig_ip->ihl) * 4;

        if (orig_ip_hdr_len < 20 || orig_ip_hdr_len > 60)
            return -1;

        if (icmp_len < (ssize_t)(ICMP_HDR_SIZE + 4 + orig_ip_hdr_len + ICMP_HDR_SIZE))
            return -1;

        const struct icmphdr *orig_icmp =
            (const struct icmphdr *)((const char *)orig_ip + orig_ip_hdr_len);

        if (ntohs(orig_icmp->un.echo.id) != ping->pid)
            return -1;

        uint16_t orig_seq = ntohs(orig_icmp->un.echo.sequence);
        char router_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip->saddr, router_ip, sizeof(router_ip));

        ping->stats.nerrors++;

        printf("From %s icmp_seq=%u ", router_ip, orig_seq);
        if (icmp->type == ICMP_TIME_EXCEEDED)
            printf("Time to live exceeded\n");
        else
            printf("Destination Unreachable\n");
        return 0;
    }
    }

    return -1;
}
