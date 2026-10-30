# Preflight — antes de conectar nada (P0 pasiva, ventana desde 2026-11-12)

**Alcance del preflight:** preparar y verificar una única prueba **pasiva**: audio RX de BASE en la salida
**S/P**, capturado por un banco externo no invasivo. **No** incluye MUX, TX/PTT/ATU, firmware, GUI,
deploy ni cambios de configuración. Si algún chequeo de este documento no se puede completar, **no se
conecta nada** y se registra el bloqueo (`P0_PENDING_EQUIPMENT` / `BLOCKED_UNSAFE_AUDIO_INPUT`).

> Recordatorio: el operador (owner) es quien opera la radio y quien autoriza conectar/desconectar. El
> agente no está físicamente presente antes del 2026-11-12 ni conecta nada en esta fase.

## A. Confirmación de equipamiento (gate de entrada)

- [ ] Cable **TRS macho 3,5 mm de 3 contactos** disponible y identificado (no TS, no adaptador 2→3).
- [ ] **Medidor de continuidad / breakout** disponible para verificar el pinout con radio apagada.
- [ ] **Interfaz de audio externa con entrada LINE** (no MIC con plug-in power) con **límite de entrada
      declarado** (Vpp o Vrms), impedancia de entrada e **aislamiento galvánico** declarado.
- [ ] **Atenuador pasivo / limitador** con valores conocidos, disponible para el primer nivel seguro.
- [ ] **Instrumento de medida** (multímetro True RMS u osciloscopio/sonda) para medir DC/AC del S/P antes
      de conectar la interfaz; con su límite de entrada y categoría declarados.
- [ ] **Host** con captura WAV y `sha256`; reloj monótono; espacio suficiente.
- [ ] **Manual Rev.05** en mano (contrastar dibujo del S/P y advertencias).
- [ ] **Operador presente**, descansado y con capacidad de cortar el ensayo; volumen auditivo prudente.
- [ ] Si algo de lo anterior **falta o no está declarado**: **STOP** `P0_PENDING_EQUIPMENT`. No se
      improvisa con otro puerto, sonda casera ni interfaz sin tolerancia.

## B. Antes de conectar: límites eléctricos y aislamiento

- [ ] La interfaz **no** es una entrada MIC con bias/plug-in power: si sólo hay MIC, **no conectar**
      (`BLOCKED_UNSAFE_AUDIO_INPUT`).
- [ ] **No** se conecta el S/P a otra salida de audio, ni se comparte masa con equipos con referencia de
      red sin aislamiento.
- [ ] Nivel/impedancia/offset del S/P: **desconocidos**. Primero se mide con el instrumento (paso C.3) y
      recién después se elige la atenuación.
- [ ] La atenuación inicial se elige para que la señal quede por debajo del **50 % del máximo declarado**
      de la entrada; sin dato del fabricante, atenuar de más no es una excusa para adivinar: se mide.
- [ ] **No se asignan voltajes**: si alguien “sabe” el nivel del S/P sin haberlo medido en este equipo, no
      se usa. La planilla tiene columnas `input_limit` y `attenuation` justamente para registrarlo.

## C. Radio antes de conectar (sólo lectura, sin escrituras)

1. **Radio apagada** para conectar/desconectar (regla del issue; el manual ya la exige para el KEY).
2. Con la radio todavía apagada: verificar continuidad del cable (punta↔AUDIO, anillo↔NC, manguito↔GND).
3. Encender en **RX**, antena conectada, sin ATU/PTT/CW. Verificar y registrar:
   - [ ] `tx=false` y `radio=RX` (GET read-only del control remoto, dos muestras separadas ≥10 s).
   - [ ] Modo/frecuencia/volumen iniciales **estables** entre las dos muestras (si cambian solos, no es una
         ventana segura: STOP).
   - [ ] Sin ATU tuneando, sin `app` de TX/digital activa, sin Bluetooth (evitar rutas de audio ambiguas).
   - [ ] `S/P MODE = EAR PHONE` seleccionado por el operador en el menú (nunca `SPEAKER` con auriculares).
   - [ ] Volumen inicial **bajo**; el operador sube sólo si la captura es insuficiente.
4. Medir con el instrumento declarado el **DC y el nivel AC** de la salida S/P (punta–manguito) con
   volumen bajo. Registrar en la planilla: instrumento, límite de entrada, atenuación elegida.
5. Recién entonces: **radio apagada**, insertar atenuador + interfaz LINE aislada; encender y repetir el
   chequeo `tx=false`.

> **Nunca** conectar/desconectar con la radio encendida, ni tocar KEY/ACC, ni “probar” con auriculares en
> modo SPEAKER. Ver [`wiring.md`](wiring.md).

## D. Rol del operador

- El **operador** (owner) selecciona el modo `S/P MODE`, ajusta volumen, escucha niveles y decide cortar.
- El **host** sólo captura y registra; no escribe en la radio, no cambia modo/VFO/volumen/mute, no llama a
  `audio_play_en()` ni a `x6200_control_record_set()`, no conmuta el MUX.
- **Una sola persona** autoriza conectar/desconectar y puede abortar. Si hay dos personas, una es la
  autoridad y la otra no toca nada sin pedir.
- El operador no debe usar auriculares en modo SPEAKER; en modo EAR PHONE, volumen prudente.

## E. Backup / rollback (hash de línea base)

- [ ] Registrar el **hash actual del GUI** antes de cualquier cosa: `sha256sum /usr/sbin/x6200_gui` y
      `md5sum /usr/sbin/x6200_gui`. Línea base conocida de la evidencia previa: `sha256
      4ee87aefb389b6a6b5d2d90df2c51d0440aeff19ed4464d44649f48757dd66b0`, `md5 80bbe5bd70b3…`.
- [ ] Verificar que exista una copia de rollback del GUI con ese hash (`/mnt/ui-bak-*`) y registrar su ruta
      y hash. Si no existe, **no se despliega nada**: P0 no modifica el GUI; una copia sólo hace falta si
      otra fase (fuera de #371) lo cambiara.
- [ ] **P0 no despliega ni reemplaza binarios.** Si alguien propone un deploy en esta ventana: está fuera
      de alcance, se detiene y se trata en su issue.
- [ ] Al cierre: mismo hash de GUI y `tx=false` que al inicio.

## F. Configuración de captura (segura, sin audio de terceros)

- [ ] WAV/PCM **44100 Hz**, mono, `s16le`, con reloj monótono y **`sha256`** del archivo.
- [ ] Registro simultáneo (si está autorizado y es sólo lectura) del **PCM source PA** de la radio
      (`parec`, pre-Speex), para estimar afinidad; **no** confundirlo con la salida procesada de la GUI.
- [ ] Fuente de señal: RX propia/ruido, o material propio/licenciado. **Prohibido publicar** grabaciones de
      QSO de terceros: al repo sólo van metadatos, estadísticas y hashes.
- [ ] Nada de binarios/audio en `docs/evidencia/`: sólo la planilla y los comandos.
- [ ] `measurements-template.csv` se completa **sólo con mediciones reales** de esta ventana.

## G. Condiciones de interrupción (abortar ya)

**STOP inmediato** y apagar/desconectar (radio apagada para desconectar) si:

- **Clipping / saturación**: pico contra 0 dBFS, forma de onda plana, THD visible o indicador de overload de
  la interfaz.
- **Pop / click** fuerte o repetido, o ruido raro que no estaba antes.
- **Cualquier indicio de TX/PTT/ATU**: `tx=true`, medidor de RF moviéndose, ATU tuneando, llave CW conectada.
- **Inestabilidad**: GUI que se cae/reinicia, ruta de audio que desaparece, pérdida de la radio, cambios
  solos de modo/VFO/volumen.
- **Sobrecalentamiento, olor, chispa, calentamiento del conector**, DC inesperada o descarga.
- **Molestia/dolor auditivo** del operador, o cualquier duda sobre la seguridad.

Acción: apagar, esperar, desconectar con radio apagada, registrar hora/estado/hashes, **no reintentar**
hasta entender la causa. No “arreglar” escribiendo registros ni cambiando la GUI.

## H. Cierre

- [ ] Radio apagada; cable e interfaz desconectados.
- [ ] Snapshot final read-only: `tx=false`, modo/frecuencia/volumen, hash de GUI igual a la línea base.
- [ ] Registrar filas reales en el CSV (o dejar constancia de que no hubo medición).
- [ ] `sha256` de los archivos de captura si existieron; nada de audio de terceros al repo.
- [ ] Reporte al proyecto: qué se midió, qué no, y por qué. **No** se declara PASS física: la física la
      declara la evidencia capturada, no este plan.

## Firma de la ventana (a completar por el operador)

| Campo | Valor |
|---|---|
| Fecha/hora local y UTC | |
| Operador | |
| Cable TRS / medidor usados (marca/modelo) | |
| Interfaz LINE (marca/modelo, límite de entrada, aislamiento) | |
| Atenuador (valores) | |
| `S/P MODE` | EAR PHONE |
| `tx=false` inicio / fin | |
| GUI sha256 inicio / fin | |
| Resultado P0 | `P0_PENDING_EQUIPMENT` / `P0_PASS` / `P0_BLOCKED` |
