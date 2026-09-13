# ft_ping

An implementation of `ping` for Linux using ICMP echo requests over raw sockets. Output-compatible with GNU inetutils-2.0 (the subject's reference), also verified against inetutils-2.6.

## Requirements

- Linux (raw socket requires `CAP_NET_RAW`)
- C compiler (gcc/cc)
- `make`

## Build

```sh
make
sudo setcap cap_net_raw+ep ft_ping   # or run with sudo
```

## Usage

```
ft_ping [-nrv?h] [-c NUMBER] [-w N] [-W N] [--ttl=N] HOST
```

| Flag | Description |
|------|-------------|
| `HOST` | Hostname or IPv4 address |
| `-v`, `--verbose` | Verbose output: ICMP id in the header, IP header dump on ICMP errors |
| `-?`, `-h`, `--help` | Display help |
| `-c`, `--count=NUMBER` | Stop after sending NUMBER packets |
| `-w`, `--timeout=N` | Stop after N seconds |
| `-W`, `--linger=N` | Seconds to wait for a response after the last packet (default 10) |
| `--ttl=N` | Set IP time-to-live |
| `-r`, `--ignore-routing` | Send directly to a host on an attached network (SO_DONTROUTE) |
| `-n`, `--numeric` | Accepted for compatibility (addresses are never reverse-resolved) |

## Tests

```sh
make test-docker
```

Builds a Debian image with inetutils-2.0 and inetutils-2.6 compiled from GNU sources, then diffs
ft_ping against each (stdout, stderr and exit status) for every case in the grading scale:
argument errors, good/bad IP and hostname, `-v`, TTL-exceeded errors, Ctrl+C, and the bonus flags.
Only the allowed differences are normalized: `time=` values, the round-trip line, reverse DNS names,
and per-run ids/checksums.

Run a single reference by hand:

```sh
docker run --rm -it --cap-add=NET_RAW ft_ping-test bash
ping -v --ttl 1 8.8.8.8      # inetutils 2.0
ft_ping -v --ttl 1 8.8.8.8
```

## Files

```
include/ft_ping.h   project header
src/main.c          setup: socket, resolve, header line, exit status
src/args.c          command line parsing and validation
src/loop.c          send/receive loop (interval, linger, timeout, Ctrl+C)
src/packet.c        echo request build, reply and ICMP error parsing/printing
src/resolve.c       DNS resolution
src/checksum.c      ICMP checksum
src/stats.c         statistics
Dockerfile.test     test image (bookworm + inetutils 2.0 and 2.6)
test-diff.sh        diff-based test suite
```
