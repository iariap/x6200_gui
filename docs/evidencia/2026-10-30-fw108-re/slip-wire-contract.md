# Contrato de wire del uplink serie BASE→Linux en GUI 1.0.8 (payload `UNVERIFIED_VENDOR_PAYLOAD`)

- **Binario analizado:** `x6200_ui_v100` (ELF32 ARM EABI5, little-endian, no stripped) SHA256 `26623266bfcd3ed4d2e08b03b9f023cef809579f5ed497899670fc01f66e9b2b`, extraído del paquete 1.0.8 **no autenticado**.
- **Referencia auténtica comparada:** GUI 1.0.6 `x6200_ui_v100` SHA256 `b76331c5aa859ae98dc1819b1079a3e40489e9986c9656e0dc2c553aeb28871b`.
- **Método:** `readelf -hS`, `nm -nS`, `objdump -d` (ARM), escáner de xrefs con capstone 5.0.7, decompilador REA 6.7.0/Ghidra 12.1.4. **Sin ejecución del binario.**
- Convención de direcciones: son **vaddr del ELF** tal como las reporta `nm`/`objdump`. En este binario `.text` tiene `vaddr = fileoffset + 0x10000` y `.rodata` también (`Addr 0x106220`, `Off 0xf6220`), útil para `dd`/`xxd` en verificación.

Marca de cada afirmación: **[HECHO]** = verificado en binario/offset; **[HIPÓTESIS]** = inferencia razonada no demostrada; **[UNKNOWN]** = no resuelto.

---

## 1. Framing SLIP

### 1.1 `slip_encode` — vaddr `0x0f7424`, tamaño `0x70`, archivo `slip.c` [HECHO]

Disassembly (`objdump -d`, extracto literal):

```
f7424 <slip_encode>:
  push {r4,r5,r6,r7,r8,lr}
  mov  ip, #0                 ; ip = out_len
  add  r2, r0, r2             ; end = src + len
  mvn  r5, #36  ; 0xFFFFFFDB  -> byte 0xDB (ESC)
  mvn  r7, #35  ; 0xFFFFFFDC  -> byte 0xDC (ESC_END)
  mvn  r8, #34  ; 0xFFFFFFDD  -> byte 0xDD (ESC_ESC)
  cmp  r0, r2 ; add r3, ip,#1 ; add lr, r1, ip ; bne loop
  mvn  r2, #63  ; 0xFFFFFFC0  -> byte 0xC0 (END)
  mov  r0, r3                 ; return out_len
  strb r2, [lr]               ; out[out_len] = END
loop:
  ldrb r4, [r0], #1           ; b = *src++
  cmp  r4, #192 ; 0xc0
  strbeq r5,[lr] ; addeq r3,ip,#2 ; strbeq r7,[r6]   ; out = DB DC ; out_len += 2
  cmp  r4, #219 ; 0xdb
  strbeq r5,[lr] ; addeq r3,ip,#2 ; strbeq r8,[r6]   ; out = DB DD ; out_len += 2
  strbne r4, [lr]                                    ; out = b      ; out_len += 1
```

**Regla:** `wire = concat(escape(payload)) || 0xC0`, con `escape(C0)=DB DC`, `escape(DB)=DB DD`, resto sin cambios. **Sin `END` inicial.** Retorna la longitud total **incluyendo el `END` final**.

### 1.2 `slip_decode` — vaddr `0x0f7494`, tamaño `0x78` [HECHO]

Pseudocódigo transcrito del disassembly (equivalente al de `fixtures-method.md`, validado con fixtures):

```
slip_decode(src, dst, maxout):
  out = 0; i = 0
  loop:
    b = src[i]
    if b == 0xC0:                 # END
        if out != 0: return out   # trama terminada; END no se copia
        i += 1; continue          # END con salida vacía: se ignora (resync)
    if b == 0xDB:                 # ESC
        nxt = src[i+1]; i += 2
        if   nxt == 0xDC: b = 0xC0
        elif nxt == 0xDD: b = 0xDB
        else:             b = nxt     # secuencia inválida: se emite nxt literal (laxo)
    else:
        i += 1
    if out >= maxout: return -1   # overflow de salida
    dst[out++] = b
```

**Hechos derivados relevantes:**
- **No existe cota de longitud de entrada** en la función (no recibe `srclen`): el caller debe garantizar que hay un `END`. Es un detalle de implementación que el caller sortea pasando como `maxout` la posición del primer `END` (ver §2.3).
- **`END` consecutivos con salida vacía se saltan** (tolerancia a resync/prefijo).
- **Trama vacía** (`END END`) no produce salida; el decoder la ignora y sigue.
- **Escape inválido `DB xx` con `xx ∉ {DC,DD}` consume ambos bytes y emite `xx`** (sin código de error).
- **Retornos:** `>0` = bytes decodificados (trama terminada por `END`), `-1` = overflow de salida (`out >= maxout`). No hay otro código de error.

### 1.3 ¿Quién usa el encoder? [HECHO]

Búsqueda de xrefs en todo `.text` (escáner capstone + `objdump`): `bl slip_decode` aparece **una sola vez** (`0x0de9a0`, dentro de `BaseComm::start`). `slip_encode` **no tiene ninguna referencia** en la GUI 1.0.8: es código compilado pero no llamado (la APP no emite tramas SLIP; el encoder quedó por simetría/uso futuro). El **downlink** Linux→BASE no es serie: es I²C (ver `audio-mux-diff.md`).

---

## 2. Lector de `/dev/ttyS1`: `BaseComm::start` — vaddr `0x0de4bc`, tamaño `0x814` [HECHO]

Decompilado completo con REA/Ghidra y contrastado instrucción por instrucción con `objdump`. Log de origen: `../x6200_ui_v100/basecomm.cpp`, `void BaseComm::start()`.

### 2.1 Apertura del puerto [HECHO]

- `uart_create("/dev/ttyS1", 0x1009)` en `0x0de578`; cadena en `.rodata` vaddr `0x121e5a`. `uart_create` (`0x08a9bc`) hace `open(path, 0x102 = O_RDWR|O_NOCTTY)`, `cfsetispeed/cfsetospeed(speed)` y `tcsetattr`; `0x1009` es el valor Linux de **`B1152000`** ⇒ **1.152.000 bps 8N1 raw** (consistente con legacy `flow.c` y con la cadena de diagnóstico `stty -F /dev/ttyS1 1152000 …` vaddr `0x123917`).
- `tcflush(fd, TCIOFLUSH)` tras abrir. El fd se guarda en `BaseComm+0x23c` (`0x0de594`).
- Buffer de recepción de `uart_t`: `buf` en `uart_t+0x0c`, `len` en `uart_t+0x10`.

### 2.2 Acumulación [HECHO]

- `read()` no bloqueante; los bytes se **acumulan** en un `QByteArray` local (`sl`) con `QByteArray::append(buf, nread)` (`0x0de794`).
- Umbral de "rx buffer full": si `QByteArray::size() > 0x2800` (10240) se registra `rx buffer full, N` (`0x121ea6`) y se fuerza un intento de parseo.
- Si no hay datos (`read ≤ 0`): contador `local_44c`; a partir de la **6.ª lectura vacía consecutiva** (`>4`) y con buffer no vacío se hace `QByteArray::clear()` (resync por inactividad). Con `msleep(5)` por iteración ⇒ requiere ≈30 ms de silencio.
- Contador de timeout de UART: tras 1000 lecturas vacías se loguea `uart read time out,<fd>`.

### 2.3 Segmentación y decodificación [HECHO]

```
i = QByteArray::indexOf(0xC0)            ; primer END en el buffer acumulado (from=0)
if i >= 0x5000:  log "[ERR] SLIP Packet Too Long, length = " + i        ; 0x0de924..0x0de978
elif i >= 1:
    ret = slip_decode(qba.data(), BaseComm+0x240, maxout=i)            ; 0x0de9a0
    if ret != 552:  log "[ERR] Wrong SLIP Packet, size =" + ret + 552  ; 0x0deab4..
    else:
        memcpy(BaseComm+0x9, BaseComm+0x240, 552)                      ; 0x0de9c4
        CRC-check (ver §2.4)
        sig_slaveReport(BaseComm*, &BaseComm+0x9)  si CRC ok           ; 0x0dea30
QByteArray::remove(0, i)                  ; 0x0deaa0-0x0deab0
```

Constantes: `0x5000` = 20480 (límite de "Packet Too Long"); `0x228` = 552 = tamaño exacto exigido; `0x240` = buffer scratch de decodificación dentro del objeto `BaseComm`; `0x9` = offset donde se deposita el `basecomm_pack` (el CRC y los campos se leen con base `BaseComm+0x9`).

> **Hallazgo de código [HECHO, efecto práctico no probado dinámicamente]:** `remove(0, i)` elimina `i` bytes, es decir **deja el byte `END` en la posición 0** del `QByteArray`. Como el parseo siguiente exige `indexOf(0xC0) >= 1`, una trama siguiente contigua no se decodifica: la APP se resincroniza sólo por el `clear()` de las 6 lecturas vacías (§2.2), lo que exige huecos de ≈30 ms entre tramas. Es una fragilidad real del parser de la APP 1.0.8 (relevante para #377), no un contrato robusto de delimitación. **[HIPÓTESIS]** el BASE emite tramas con cadencia ≈35 ms (dato legacy), lo que enmascara el problema dejando silencios entre tramas.

### 2.4 Verificación de CRC [HECHO]

- La tabla CRC de 256 entradas × u32 está en `.rodata` **vaddr `0x121a3c`** (1024 B; `dd` en file offset `0x111a3c`); al decodificar una trama se copia al stack (`memcpy(local, 0x121a3c, 0x400)` en `0x0de9d4`) — patrón idéntico en 1.0.6.
- **SHA256 de la tabla binaria:** `0053f121cdcaf9f510a940ba891fb2bccd41487953c5a4c0048c4a3ccdec782e`.
- Algoritmo (`0x0de9ec..0x0dea18`): `crc = 0xFFFFFFFF`; para cada **palabra u32 little-endian** de los primeros 548 B: `crc ^= word`, y 4 veces `crc = table[(crc>>24)&0xff] ^ ((crc<<8)&0xffffffff)`. Se compara con el u32 LE en `payload[548..552)` (`0x0dea1c`: `ldr r1,[r5,#0x22d]` ⇒ `BaseComm+0x9+0x224`).
- **La tabla y el algoritmo son idénticos al contrato legacy** `refs/X6200Control/src/low/flow.c` (`calc_crc32`) — verificado programáticamente (tabla byte a byte y equivalencia del algoritmo contra implementación bit-a-bit; ver `fixtures-method.md` §2). Esto equivale a: CRC sobre los 548 B interpretados como 137 u32 LE, con CRC-32 MSB-first poly `0x04C11DB7` init `0xFFFFFFFF` **procesando cada palabra en orden de byte alto→bajo dentro de la palabra** (no es `zlib.crc32` ni CRC-32/MPEG-2 sobre el flujo de bytes crudo).
- En fallo de CRC: `QMessageLogger::debug("[ERR] CRC Failed, Header = %08X, CRC Mine = %08X, Theirs = %08X, Packet Length = %d", payload[0], crc_calc, payload[548], qba_size)` (`0x0dea88`, formato en `.rodata` vaddr `0x121edc`). **El "Header" = primer u32 del payload se imprime pero NO se compara** con ningún magic.
- Tras un intento (CRC ok o fallo) fija `BaseComm+0x238 = 1` (indicador de reporte válido) y `BaseComm+0x234 = 0`.

### 2.5 Magic [HECHO / HIPÓTESIS]

- **[HECHO]** La GUI 1.0.8 **no contiene** ninguna comprobación del magic `0xAA5555AA` en el camino de recepción: la única aparición del valor en el binario es un literal en el constructor de `DialogFactoryMode` (`0x03cefc`, ajeno al protocolo), y no hay comparación contra `payload[0]`.
- **[HECHO]** La GUI 1.0.6 **sí** lo usa para sincronizar: `indexOf` de la cadena `{AA 55 55 AA}` (`_ZNK10QByteArray7indexOfEPKci` en `0x0df22c`), con el literal `0xAA5555AA` en el pool de `BaseComm::start` (`0x0df4dc`).
- **[HIPÓTESIS]** que el payload 1.0.8 conserve `magic = 0xAA5555AA` en `payload[0]` (el layout conserva el campo y nuestra librería legacy lo usa). **No demostrado**: sin captura real ni firmware BASE decodificado.

---

## 3. Payload decodificado (`basecomm_pack`) — layout [HECHO]

Derivado de tres evidencias independientes y concordantes:
1. `MainWidget::slot_slaveReportMessage(basecomm_pack const&)`@`0x05c854` (decompilado): offsets leídos y significado.
2. `BaseComm::start` (§2.3/§2.4): tamaño exacto 552, CRC en `+0x224`.
3. `refs/X6200Control/include/.../low/flow.h` v0.1.1 (`x6200_flow_t`, packed, `sizeof`=552).

| Offset | Tipo | Campo | Evidencia en GUI 1.0.8 |
|---:|---|---|---|
| `0x000` | u32 LE | `magic`/header | sólo se imprime en el log de CRC fallido (`Header = %08X`) [HECHO] |
| `0x004` | u8[512] | `samples[512]` | `slot_slaveReportMessage`: `byte - 0x96` (quita offset 150) y resampleo/copia a 512 floats |
| `0x204` | u32 | `flag` (bitfield) | bits leídos: `0x2` tx, `0x4` atu_status, `0x8` vext, `0x10` charging, `0x20` battery_not_found, `0x40` battery_high_temp, `0x80` pa_high_temp, `0x800` power_key (`(flag&0xfff)>>11`), `0x1000` sql_mute, `0x2000` sql_fm_mute, `0x4000` dc_in_too_low, `0x8000` flag15, `0x10000` battery_low_temp. `bit0` (resync) no se lee aquí [HECHO de código] |
| `0x208` | i8 | `dbm` | `VectorSignedToFloat` (carga con signo) |
| `0x209` | i8 | `tx_power` | idem |
| `0x20a` | i8 | `vswr` | idem |
| `0x20b` | i8 | `alc_level` | idem |
| `0x20c` | u8 | `vext` | `×0.1` a float |
| `0x20d` | u8 | `vbat` | `×0.1` a float |
| `0x20e` | u8 | `batcap` | leído como u16 `(v&0xffffff)>>16` (bytes `0x20e..0x20f`) |
| `0x20f` | u8 | `reserved_2` | leído aparte `v>>24` |
| `0x210` | u32 | `atu_params` | leído como 3 bytes `0x210/0x211/0x212` |
| `0x214` | u16 | `reserved_3[0]` (bajo) | leído como `short` |
| `0x218`,`0x21c` | — | `reserved_3[1..2]` | **no leídos** por el slot (pero forman parte del CRC) |
| `0x220` | u32 | `hkey` | `QVariant(*(u32*)(p+0x220))` → `XRadioInputDev::slot_inputMessageReceiver(...,"handle_key",...)` |
| `0x224` | u32 | `crc` | validado por `BaseComm::start` §2.4 |

`sizeof = 0x228 = 552`. Endianness: ARM little-endian; u32/i8 igual que la estructura legacy packed.

**Equivalencia con `x6200_flow_t` legacy (v0.1.1): OFFSETS Y CRC IDÉNTICOS.** El único cambio de tipo respecto de la versión legacy previa a 1.0.7 es `samples`: `float[512]` → `uint8_t[512]` (commit `0ee1ae3` «Fix flow on 1.0.7 base» en `X6200Control`, `flow.h`). `hkey` en `offsetof=0x220` y `crc` en `offsetof=0x224` coinciden con `flow.h`. La librería legacy además normaliza `hkey` con `prev_hkey` si el CRC falla; **la APP 1.0.8 no hace ese fallback** [HECHO: no existe en `BaseComm::start`; puede ser un parche sólo de la librería comunitaria].

---

## 4. Comparación 1.0.6 (referencia auténtica) — el cambio aislado

`BaseComm::start` 1.0.6: vaddr `0x0dedd4`, tamaño `0x72c` (decompilado completo). Diferencias [HECHO]:

| Aspecto | GUI 1.0.6 | GUI 1.0.8 |
|---|---|---|
| Transporte | **sin SLIP**: sincroniza por magic en el stream | **SLIP** (escape + `END`) |
| Sincronización | `indexOf` de la cadena `AA 55 55 AA` en el `QByteArray` | `indexOf(0xC0)` (no hay chequeo de magic) |
| Tamaño de trama | `0x828` = **2088** B | `0x228` = **552** B |
| `samples` | `float[512]` = 2048 B en `+0x004..0x804` | `uint8[512]` = 512 B en `+0x004..0x204` |
| Flags/campos | `+0x804` en adelante (`slot` los lee con ese offset) | `+0x204` en adelante |
| hkey | `+0x820` | `+0x220` |
| CRC | tabla `.rodata` vaddr `0x11cc7c` (SHA256 **idéntica** al 1.0.8/legacy), sobre **2080** B, valor en `+0x824`, comparado en `BaseComm+0x82d` | tabla vaddr `0x121a3c`, sobre **548** B, valor en `+0x224`, comparado en `BaseComm+0x22d` |
| Buffer post-parseo | `QByteArray::clear()` (descarta todo) | `QByteArray::remove(0, i)` (ver §2.3) |
| `msleep` del loop | 1 ms | 5 ms |
| Buffer-full / too-long | `>0x2800` / (sin límite `0x5000`) | `>0x2800` / `i >= 0x5000` |
| Callers de `slip_*` | no existen (`slip.c` no está en el binario) | `slip_decode`×1, `slip_encode`×0 |

**Aislamiento del cambio:** 1.0.8 suma `slip.c` (nuevo translation unit) y reduce la trama de 2088→552 B al pasar `samples` de float a uint8, alineándola con `x6200_flow_t` legacy. Esto coincide con la nota oficial de 1.0.7 («Changed the baseband uplink data to use SLIP encoding») y con el fix de `X6200Control` para 1.0.7. **No hay ningún otro cambio en el camino serie** (el downlink I²C/audio es idéntico; ver `audio-mux-diff.md`).

---

## 5. Veredicto de contrato de wire (exploratorio)

**`wire_contract_for_received_payload = PARTIAL`**

Demostrado (**[HECHO]**):
- Framing exacto (constantes, escapes, caso de error laxo, `END` final único, sin `END` inicial del encoder).
- Longitud exacta exigida: 552 B decodificados.
- CRC: tabla y algoritmo **idénticos** al contrato legacy; posición `payload[548..552)`.
- Layout de campos offset por offset **coincide** con `x6200_flow_t` v0.1.1 (incluidos `hkey 0x220` y `crc 0x224`), y el consumidor de la APP interpreta exactamente esos offsets.
- El cambio de `samples` float→uint8 es el documentado para 1.0.7.

No demostrado / **[UNKNOWN]**:
- Valor real de `magic` emitido por el BASE 1.0.8 (la APP no lo valida; `0xAA5555AA` es [HIPÓTESIS] heredada).
- Bytes reales en el cable (no hay captura; payload además **no autenticado**).
- Comportamiento del BASE ante las fragilidades del parser (resync por inactividad).
- Semántica de los campos que la APP no usa (p. ej. `reserved_3[1..2]`, bit `resync`), aunque forman parte del CRC.

Por eso **no** se declara `SLIP_PAYLOAD_PROVEN`: el gate de salida de la fase queda en **`SLIP_FORMAT_UNVERIFIED`** por la falta de autenticidad del payload, y el contrato se reporta como `PARTIAL`. **#377 no puede implementar compatibilidad con esta base** (sigue bloqueado).
