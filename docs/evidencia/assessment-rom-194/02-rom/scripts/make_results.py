#!/usr/bin/env python3
"""Genera results.csv y reliability.csv de la unidad #196 a partir de la evidencia.

Lee los JSONL de estrés/reposo/negativas y emite una fila por métrica con el contrato
de la épica #194. No inventa valores: lo que no se midió queda con status BLOCKED o
UNSUPPORTED y limitación explícita.
"""
import csv
import json
import re
import statistics
import sys
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
CAP = BASE / "capturas"

COLS = ["scenario_id", "layer", "coverage", "preconditions", "actions", "expected",
        "observed", "status", "metric", "value", "unit", "samples", "evidence", "limitation"]

rows = []


def add(sid, layer, cov, pre, act, exp, obs, status, metric, value, unit, samples, ev, lim=""):
    rows.append(dict(zip(COLS, [sid, layer, cov, pre, act, exp, obs, status, metric, value,
                                unit, samples, ev, lim])))


# ---------------------------------------------------------------- ROM-01
H = "HOST"
Q = "QEMU_USER"
add("ROM-01-SHA", H, "REAL", "artefacto #195 en clone-buildroot/build/images",
    "sha256sum de sdcard.img/rootfs.tar/rootfs.ext4/boot.vfat/zImage/dtb/u-boot",
    "coincidir con artifact-manifest.json de #195", "7/7 SHA256 idénticos", "PASS",
    "sha256_match", 7, "archivos", 7, "logs/rom01/sha256.log")
add("ROM-01-IMG-SIZE", H, "REAL", "misma imagen", "stat -c%s sdcard.img",
    "571473920 B", "571473920 B", "PASS", "image_bytes", 571473920, "bytes", 1,
    "logs/rom01/sha256.log")
add("ROM-01-PART-COUNT", H, "REAL", "imagen", "sfdisk --json sdcard.img",
    "2 particiones (boot+rootfs), sin p3", "2 particiones, tabla dos", "PASS",
    "partitions", 2, "particiones", 1, "logs/rom01/layout.log")
add("ROM-01-P1", H, "REAL", "imagen", "sfdisk/parted", "p1 inicio 1 MiB, 32 MiB, bootable",
    "inicio 1048576 B, 33554432 B, type 0x0c, bootable", "PASS", "p1_start_bytes", 1048576,
    "bytes", 1, "capturas/rom01-layout.json")
add("ROM-01-P2", H, "REAL", "imagen", "sfdisk/parted", "p2 inicio 33 MiB, 512 MiB ext4",
    "inicio 34603008 B, 536870912 B, type 0x83", "PASS", "p2_start_bytes", 34603008,
    "bytes", 1, "capturas/rom01-layout.json")
add("ROM-01-RELEASE-OFFSETS", H, "REAL", "tools/release scripts", "comparar 33 MiB/512 MiB",
    "coincidir con layout real", "coincide exacto", "PASS", "release_offsets_match", 1,
    "bool", 1, "capturas/rom01-layout.json", "no se ejecutó el dd del release")
add("ROM-01-SFDISK-VERIFY", H, "REAL", "imagen", "sfdisk --verify", "sin errores",
    "No errors detected, rc=0", "PASS", "sfdisk_verify_rc", 0, "exit", 1,
    "logs/rom01/sfdisk-verify.log")
add("ROM-01-UBOOT", H, "REAL", "imagen", "dd offset 8K + cmp", "u-boot en 8 KiB idéntico",
    "cmp OK, U-Boot 2023.01", "PASS", "uboot_offset_bytes", 8192, "bytes", 1,
    "logs/rom01/extract-partitions.log")
add("ROM-01-BOOTFS", H, "REAL", "p1 extraída", "mdir + fsck.fat -n", "3 archivos, fsck limpio",
    "3 archivos (zImage, dtb, boot.scr), fsck 0", "PASS", "bootfs_files", 3, "archivos", 1,
    "logs/rom01/bootfs-inventory.log")
add("ROM-01-BOOTARGS", H, "REAL", "boot.scr", "strings boot.scr", "root=/dev/mmcblk0p2",
    "root=/dev/mmcblk0p2 rootwait panic=10", "PASS", "root_arg", "/dev/mmcblk0p2", "device",
    1, "logs/rom01/boot-artifacts.log")
add("ROM-01-FSCK", H, "REAL", "p2 extraída por offset", "e2fsck -fn p2-rootfs.ext4",
    "exit 0 sin errores", "exit 0, 5375/32768 inodos, 65878/131072 bloques", "PASS",
    "e2fsck_exit", 0, "exit", 1, "logs/rom01/e2fsck.log")
add("ROM-01-INODES", H, "REAL", "p2 extraída", "dumpe2fs -h", "medir inodos usados",
    "5375 usados de 32768 (16.4%)", "PASS", "inodes_used", 5375, "inodos", 1,
    "logs/rom01/e2fsck.log")
add("ROM-01-BLOCKS", H, "REAL", "p2 extraída", "dumpe2fs -h", "medir bytes usados/libres",
    "65878 usados / 65194 libres, usado 269836288 B, libre 267034624 B", "PASS",
    "bytes_free", 267034624, "bytes", 1, "logs/rom01/e2fsck.log")
add("ROM-01-SETUID", H, "REAL", "rootfs.tar", "tar -tvf + auditoría", "inventariar setuid",
    "3 setuid (busybox, dbus helper, ssh-keysign)", "PASS", "setuid_files", 3, "archivos", 1,
    "capturas/rootfs-audit.json")
add("ROM-01-SYMLINKS", H, "REAL", "rootfs.tar", "resolver symlinks dentro del archivo",
    "0 enlaces rotos funcionales", "11 no resolubles en archivo: 9 provistos en boot, 2 dev ICU",
    "PASS", "broken_symlinks", 0, "enlaces", 11, "capturas/rootfs-audit.json",
    "clasificación estática; no se arrancó el rootfs real")
add("ROM-01-PACKAGES", H, "REAL", "build de #195", "stamps .stamp_target_installed",
    "inventariar paquetes", "142 paquetes", "PASS", "packages", 142, "paquetes", 1,
    "capturas/packages-installed.txt", "lista del build, no hay DB de paquetes en la rootfs")
add("ROM-01-LIBS", H, "REAL", "rootfs.tar", "auditoría de .so", "inventariar bibliotecas",
    "363 bibliotecas", "PASS", "shared_libs", 363, "bibliotecas", 1,
    "capturas/rootfs-libs.txt")
add("ROM-01-INITD", H, "REAL", "rootfs.tar", "listar etc/init.d", "inventariar servicios",
    "26 entradas (24 S* + rcS/rcK)", "PASS", "init_scripts", 26, "scripts", 1,
    "capturas/rootfs-initd.txt")
add("ROM-01-GUI-MD5", H, "REAL", "rootfs.tar y p2", "md5sum/debugfs dump", "idéntico al manifiesto",
    "md5 1f998c46909307397525c975a712adfd en ambas vías", "PASS", "gui_md5_match", 1, "bool",
    2, "logs/rom01/boot-artifacts.log")
add("ROM-01-SRC-MD5", H, "REAL", "clone-gui c888308e", "sh tools/fuente-md5.sh .",
    "964193ae9306b5127845cec44a57d52c", "964193ae9306b5127845cec44a57d52c", "PASS",
    "source_md5", "964193ae9306b5127845cec44a57d52c", "md5", 1, "logs/rom01/gui-elf.log")
add("ROM-01-NEEDED", H, "REAL", "GUI del rootfs", "readelf -d", "contar NEEDED", "28 NEEDED",
    "PASS", "needed_libs", 28, "bibliotecas", 1, "logs/rom01/gui-elf.log")
add("ROM-01-GATE86", "HOST", "REAL", "rootfs.tar del candidato", "verifica-firmware-port.py --tar",
    "gate #86 PASS", "exit 0: ELF32 ARM 13.407.624 B, 3/3 marcas", "PASS", "gate86_exit", 0,
    "exit", 1, "logs/rom01/gate86.log")
add("ROM-01-LOADER", "QEMU_USER", "REAL", "rootfs extraída + qemu-arm-static",
    "ld-linux-armhf.so.3 --list x6200_gui con QEMU_LD_PREFIX", "0 faltantes, exit 0",
    "47 bibliotecas resueltas, 0 not found, exit 0", "PASS", "loader_missing", 0,
    "bibliotecas", 47, "logs/rom01/gui-ldd-qemu.log")

# ---------------------------------------------------------------- ROM-02
add("ROM-02-PRUNE", H, "REAL", "clone-gui limpio", "ejecutar bloque run de boot-rootfs.yml",
    "S20gui queda, S95gui no, S99vendor queda, guards rc!=0", "todo OK; 4/4 guards rc=2 sin borrar",
    "PASS", "prune_workflow_exit", 0, "exit", 1, "logs/rom02/prune-workflow.log")
add("ROM-02-S20GUI", H, "REAL", "rootfs.tar", "listar etc/init.d", "S20gui presente", "presente",
    "PASS", "s20gui_present", 1, "bool", 1, "logs/rom02/image-services.log")
add("ROM-02-S95GUI", H, "REAL", "rootfs.tar", "buscar S95gui", "ausente", "0 coincidencias",
    "PASS", "s95gui_present", 0, "bool", 1, "logs/rom02/image-services.log")
add("ROM-02-SH-SYNTAX", "QEMU_USER", "REAL", "intérprete real /bin/sh -> bash 5.2.15 de la imagen",
    "sh -n de los 26 scripts con el binario ARM real", "todos parsean",
    "26/26 rc=0 (S04 con [[ ]] also parsea)", "PASS", "init_scripts_parse_ok", 26, "scripts", 26,
    "logs/rom02/sh-syntax-real.log")
add("ROM-02-BASH-DEP", "QEMU_USER", "REAL", "BusyBox ash de la imagen (control negativo)",
    "busybox sh -n S01/S04", "documentar dependencia de bash",
    "BusyBox ash falla S01 (arrays) y S04 ([[ ]] then); /bin/sh es bash", "REVIEW_REQUIRED",
    "bash_dependency_failures", 2, "scripts", 26, "logs/rom02/sh-syntax.log",
    "no es fallo actual; ata el arranque a que bash siga en la imagen")
add("ROM-02-S04-EXEC", "QEMU_USER", "REAL", "bash 5.2.15 de la imagen", "snippet if [[ ! -d ]] then",
    "semántica correcta", "mkdir condicional ejecuta OK", "PASS", "s04_bash_semantics", 1,
    "bool", 1, "logs/rom02/sh-syntax-real.log")
add("ROM-02-UHID", H, "REAL", "kernel 6.1.82 del Buildroot congelado", "revisar CONFIG_UHID y modprobe uhid",
    "soporte de teclados BLE/HOGP", "CONFIG_UHID no seteado, sin uhid.ko; modprobe no-op", "REVIEW_REQUIRED",
    "uhid_built", 0, "bool", 1, "logs/rom02/bt-modules.log", "no es un crash; brecha funcional de BT")
add("ROM-02-WATCHDOG", H, "REAL", "rootfs.tar", "grep init.d/config watchdog", "watchdog armado",
    "0 servicios; applet busybox sin uso; wrapper GUI while true sin backoff", "REVIEW_REQUIRED",
    "watchdog_services", 0, "servicios", 1, "logs/rom02/image-services.log",
    "no se arrancó init real")
add("ROM-02-BOOT", None, "UNSUPPORTED", "sin QEMU system ni hardware", "boot kernel/init",
    "medir arranque real", "no ejecutado", "PENDING_HARDWARE", "boot_tested", 0, "bool", 0,
    "logs/rom02/init-scripts.log", "requiere hardware o QEMU system; #199")

# ---------------------------------------------------------------- ROM-03
add("ROM-03-FSTAB", H, "REAL", "rootfs.tar", "leer /etc/fstab", "/mnt ext4",
    "/dev/mmcblk0p3 /mnt ext4 rw 0 2", "PASS", "mnt_fstype", "ext4", "tipo", 1,
    "logs/rom02/key-files.log")
add("ROM-03-GENIMAGE", H, "REAL", "Buildroot d084e287", "leer genimage.cfg", "sólo boot+rootfs",
    "2 particiones; sin DATA", "PASS", "genimage_partitions", 2, "particiones", 1,
    "capturas/rom01-layout.json")
add("ROM-03-S01-PARSE", "QEMU_USER", "REAL", "intérprete real /bin/sh -> bash", "sh -n S01create_data",
    "rc=0", "rc=0; arrays bash disponibles", "PASS", "s01create_parse_rc", 0, "exit", 1,
    "logs/rom02/sh-syntax-real.log")
add("ROM-03-NEW-CARD", H, "PARTIAL", "tarjeta sin p3; copia privada agrandada a 2 GiB",
    "trazar inittab/fstab/S01 y resolver mount por fstab",
    "crear DATA ext4 y montar /mnt en el primer boot",
    "crea p3 vfat; mount /dev/mmcblk0p3 resuelve ext4 y falla; primer boot escribe en rootfs; segundo boot monta vfat",
    "FAIL", "new_card_first_boot_mounted", 0, "bool", 1, "logs/rom03/fstab-resolve.log",
    "no se ejecutó contra /dev/mmcblk0; código + bash real + fstab/libmount en host")
add("ROM-03-VFAT-CARD", H, "PARTIAL", "p3 DATA vfat existente", "trazar mount -a y rama existingData",
    "montar DATA vfat con su tipo", "mount -a falla ext4; S01 monta -t vfat", "PASS",
    "vfat_card_mounted", 1, "bool", 1, "logs/rom03/fstab-resolve.log",
    "camino estático; no se probó en tarjeta física")
add("ROM-03-EXT4-CARD", H, "PARTIAL", "p3 DATA ext4 existente", "trazar inittab mount -a",
    "montar /mnt ext4", "mount -a (inittab) monta ext4 antes de rcS", "PASS",
    "ext4_card_mounted", 1, "bool", 1, "logs/rom02/init-scripts.log",
    "camino estático; observado históricamente en la radio del owner")
add("ROM-03-MKFS-VFAT", "QEMU_USER", "REAL", "mkfs.fat de la imagen sobre archivo privado de 16 MiB",
    "qemu-arm-static mkfs.fat -n DATA", "crear vfat con label DATA", "rc=0; label DATA; blkid TYPE=vfat",
    "PASS", "mkfs_vfat_label", "DATA", "label", 1, "logs/rom03/mkfs-vfat.log",
    "paso de formato aislado; no se formateó ningún dispositivo")
add("ROM-03-GUARD-FAT32", H, "PARTIAL", "S01create_data línea 22", "parted print | grep fat32",
    "detectar una DATA fat32 preexistente sin confundir boot",
    "p1 es fat16: rc=1 y sigue; si el boot fuera FAT32 saldría sin crear DATA", "REVIEW_REQUIRED",
    "guard_fat32_rc", 1, "exit", 1, "logs/rom03/fstab-resolve.log",
    "guard frágil; el artefacto actual no lo dispara")
add("ROM-03-TOOLS-PATH", H, "PARTIAL", "rootfs", "ubicación de parted/partprobe/blkid/mkfs",
    "herramientas resolubles desde el init",
    "parted/partprobe en /usr/sbin; BusyBox trae PATH default /sbin:/usr/sbin:/bin:/usr/bin",
    "PASS", "parted_path", "/usr/sbin/parted", "path", 2,
    "logs/rom03/fstab-resolve.log", "PATH del init no verificado en boot real")
add("ROM-03-PERSIST-RESTART", "QEMU_USER", "PARTIAL", "DB post-estrés en chroot privado",
    "reiniciar el proceso GUI y leer el estado", "restaurar freq/modo/banda persistidos",
    "freq=14074000 mode=CW band=20m SSB filter=500; hash de DB sin cambios; 0 CRASHLOG", "PASS",
    "persisted_freq", 14074000, "Hz", 1, "logs/rom03/persist-restart.log",
    "reinicio de proceso, no corte de energía")
add("ROM-03-DB", H, "REAL", "params.default.db del rootfs", "PRAGMA integrity_check",
    "ok", "ok; 13 tablas + sqlite_sequence; version=1", "PASS", "db_integrity", "ok", "texto",
    1, "logs/rom03/db-integrity.log")
add("ROM-03-MIGRATION2", H, "PARTIAL", "src/params/migrations.c", "revisar idempotencia",
    "migraciones idempotentes", "migración 2 multiplica freq si version se pierde", "REVIEW_REQUIRED",
    "migration2_idempotent", 0, "bool", 1, "logs/rom03/db-integrity.log",
    "no se inyectó pérdida de version en DATA real")

# ---------------------------------------------------------------- ROM-04
add("ROM-04-HOST-SUDO", H, "REAL", "host sin new privileges", "sudo -n true",
    "root en el host", "no disponible; se usó root de un contenedor privado con montajes privados",
    "BLOCKED", "host_sudo", 0, "bool", 1, "logs/rom04/update-nonroot.log",
    "el camino de permisos se preservó con extracción root real, no se sustituyó")
add("ROM-04-MAKE-TGZ", H, "REAL", "root en contenedor privado, rootfs.tar del candidato",
    "make_update_tgz.sh rootfs.tar out.tgz", "empaquetar y pasar sus asserts",
    "tgz 102736632 B, 5466 entradas, 0 dirs, 0 ./; exit 1 por setuid 3 vs 5", "FAIL",
    "make_update_tgz_exit", 1, "exit", 1, "logs/rom04/make-update-container.log",
    "no es SD/hardware; el tgz se generó igual")
add("ROM-04-SUID-ASSERT", H, "REAL", "tgz generado", "contar setuid del tgz",
    "5 según el script", "3 reales → exit 1 'se perdieron bits setuid'", "FAIL",
    "setuid_expected_vs_actual", "5_vs_3", "conteo", 1, "logs/rom04/make-update-container.log")
add("ROM-04-OVERLAY-OWNER", H, "REAL", "tgz generado", "tar -tvzf del S02update del overlay",
    "rootfs 0/0", "etc/init.d/S02update 1000/1000 (uid 1000 = pulse); confirmado tras extraer con BusyBox tar del target", "FAIL",
    "overlay_owner", "1000:1000", "owner", 1, "logs/rom04/overlay-owner-target.log",
    "la extracción root preserva el owner; el assert del script no lo revisa")
add("ROM-04-TGZ-IDENTITY", H, "REAL", "tgz generado", "extraer GUI y S02update del tgz y hashear",
    "GUI y overlay idénticos a la imagen/fuente",
    "GUI 7e2e9331… = imagen; S02update d3c7d76b… = overlay", "PASS", "tgz_identity", 1,
    "bool", 2, "logs/rom04/make-update-container.log", "verificado por tar -xOzf")
add("ROM-04-TEST-UPDATE", Q, "REAL", "root en contenedor privado, chroot ARM bajo binfmt",
    "test_update.sh rootfs.tar tgz T", "aplicar update y verificar backup/GUI",
    "exit 0; backup 5466 entradas; GUI 7e2e9331…; tgz consumido; setuid 3", "PASS",
    "test_update_exit", 0, "exit", 1, "logs/rom04/test-update-container.log",
    "preinstala el overlay: prueba el camino posterior, no la primera actualización")
add("ROM-04-FIRST-UPDATE", "QEMU_USER", "REAL", "chroot privado con el rootfs de la imagen y el S02update stock",
    "S02update start con el tgz en /mnt", "actualizar y respaldar completo",
    "rc=0; update aplicado; backup 102431284 B pero solo 706/5466 entradas legibles (xargs BusyBox)", "FAIL",
    "first_update_backup_entries", 706, "entradas", 1, "logs/rom04/first-update-exec.log",
    "una emulación con xargs de GNU dio 3464; el número del target es 706")
add("ROM-04-BACKUP-FIXED", H, "REAL", "lista + un solo tar (overlay)", "emular backup corregido",
    "backup completo", "5466/5466 entradas, GUI incluida", "PASS", "fixed_backup_entries", 5466,
    "entradas", 1, "logs/rom04/test-update-container.log")
add("ROM-04-INTEGRITY", H, "REAL", "S02update imagen/overlay", "revisar verificación del tgz",
    "checksum/firma antes de extraer", "no verifica; extrae sobre / y borra el tgz", "FAIL",
    "update_checksum", 0, "bool", 1, "logs/rom04/overlay-syntax.log")
add("ROM-04-ROLLBACK", H, "REAL", "S02update", "revisar rollback", "rollback automático",
    "no hay; backup best-effort y fallo de backup no detiene", "FAIL", "update_rollback", 0,
    "bool", 1, "logs/rom04/overlay-syntax.log")
add("ROM-04-TEST-LIMITS", H, "REAL", "tools/release/test_update.sh", "revisar asserts",
    "prueba vinculante y evidencia retenida", "nombre de tgz hardcodeado, rm -rf al inicio/fin",
    "REVIEW_REQUIRED", "test_update_binding", 0, "bool", 1, "logs/rom04/test-update-container.log",
    "el assert de tgz borrado es vacuo con otro nombre")


# ---------------------------------------------------------------- ROM-05/06/07
add("ROM-05-BANK", None, "UNSUPPORTED", "banco compartido", "docker ps", "banco libre",
    "banco-apps ocupado por otra sesión; scripts con nombre/volumen fijos", "BLOCKED",
    "bank_available", 0, "bool", 1, "logs/rom02/image-services.log",
    "no se usó ni detuvo el contenedor ajeno")
add("ROM-05-IDENTITY", "QEMU_USER", "REAL", "chroot privado del rootfs", "md5/sha256/fuente-md5",
    "coincidir con manifiesto", "md5 1f998c46…, sha256 7e2e9331…, fuente 964193ae…", "PASS",
    "gui_identity_match", 1, "bool", 3, "logs/rom05/handshake.log")
add("ROM-05-SOCKET", "QEMU_USER", "REAL", "contenedor assessment196-qemu", "medir tiempo a socket",
    "socket de control disponible", "17 s (wall, QEMU)", "PASS", "socket_ready_s", 17, "s", 2,
    "logs/rom05/handshake.log", "no es tiempo de hardware")
add("ROM-05-HANDSHAKE", "QEMU_USER", "REAL", "GUI corriendo", '{"cmd":"get"}', "state",
    "state con freq/mode/app/tx", "PASS", "handshake", 1, "bool", 1, "logs/rom05/handshake.log")
add("ROM-05-CAPTURE", "QEMU_USER", "REAL", "framebuffer falso", "PNG 800x480", "captura no vacía",
    "6770 colores, 84735 B", "PASS", "screenshot_colors", 6770, "colores", 1,
    "capturas/rom05-baseline.png")
add("ROM-06-SWEEPS", None, "UNSUPPORTED", "banco compartido", "3 x barrido-apps-banco.sh",
    "3 barridos de 25 apps", "no ejecutado", "BLOCKED", "sweeps_completed", 0, "barridos", 0,
    "logs/rom02/image-services.log", "banco-apps ocupado; no se sustituyó el arnés")
add("ROM-06-APPS", None, "UNSUPPORTED", "APPS del issue", "contar entradas", "25 sin off + FT4 alias",
    "lista de 25 definida; FT4 alias de FT8; DFLEditor fuera del APPS default", "BLOCKED",
    "apps_in_sweep", 25, "apps", 0, "logs/rom06/app-inventory.log", "no medido")

def load_jsonl(name):
    return [json.loads(l) for l in open(CAP / name)]


stress_a = load_jsonl("stress-rx-100.jsonl")
stress_b = load_jsonl("stress-rx-100b.jsonl")
stress = stress_b  # misma cadena que el reposo de 1 h
settle = [r["settle_ms"] for r in stress]
cmd = [r["duration_ms"] for r in stress]
verdicts = {}
for r in stress:
    verdicts[r["verdict"]] = verdicts.get(r["verdict"], 0) + 1
p95 = lambda a: sorted(a)[min(len(a) - 1, int(0.95 * len(a)))]


def stress_row(sid, recs, evidence, pre):
    st = [r["settle_ms"] for r in recs]
    cd = [r["duration_ms"] for r in recs]
    v = {}
    for r in recs:
        v[r["verdict"]] = v.get(r["verdict"], 0) + 1
    add(sid, "QEMU_USER", "PARTIAL", pre,
        "100 ciclos app off→USB→14074000→2400→CW→Settings→off", "100 ciclos sin crash",
        f"{len(set(r['cycle'] for r in recs))} ciclos, {len(recs)} pasos, {v.get('ok',0)} ok; "
        f"settle p50 {statistics.median(st):.0f}/p95 {p95(st)}/max {max(st)} ms",
        "PASS", "cycles", len(set(r["cycle"] for r in recs)), "ciclos", len(recs), evidence,
        "QEMU_USER, sin radio/FFT; no es hardware")


stress_row("ROM-07-CYCLES-A", stress_a, "capturas/stress-rx-100.jsonl",
           "GUI ARM del candidato, proceso original")
stress_row("ROM-07-CYCLES-B", stress_b, "capturas/stress-rx-100b.jsonl",
           "GUI ARM, proceso nuevo assessment196-soak (misma cadena que el reposo)")
add("ROM-07-COMMANDS", "QEMU_USER", "PARTIAL", "corrida B", "contar comandos/respuestas",
    "700 comandos ok", f"{len(stress)} comandos, 0 errores, 0 timeouts", "PASS", "commands",
    len(stress), "comandos", len(stress), "capturas/stress-summary-b.json")
add("ROM-07-SETTLE", "QEMU_USER", "PARTIAL", "corrida B", "medir comando→estado",
    "sin divergencias", f"p50 {statistics.median(settle):.0f} ms, p95 {p95(settle)} ms, max {max(settle)} ms",
    "PASS", "settle_p50_ms", statistics.median(settle), "ms", len(settle),
    "capturas/stress-summary-b.json", "host con carga; outlier de 1,47 s, sin timeout")
add("ROM-07-CMD-LAT", "QEMU_USER", "PARTIAL", "corrida B", "medir cliente",
    "latencia del cliente", f"p50 {statistics.median(cmd):.0f} ms, max {max(cmd)} ms", "PASS",
    "command_p50_ms", statistics.median(cmd), "ms", len(cmd), "capturas/stress-summary-b.json")
add("ROM-07-CRASH", "QEMU_USER", "PARTIAL", "corrida B", "CRASHLOG/Overflow/cola", "0 crashes",
    "0 CRASHLOG sig=, 0 Overflow, 0 reinicios de PID", "PASS", "crashes", 0, "eventos", 700,
    "capturas/stress-summary-b.json")

idle_a = load_jsonl("idle-3600-partial-external-stop.jsonl")
idle_b = load_jsonl("idle-3600b.jsonl") + load_jsonl("idle-3600b-extra.jsonl")
idle = idle_b


def samp(k, rows=None):
    rows = idle if rows is None else rows
    out = []
    for r in rows:
        m = re.search(rf"{k}:?=(\d+)", r["raw_sample"])
        if m:
            out.append(int(m.group(1)))
    return out


gets_ok = sum(r["gets_ok"] for r in idle)
gets_err = sum(r["gets_err"] for r in idle)
rss, thr, fds = samp("VmRSS"), samp("Threads"), samp("fds")
crash = samp("crash")
overflow = samp("overflow")
logs = samp("log_bytes")
minutes = len(idle)
gets_ok_a = sum(r["gets_ok"] for r in idle_a)
gets_err_a = sum(r["gets_err"] for r in idle_a)
add("ROM-07-IDLE-A-PARTIAL", "QEMU_USER", "PARTIAL", "proceso original, 31 min",
    "get cada 5 s hasta la detención externa", "completar 1 h",
    f"detenido externamente a los {len(idle_a)} min (exit 137, sin CRASHLOG/OOM); {gets_ok_a} get OK, {gets_err_a} errores",
    "BLOCKED", "idle_a_minutes", len(idle_a), "min", len(idle_a),
    "capturas/idle-3600-partial-external-stop.jsonl",
    "no es crash del binario; actor de la detención desconocido")
add("ROM-07-IDLE", "QEMU_USER", "PARTIAL", "proceso nuevo assessment196-soak, 1 h",
    "get cada 5 s; muestreo cada 60 s (60 + 4 muestras extra)", "sin crash ni degradación",
    f"{minutes} muestras, {gets_ok} get OK, {gets_err} errores, 3605 s",
    "PASS", "idle_samples", minutes, "muestras", minutes, "capturas/idle-3600b.jsonl",
    "proceso nuevo (no el de la corrida A); host no aislado; 3605 s acumulados")
add("ROM-07-IDLE-ELAPSED", "QEMU_USER", "PARTIAL", "proceso nuevo", "sumar elapsed de las dos ventanas",
    ">= 3600 s", "3379 s + 226 s = 3605 s", "PASS", "idle_elapsed_s", 3605, "s", 2,
    "logs/rom07/idle-3600b.log")
add("ROM-07-DB-POST", "QEMU_USER", "PARTIAL", "DB escrita por la GUI durante 100 ciclos",
    "PRAGMA integrity_check sobre copia", "DB sana y cambios persistidos",
    "ok; version=5; 9 params; band 7 vfoa_freq=14074000 vfoa_mode=4", "PASS",
    "db_post_stress_integrity", "ok", "texto", 1, "logs/rom07/db-post-stress.log",
    "copia privada; no es corte de energía")
add("ROM-07-IDLE-RSS", "QEMU_USER", "PARTIAL", "proceso nuevo", "RSS cada 60 s", "RSS estable",
    f"{min(rss)}–{max(rss)} kB, delta {max(rss)-min(rss)} kB", "PASS", "rss_delta_kb",
    max(rss) - min(rss), "kB", len(rss), "capturas/idle-3600b.jsonl")
add("ROM-07-IDLE-FDS", "QEMU_USER", "PARTIAL", "proceso nuevo", "threads/fds cada 60 s",
    "sin crecimiento", f"threads {min(thr)}–{max(thr)}, fds {min(fds)}–{max(fds)}", "PASS",
    "fds_delta", max(fds) - min(fds), "fds", len(fds), "capturas/idle-3600b.jsonl")
add("ROM-07-IDLE-CRASH", "QEMU_USER", "PARTIAL", "proceso nuevo", "CRASHLOG/Overflow/log",
    "0 crashes, log plano", "0 CRASHLOG sig=, 0 Overflow; log real out-soak.txt 296731 B constante",
    "PASS", "idle_crashes", 0, "eventos", minutes, "logs/rom07/soak-log-check.log",
    "las muestras JSONL leyeron out-banco.txt (path viejo); ver nota §7.4")
add("ROM-07-IDLE-LOG", "QEMU_USER", "PARTIAL", "proceso nuevo", "stat y grep del log real out-soak.txt",
    "log sin crecimiento", "296731 B antes y después; 0 B/min; 0 CRASHLOG/overflow", "PASS",
    "soak_log_bytes", 296731, "bytes", 2, "logs/rom07/soak-log-check.log")

# ---------------------------------------------------------------- ROM-08
neg = [json.loads(l) for l in open(CAP / "negatives.jsonl")]
add("ROM-08-PROTOCOL", "QEMU_USER", "REAL", "GUI ARM privada", "12 negativas de protocolo",
    "err/sin cambio/0 crash", f"{sum(1 for n in neg if n['verdict']=='ok')}/{len(neg)} ok, 0 CRASHLOG",
    "PASS", "negative_cases_ok", sum(1 for n in neg if n["verdict"] == "ok"), "casos", len(neg),
    "capturas/negatives.jsonl")
add("ROM-08-DB-CORRUPT", "QEMU_USER", "SYNTHETIC", "DB corrupta en chroot privado", "arrancar GUI",
    "degradar/fallback sin crash", "SIGSEGV exit 139, PC 0x42e12c, 2/2", "FAIL",
    "db_corrupt_exit", 139, "exit", 2, "logs/rom08/db-corrupt-gui.log",
    "fixture privado; no se inyectó en DATA de la radio")
add("ROM-08-DB-RO", "QEMU_USER", "SYNTHETIC", "/mnt read-only en chroot privado", "arrancar GUI",
    "degradar sin crash", "SQLITE_READONLY en migración 2 → SIGSEGV exit 139", "FAIL",
    "db_readonly_exit", 139, "exit", 1, "logs/rom08/db-readonly-gui.log",
    "fixture privado; no se inyectó en DATA de la radio")
add("ROM-08-DB-MISSING", "QEMU_USER", "SYNTHETIC", "sin params.db, /mnt escribible", "arrancar GUI",
    "crear DB y arrancar", "DB vacía 8192 B; migración 1 falla → SIGSEGV exit 139", "FAIL",
    "db_missing_exit", 139, "exit", 1, "logs/rom08/db-missing-gui.log",
    "en el boot normal S20gui siembra la DB; se prueba el camino sin seed")
add("ROM-08-CRASHLOOP", "QEMU_USER", "SYNTHETIC", "wrapper x6200_daemon + DB corrupta, chroot privado",
    "correr el wrapper 10 s", "sin lazo de reinicios", "34 GUI inicios, 19 CRASHLOG, 20 logs, boot.log 18 KB en 10 s",
    "FAIL", "crashloop_starts_10s", 34, "inicios", 1, "logs/rom08/crashloop-summary.log",
    "fixture privado; en hardware el lazo sería equivalente si la DB está corrupta/RO")
add("ROM-08-DB-FULL", None, "UNSUPPORTED", "sin fixture seguro de DATA llena", "DB full",
    "medir error de escritura", "no ejecutado", "BLOCKED", "db_full", None, "", 0,
    "logs/rom03/db-integrity.log", "params_write_* ignoran rc de sqlite3_step (riesgo de pérdida silenciosa)")
add("ROM-08-PERIPH", "QEMU_USER", "PARTIAL", "sin i2c/CAT/radio/NM/audio", "100 ciclos + 1 h",
    "seguir sin crash", "errores logueados, GUI responde; 0 crashes", "PASS",
    "peripheral_absence_crashes", 0, "eventos", 700, "logs/rom05/handshake.log",
    "ausencia observada en QEMU, no inyección de fallos")

# ---------------------------------------------------------------- reproducibilidad / stale
add("REPRO-DEFCONFIG", H, "REAL", "X6200_defconfig d084e287", "grep PULSEAUDIO/LIBSNDFILE",
    "defconfig suficiente para compilar el GUI", "faltan PULSEAUDIO y LIBSNDFILE; build usó desviación local",
    "FAIL", "defconfig_audio_symbols", 0, "bool", 1, "logs/rom00/refs.log",
    "no se tocó el defconfig; el artefacto se construyó con .config privado")
add("REPRO-BIT", H, "UNSUPPORTED", "sin doble build", "comparar dos builds", "bit a bit",
    "no medido", "INCONCLUSIVE", "bit_reproducible", None, "", 0, "logs/rom00/refs.log")
add("STALE-TIP", H, "REAL", "tip de desarrollo", "git fetch + diff candidate..tip",
    "sin cambios de producción", "24 archivos de producción; tip 50bf8ab0→0d00e7fc", "REVIEW_REQUIRED",
    "production_files_changed", 24, "archivos", 1, "logs/rom00/refs.log",
    "dossier válido sólo para c888308e; repetir cobertura si se adopta el tip")

out = BASE / "results.csv"
with open(out, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=COLS, quoting=csv.QUOTE_ALL)
    w.writeheader()
    w.writerows(rows)
print(f"wrote {out} rows={len(rows)}")

# ---------------------------------------------------------------- reliability.csv
rel = [
    ("ROM-07", "apps_intentadas", 1, "apps", "sólo Settings en los 100 ciclos; barrido de apps BLOCKED"),
    ("ROM-07", "apps_abiertas", 100, "aperturas", "Settings abierta y verificada por estado en cada ciclo"),
    ("ROM-07", "apps_cerradas", 100, "cierres", "app off verificado por estado"),
    ("ROM-07", "primer_intento", 100, "aperturas", "0 reintentos"),
    ("ROM-07", "reintentos", 0, "reintentos", ""),
    ("ROM-07", "distinct_apps", 1, "apps", "no es el barrido de 25"),
    ("ROM-07", "barridos_completos", 0, "barridos", "BLOCKED por banco ocupado"),
    ("ROM-07", "ciclos", 100, "ciclos", "secuencia RX completa"),
    ("ROM-07", "duracion_real", 239, "s", "corrida B 21:44:14Z→21:48:13Z; corrida A 207 s"),
    ("ROM-07", "comandos", 700, "comandos", "7 pasos x 100 ciclos"),
    ("ROM-07", "errores", 0, "comandos", ""),
    ("ROM-07", "timeouts", 0, "comandos", "timeout de 5 s por comando"),
    ("ROM-07", "crashlog_firmas", 0, "eventos", "0 líneas CRASHLOG sig="),
    ("ROM-07", "overflow", 0, "eventos", "0 Overflow / scheduler queue overflow"),
    ("ROM-07", "reinicios_pid", 0, "reinicios", "PID 7 estable"),
    ("ROM-07", "rss_inicial", rss[0], "kB", "primera muestra del reposo B"),
    ("ROM-07", "rss_final", rss[-1], "kB", "última muestra del reposo B"),
    ("ROM-07", "rss_delta", rss[-1] - rss[0], "kB", "delta en 1 h"),
    ("ROM-07", "threads_inicial", thr[0], "threads", ""),
    ("ROM-07", "threads_final", thr[-1], "threads", ""),
    ("ROM-07", "fds_inicial", fds[0], "fds", ""),
    ("ROM-07", "fds_final", fds[-1], "fds", ""),
    ("ROM-07", "memoria_pendiente", rss[-1] - rss[0], "kB/h", "delta en 3605 s; no prueba ausencia de fugas"),
    ("ROM-07", "fds_pendiente", 0, "fds/h", "60 muestras"),
    ("ROM-07", "db_errores_normales", 0, "eventos", "DB válida; corrupta/RO/faltante fallan (ver results.csv)"),
    ("ROM-07", "logs_bytes_inicial", 296731, "bytes", "out-soak.txt medido directo (JSONL leyó out-banco.txt)"),
    ("ROM-07", "logs_bytes_final", 296731, "bytes", "sin crecimiento en reposo"),
    ("ROM-07", "logs_bytes_por_min", 0, "bytes/min", "out-soak.txt constante; ver nota §7.4"),
    ("ROM-07", "estado_inicial", "freq=14074000 mode=CW band=20m-SSB filter=500 app='' tx=false", "estado",
     "inicio de la corrida B, heredado de la DB persistida"),
    ("ROM-07", "estado_final", "freq=14074000 mode=CW band=20m-SSB filter=500 app='' tx=false", "estado",
     "fin de los 100 ciclos B y durante el reposo"),
    ("ROM-07", "diferencias_estado", "QSY 10.100->14.074 MHz, USB->CW, filter 2800->500, app cerrada", "estado",
     "cambios producidos por la secuencia del issue; sin pérdida de estado entre ciclos"),
    ("ROM-07", "idle_duracion", 3605, "s", "60 muestras + 4 extra; 3605 s"),
    ("ROM-07", "idle_gets", gets_ok, "gets", f"{gets_err} errores"),
    ("ROM-07", "idle_gets_err", gets_err, "gets", ""),
    ("ROM-07", "idle_a_minutes", len(idle_a), "min", "detención externa; exit 137 sin CRASHLOG"),
    ("ROM-07", "idle_a_gets_ok", gets_ok_a, "gets", f"{gets_err_a} errores al final"),
    ("ROM-07", "idle_rss_rango", f"{min(rss)}-{max(rss)}", "kB", ""),
    ("ROM-07", "idle_threads_rango", f"{min(thr)}-{max(thr)}", "threads", ""),
    ("ROM-07", "idle_fds_rango", f"{min(fds)}-{max(fds)}", "fds", ""),
    ("ROM-07", "idle_crashlog", max(crash), "eventos", ""),
    ("ROM-07", "idle_overflow", max(overflow), "eventos", ""),
    ("ROM-06", "sweeps", 0, "barridos", "BLOCKED: banco-apps ocupado"),
    ("ROM-07", "capa", "QEMU_USER", "capa", "no es hardware ni kernel real"),
]
# uptime observado del proceso a partir de /proc (HZ=100)
import re as _re
_st = int(_re.search(r"start_ticks=(\d+)", idle[-1]["raw_sample"]).group(1))
_up = int(_re.search(r"uptime_s=(\d+)", idle[-1]["raw_sample"]).group(1))
rel.append(("ROM-07", "pid_proceso", 7, "pid", "estable en las 64 muestras"))
rel.append(("ROM-07", "uptime_proceso_final", round(_up - _st / 100.0), "s",
            "calculado de /proc/uptime - start_ticks (HZ=100)"))

relout = BASE / "reliability.csv"
with open(relout, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f, quoting=csv.QUOTE_ALL)
    w.writerow(["scenario_id", "metric", "value", "unit", "notes"])
    w.writerows(rel)
print(f"wrote {relout} rows={len(rel)}")
