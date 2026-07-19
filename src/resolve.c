#include "ft_ping.h"

int resolve_destination(t_ping *ping)
{
    struct addrinfo hints, *result, *rp;
    int err;
    int status = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = IPPROTO_ICMP;

    err = getaddrinfo(ping->hostname, NULL, &hints, &result);
    if (err != 0)
        RETURN_ERR(-1, "ft_ping: unknown host\n");

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        if (rp->ai_family == AF_INET)
        {
            ping->dest_addr = *(struct sockaddr_in *)rp->ai_addr;
            inet_ntop(AF_INET, &ping->dest_addr.sin_addr,
                      ping->ip_str, sizeof(ping->ip_str));
            status = 0;
            goto cleanup;
        }
    }

    fprintf(stderr, "ft_ping: %s: No IPv4 address found\n", ping->hostname);

cleanup:
    freeaddrinfo(result);
    return status;
}
