# Plan de ejecución — fase B (hardware / QA físico)

**Estado actual: BLOCKED / PENDING_HARDWARE.** No se ejecutó nada de este plan en fase A: no hubo
SSH, socket, RF, captura, power-cycle ni cambio de SD/DATA. Este documento es el runbook para que
el **orquestador + owner** lo ejecuten cuando haya autorización, ventana exclusiva e instrumentos.

Los 14 casos con columnas completas (precondiciones, instrumento, pasos, medición, expected, abort,
restore, evidencia) están en [`hardware-plan.csv`](hardware-plan.csv). Acá va la secuencia
operativa y los comandos concretos.

---

## 1. Prerrequisitos que debe proveer el owner/orquestador

1. **Autorización escrita** de la fase B y **ventana exclusiva** (sin DeepSeek ni otro agente
   controlando la radio).
2. **Decisión de identidad:** ¿se evalúa el software **instalado** o se autoriza una SD/prueba con
   el artefacto exacto `c888308e`? Este PR **no** autoriza desplegar/flashear. Si la radio sigue
   con el tip (`3b22a03a…`), la evidencia física es del software instalado, no del candidato; el
   candidato seguirá `PENDING_HARDWARE`.
3. **Backup restaurable** de DATA/params y procedimiento de restore probado; SD(s) de prueba
   separadas para HW-06. Nunca la SD/DATA operativa del owner.
4. **Instrumentos:** fuente/medidor V/I/P calibrado (HW-05), cámara para input-to-feedback
   (HW-02), generador/analizador de audio (HW-03), carga fantasma + wattmeter/analizador (HW-04,
   solo con autorización TX específica).
5. Confirmación de que la radio está en red y de que el helper `tools/lib-radio.sh` puede
   descubrirla (no fijar IP a ciegas).

## 2. Preparación de la sesión (lectura, sin modificar nada)

```bash
cd /home/pablo/desa/x6200/assessment-194/clone-gui
. tools/lib-radio.sh
IP=$(radio_ip) || { echo "radio no encontrada"; exit 1; }
OUT=/home/pablo/desa/x6200/assessment-194/scratch-199/hardware-remote/$(date -u +%Y%m%dT%H%M%SZ)
mkdir -p "$OUT"
rssh()   { ssh_radio "$IP" "$@"; }
remote() { rssh "/tmp/x6200-remote '$1'"; }
```

Si `/tmp/x6200-remote` no existe, copiarlo **solo** con el procedimiento existente (no instalar
servicios ni reemplazar firmware):

```bash
sshpass -p "$RADIO_CLAVE" scp -F /dev/null -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null -o ConnectTimeout=8 \
  tools/remote/x6200-remote "root@$IP:/tmp/x6200-remote"
rssh 'chmod 755 /tmp/x6200-remote'
```

## 3. HR-01 — exclusividad, identidad y snapshot (solo lectura)

```bash
# conexiones y procesos
rssh 'ss -tn state established "( sport = :22 )" 2>/dev/null; w; \
      ps w | grep -E "x6200_gui|x6200_daemon|sshd" | grep -v grep'
# identidad y vida
rssh 'cat /proc/uptime; cat /proc/sys/kernel/random/boot_id; \
      pidof x6200_gui; pidof x6200_daemon; \
      md5sum /usr/sbin/x6200_gui; sha256sum /usr/sbin/x6200_gui; \
      ls -l /usr/sbin/x6200_gui; \
      grep -a -o -E "[0-9a-f]{32}" /usr/sbin/x6200_gui | sort -u'
# librerias/montajes/logs
rssh 'ls -l /usr/lib/libpulse* /usr/lib/libsndfile* 2>/dev/null; \
      findmnt /mnt 2>/dev/null; df -h / /mnt 2>/dev/null; \
      cat /mnt/lastbreath 2>/dev/null; \
      ls -l /mnt/app_logs 2>/dev/null | tail -n 10; \
      tail -n 40 /mnt/app_logs/boot.log 2>/dev/null'
# snapshot JSON crudo
remote '{"cmd":"get"}' | tee "$OUT/hr01-get-initial.json"
```

**Aceptación HR-01:** sesión exclusiva (solo la conexión de esta corrida), identidad SHA256/md5
del binario y `fuente-md5` documentadas, sin TX/app operativa, snapshot guardado.

**Abortar sin cerrar apps** si: hay TX activa, una app operativa abierta, identidad no demostrable,
otro agente/operador conectado, o la radio no responde. Documentar BLOCKED con la evidencia.

## 4. HR-02 — disponibilidad y baseline física

```bash
bash tools/verifica-radio.sh "$IP" 2>&1 | tee "$OUT/hr02-verifica.log"
bash tools/captura-radio.sh "$OUT/hr02-home-before.png"
remote '{"cmd":"get"}' | tee "$OUT/hr02-get-before.json"
# CRASHLOG real y app_logs (el script legacy puede omitir firmas)
rssh 'grep -c "CRASHLOG" /tmp/x6200_log.txt 2>/dev/null; \
      ls /mnt/app_logs/x6200_log_*.txt 2>/dev/null | wc -l; \
      tail -n 20 /mnt/app_logs/x6200_log_*.txt 2>/dev/null | tail -n 20'
```

Guardar también `freq`, `mode`, `vfo`, `band`, filtros, volumen, `app` y `tx` del `get`. Si un
campo no se puede leer/restaurar, **no se modifica**.

## 5. HR-03 — navegación RX verificable

Antes de abrir, auditar los handlers contra el SHA instalado (read-only, en el clon):

```bash
grep -RIn 'app_names\|"Status"\|"Settings"\|"Bands"\|"Eq"\|"DFLEditor"' \
  src/remote_server.cpp src/main_screen.c src/dialog_*.c | head -n 80
```

Solo si la auditoría excluye TX y cambios irreversibles, abrir una por una (nunca el barrido
default de 25 apps):

```bash
for app in Status Settings Bands Eq DFLEditor; do
  remote "{\"cmd\":\"app\",\"text\":\"$app\"}" | tee "$OUT/hr03-$app-open.json"
  sleep 20
  bash tools/captura-radio.sh "$OUT/hr03-$app.png"
  remote '{"cmd":"get"}' | tee "$OUT/hr03-$app-get.json"
  remote '{"cmd":"app","text":"off"}' | tee "$OUT/hr03-$app-close.json"
done
```

Verificar que la app observada en el `get` sea la solicitada (no basta `t:ok`):

```bash
python3 - "$OUT" <<'PY'
import json, pathlib, sys
out = pathlib.Path(sys.argv[1])
for p in sorted(out.glob("hr03-*-get.json")):
    d = json.loads(p.read_text())
    print(p.name, "app=", d.get("app"), "tx=", d.get("tx"))
PY
```

## 6. HR-04 — QSY reversible, 20 repeticiones

Solo si el API permite verificar contexto/propiedad y el snapshot es restaurable. Mantener misma
banda/modo/VFO; validar destino antes de mover.

```bash
F0=$(remote '{"cmd":"get"}' | python3 -c 'import json,sys;print(json.load(sys.stdin)["freq"])')
M0=$(remote '{"cmd":"get"}' | python3 -c 'import json,sys;print(json.load(sys.stdin)["mode"])')
B0=$(remote '{"cmd":"get"}' | python3 -c 'import json,sys;print(json.load(sys.stdin)["band"])')
V0=$(remote '{"cmd":"get"}' | python3 -c 'import json,sys;print(json.load(sys.stdin)["vfo"])')
: > "$OUT/hr04-qsy.jsonl"
for i in $(seq 1 20); do
  F1=$((F0 + 100))
  remote "{\"cmd\":\"freq\",\"val\":$F1}" >> "$OUT/hr04-qsy.jsonl"
  remote '{"cmd":"get"}' >> "$OUT/hr04-qsy.jsonl"
  remote "{\"cmd\":\"freq\",\"val\":$F0}" >> "$OUT/hr04-qsy.jsonl"
  remote '{"cmd":"get"}' >> "$OUT/hr04-qsy.jsonl"
done
remote '{"cmd":"get"}' | tee "$OUT/hr04-get-after.json"
```

**PASS:** las 20 vueltas terminan exactamente en `F0`, mismo `M0/B0/V0`, sin guardar memorias ni
ajustes. **FAIL:** deriva, cambio de contexto o frecuencia fuera de banda. **BLOCKED:** el API no
permite verificar contexto; no se mueve el dial.

## 7. HR-05 — 60 min RX con recursos nativos

```bash
PID=$(rssh 'pidof x6200_gui' | tr -d '\r')
: > "$OUT/hr05-get.jsonl"; : > "$OUT/hr05-resources.txt"
for i in $(seq 1 720); do
  remote '{"cmd":"get"}' >> "$OUT/hr05-get.jsonl"
  if [ $((i % 12)) -eq 0 ]; then
    {
      echo "=== t=$((i*5))s utc=$(date -u +%FT%TZ)"
      rssh "cat /proc/$PID/stat; grep -E 'VmRSS|VmHWM' /proc/$PID/status; \
            ls /proc/$PID/fd | wc -l; ls /proc/$PID/task | wc -l; \
            cat /proc/uptime; cat /proc/sys/kernel/random/boot_id; \
            grep -c 'CRASHLOG' /tmp/x6200_log.txt 2>/dev/null; \
            cat /mnt/lastbreath 2>/dev/null; \
            tail -n 3 /mnt/app_logs/boot.log 2>/dev/null"
    } >> "$OUT/hr05-resources.txt"
  fi
  sleep 5
done
```

CPU% de un core se calcula después como
`100*delta(utime+stime)/(CLK_TCK*delta_t_s)`, identificando PID y ámbito nativo. El RTT del
socket/SSH es tiempo del control remoto, **no** latencia de input ni del DSP.

## 8. HR-06 — cierre y restauración

```bash
remote '{"cmd":"get"}' | tee "$OUT/hr06-get-after.json"
# cerrar solo apps abiertas por esta corrida, si el estado sigue siendo propio
remote '{"cmd":"app","text":"off"}' | tee "$OUT/hr06-app-off.json"
bash tools/captura-radio.sh "$OUT/hr06-home-after.png"
diff -u "$OUT/hr02-get-before.json" "$OUT/hr06-get-after.json" | tee "$OUT/hr06-diff.txt" || true
```

Restaurar solo valores propios (freq/modo/VFO/volumen) si el contexto sigue intacto. Si un operador
externo intervino, **abortar sin sobrescribir** y registrar la diferencia.

## 9. Protocolo físico completo (HW-01..HW-08)

Resumen operativo; detalle en [`hardware-plan.csv`](hardware-plan.csv).

| Caso | Qué se ejecuta | PASS | FAIL | BLOCKED |
|---|---|---|---|---|
| **HW-01** identidad/boot | SD de prueba con SHA exacto; 5 fríos + 5 ordenados; video+boot.log | identidad exacta; 1 GUI+1 daemon; sin doble init; ready-to-RX | identidad distinta, doble init, no bootea, no restaura | falta SD/prueba o restore |
| **HW-02** controles/UI | 25 tareas, 6 temas, 20 reps, video 60 fps, luz | sin pérdida/inversión; rotary0 sintoniza; VOL edita DFL; cancel/restore | paso invertido/perdido, foco inaccesible, control muerto | falta actuador/ventana |
| **HW-03** audio/RX/CW | generador, modos, NR/NB/AGC/squelch, Auto Tune ±35 Hz | audio estable, Auto Tune ≤300 Hz y verificación ≤10 Hz | dropout/click, QSY sin señal confiable | falta instrumental |
| **HW-04** TX | autorización específica, carga fantasma, wattmeter/analizador | potencia/banda correctas, stop efectivo, CB gate | RF inesperada, stop fallido, potencia fuera | sin autorización/instrumentos |
| **HW-05** energía/térmica | 3 pares A/B 10 min, 2 min estabilización, 60 min RX | diferencia > incertidumbre sin romper operación | consumo anómalo/sobrecalentamiento | sin medidor calibrado |
| **HW-06** DATA/DB/update | medios de prueba virgen/vfat/ext4; DB corrupta/RO/llena; update/backup/rollback; corte autorizado | DATA ext4 rw, DB robusta, update completo/rollback, backup 5466/5466 | crash-loop, pérdida de datos, update a medias sin rollback | sin medio de prueba o autorización de corte |
| **HW-07** conectividad | CAT/USB, remoto/wfview/web, WiFi/BT/GPS/cluster | reconexión sin crash ni TX; config restaurada | caída/pérdida de estado | falta periférico/red |
| **HW-08** estabilidad 8 h | RX sostenida, 1 s liveness, 60 s recursos | 0 reinicios/crashes/Overflow; sin pérdida | reinicio/crash/overflow | sin ventana de 8 h |

## 10. Criterios transversales y riesgos

**PASS** exige contrato cumplido, evidencia de la capa correcta y estado restaurado. **FAIL** exige
repro y evidencia. **BLOCKED** exige causa externa exacta (no es FAIL del producto).

**Riesgos y mitigación:**

| Riesgo | Mitigación |
|---|---|
| Perder acceso a la radio | boot_id/uptime/LASTBREATH antes; no cerrar apps ajenas; abortar y conservar evidencia |
| Dañar DATA/SD del owner | solo medios de prueba; backup restaurable; nunca formatear la SD operativa |
| Corte de energía deja rootfs a medias | HW-06 solo con autorización, fuente con corte y restore probado |
| TX no autorizada | HW-04 separado, carga fantasma, wattmeter; abortar ante RF inesperada |
| MBR/p3 inconsistente | verificar p3 extremo a extremo post-flash; `parted` por sectores exactos, no `100%` |
| Ruido/ventana corta | no atribuir deltas finos; registrar carga/ambiente; repetir solo si el contrato lo pide |
| Evidencia inventada | cada fila con comando, UTC, exit, log y hash; ausencia = null+motivo |

## 11. Entregables de la fase B

En `docs/evidencia/assessment-rom-194/05-summary/hardware-remote/` (misma rama/PR o uno autorizado):
`results.csv`, `commands.jsonl`, `artifact-manifest.json`, snapshots antes/después, capturas,
logs completos con hashes y `SHA256SUMS`. Integrar los resultados reales en `coverage-final.csv`,
`findings.csv`, `docs/PRUEBAS.md` y `docs/ASSESSMENT_ROM_194.md`. **No** mergear ni aprobar release
sin revisión del maintainer.

## 12. Lo que este plan NO hace

- No despliega, flashea, formatea, corta energía ni transmite por sí solo.
- No reserva la radio ni reemplaza la autorización del owner.
- No convierte un PASS remoto RX en validación instrumental de RF/audio/consumo.
- No autoriza release: el cierre de recolección no es aprobación de producto.
