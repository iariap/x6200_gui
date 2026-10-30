# Fixtures sintéticos del contrato SLIP/uplink 1.0.8 — método reproducible

> ⚠️ **Todos los bytes de este documento son INVENTADOS.** No son capturas de radio, ni tráfico real, ni salida de firmware. Se construyeron aplicando **la transcripción del código estático** de la GUI 1.0.8 (payload `UNVERIFIED_VENDOR_PAYLOAD`, SHA256 `26623266…`) y se usan sólo para auditar que la transcripción del contrato es coherente. Prohibido presentarlos como evidencia de campo.

## 1. Insumos y transcripción

| Insumo | Origen estático | SHA256 |
|---|---|---|
| Tabla CRC (1024 B) | GUI 1.0.8 `.rodata` **vaddr `0x121a3c`**, file offset `0x111a3c` | `0053f121cdcaf9f510a940ba891fb2bccd41487953c5a4c0048c4a3ccdec782e` |
| Transcripción encoder/decoder | `slip_encode`@`0x0f7424`, `slip_decode`@`0x0f7494` | ver `slip-wire-contract.md` §1 |
| Longitud/CRC/ventana | `BaseComm::start`@`0x0de4bc` | ver `slip-wire-contract.md` §2 |
| Layout de payload | `MainWidget::slot_slaveReportMessage`@`0x05c854` + `x6200_flow_t` v0.1.1 | ver `slip-wire-contract.md` §3 |

Script completo usado (fuera de Git): `/home/pablo/desa/x6200/fw108-scratch/re376/fixtures.py` SHA256 `571a4703d316f86d0172e8b37722e46a200d812e5b39cc5141fa7dd1e74da263`; salida completa: `…/fixtures-output.txt` SHA256 `44e536554940dacfd11e8078c0dcead46e50e205a4ebd756dd51634e2a107b57`.

### 1.1 Encoder (transcripción, sin ejecutar el binario)

```python
def slip_encode(payload):
    out = bytearray()
    for b in payload:
        if   b == 0xC0: out += b"\xdb\xdc"   # END -> DB DC
        elif b == 0xDB: out += b"\xdb\xdd"   # ESC -> DB DD
        else:           out.append(b)
    out.append(0xC0)                         # END final único; sin END inicial
    return bytes(out)
```

### 1.2 Decoder (transcripción, sin ejecutar el binario)

```python
def slip_decode(frame, maxout):
    out = bytearray(); i = 0
    while True:
        b = frame[i]
        if b == 0xC0:
            if out: return bytes(out), "END"     # trama terminada (END no se copia)
            i += 1; continue                      # END con salida vacía: se ignora
        if b == 0xDB:
            nxt = frame[i+1]; i += 2
            b = 0xC0 if nxt == 0xDC else 0xDB if nxt == 0xDD else nxt  # laxo
        else:
            i += 1
        if len(out) >= maxout: return bytes(out), "OVERFLOW"  # -1 en el binario
        out.append(b)
```

### 1.3 CRC (transcripción de `BaseComm::start`, tabla extraída del ELF)

```python
def calc_crc32_vendor(table, data):        # len(data) múltiplo de 4
    crc = 0xFFFFFFFF
    for (w,) in struct.iter_unpack("<I", data):
        crc ^= w
        for _ in range(4):
            crc = table[(crc >> 24) & 0xFF] ^ ((crc << 8) & 0xFFFFFFFF)
    return crc
```

**Caracterización [HECHO]:** esta función, con la tabla del ELF, **no** es `zlib.crc32` ni CRC-32/MPEG-2 sobre el flujo de bytes crudo; equivale a CRC-32 MSB-first (poly `0x04C11DB7`, init `0xFFFFFFFF`, xorout 0) procesando **cada palabra u32 little-endian en orden de byte alto→bajo**. Verificado contra una implementación bit-a-bit independiente (`0x550d4818` para `bytes(range(1,17))` en ambas) y contra la tabla de `refs/X6200Control/src/low/flow.c` (idéntica byte a byte). La misma tabla/valor existe en 1.0.6 (`.rodata` vaddr `0x11cc7c`, mismo SHA256).

---

## 2. Fixture 1 — cobertura mínima de escapes (sintético)

- **Payload inventado (9 B):** `0001c002db03dbc004` (contiene `C0` en pos. 2, `DB` en 4 y 6, y `DB C0` consecutivos).
- **Wire SLIP esperado (14 B):**
  `0001dbdc02dbdd03dbdddbdc04c0`
  (desglose: `00 01 DB DC 02 DB DD 03 DB DD DB DC 04 C0`)
- **decode(wire, maxout=14) ⇒** `status=END`, `len=9`, roundtrip `True`.
- **SHA256 del wire:** `998ad8acc14ca75136af7fb8ae23a097adb905def5a9b639dadcc4709a827dcf`

Demuestra: escape de `C0` y `DB`, escape consecutivo (`DB C0`→`DB DD DB DC`), `END` final único, ausencia de `END` inicial.

---

## 3. Fixture 2 — trama completa sintética de 552 B con layout `x6200_flow_t` (sintético)

Construida con estos valores **inventados** (campo → valor):

| Campo | Offset | Valor sintético |
|---|---:|---|
| `magic` | 0x000 | `0xAA5555AA` (valor **hipotético** del contrato legacy) |
| `samples[512]` | 0x004 | `((i*7) % 201) - 100` como `int8` (patrón determinista) |
| `flag` | 0x204 | `0x00001806` (bits tx(`0x2`)+atu_status(`0x4`)+vext(`0x8`)+power_key(`0x800`)+sql_mute(`0x1000`)) |
| `dbm,tx_power,vswr,alc_level` | 0x208 | `(-80, 5, 12, 3)` |
| `vext,vbat,batcap,reserved_2` | 0x20c | `(130, 125, 77, 0x5A)` |
| `atu_params` | 0x210 | `0x000A0201` |
| `reserved_3[3]` | 0x214 | `0x11111111, 0x22222222, 0x33333333` |
| `hkey` | 0x220 | `0xC1` (`X6200_HKEY_SPCH` legacy) |
| `crc` | 0x224 | `calc_crc32_vendor(tabla, payload[0..548))` |

- **payload (552 B) SHA256:** `ad2db4be0a4e9b299db4840d2b4242efed35b881e1b6b8b6d8901c636d2b8593`
- **wire SLIP (558 B) SHA256:** `025f34b9f894229d13cc8bac5c6737c2d4efa730aeaa04699c5e2c5708873853`
- **decode(wire, maxout=558) ⇒** `status=END`, `len=552`, `crc_check=True` (con la tabla del ELF).
- El wire tiene **5 expansiones de escape** (bytes `DB` en `samples` y en el CRC; `wire.count(0xDB)=5`), por eso **552+5+1(END)=558 B**.
- **wire hex (558 B):**

```
aa5555aa9ca3aab1b8bfc6cdd4dbdde2e9f0f7fe050c131a21282f363d444b5259609ea5acb3bac1c8cfd6dde4ebf2f900070e151c232a31383f464d545b62a0a7aeb5bcc3cad1d8dfe6edf4fb020910171e252c333a41484f565d64a2a9b0b7bec5ccd3dae1e8eff6fd040b121920272e353c434a51585f9da4abb2b9dbdcc7ced5dce3eaf1f8ff060d141b222930373e454c535a619fa6adb4bbc2c9d0d7dee5ecf3fa01080f161d242b323940474e555c63a1a8afb6bdc4cbd2d9e0e7eef5fc030a11181f262d343b424950575e9ca3aab1b8bfc6cdd4dbdde2e9f0f7fe050c131a21282f363d444b5259609ea5acb3bac1c8cfd6dde4ebf2f900070e151c232a31383f464d545b62a0a7aeb5bcc3cad1d8dfe6edf4fb020910171e252c333a41484f565d64a2a9b0b7bec5ccd3dae1e8eff6fd040b121920272e353c434a51585f9da4abb2b9dbdcc7ced5dce3eaf1f8ff060d141b222930373e454c535a619fa6adb4bbc2c9d0d7dee5ecf3fa01080f161d242b323940474e555c63a1a8afb6bdc4cbd2d9e0e7eef5fc030a11181f262d343b424950575e9ca3aab1b8bfc6cdd4dbdde2e9f0f7fe050c131a21282f363d444b5259609ea5acb3bac1c8cfd6dde4ebf2f900070e151c232a31383f464d545b62a0a7aeb5bcc3cad1d8dfe6edf4fb020910171e252c333a41484f565d64a2a9b0b7bec5ccd3dae1e8eff6fd040b121920272e353c06180000b0050c03827d4d5a01020a00111111112222222233333333c10000002b0b4033c0
```

*(La repetición del patrón es la naturaleza determinista del campo `samples` sintético; el total es **558 B = 552 payload + 5 escapes + 1 END (`0xC0`)**; `wire.count(0xDB)=5`.)*

---

## 4. Fixture 3 — casos límite del decoder (sintéticos)

| Caso | wire hex | Resultado de la transcripción | Interpretación |
|---|---|---|---|
| `END` consecutivos + trama | `c0c0` + `0102` + `c0` | `status=END`, decoded `0102` | **[HECHO]** `END` con salida vacía se ignora (resync) |
| Escape inválido `DB 00` | `aa db 00 bb c0` | `status=END`, decoded `aa00bb` | **[HECHO]** `DB xx` con `xx∉{DC,DD}` consume ambos y emite `xx` (laxo) |
| Escape inválido `DB DB` | `aa db db bb c0` | `status=END`, decoded `aadbbb` | **[HECHO]** idem |
| Overflow de salida | `slip_encode(010203)` con `maxout=2` | `status=OVERFLOW`, decoded `0102` | **[HECHO]** retorno `-1` del binario cuando `out >= maxout` |
| Trama vacía `END END` | `c0c0` | sin salida; el decoder sigue | **[HECHO]** |

SHA256 de cada salida decodificada está en `fixtures-output.txt` (scratch, hash arriba).

---

## 5. Qué demuestran y qué NO demuestran estos fixtures

**Demuestran:** que la transcripción del framing (constantes/escapes/terminación/errores), de la longitud exigida (552) y del CRC (tabla del ELF, posición 548) es **autoconsistente** y reproduce el código recuperado; sirven como test de regresión de la transcripción y como insumo de futuros tests offline (cuando #377 se desbloquee).

**NO demuestran:** nada sobre los bytes reales del BASE 1.0.8, sobre el valor del magic emitido, ni sobre la autenticidad del payload. **No son capturas reales** y no deben usarse como evidencia de campo ni como golden vectors del firmware. La autenticidad sigue `UNVERIFIED` (STOP de Fase A vigente).
