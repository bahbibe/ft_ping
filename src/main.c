#include "ft_ping.h"

void print_help(void)
{
    printf("Usage: ft_ping [-v] [-?] [-n] [-r] [-W timeout] [-w deadline]\n"
           "              [--ttl N] <hostname or IP>\n"
           "\n"
           "Options:\n"
           "  -v        verbose output\n"
           "  -n        numeric output only\n"
           "  -r        bypass the routing table\n"
           "  -W sec    time to wait for a response\n"
           "  -w sec    stop after N seconds\n"
           "  --ttl N   set IP Time To Live\n"
           "  -?        display this help and exit\n");
}

static void print_startup(t_ping *ping)
{
    printf("PING %s (%s): %d data bytes\n",
           ping->numeric ? ping->ip_str : ping->hostname,
           ping->ip_str, DATA_SIZE);
}

int main(int argc, char **argv)
{
    t_ping ping;
    int opt;
    int status = 2;

    if (argc < 2)
        RETURN_ERR(1, "error: no arguments. Use -? for usage.\n");
    memset(&ping, 0, sizeof(ping));
    ping.sockfd = -1;
    ping.verbose = false;
    ping.numeric = false;
    ping.dontroute = false;
    ping.waittimeout = 0;
    ping.deadline = 0;
    ping.ttl = 0;
    ping.pid = getpid() & 0xFFFF;
    ping.sequence = 0;
    stats_init(&ping.stats);

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--ttl") == 0 && i + 1 < argc)
        {
            ping.ttl = atoi(argv[i + 1]);
            memmove(&argv[i], &argv[i + 2], (argc - i - 1) * sizeof(char *));
            argc -= 2;
            break;
        }
    }

    opterr = 0;
    while ((opt = getopt(argc, argv, "vnrW:w:")) != -1)
    {
        switch (opt)
        {
        case 'v':
            ping.verbose = true;
            break;
        case 'n':
            ping.numeric = true;
            break;
        case 'r':
            ping.dontroute = true;
            break;
        case 'W':
            ping.waittimeout = atoi(optarg);
            break;
        case 'w':
            ping.deadline = atoi(optarg);
            break;
        case '?':
            if (optopt == '?')
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
        RETURN_ERR(2, "ft_ping: missing host operand\n"
                      "Usage: ft_ping [-v] [-?] [-n] [-r] [-W timeout] [-w deadline] [--ttl N] <hostname or IP>\n");

    if (optind != argc - 1)
        RETURN_ERR(2, "ft_ping: too many arguments\n");

    ping.hostname = argv[optind];

    if (resolve_destination(&ping) != 0)
        goto cleanup;

    ping.sockfd = create_raw_socket();
    if (ping.sockfd < 0)
        goto cleanup;

    if (ping.dontroute)
    {
        int opt = 1;
        setsockopt(ping.sockfd, SOL_SOCKET, SO_DONTROUTE,
                   &opt, sizeof(opt));
    }
    if (ping.ttl > 0)
        setsockopt(ping.sockfd, IPPROTO_IP, IP_TTL,
                   &ping.ttl, sizeof(ping.ttl));

    if (setup_signal_handler() != 0)
        goto cleanup;

    print_startup(&ping);
    event_loop(&ping);
    print_stats(&ping);
    status = 0;

cleanup:
    if (ping.sockfd >= 0)
        close(ping.sockfd);
    return status;
}
