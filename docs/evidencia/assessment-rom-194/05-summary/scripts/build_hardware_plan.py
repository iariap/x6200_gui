#!/usr/bin/env python3
"""Genera hardware-plan.csv: protocolo fisico (HW-01..HW-08) y fase remota
RX (HR-01..HR-06). Todo nace PENDING_HARDWARE o BLOCKED: no se ejecuto nada
en la radio en esta fase.
"""
from __future__ import annotations

import csv
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE.parent

FIELDS = [
    "id", "priority", "topic", "preconditions", "instrument", "initial_state",
    "steps", "measurement", "expected", "abort", "restore", "evidence",
    "authorization", "status", "blocker",
]

BLOCK_A = (
    "Fase A documental por directiva del orquestador: la radio no se contacto "
    "(sin SSH/socket/RF), no hay sesion exclusiva verificada, ni identidad del "
    "binario instalado, ni snapshot. La fase B la coordina el owner; no es un "
    "fallo del candidato."
)

ROWS = [
    {
        "id": "HW-01", "priority": "P0", "topic": "Identidad de artefacto y boot real",
        "preconditions": "SD/copia de prueba con el artefacto exacto verificado por SHA256; respaldo recuperable de DATA/binario/libs; sesion exclusiva; procedimiento de restore probado; 5 arranques en frio + 5 reinicios ordenados",
        "instrument": "Fuente con interruptor/registrador externo; camara de video; boot.log de S01/S20/main; boot_id/uptime; sha256sum/md5sum; cronometro con origen explicito",
        "initial_state": "Equipo apagado, SD de prueba insertada, DATA respaldada, version/identidad anotadas",
        "steps": "1) Verificar SHA256 de la SD/imagen y del binario instalado; 2) alimentar y filmar desde el instante 0; 3) capturar boot.log, boot_id, uptime, PID de GUI/daemon; 4) repetir 5 frios; 5) 5 reinicios ordenados; 6) primera pantalla, primer waterfall y primera accion RX aceptada; 7) restaurar",
        "measurement": "Tiempo alimentacion->primer pixel; marcas S01/S20/main; ready-to-RX; 1 GUI + 1 daemon; doble init/flicker; boot_id",
        "expected": "Artefacto exacto; 1 GUI + 1 daemon; sin doble init ni bloqueo; ready-to-RX; tiempos con origen explicito (no fijar segundos sin baseline)",
        "abort": "Identidad distinta; arranque que no permite restore; doble init; GUI/daemon ausente; sin via de recuperacion",
        "restore": "Reinstalar la SD respaldada o cambiar a la SD original; verificar DATA y params",
        "evidence": "video de los 10 arranques, boot.log completo, boot_id/uptime, SHA256, dmesg, captura de pantalla",
        "authorization": "Owner: ventana + SD de prueba + restore verificado",
        "status": "PENDING_HARDWARE", "blocker": "Requiere hardware y ventana exclusiva",
    },
    {
        "id": "HW-02", "priority": "P1", "topic": "Controles, touch, encoders, temas y legibilidad",
        "preconditions": "HW-01 PASS; seis temas; luz interior/exterior y brillo anotados; estado inicial home; snapshot de freq/modo/VFO/volumen/app",
        "instrument": "Dedo + teclas cortas/largas + rotary0/1/2/3 reales; camara de alta velocidad (o 60 fps) para input-to-feedback; luxometro/medicion de luz",
        "initial_state": "Home RX, DFL cerrado, tema y brillo registrados",
        "steps": "Ejecutar las 25 tareas de 03-ui-ux/operator-tasks.csv; 20 repeticiones por giro/edicion/cancelacion representativa; seis temas; video con frecuencia/valor y accion visibles; registrar errores de seleccion",
        "measurement": "Latencia input->feedback por frames de video; detents vs acciones; errores de seleccion; contraste/legibilidad al sol",
        "expected": "Sin perdida/inversion de pasos fuera de contrato; rotary0 sintoniza; VOL edita DFL cuando corresponde; foco accesible; cancel/restore previsible; contraste analitico no sustituye LCD al sol",
        "abort": "Control que no responde o cambia estado irreversible; no poder restaurar; lectura ilegible que impide operar",
        "restore": "Cerrar apps abiertas; volver a freq/modo/VFO/volumen/tema/brillo del snapshot; home",
        "evidence": "video con frames, capturas, planilla de 20 reps, luxometria, snapshot antes/despues",
        "authorization": "Owner: sesion exclusiva y ventana",
        "status": "PENDING_HARDWARE", "blocker": "Requiere actuacion fisica y ventana",
    },
    {
        "id": "HW-03", "priority": "P1", "topic": "Audio analogico, DSP, RX y CW/Auto Tune",
        "preconditions": "HW-01 PASS; generador/grabacion de referencia a entrada adecuada; interfaz/analizador de audio aislado; snapshot de audio/mixer/modo/filtros/AGC/NR/NB/squelch",
        "instrument": "Generador RF o grabacion calibrada; analizador de audio/interfaz; loopback snd-aloop o /dev/snd real; medidor de nivel",
        "initial_state": "RX home, modo/filtro/AGC/NR/NB/squelch/volumen/presets anotados",
        "steps": "Ruido/silencio y senal por USB/LSB/AM/NFM/CW/CWR; NR/NB/AGC/squelch/presets; decoder AUTO ON/OFF; Auto Tune con senales +/-35 Hz del pitch, debiles/intermitentes y QRM; medir tasa de muestras, RMS/pico, latencia y dropouts",
        "measurement": "RMS/pico dBFS; tasa de muestras; latencia entrada->salida definida; clicks/dropouts/underruns; correccion Auto Tune (Hz) y verificacion",
        "expected": "Audio audible y estable; sin clicks/dropouts atribuibles al firmware; Auto Tune: sin senal confiable no QSY, maximo una correccion <=300 Hz, exito solo con verificacion <=10 Hz; dial/CAT/PTT prevalecen",
        "abort": "TX inesperada, realimentacion/audio destructivo, perdida de control o de estado",
        "restore": "Restaurar mixer/volumen/modo/filtro/AGC/presets desde snapshot; home RX",
        "evidence": "WAV/FLAC con hash, capturas de analizador, latencia, tabla de casos, video de Auto Tune",
        "authorization": "Owner: ventana; senal RF solo si esta autorizada",
        "status": "PENDING_HARDWARE", "blocker": "Requiere instrumental de audio/RF",
    },
    {
        "id": "HW-04", "priority": "P1", "topic": "TX, PTT, keyer, ATU, SWRScan e interlocks (separado)",
        "preconditions": "Autorizacion futura especifica y escrita del owner; carga fantasma adecuada; wattmeter/analizador; banda/frecuencia/potencia compatibles con contrato/hardware fijadas antes de empezar; RX seguro verificado",
        "instrument": "Carga fantasma, wattmeter, analizador de espectro, medidor de ROE, llave/PTT",
        "initial_state": "RX, potencia minima/consentida, banda/frecuencia anotadas, ATU/SWR en estado conocido",
        "steps": "PTT micro, CW keyer, ATU, SWRScan y transmisores digitales: start/stop/cancel, maximos y restauracion; CB 26.515 bloqueado vs 27.185 permitido y 28.074 fuera del gate CB; verificar potencia real; nunca emitir al aire",
        "measurement": "Potencia W, armonicos, ROE, tiempos de start/stop, estado de interlocks, restauracion",
        "expected": "TX solo en condiciones permitidas; stop/cancel efectivo; potencia y banda dentro del contrato; CB gate correcto; sin emision fuera de autorizacion",
        "abort": "RF inesperada, stop fallido, potencia/banda fuera de limite: abortar, pasar a RX seguro y conservar evidencia",
        "restore": "RX, potencia/ATU/SWR al estado inicial, verificar dial/CAT",
        "evidence": "video, wattmeter/analizador, logs, estado antes/despues",
        "authorization": "Owner: autorizacion TX especifica e instrumental",
        "status": "PENDING_HARDWARE", "blocker": "Requiere autorizacion TX e instrumentos; no se ejecuta en fase A/B RX",
    },
    {
        "id": "HW-05", "priority": "P1", "topic": "Energia, consumo y termica",
        "preconditions": "Alimentacion estabilizada; medidor externo V/I/P con precision/calibracion/sampling registrados; 2 min de estabilizacion por estado; WiFi/BT/volumen/brillo/frecuencia/modo/senal constantes",
        "instrument": "Fuente de banco + medidor (INA219 o shunt calibrado); termometro/sensores SoC; muestreo nativo 1 s",
        "initial_state": "RX home normal, bateria/carga/fuente anotadas, temperatura ambiente",
        "steps": "Tres pares A/B alternados de 10 min: RX home normal vs ECO; display on vs off; apps cerradas vs carga RX/digital equivalente; registrar CPU/wakeups/RSS/PSS/IO nativos en paralelo; 60 min RX sostenida para termica",
        "measurement": "W media, mA a voltaje anotado, Wh=integral P*dt/3600, dispersion, diferencia emparejada con incertidumbre, temperatura CPU/PA",
        "expected": "Ahorro solo si la diferencia excede la incertidumbre y no rompe operacion; sin presupuesto fisico previo no inventar umbral de mA ni % autonomia",
        "abort": "Voltaje/corriente anomala, sobrecalentamiento, reinicio o intervencion del owner",
        "restore": "Volver a settings/estado inicial; desconectar instrumentos",
        "evidence": "CSV crudo V/I/P, temperatura, potencia integrada, video del estado",
        "authorization": "Owner: fuente/instrumentos y ventana",
        "status": "PENDING_HARDWARE", "blocker": "Requiere medidor calibrado y ventana",
    },
    {
        "id": "HW-06", "priority": "P0", "topic": "Persistencia, DATA, DB y update/rollback con corte de energia",
        "preconditions": "Medios de prueba separados para DATA virgen, vfat existente y ext4 existente; backup restaurable verificado; autorizacion para interrupcion controlada; NUNCA la SD/DATA del owner",
        "instrument": "SD/imagen de prueba, fuente con corte controlado, sqlite3, e2fsck, sha256sum, copias de params/IDs/memorias/favoritos/logs",
        "initial_state": "Medio de prueba identificado por hash; DATA respaldada; estado de params/IDs/memorias/logs inventariado",
        "steps": "1) Primer boot en tarjeta nueva: tipo/montaje de DATA, params, reinicio; 2) DATA vfat/ext4 existente: montaje y persistencia; 3) DB corrupta/RO/llena en copia; 4) update/backup/rollback: primera update vs overlay posterior; 5) una interrupcion controlada por escenario (corte de energia) solo con autorizacion; 6) restaurar y verificar",
        "measurement": "Tipo/montaje rw, PRAGMA integrity_check de copias, params/IDs/memorias/favoritos/logs antes/despues, LASTBREATH, entradas de backup (5466), boot recuperado, perdida de datos",
        "expected": "DATA ext4 rw desde el primer boot; DB robusta (sin crash-loop) o error controlado; update completo o rollback; backup 5466/5466; sin reformateo inesperado; boot recuperable tras corte",
        "abort": "No hay backup/restore verificado; se esta por tocar la SD del owner; el equipo no vuelve a bootear",
        "restore": "Reimagen del medio de prueba; restaurar DATA desde backup; verificar UUID/params",
        "evidence": "logs de boot, findmnt/blkid, integrity_check, hashes, video del corte, backup/restore",
        "authorization": "Owner: medios de prueba + autorizacion de corte; no la SD operativa",
        "status": "PENDING_HARDWARE", "blocker": "Requiere medios de prueba y autorizacion de corte",
    },
    {
        "id": "HW-07", "priority": "P2", "topic": "Conectividad: CAT/USB, remoto/wfview/web, WiFi/BT/GPS/cluster",
        "preconditions": "HW-01 PASS; version del cliente/periferico/red documentada; snapshot de configuracion; sin TX",
        "instrument": "PC/cliente, cable USB/serial, router/AP, GPS/BT reales, logs de red",
        "initial_state": "Configuracion de red/CAT/BT/GPS anotada; servicios arriba",
        "steps": "CAT/USB; remoto localhost; wfview; webserver; WiFi scan/reconnect; BT pair/audio; GPS; cluster; perdida y recuperacion de enlace",
        "measurement": "Conexion/reconexion, latencia de control, errores, estado de servicios, ausencia de TX",
        "expected": "Reconexion sin crash ni TX; comandos responden; configuracion restaurada",
        "abort": "TX inesperada, perdida de estado, caida de GUI",
        "restore": "Restaurar red/CAT/BT/GPS/config desde snapshot",
        "evidence": "logs, capturas, versiones, tablas de reconexion",
        "authorization": "Owner: red/ventana; sin TX",
        "status": "PENDING_HARDWARE", "blocker": "Requiere perifericos/red reales",
    },
    {
        "id": "HW-08", "priority": "P1", "topic": "Estabilidad 8 h RX",
        "preconditions": "HW-01 PASS; carga RX reproducible; alimentacion monitorizada; sesion exclusiva; snapshot",
        "instrument": "Fuente/medidor, reloj, muestreo /proc 1 s/60 s, logs",
        "initial_state": "RX home con parametros iniciales; boot_id/PID/uptime/DB hash anotados",
        "steps": "8 h RX; senal de vida cada 1 s; recursos cada 60 s; estado/uptime/boot_id/PID/CRASHLOG/LASTBREATH/colas/fds/threads/DB; conservar primer fallo",
        "measurement": "Reinicios/crashes/Overflow, crecimiento de logs/RSS/fds/threads, integridad de DB, continuidad de control",
        "expected": "Cero reinicios/crashes/Overflow en la ventana; sin perdida de controles ni datos; una corrida limpia no demuestra ausencia de defectos",
        "abort": "Perdida de enlace, reinicio, filesystem ro o intervencion externa: detener y conservar evidencia; no repetir para disimular",
        "restore": "Home RX y estado inicial; no matar GUI/daemon",
        "evidence": "serie temporal 1 s/60 s, logs completos, capturas antes/despues",
        "authorization": "Owner: ventana de 8 h",
        "status": "PENDING_HARDWARE", "blocker": "Requiere 8 h de ventana exclusiva",
    },
    # ------------------------------------------------------------------
    # Fase remota RX (issue #199). En fase A no se contacto la radio.
    # ------------------------------------------------------------------
    {
        "id": "HR-01", "priority": "P0", "topic": "Exclusividad, identidad y lectura inicial (remoto RX)",
        "preconditions": "Sesion exclusiva confirmada; cliente auxiliar disponible; radio en red; autorizacion de lectura",
        "instrument": "SSH/sshpass, ss, /proc, sha256sum/md5sum, x6200-remote, boot.log",
        "initial_state": "Sin sesion de otro agente; radio idle; estado del operador anotado",
        "steps": "Descubrir IP con radio_ip; verificar conexiones SSH y procesos; leer uptime/boot_id/PID/binario/libs/fuente/montajes/espacio/LASTBREATH/logs; `get` inicial a JSON crudo",
        "measurement": "Identidad SHA256/md5; boot_id/uptime; montajes; estado; logs",
        "expected": "Sesion exclusiva; identidad demostrable; sin TX/app operativa; snapshot inicial",
        "abort": "TX activa, app operativa abierta, identidad no demostrable o falta de exclusividad: BLOCKED sin cerrar apps",
        "restore": "No aplica (solo lectura)",
        "evidence": "05-summary/hardware-remote/results.csv, JSON crudo, capturas, logs",
        "authorization": "Owner/orquestador: fase B",
        "status": "BLOCKED", "blocker": BLOCK_A,
    },
    {
        "id": "HR-02", "priority": "P0", "topic": "Disponibilidad y baseline fisica (remoto RX)",
        "preconditions": "HR-01 PASS",
        "instrument": "tools/verifica-radio.sh, tools/captura-radio.sh, get",
        "initial_state": "Home RX; snapshot HR-01",
        "steps": "verifica-radio; CRASHLOG real; app_logs nuevos; captura home-before.png; snapshot freq/mode/VFO/banda/filtros/volumen/app/tx",
        "measurement": "Exit/salida de verificacion; captura valida; valores modificables",
        "expected": "GUI/daemon vivos; assets/input/audio OK; sin crash; snapshot completo",
        "abort": "Filesystem ro, GUI caida, perdida de enlace",
        "restore": "No aplica si no se modifico",
        "evidence": "logs, PNG, JSON",
        "authorization": "Owner/orquestador: fase B",
        "status": "BLOCKED", "blocker": BLOCK_A,
    },
    {
        "id": "HR-03", "priority": "P1", "topic": "Navegacion RX verificable (remoto RX)",
        "preconditions": "HR-02 PASS; auditoria de handlers de Status/Settings/Bands/Eq/DFLEditor sin TX",
        "instrument": "x6200-remote, captura-radio, get, app_logs",
        "initial_state": "Home; apps cerradas",
        "steps": "Abrir/capturar/get/cerrar una por una con 20 s de permanencia; verificar app observada; auditar precondiciones",
        "measurement": "app solicitada vs observada; PNG valido; PID estable; sin TX",
        "expected": "Navegacion estable; sin crash ni cambio irreversible",
        "abort": "TX, crash, app que no cierra, cambio de estado no restaurable",
        "restore": "Cerrar solo apps abiertas por la corrida; home",
        "evidence": "capturas/JSON/logs por app",
        "authorization": "Owner/orquestador: fase B",
        "status": "BLOCKED", "blocker": BLOCK_A,
    },
    {
        "id": "HR-04", "priority": "P1", "topic": "Operacion RX reversible, 20 repeticiones (remoto RX)",
        "preconditions": "HR-02 PASS; API permite verificar contexto/propiedad de freq; snapshot restaurable",
        "instrument": "x6200-remote, python para parsear get, captura",
        "initial_state": "F0/modo/banda/VFO conocidos",
        "steps": "get -> freq=F0+100 -> get -> freq=F0 -> get, 20 veces; validar destino antes; cancel/close redundante y home",
        "measurement": "Freq esperada/observada; contexto; captura",
        "expected": "Vuelve exactamente a F0; mismo modo/banda/VFO; sin guardar ajustes/memorias",
        "abort": "API no permite verificar contexto; frecuencia fuera de banda; cambio de estado",
        "restore": "freq=F0 y contexto original; home",
        "evidence": "JSON de las 20 reps, capturas",
        "authorization": "Owner/orquestador: fase B",
        "status": "BLOCKED", "blocker": BLOCK_A,
    },
    {
        "id": "HR-05", "priority": "P1", "topic": "Estabilidad y recursos nativos 60 min RX (remoto RX)",
        "preconditions": "HR-02 PASS; sesion exclusiva",
        "instrument": "get cada 5 s; /proc cada 60 s; CRASHLOG/LASTBREATH/logs",
        "initial_state": "Home con parametros iniciales",
        "steps": "60 min RX; get cada 5 s; cada 60 s PID/boot_id/uptime/CRASHLOG/Overflow/LASTBREATH/CPU/RSS/PSS/fds/threads/logs",
        "measurement": "CPU%=100*delta(utime+stime)/(CLK_TCK*delta_t); RSS/PSS; fds/threads; crecimiento de logs; RTT de control (no latencia DSP)",
        "expected": "Sin reinicios/crashes/Overflow; recursos estables; DB intacta",
        "abort": "Perdida de enlace, reinicio, filesystem ro, intervencion externa",
        "restore": "Home; no matar GUI/daemon",
        "evidence": "series crudas, logs, capturas",
        "authorization": "Owner/orquestador: fase B",
        "status": "BLOCKED", "blocker": BLOCK_A,
    },
    {
        "id": "HR-06", "priority": "P1", "topic": "Cierre y restauracion (remoto RX)",
        "preconditions": "HR-03/04/05 ejecutados",
        "instrument": "get, captura, snapshot",
        "initial_state": "Estado inicial HR-02",
        "steps": "Comparar con snapshot; cerrar solo apps abiertas; restaurar solo valores propios si contexto/propiedad intactos; home-after.png y get final",
        "measurement": "Diferencias y restauraciones fallidas",
        "expected": "Estado identico al inicial o diferencias justificadas; home RX",
        "abort": "Operador externo interviene: abortar sin sobrescribir",
        "restore": "Aplicar snapshot solo si es propio y seguro",
        "evidence": "JSON/PNG antes/despues, tabla de diferencias",
        "authorization": "Owner/orquestador: fase B",
        "status": "BLOCKED", "blocker": BLOCK_A,
    },
]


def main() -> int:
    with open(OUT / "hardware-plan.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS, lineterminator="\n")
        w.writeheader()
        w.writerows(ROWS)
    from collections import Counter
    print("cases", len(ROWS))
    print("status", dict(Counter(r["status"] for r in ROWS)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
