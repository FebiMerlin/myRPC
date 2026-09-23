#!/bin/bash
# manual_check.sh -- quick manual smoke test used during development.
set -x
W=/tmp/mt
rm -rf "$W"; mkdir -p "$W"
printf 'port = 18234\nsocket_type = stream\ndaemon = no\n' > "$W/myRPC.conf"
id -un > "$W/users.conf"
cd /root/myrpc || exit 1

./server/myRPC-server -c "$W/myRPC.conf" -u "$W/users.conf" -l "$W/server.log" -f &
SP=$!
sleep 1

timeout 15 ./client/myRPC-client -h 127.0.0.1 -p 18234 -s -l "$W/client.log" -c "echo integration-ok"
echo "CLIENT_EXIT=$?"

timeout 15 ./client/myRPC-client -h 127.0.0.1 -p 18234 -s -l "$W/client.log" -c "exit 3"
echo "FAIL_CMD_EXIT=$?"

kill -TERM "$SP"
sleep 1
echo "--- server log ---"
cat "$W/server.log"
