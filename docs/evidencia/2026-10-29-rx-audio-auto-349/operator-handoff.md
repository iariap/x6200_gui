# Handoff al operador — escucha del NR/DFL del BASE (después del 2026-11-12)

Este banco **no mide el audio audible del BASE**: mide STOI/ESTOI sobre voz de
LibriSpeech y pasabandas proxy. El gate del hardware queda en
`BASE_NR_DFL_NOT_MEASURED`. Esta página es para retomar, si sigue haciendo falta,
la escucha controlada del NR/DFL del BASE cuando el operador esté disponible
(según el issue #349 revisado, a partir del 2026-11-12). No abre otro issue ni
implica que la escucha esté hecha.

## Qué falta medir

El A/B subjetivo del **DSP del BASE** (NR real en niveles 10/30/60 y filtros DFL
`Normal`/`DX`) sobre el parlante, con voz SSB real y puntajes del operador. Es
independiente de este banco y no se puede inferir de STOI ni de `parec`.

## Precondiciones

- Radio en **RX** (`tx=false` verificado por `{"cmd":"get"}`), sin TX ni PTT.
- Modo **LSB o USB normal** (no `-D`), VFO A sin split.
- DFL factory **`Normal`** (150–2850 Hz) y después **`DX`** (300–2400 Hz).
- `Capture filters` en **Off** y sin cambios durante las ventanas.
- Salida física **parlante** (o jack, pero la misma en toda la serie).
- AGC AUTO, RF gain 0, volumen cómodo y **fijo**; anotar el valor.
- Señal S1: voz SSB fuerte y estable; S2: voz débil al borde de inteligibilidad.
  Si hay QSB o cambia el interlocutor, marcar la ventana inválida y repetir.

## Serie A — NR del BASE con DFL Normal (20 s por ventana)

`A0 Off` · `A1 lvl10` · `A2 Off` · `A3 lvl30` · `A4 Off` · `A5 lvl60` ·
`A6 Off` · `A7 lvl60` · `A8 Off` · `A9 lvl30` · `A10 Off` · `A11 lvl10` ·
`A12 Off`

Repetir en **S1** y en **S2**. El operador cambia NR/nivel; el agente sólo lee
GET y anota UTC.

## Serie B — DFL Normal vs DX (20 s por ventana)

Con NR Off: `Normal → DX → Normal`.
Con NR nivel 30: `Normal → DX → Normal`.
Repetir en S1 y S2.

## Por ventana, anotar

`I` inteligibilidad 1–5 · `F` fatiga 1–5 · `A` artefactos 0–3 · `P` palabras
perdidas (número o `UNK`) · `N` ruido 1–5 · `E` estable `Y/N/U` + nota.
Ejemplo: `A3 I4 F2 A1 PUNK N4 EY`.

## Cierre

- Restaurar **a mano** NR, nivel, filtros, modo/VFO, volumen y salida originales;
  el agente no restaura por remoto.
- Registrar `get` final y confirmar `tx=false`.
- No grabar ni publicar QSO de terceros; no cambiar ruteo, ALSA, mixer, A2DP ni
  registros del BASE.
- El resultado de este handoff, si se ejecuta, se documenta aparte; no cambia el
  veredicto del banco automático.
