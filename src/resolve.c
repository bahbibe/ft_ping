#include "ft_ping.h"

int resolve_destination(t_ping *ping)
{
    struct addrinfo hints, *result;
    const char *name;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_CANONNAME;

    if (getaddrinfo(ping->target, NULL, &hints, &result) != 0)
        RETURN_ERR(-1, "ft_ping: unknown host\n");

    ping->dest_addr = *(struct sockaddr_in *)result->ai_addr;
    inet_ntop(AF_INET, &ping->dest_addr.sin_addr,
              ping->ip_str, sizeof(ping->ip_str));

    /* The header shows the canonical name (CNAME target), not argv. */
    name = result->ai_canonname ? result->ai_canonname : ping->target;
    snprintf(ping->hostname, sizeof(ping->hostname), "%s", name);

    freeaddrinfo(result);
    return 0;
}
