#!/bin/bash
# ft_ping test suite, following the grade.pdf checklist.
#
# Every test runs the reference ping and ft_ping side by side and diffs
# stdout+stderr+exit status. Allowed differences are normalized away:
#   - time= values and the round-trip line (subject: RTT line is exempt)
#   - reverse DNS in "from host (ip)" (subject: not required)
#   - ICMP id (process id) and per-packet IP id/checksum in -v dumps
#   - program name (ping vs ft_ping)
#
# Usage: REF_PING=/usr/bin/ping-2.6 ./test-diff.sh

REF_PING=${REF_PING:-/usr/bin/ping}
FT_PING=${FT_PING:-/usr/local/bin/ft_ping}
REMOTE=${REMOTE:-8.8.8.8}
BAD_IP=192.0.2.1          # TEST-NET-1: routable, never answers

PASS=0
FAIL=0
SKIP=0
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

pass() { echo "  PASS: $1"; ((PASS++)); }
fail() { echo "  FAIL: $1"; ((FAIL++)); }
skip() { echo "  SKIP: $1"; ((SKIP++)); }

normalize() {
    sed -E \
        -e 's/(^|[^_])ft_ping/\1ping/g' \
        -e 's#(/usr/bin/)?ping-2\.6#ping#g' \
        -e 's#/usr/bin/ping#ping#g' \
        -e 's/time=[0-9.]+ ms/time=X ms/' \
        -e '/^round-trip /d' \
        -e 's/from [^ ]+ \(([0-9.]+)\):/from \1:/' \
        -e 's/, id 0x[0-9a-f]{4} = [0-9]+$/, id ID/' \
        -e 's/, id 0x[0-9a-f]{4}, seq/, id ID, seq/' \
        -e 's/^ (4[0-9a-f]{3} [0-9a-f]{4}) [0-9a-f]{4} ([0-9a-f]{4} [0-9a-f]{4}) [0-9a-f]{4} /\1 IDID \2 CKSM /' \
        -e 's/^( [0-9a-f]  [0-9a-f]  [0-9a-f]{2} [0-9a-f]{4}) [0-9a-f]{4}(   [0-9a-f] [0-9a-f]{4}  [0-9a-f]{2}  [0-9a-f]{2}) [0-9a-f]{4} /\1 IDID\2 CKSM /'
}

# run NAME SECONDS ARGS...  -> runs the reference, then ft_ping. SECONDS > 0
# sends SIGINT after that delay, like an evaluator pressing Ctrl+C.
# Never run them in parallel: each raw socket sees the other's packets,
# and routers rate-limit ICMP errors, so results would not be comparable.
run() {
    local name=$1 secs=$2; shift 2
    local wrap=()
    [ "$secs" != 0 ] && wrap=(timeout -s INT -k 5 "$secs")
    "${wrap[@]}" "$REF_PING" "$@" > "$TMP/ref" 2>&1; echo "exit=$?" >> "$TMP/ref"
    "${wrap[@]}" "$FT_PING"  "$@" > "$TMP/ft"  2>&1; echo "exit=$?" >> "$TMP/ft"
    normalize < "$TMP/ref" > "$TMP/ref.n"
    normalize < "$TMP/ft"  > "$TMP/ft.n"
    if diff -u "$TMP/ref.n" "$TMP/ft.n" > "$TMP/diff"; then
        pass "$name"
    else
        fail "$name"
        sed 's/^/    /' "$TMP/diff"
    fi
}

has_network() {
    timeout 3 "$REF_PING" -c 1 -W 2 "$REMOTE" > /dev/null 2>&1
}

echo "=== ft_ping test suite (reference: $("$REF_PING" -V | head -1)) ==="

echo ""
echo "--- Preliminaries ---"
SRC_DIR=$(dirname "$0")/src
if [ -d "$SRC_DIR" ]; then
    if grep -rnE '\b(poll|ppoll|fcntl|select)\s*\(' "$SRC_DIR" "$(dirname "$0")/include"; then
        fail "forbidden function found"
    else
        pass "no poll/ppoll/fcntl/select"
    fi
else
    skip "sources not in image"
fi

echo ""
echo "--- Checking arguments ---"
for flag in '-?' --help -h; do
    out=$("$FT_PING" "$flag" 2>&1); code=$?
    if [ $code -eq 0 ] && grep -q "Usage" <<< "$out"; then
        pass "$flag prints help"
    else
        fail "$flag prints help (exit=$code)"
    fi
done
run "no host operand"          0
run "invalid short option"     0 -x 127.0.0.1
run "unrecognized long option" 0 --nope 127.0.0.1
run "missing option argument"  0 127.0.0.1 -c
run "--ttl 0 too small"        0 --ttl 0 127.0.0.1
run "--ttl 256 too big"        0 --ttl 256 127.0.0.1
run "-c garbage"               0 -c abc 127.0.0.1

echo ""
echo "--- ft_ping ip ---"
run "good ip, Ctrl+C"          3.5 127.0.0.1
run "good ip -v, Ctrl+C"       3.5 -v 127.0.0.1
run "bad ip, Ctrl+C"           3.5 $BAD_IP
run "-v bad ip, Ctrl+C"        3.5 -v $BAD_IP

echo ""
echo "--- ft_ping hostname ---"
run "good hostname, Ctrl+C"    3.5 localhost
run "bad hostname"             0 nonexistent-12345.invalid
run "-v bad hostname"          0 -v nonexistent-12345.invalid

echo ""
echo "--- TTL errors (scale: change the TTL) ---"
if has_network; then
    run "--ttl 1 remote, Ctrl+C"     3.5 --ttl 1 $REMOTE
    run "-v --ttl 1 remote, Ctrl+C"  3.5 -v --ttl 1 $REMOTE
    run "--ttl=1 form"               3.5 --ttl=1 $REMOTE
    run "good remote ip -c 3"        0 -c 3 $REMOTE
else
    skip "no route to $REMOTE"
fi

echo ""
echo "--- Bonus flags ---"
run "-c 3"                     0 -c 3 127.0.0.1
run "-c 2 -W 1 linger"         0 -c 2 -W 1 $BAD_IP
run "-w 3 timeout"             0 -w 3 127.0.0.1
run "-w 2 bad ip"              0 -w 2 $BAD_IP
run "-r local"                 0 -r -c 2 127.0.0.1
run "-r remote: send fails"    0 -r -c 2 $REMOTE
run "-n"                       0 -n -c 2 localhost

echo ""
echo "=== Results: $PASS passed, $FAIL failed, $SKIP skipped ==="
exit $FAIL
