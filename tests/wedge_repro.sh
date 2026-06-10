#!/usr/bin/env bash
# Deterministic reproduction of the runtime web-server wedge.
# Symptom: after closing N>=2 simultaneous SSE subscribers, the library polls
# torn-down _ssl pointers and spams `esp_tls_get_bytes_avail()` empty-arg
# errors at ~150/sec, starving the runtime web server.
#
# Args:
#   $1 = device IP (default 192.168.10.188)
#   $2 = auth user:pass (default testing:testing)
#   $3 = number of SSE subscribers to open (default 2)
#   $4 = seconds each subscriber stays open before kill (default 4)
#   $5 = seconds to sample log after kill (default 10)
set -u
IP="${1:-192.168.10.188}"
AUTH="${2:-testing:testing}"
N="${3:-2}"
HOLD="${4:-4}"
SAMPLE="${5:-10}"
LOG="${LOG:-/tmp/portal_log.txt}"

echo "=== wedge_repro: $N SSE subs, hold ${HOLD}s, sample ${SAMPLE}s ==="

# Mark a unique anchor in the serial log so we can scope our search to lines
# emitted AFTER we started, not historical wedge symptoms.
ANCHOR="WEDGE_REPRO_$(date +%s%N)"
echo "$ANCHOR" >> "$LOG"   # writing to the file the monitor appends to is safe
                            # because we append + the monitor reattaches by
                            # truncate-then-reopen on disconnect, not concurrent
                            # access.

# Spawn N curl SSE subscribers in background.
pids=()
for ((i=0; i<N; i++)); do
    (curl -sk -u "$AUTH" -N --max-time $((HOLD + 60)) \
          -H 'Accept: text/event-stream' \
          "https://$IP/api/events" >/dev/null 2>&1) &
    pids+=("$!")
done
echo "  spawned ${#pids[@]} SSE subscribers: ${pids[*]}"

# Let them subscribe.
sleep "$HOLD"

# Kill them all roughly simultaneously (TERM, fall back to KILL).
echo "  killing all subscribers..."
for pid in "${pids[@]}"; do kill -TERM "$pid" 2>/dev/null; done
sleep 0.5
for pid in "${pids[@]}"; do kill -KILL "$pid" 2>/dev/null; done
wait 2>/dev/null

# Sample the log for the storm signature after kill.
echo "  sampling log for ${SAMPLE}s..."
sleep "$SAMPLE"

# Extract everything after the anchor; count the storm signature.
storm_lines=$(awk -v a="$ANCHOR" '
    $0 == a { seen=1; next }
    seen
' "$LOG" | grep -c 'empty arg passed to esp_tls_get_bytes_avail' || true)

# Also probe whether the server still responds.
post_status=$(curl -sk -u "$AUTH" --max-time 5 -o /dev/null \
                  -w "%{http_code}" "https://$IP/api/config" 2>/dev/null)

echo "--- RESULT ---"
echo "  storm lines after kill:   $storm_lines"
echo "  GET /api/config response: HTTP $post_status"
if [ "$storm_lines" -gt 100 ]; then
    echo "  VERDICT: WEDGE REPRODUCED ($storm_lines empty-arg errors in ${SAMPLE}s)"
    exit 1
elif [ "$post_status" = "200" ] || [ "$post_status" = "401" ]; then
    echo "  VERDICT: server still responsive, no storm detected"
    exit 0
else
    echo "  VERDICT: server unresponsive (HTTP $post_status) but no storm"
    exit 2
fi
