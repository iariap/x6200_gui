#!/usr/bin/env sh
#
# Stub del start-stop-daemon de BusyBox/Debian para el banco de HOST
# (tests/check_bootanim.sh). Implementa SOLO las dos formas que usan los scripts
# de init del port:
#
#   start-stop-daemon -S -b -q -m -p <pidfile> -x <exe>
#   start-stop-daemon -K -q -p <pidfile>
#
# Existe porque el host de desarrollo puede no tener start-stop-daemon (Fedora
# no lo trae): el gate prueba de verdad la logica PROPIA de S01bootanim
# (singleton, PIDFILE, identidad por /proc/<pid>/exe, stop sin killall) con este
# doble, y deja como pendiente la corrida con el start-stop-daemon de la imagen.
#
# Cada invocacion queda registrada en $SSD_LOG (una linea por llamada) para que
# el test pueda contar cuantas instancias se arrancaron.
set -u

CMD=""
PIDFILE=""
EXEC=""
MAKE_PIDFILE=0

while [ $# -gt 0 ]; do
	case "$1" in
		-S|--start) CMD="start" ;;
		-K|--stop) CMD="stop" ;;
		-m|--make-pidfile) MAKE_PIDFILE=1 ;;
		-p|--pidfile) PIDFILE="$2"; shift ;;
		-x|--exec) EXEC="$2"; shift ;;
		-b|--background|-q|--quiet) : ;;
		-a|--startas) shift ;;
		*) : ;;
	esac
	shift
done

[ -n "${SSD_LOG:-}" ] && printf '%s pidfile=%s exe=%s\n' "$CMD" "$PIDFILE" "$EXEC" >> "$SSD_LOG"

pid_alive() {
	[ -e "/proc/$1" ] || return 1

	case "$(cat "/proc/$1/stat" 2>/dev/null)" in
		*") Z "*) return 1 ;; # zombi
	esac

	return 0
}

case "$CMD" in
	start)
		[ -n "$EXEC" ] || exit 1

		if [ -n "$PIDFILE" ] && [ -s "$PIDFILE" ]; then
			old="$(cat "$PIDFILE" 2>/dev/null || true)"
			case "$old" in
				''|*[!0-9]*) : ;;
				*)
					if pid_alive "$old" &&
						[ "$(readlink "/proc/$old/exe" 2>/dev/null)" = "$EXEC" ]; then
						exit 1 # ya esta corriendo
					fi
					;;
			esac
		fi

		"$EXEC" >/dev/null 2>&1 &
		new=$!

		if [ "$MAKE_PIDFILE" -eq 1 ] && [ -n "$PIDFILE" ]; then
			printf '%s\n' "$new" > "$PIDFILE"
		fi

		exit 0
		;;
	stop)
		rc=1

		if [ -n "$PIDFILE" ] && [ -s "$PIDFILE" ]; then
			pid="$(cat "$PIDFILE" 2>/dev/null || true)"

			case "$pid" in
				''|*[!0-9]*) : ;;
				*)
					if pid_alive "$pid"; then
						kill "$pid" 2>/dev/null && rc=0

						i=0
						while [ "$i" -lt 20 ] && pid_alive "$pid"; do
							sleep 0.1
							i=$((i + 1))
						done
					fi
					;;
			esac
		fi

		exit "$rc"
		;;
	*)
		exit 1
		;;
esac
