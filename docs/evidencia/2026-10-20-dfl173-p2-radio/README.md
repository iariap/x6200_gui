# Verificacion en la radio del PR 2 de #173 (editor DFL)

- Rama `lu2dsx/x6200-rom`, merge **`39539f93`** ("DFL #173 (PR 2): editor DFL completo (modal, handles,
  quinta celda y accion asignable) (#176)").
- Build en la radio: **`ca5cc59151934f01f55cf62faeb59cb4`** (deploy del otro agente, md5 confirmado).
- Radio real, tema Aurora, RX, sin TX.

## Verificado

| Item | Resultado | Evidencia |
|---|---|---|
| Pagina DFL (pulsacion corta) | la 5.a celda ahora dice **"DFL editor"** | `01-pagina-dfl.png` |
| Banda pasante en el espectro | dibujada a escala, texto **"2780"** (exacto, sin redondear) | `01-pagina-dfl.png` |
| Editor abre con la 5.a celda | `{"cmd":"btn","val":4}` → `{"t":"ok"}` y aparece el modal | `02-editor-dfl.png` |
| Contenido del modal | titulo "DFL - Digital Filter", modo (USB), **Default** y **X**; **LOW 20 / HIGH 2800 / BW 2780** con -/+; grafico "Esquema de banda pasante" | `02-editor-dfl.png` |
| Grafico del editor | meseta + **flancos** a la baseline (por diseño: `src/dialog_dfl.c:286`, "respuesta esquematica") | `02-editor-dfl.png` |
| Cierre | 5.a celda pasa a **"Cerrar DFL"**; al pulsarla cierra y vuelve a la pantalla principal | `03-cierre.png` |
| Barrido de 23 apps | **24/24 ok**, 0 "SE CAYO", 0 "no responde" | `prueba-apps-101618.log` |
| Verificacion general | `TODO OK` (audio real, sin crashes en el log vivo) | — |
| Posicion de la banda | USB 20..2800 Hz → **a la derecha del VFO** (x≈400..447), sin regresion del PR 1 | `01-pagina-dfl.png` |

## No medido por este rol

- **Interaccion tactil** (arrastrar los handles, tocar -/+, Default/X con el dedo): el control remoto
  solo puede pulsar las 5 celdas de abajo, no tocar dentro del modal. Lo cubre la evidencia de banco del
  PR (test de ciclos/interaccion con eventos sinteticos).
- **Pulsacion larga fisica de la tecla DFL** con la accion configurada: requiere la tecla fisica.

## Estado dejado

- Pagina **GEN** restaurada; frecuencia/modo/filtro del dueno **intactos** (14.117.000 USB BW 2780);
  radio en RX, sin TX. No se toco ningun ajuste.
