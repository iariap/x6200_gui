---
name: hallazgos-fundados
description: Mantiene el registro de hallazgos fundados del X6200 (x6200_gui/docs/HALLAZGOS.md) -agrega hallazgos con su evidencia, audita los que ya estan, marca o mueve al anexo lo que no se puede fundar, y corre el chequeo automatico-. Use when adding, editing or auditing findings/hallazgos, when writing or reviewing docs/HALLAZGOS.md, or when the user says "hallazgo", "registro", "fundado", "sin fundar", "evidencia", "anexo de hipotesis" or asks to document a finding of the radio.
---

# Registro de hallazgos fundados

Mantener `x6200_gui/docs/HALLAZGOS.md` con esta regla, que es del dueño del proyecto:

> todos los hallazgos deben estar fundados, si no se pueden fundar no debe ser un hallazgo

## Que significa "fundado"

Un hallazgo esta fundado cuando la afirmacion viene acompanada, **en el mismo lugar**, por una de
estas tres cosas:

| Estado | Que se escribe | Ejemplo |
|---|---|---|
| **medido** | el comando y su salida pegada, o el numero con su origen y fecha | `$ id pulse` → `groups=1000(pulse)` |
| **codigo** | el archivo (y la linea si importa) que lo dice | `codigo: src/theme.c, theme_list() omite B_LEGACY` |
| **sin fundar** | NO es hallazgo: va al anexo A con lo que haria falta para fundarlo | "el codec no esta cableado a la salida" |

Si se midio en su momento y **no quedo la captura**, se dice asi ("medido entonces; el registro que
quedo es el comentario en `audio.c`"). Si una comprobacion **no se pudo repetir**, tambien se dice.
Una estimacion (`~`, "unos", "casi") puede quedar **si esta dicha como estimacion**; lo que no puede
quedar es un numero inventado presentado como medicion.

## Antes de escribir un hallazgo

```text
Task Progress:
- [ ] Paso 1: ¿lo medi o lo lei en el codigo? Si no: al anexo A, no al cuerpo.
- [ ] Paso 2: ¿tengo el comando y su salida (o el archivo y la linea)? Pegarlos.
- [ ] Paso 3: ¿es una sola muestra? Decirlo, o tomar una segunda.
- [ ] Paso 4: ¿hay una conclusion causal ("es por X")? Separarla de lo medido.
- [ ] Paso 5: si corrige algo ya escrito, dejar la correccion visible (no borrar en silencio).
- [ ] Paso 6: actualizar el indice del principio si se agrego una seccion.
- [ ] Paso 7: correr `scripts/audita-hallazgos.sh` (ver abajo).
```

## Plantilla de seccion

```markdown
## N. <titulo de una linea, sin adjetivos>

**Sintoma** (si lo hay): que se ve, y quien lo reporta (si es el usuario, decirlo).

**Causa, medida**:

```
$ <comando>
<salida real>
```

**Correccion de lo que decia antes este registro** (si aplica): que decia, por que estaba mal y con
que se refuto.

**Arreglo** + **Verificacion, medida**: el comando y la salida despues del arreglo.

**Sin fundar** (anexo A): lo que NO se puede afirmar todavia.
```

## Cuando una afirmacion se cae

No se borra en silencio: se deja escrito **que decia, que la refuto y con que medicion**. Ya paso
dos veces en este registro (el "PulseAudio lo mandaba al nulo" y "el log probaba que el audio se
escribio") y las dos correcciones son parte del valor del registro.

## Anexo A

Todo lo que no se puede fundar va al final, en una tabla con tres columnas: **hipotesis**, **por que
no esta fundada**, **que haria falta**. Tambien van ahi las cosas **"no determinadas"** (ni
afirmadas ni descartadas). El anexo es util: es la lista de experimentos pendientes.

## Auditoria

```sh
.agents/hallazgos-fundados/scripts/audita-hallazgos.sh docs/HALLAZGOS.md
```

El script usa `docs/HALLAZGOS.md` por defecto, asi que alcanza con correrlo **desde la raiz del
repo** (donde vive el registro). Las rutas de esta skill son relativas al repo, para que valgan
igual en cualquier copia del repositorio.

Revisa: **(1) estructura** (el indice y las secciones `## N.` coinciden), **(2) estimaciones o
recuerdos fuera del anexo** (marcadores con limite de palabra: `~N`, "estimad", "casi",
"probablemente", "deberia", "creo", "parece", "supuest", "de memoria", "aprox"; no marca las lineas
que ya dicen "no medido", "no se cronometro" o "Correccion"), y **(3) secciones con numeros en
negrita y sin decir de donde salen**. Sale 1 si hay problemas de estructura; con `--estricto` sale 1
si ademas hay lineas para revisar.

Las lineas de "revisar" no son errores automaticos: son **candidatas**. Para cada una la pregunta es
"¿es una medicion redondeada (esta bien decirlo asi) o un numero que no medí (hay que medirlo o
marcarlo como estimacion)?".

Para una auditoria de fondo (una pasada completa, ~130 afirmaciones) sirve delegarla a un subagente
con esta consigna: *"marca cada afirmacion cuantitativa o causal y decí si el documento la respalda
con evidencia citada; no opines si es verdadera en el mundo real, solo si el documento la
respalda"*. Despues **revisar a mano los casos limite** que el subagente mismo marque: en la
auditoria del 13/10 fueron `if_shift` (que si aparece, pero solo como plumbing) y `tx_worker` (que
existe como `ftx_worker_*`): con `grep -w` daban 0 y parecian faltantes.

## Guardas al reescribir el archivo

El registro es largo y perdi el contenido una vez. Reglas:

- Nunca `cat parte1 parte2 > HALLAZGOS.md`: si una parte no existe, el shell **trunca** el destino.
  Escribir a un temporal (`docs/.nuevo`) y despues `mv`.
- Antes del `mv`, verificar: `wc -l` (que no quede corto), que la seccion nueva este y que una
  seccion vieja siga estando.
- Para reemplazar una seccion: `head -<linea anterior> archivo` + la parte nueva, en el temporal.

## Evidencia en este proyecto

Los comandos que se usan para fundar cosas estan en `docs/HALLAZGOS.md` §8: SSH a la radio
(`sshpass ... root@192.168.0.236`), `boot.log` y `boot_scripts.log`, `voice.log`, `tools/verifica-radio.sh`,
`tools/prueba-gui-en-radio.sh` y el banco en Docker (`capture-banco.sh`). Reglas que valen al medir:

- **Medir en la radio, no en el banco**: el banco (QEMU) da 0,15 s donde la radio da 2,0 s.
- **A/B honesto**: los dos binarios, el mismo equipo, los dos `md5` y el `boot.log` completo.
- **No concluir con una muestra** (paso en §7 con `pantalla principal` +0,55 s, que era ruido).
- El `busybox` de la radio **no tiene `timeout`**: usar `cmd & p=$!; sleep N; kill -0 $p && kill $p`.
