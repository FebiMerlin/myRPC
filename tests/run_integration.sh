#!/bin/bash
# run_integration.sh -- start the server, send commands with the client and
# check the answers.  Used both by "make test" and by the CI pipeline.

ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK=$(mktemp -d /tmp/myrpc_it_XXXXXX)
PORT=${MYRPC_TEST_PORT:-18234}
DGRAM_PORT=$((PORT + 1))
USER_NAME=$(id -un)
SERVER_PID=""
FAILED=0

# Every call of the client is limited in time, so that a broken server can
# never block the pipeline.
CLIENT="timeout 15 $ROOT/client/myRPC-client"

cleanup ()
{
  if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
    kill -TERM "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null
  fi
  rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

check ()
{
  if [ "$1" -eq 0 ]; then
    echo "  ok   $2"
  else
    echo "  FAIL $2"
    FAILED=$((FAILED + 1))
  fi
}

# Wait until the given TCP port accepts connections.
wait_for_port ()
{
  local port=$1
  local i=0

  while [ $i -lt 50 ]; do
    if (exec 3<>"/dev/tcp/127.0.0.1/$port") 2>/dev/null; then
      exec 3>&- 2>/dev/null
      return 0
    fi
    i=$((i + 1))
    sleep 0.1
  done

  return 1
}

write_config ()
{
  cat > "$WORK/myRPC.conf" <<EOF
port = $1
socket_type = $2
daemon = no
EOF
}

echo "=== integration test of myRPC ==="

write_config "$PORT" stream
echo "$USER_NAME" > "$WORK/users.conf"

"$ROOT/server/myRPC-server" -c "$WORK/myRPC.conf" -u "$WORK/users.conf" \
    -l "$WORK/server.log" -f &
SERVER_PID=$!

wait_for_port "$PORT"
check $? "the server accepts connections on port $PORT"

# 1. A command of an allowed user succeeds and returns its output.
OUT=$($CLIENT -h 127.0.0.1 -p "$PORT" -s -l "$WORK/client.log" \
      -c "echo integration-ok" 2>"$WORK/err1")
echo "$OUT" | grep -q "integration-ok"
check $? "an allowed user runs a command"

# 2. A failing command is reported with the error code.
$CLIENT -h 127.0.0.1 -p "$PORT" -s -l "$WORK/client.log" -c "exit 3" \
    >"$WORK/out2" 2>"$WORK/err2"
[ $? -ne 0 ]
check $? "a failing command returns a non zero status"

grep -q "status 3" "$WORK/err2"
check $? "the error description reaches the client"

# 3. Special characters are transported unchanged.
OUT=$($CLIENT -h 127.0.0.1 -p "$PORT" -s -l "$WORK/client.log" \
      -c 'echo "quoted text"' 2>/dev/null)
echo "$OUT" | grep -q "quoted text"
check $? "quotes are escaped correctly"

# 4. The output of a command is written into the temporary files required
#    by the task.
grep -q "/tmp/myRPC_.*\.stdout" "$WORK/server.log"
check $? "the output is collected in /tmp/myRPC_XXXXXX.stdout"

# 5. A user outside the white list is rejected after SIGHUP.
echo "nobody-else" > "$WORK/users.conf"
kill -HUP "$SERVER_PID"
sleep 0.5

$CLIENT -h 127.0.0.1 -p "$PORT" -s -l "$WORK/client.log" \
    -c "echo should-not-run" >"$WORK/out5" 2>"$WORK/err5"
[ $? -ne 0 ]
check $? "a user outside the white list is rejected"

grep -q "not allowed" "$WORK/err5"
check $? "the client is told why the command was rejected"

grep -q "SIGHUP" "$WORK/server.log"
check $? "the reload of the configuration is logged"

# 6. The datagram transport works as well.
write_config "$DGRAM_PORT" dgram
echo "$USER_NAME" > "$WORK/users.conf"
kill -HUP "$SERVER_PID"
sleep 1

OUT=$($CLIENT -h 127.0.0.1 -p "$DGRAM_PORT" -d -l "$WORK/client.log" \
      -c "echo dgram-ok" 2>"$WORK/err6")
echo "$OUT" | grep -q "dgram-ok"
check $? "the datagram transport works"

# 7. A long running command keeps a worker busy; the daemon must stop that
#    worker before it exits itself.
write_config "$PORT" stream
echo "$USER_NAME" > "$WORK/users.conf"
kill -HUP "$SERVER_PID"
sleep 1

$CLIENT -h 127.0.0.1 -p "$PORT" -s -l "$WORK/client.log"     -c "sleep 30" >/dev/null 2>&1 &
BUSY_CLIENT=$!
sleep 1

pgrep -f "sleep 30" >/dev/null
check $? "a long running command occupies a worker process"

kill -TERM "$SERVER_PID"
wait "$SERVER_PID" 2>/dev/null
SERVER_PID=""
wait "$BUSY_CLIENT" 2>/dev/null

grep -q "stopping the worker processes" "$WORK/server.log"
check $? "the workers are stopped before the daemon exits"

grep -q "worker process .* stopped" "$WORK/server.log"
check $? "the daemon waits for every worker"

grep -q "myRPC-server stopped" "$WORK/server.log"
check $? "the server shuts down cleanly"

echo "=== integration test finished, $FAILED failure(s) ==="
exit $FAILED
