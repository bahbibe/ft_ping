#include "ft_ping.h"

#define ARG_TTL 256
#define ARG_USAGE 257

#define USAGE_HINT \
    "Try 'ft_ping --help' or 'ft_ping --usage' for more information.\n"

static void print_help(void)
{
    printf("Usage: ft_ping [OPTION...] HOST\n"
           "Send ICMP ECHO_REQUEST packets to network hosts.\n"
           "\n"
           " Options valid for all request types:\n"
           "\n"
           "  -c, --count=NUMBER         stop after sending NUMBER packets\n"
           "  -n, --numeric              do not resolve host addresses\n"
           "  -r, --ignore-routing       send directly to a host on an attached network\n"
           "      --ttl=N                specify N as time-to-live\n"
           "  -v, --verbose              verbose output\n"
           "  -w, --timeout=N            stop after N seconds\n"
           "  -W, --linger=N             number of seconds to wait for response\n"
           "\n"
           "  -?, -h, --help             give this help list\n"
           "      --usage                give a short usage message\n");
}

static void print_usage(void)
{
    printf("Usage: ft_ping [-nrv?h] [-c NUMBER] [-w N] [-W N] [--count=NUMBER]\n"
           "               [--numeric] [--ignore-routing] [--ttl=N] [--verbose]\n"
           "               [--timeout=N] [--linger=N] [--help] [--usage] HOST\n");
}

/* Whole string must be a number; same checks and messages as the reference ping. */
static int parse_number(const char *arg, unsigned long maxval, bool allow_zero,
                        unsigned long *out)
{
    char *end;
    unsigned long n;

    errno = 0;
    n = strtoul(arg, &end, 0);
    if (*end || end == arg || errno == ERANGE)
        RETURN_ERR(-1, "ft_ping: invalid value (`%s' near `%s')\n", arg, end);
    if (n == 0 && !allow_zero)
        RETURN_ERR(-1, "ft_ping: option value too small: %s\n", arg);
    if (maxval && n > maxval)
        RETURN_ERR(-1, "ft_ping: option value too big: %s\n", arg);
    *out = n;
    return 0;
}

/*
 * Returns -1 to continue, otherwise the exit status the program must
 * return with: 0 after --help, EXIT_USAGE for a bad command line,
 * EXIT_FAILURE for a bad option value (same codes as the reference ping).
 */
int parse_args(t_ping *ping, int argc, char **argv)
{
    static const struct option long_opts[] = {
        {"count",          required_argument, NULL, 'c'},
        {"numeric",        no_argument,       NULL, 'n'},
        {"ignore-routing", no_argument,       NULL, 'r'},
        {"ttl",            required_argument, NULL, ARG_TTL},
        {"verbose",        no_argument,       NULL, 'v'},
        {"timeout",        required_argument, NULL, 'w'},
        {"linger",         required_argument, NULL, 'W'},
        {"help",           no_argument,       NULL, 'h'},
        {"usage",          no_argument,       NULL, ARG_USAGE},
        {NULL, 0, NULL, 0}
    };
    unsigned long n;
    int opt;

    opterr = 0;
    while ((opt = getopt_long(argc, argv, ":c:nrvw:W:h", long_opts, NULL)) != -1)
    {
        switch (opt)
        {
        case 'c':
            if (parse_number(optarg, 0, true, &n) < 0)
                return EXIT_FAILURE;
            ping->count = n;
            break;
        case 'n':
            ping->numeric = true;
            break;
        case 'r':
            ping->dontroute = true;
            break;
        case ARG_TTL:
            if (parse_number(optarg, 255, false, &n) < 0)
                return EXIT_FAILURE;
            ping->ttl = n;
            break;
        case 'v':
            ping->verbose = true;
            break;
        case 'w':
            if (parse_number(optarg, INT_MAX, false, &n) < 0)
                return EXIT_FAILURE;
            ping->timeout = n;
            break;
        case 'W':
            if (parse_number(optarg, INT_MAX, false, &n) < 0)
                return EXIT_FAILURE;
            ping->linger = n;
            break;
        case 'h':
            print_help();
            return 0;
        case ARG_USAGE:
            print_usage();
            return 0;
        case ':':
            if (optopt)
                fprintf(stderr, "ft_ping: option requires an argument -- '%c'\n", optopt);
            else
                fprintf(stderr, "ft_ping: option '%s' requires an argument\n", argv[optind - 1]);
            RETURN_ERR(EXIT_USAGE, USAGE_HINT);
        default:
            if (optopt == '?')
            {
                print_help();
                return 0;
            }
            if (optopt)
                fprintf(stderr, "ft_ping: invalid option -- '%c'\n", optopt);
            else
                fprintf(stderr, "ft_ping: unrecognized option '%s'\n", argv[optind - 1]);
            RETURN_ERR(EXIT_USAGE, USAGE_HINT);
        }
    }

    if (optind >= argc)
        RETURN_ERR(EXIT_USAGE, "ft_ping: missing host operand\n" USAGE_HINT);
    if (optind != argc - 1)
        RETURN_ERR(EXIT_USAGE, "ft_ping: only one host is supported\n" USAGE_HINT);
    ping->target = argv[optind];
    return -1;
}
