#!/bin/bash
# Uso: dfl_run.sh cmds.json   -> manda cada comando al socket y muestra la respuesta
set -e
CMDS_B64=$(base64 -w0 "$1")
. tools/lib-radio.sh
IP=${RADIO_IP:-$(radio_ip)}
sshpass -p 123 ssh -F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  -o ConnectTimeout=10 -o LogLevel=ERROR -o PubkeyAuthentication=no "root@$IP" "python3 -" <<PY
import json, socket, base64, time
cmds = json.loads(base64.b64decode("$CMDS_B64"))
for c in cmds:
    time.sleep(c.pop("_wait", 0.8))
    s = socket.socket(socket.AF_UNIX); s.settimeout(8)
    try:
        s.connect("/tmp/x6200_remote.sock")
        s.sendall((json.dumps(c) + "\n").encode())
        r = s.recv(4096).decode().strip()
        print("->", json.dumps(c), "=>", r[:260])
    except Exception as e:
        print("->", json.dumps(c), "ERR", e)
    finally:
        s.close()
PY
