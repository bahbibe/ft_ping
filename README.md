# ft_ping

An implementation of `ping` for Linux using ICMP echo requests over raw sockets. Output-compatible with inetutils-2.0.

## Requirements

- Linux (raw socket requires `CAP_NET_RAW`)
- C compiler (gcc/cc)
- `make`

## Build

```sh
make
sudo setcap cap_net_raw+ep ft_ping   # one-time capability grant
```

## Usage

```
ft_ping [-vnr] [-c count] [-W timeout] [-w deadline] [--ttl n] destination
```

| Flag | Description |
|------|-------------|
| `destination` | Hostname or IP address |
| `-c count` | Stop after `count` packets |
| `-W timeout` | Time to wait for a reply, in seconds |
| `-w deadline` | Total runtime timeout, in seconds |
| `-n` | Numeric output only (no reverse DNS) |
| `-r` | Bypass routing tables (SO_DONTROUTE) |
| `-v` | Verbose output (ICMP error details) |
| `--ttl n` | Set IP TTL |
| `-?` | Display help |

## Docker test

```sh
make test-docker
```

Runs a comprehensive 17-test suite comparing ft_ping output against inetutils-2.0 ping inside a Debian container.

## Files

```
include/ft_ping.h   — project header
src/main.c          — CLI parsing, orchestration
src/loop.c          — event loop, send/receive, deadline, drain
src/resolve.c       — DNS resolution
src/packet.c        — packet construction and parsing
src/checksum.c      — ICMP checksum
src/stats.c         — statistics computation and output
Dockerfile.test     — multi-stage test image (bookworm + inetutils-2.0)
test-diff.sh        — test suite (17 diff-based checks)
```
