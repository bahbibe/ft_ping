#!/bin/bash
# ft_ping test suite matching grade.pdf evaluation checklist
# Exemptions: RTT line, reverse DNS, +/-30ms per packet

PASS=0
FAIL=0
REF_PING=/usr/bin/ping
FT_PING=/usr/local/bin/ft_ping

pass() { echo "  PASS: $1"; ((PASS++)); }
fail() { echo "  FAIL: $1"; ((FAIL++)); }

strip_output() {
    # strip PING line (reverse DNS exempt), round-trip line (exempt), time= values (+/-30ms exempt)
    grep -v -E '^(PING |round-trip)' | sed 's/time=[0-9.]* ms/time=X.XXX ms/g'
}

diff_output() {
    local label="$1" ref="$2" ft="$3"
    local ref_stripped=$(echo "$ref" | strip_output)
    local ft_stripped=$(echo "$ft" | strip_output)
    if diff <(echo "$ref_stripped") <(echo "$ft_stripped") >/dev/null 2>&1; then
        pass "$label"
    else
        fail "$label"
        echo "    diff follows:"
        diff <(echo "$ref_stripped") <(echo "$ft_stripped") | sed 's/^/    /'
    fi
}

echo "=== ft_ping Test Suite ==="
echo ""

# ---- 1. Help / usage ----
echo "--- Checking arguments ---"

# -? flag
HELP_TEXT=$($FT_PING -? 2>&1)
if echo "$HELP_TEXT" | grep -qi "usage"; then
    pass "-? displays usage"
else
    fail "-? does not display usage"
    echo "    got: $HELP_TEXT"
fi

# missing host
MISSING=$($FT_PING 2>&1; true)
if echo "$MISSING" | grep -qi "missing host"; then
    pass "missing host prints error"
else
    fail "missing host does not print error"
    echo "    got: $MISSING"
fi

# too many args
TOO_MANY=$($FT_PING 127.0.0.1 192.0.2.1 2>&1; true)
if echo "$TOO_MANY" | grep -qi "too many"; then
    pass "too many args prints error"
else
    fail "too many args does not print error"
    echo "    got: $TOO_MANY"
fi

# ---- 2. Good IP (127.0.0.1) ----
echo ""
echo "--- ft_ping good ip ---"

REF_GOOD=$($REF_PING -c 5 127.0.0.1 2>&1)
FT_GOOD=$($FT_PING -c 5 127.0.0.1 2>&1)
diff_output "good ip (127.0.0.1)" "$REF_GOOD" "$FT_GOOD"

# ---- 3. Bad IP (non-routable) ----
echo ""
echo "--- ft_ping bad ip ---"
BAD_IP=192.0.2.1
REF_BAD=$($REF_PING -c 3 -W 2 $BAD_IP 2>&1 || true)
FT_BAD=$($FT_PING -c 3 -W 2 $BAD_IP 2>&1 || true)

if [ -n "$FT_BAD" ]; then
    pass "bad ip produces output"
else
    fail "bad ip produces no output"
fi
diff_output "bad ip ($BAD_IP)" "$REF_BAD" "$FT_BAD"

# ---- 4. -v with bad IP ----
echo ""
echo "--- ft_ping -v bad ip ---"
REF_VBAD=$($REF_PING -v -c 3 -W 2 $BAD_IP 2>&1 || true)
FT_VBAD=$($FT_PING -v -c 3 -W 2 $BAD_IP 2>&1 || true)

if [ -n "$FT_VBAD" ]; then
    pass "-v bad ip produces output"
else
    fail "-v bad ip produces no output"
fi
diff_output "-v bad ip ($BAD_IP)" "$REF_VBAD" "$FT_VBAD"

# ---- 5. Good hostname (localhost) ----
echo ""
echo "--- ft_ping good hostname ---"

REF_HNAME=$($REF_PING -c 5 localhost 2>&1)
FT_HNAME=$($FT_PING -c 5 localhost 2>&1)
diff_output "good hostname (localhost)" "$REF_HNAME" "$FT_HNAME"

# ---- 6. Bad hostname (non-existent) ----
echo ""
echo "--- ft_ping bad hostname ---"
BAD_HOST="nonexistent-12345.example.com"
REF_BADHOST=$($REF_PING -c 1 $BAD_HOST 2>&1 || true)
FT_BADHOST=$($FT_PING -c 1 $BAD_HOST 2>&1 || true)

if echo "$FT_BADHOST" | grep -qiE "(unknown host|not known|cannot resolve|Name or service not known|Temporary failure)"; then
    pass "bad hostname prints resolution error"
else
    fail "bad hostname does not print resolution error"
    echo "    got: $FT_BADHOST"
fi

# ---- 7. -v with bad hostname ----
echo ""
echo "--- ft_ping -v bad hostname ---"
REF_VBADHOST=$($REF_PING -v -c 1 $BAD_HOST 2>&1 || true)
FT_VBADHOST=$($FT_PING -v -c 1 $BAD_HOST 2>&1 || true)

if echo "$FT_VBADHOST" | grep -qiE "(unknown host|not known|cannot resolve|Name or service not known|Temporary failure)"; then
    pass "-v bad hostname prints resolution error"
else
    fail "-v bad hostname does not print resolution error"
    echo "    got: $FT_VBADHOST"
fi

# ---- 8. -n flag (numeric only) ----
echo ""
echo "--- ft_ping -n flag ---"
REF_NUM=$($REF_PING -n -c 3 127.0.0.1 2>&1)
FT_NUM=$($FT_PING -n -c 3 127.0.0.1 2>&1)
diff_output "-n flag (numeric)" "$REF_NUM" "$FT_NUM"

# ---- 9. -r flag (bypass routing) ----
echo ""
echo "--- ft_ping -r flag ---"
REF_ROUTE=$($REF_PING -r -c 3 127.0.0.1 2>&1)
FT_ROUTE=$($FT_PING -r -c 3 127.0.0.1 2>&1)
diff_output "-r flag (dontroute)" "$REF_ROUTE" "$FT_ROUTE"

# ---- 10. -W flag (timeout) ----
echo ""
echo "--- ft_ping -W flag ---"
REF_W=$($REF_PING -W 1 -c 5 127.0.0.1 2>&1)
FT_W=$($FT_PING -W 1 -c 5 127.0.0.1 2>&1)
diff_output "-W flag (timeout=1s)" "$REF_W" "$FT_W"

# ---- 11. --ttl flag ----
echo ""
echo "--- ft_ping --ttl flag ---"
REF_TTL=$($REF_PING --ttl 128 -c 3 127.0.0.1 2>&1)
FT_TTL=$($FT_PING --ttl 128 -c 3 127.0.0.1 2>&1)
diff_output "--ttl flag (ttl=128)" "$REF_TTL" "$FT_TTL"

# ---- 12. -w flag (deadline) ----
echo ""
echo "--- ft_ping -w flag ---"
# Use longer deadline to avoid timing edge cases
REF_WL=$($REF_PING -w 5 -c 20 127.0.0.1 2>&1)
FT_WL=$($FT_PING -w 5 -c 20 127.0.0.1 2>&1)
diff_output "-w flag (deadline=5s)" "$REF_WL" "$FT_WL"

# ---- 13. Preliminaries check (no poll/ppoll/fcntl in source) ----
echo ""
echo "--- Preliminaries (no poll/ppoll/fcntl) ---"
SRC_DIR=$(dirname "$0")/src
INCLUDE_DIR=$(dirname "$0")/include
if grep -rnE '\b(poll|ppoll|fcntl)\s*\(' "$SRC_DIR" "$INCLUDE_DIR" 2>/dev/null \
    | grep -vE '(//.*\b(poll|ppoll|fcntl)\b|poll\.h|ft_ping\.h)' \
    | grep -q . ; then
    fail "source uses poll/ppoll/fcntl (not allowed)"
    grep -rnE '\b(poll|ppoll|fcntl)\s*\(' "$SRC_DIR" "$INCLUDE_DIR" 2>/dev/null \
        | grep -vE '(//|poll\.h)' | sed 's/^/    /'
else
    pass "no poll/ppoll/fcntl found in source"
fi

# ---- Summary ----
echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
exit $FAIL
