# Verificación: ¿el X6200 puede soportar lo mismo que el X6100?

Verificación previa a implementar (2026-10-11), pedida antes de portar features del
firmware X6100 de IU1WXY. Método: comparación de SoC/DTB, kernel, empaquetado, librería de
control y artefactos de la ROM. **Sin acceso al equipo físico**, así que lo que depende de
hardware queda marcado como *verificar en la radio*.

Referencias locales: `./.x6100_ref` (firmware X6100), `./.x6100_br` (buildroot X6100),
`./.x6100_ctl` (X6100Control), `./.x6200_ctl` (X6200Control), `./buildroot_new/AetherX6200Buildroot`.

---

## 1. Veredicto

| Capa | ¿Igual que el X6100? | Consecuencia |
|---|---|---|
| **SoC / CPU** | ✅ Sí, `allwinner,sun8i-r16` en ambos DTS | Mismo rendimiento, mismos drivers |
| **Panel / video** | ✅ Mismo panel `jinglitai,jlt4013a`, mismo TCON RGB666, `CONFIG_DRM_SUN4I=y` y `CONFIG_DRM_PANEL_JINGLITAI_JLT4013A=y` en los dos | El motor DRM/KMS del X6100 es portable a nivel usuarios |
| **Audio** | ✅ Mismo códec (`SND_SUN4I_CODEC`/`SUN8I_CODEC`/`I2S`) y misma topología PulseAudio (`alsa_input/platform-sound.stereo-fallback`) | TX digital y auriculares BT son viables |
| **CAT / serial** | ✅ Ambos abren `/dev/ttyS2` (`cat.cpp` línea 993 en X6200, 1220 en X6100) | wfview y BT-CAT portables |
| **Almacenamiento** | ✅ `S01create_data` crea la partición FAT32 `DATA` en `/mnt` al primer arranque | Grabaciones/galería tienen dónde vivir |
| **Kernel** | ⚠️ Casi idéntico (442 vs 446 líneas) **pero** falta `CONFIG_BT_HCIUART` | BT solo si el chip es USB; si es UART hay que tocar el kernel |
| **Librería de control** | ❌ El X6200 está **muy por detrás**: v0.1.1 (mayo-2026) vs v0.16.3 (sep-2026) | Faltan 24 funciones de TX/DSP fino |
| **Firmware BASE (MCU de la radio)** | ❌ **No existe equivalente**: `gdyuldin/x6100_base_patch` sí, `x6200_base_patch` → HTTP 404 | Las funciones que viven en el base del X6100 **no están verificadas** |

**Respuesta a la premisa:** es correcta **a nivel de plataforma** (el X6200 es, en Linux y
hardware, prácticamente el mismo equipo con mejoras), pero **no se puede dar por hecho a nivel
de firmware base**: varias de las funciones más vistosas del X6100 (CESSB, calibración de TX,
IF shift, filtro TX, FM pre/de-énfasis, % de batería) no viven en Linux sino en el MCU, y se
consiguieron allí **parcheando el firmware base** — algo que para el X6200 no existe.

---

## 2. Evidencia: plataforma (idéntica)

| Elemento | X6100 | X6200 |
|---|---|---|
| SoC (DTS) | `allwinner,sun8i-r16` | `allwinner,sun8i-r16` |
| Líneas del DTS | 546 | 561 (difiere en: `mmc2_pwrseq`, vbus en `r_pio`, orden de pines del encoder, **4º encoder** `rotary_4_pins`, i2c1 real vs gpio) |
| Panel | `jinglitai,jlt4013a` (SPI + TCON RGB666) | igual |
| DRM | `DRM_SUN4I=y`, `DRM_PANEL_JINGLITAI_JLT4013A=y` | igual (el driver entra por `board/X6200/linux/lcd.patch`) |
| Backlight | PWM | igual |
| Audio | `SND_SUN4I_CODEC`, `SUN8I_CODEC`, `SUN4I_I2S`, simple-card | igual |
| CAT | `/dev/ttyS2` | `/dev/ttyS2` |
| Capacidad extra | — | 4º encoder, `mmc2_pwrseq` |

Conclusión parcial: **todo lo que sea GUI, audio, red, almacenamiento y video es portable**.

---

## 3. Evidencia: kernel (una diferencia crítica)

| Opción | X6100 | X6200 |
|---|---|---|
| `CONFIG_BT_HCIUART` | `=m` | **ausente** |
| `CONFIG_BT_HCIBTUSB` | `=m` | `=m` |
| `CONFIG_BT` / `BT_RFCOMM` | `=y` / `=y` | `=m` / `=m` |
| `CONFIG_DRM`, `DRM_SUN4I`, `FB`, `FB_SIMPLE` | sí | sí |
| `CONFIG_RTL8723BS` (WiFi SDIO) | — | `=m` |

Módulos realmente presentes en nuestra ROM (`/lib/modules/6.1.82`):
`bluetooth.ko`, `btusb.ko`, **`btrtl.ko`** (Realtek por USB), `rtl8xxxu.ko`, `rtl8723bs/`.
**No hay `hci_uart.ko`.**

Interpretación: si el chip combo del X6200 está en **USB**, el Bluetooth ya es posible con lo
que hay en la ROM (solo faltaría la UI). Si está en **UART** (como el X6100, que por eso
habilita `HCIUART`), hay que habilitar el transporte y sus protocolos. **Es el amarillo #1 a
verificar en la radio.**

Otra diferencia de plataforma: el X6100 usa WiFi por SDIO (`mmc1` habilitado); el X6200 tiene
`mmc1` **deshabilitado** y el WiFi por USB (`rtl8xxxu`/rtw88) — coherente con el commit
"Add Wifi driver" de su buildroot.

---

## 4. Evidencia: librería de control (lo que el software puede pedirle a la radio)

| | X6100Control | X6200Control |
|---|---|---|
| Tag / último commit | **v0.16.3** (2026-09-29) | **v0.1.1** (2026-05-08) |
| Funciones públicas | 87 | 95 |

**24 funciones que existen en el X6100 y NO en el X6200:**

`tx_i_offset_set`, `tx_q_offset_set`, `adc_dac_gain_set`, `dac_gain_set` (calibración/IQ de TX) ·
`cessb_set`, `cessb_power_up_set` · `comp_makeup_set`, `comp_threshold_set` ·
`agc_time_fast/mid/slow_set` · `cw_peak_set`, `cw_peak_q_set` · `if_shift_set`, `if_shift_freq_set` ·
`tx_filter_low_set`, `tx_filter_high_set` · `fm_emp` · `moni_set` · `vfo_ham_band_set` ·
`get_base_ver`, `get_fw_version_str` · `bf16_flow_set`, `fftdec_set`, `dnf_update_set`.

**A favor del X6200:** tiene cosas que el X6100 no, entre ellas el **EQ de RX/TX y micrófono por
bandas**, **filtros por modo** (SSB/CW/AM/NFM/WFM), `host_cmd` (canal crudo) y variantes de
`sql`/`monitor`. Y **sí tiene** `nr_set/nr_level_set`, `nb_set/nb_level_set`,
`dnf_set/dnf_center_set/dnf_width_set` — o sea, NR/NB/DNF del equipo están disponibles.

---

## 5. Evidencia: firmware BASE (por qué la premisa no se cumple del todo)

`gdyuldin/x6100_base_patch` (25★, rama 1.1.6, actualizado 2026-10-10) **no es Linux**: son
firmwares `.xgf` que se flashean desde el propio equipo ("Firmware upgrade") y modifican el
**MCU/base** del X6100. Su changelog agrega: IF shift, `fft_span`, `bf16` flow, FM
modulación/demodulación y pre/de-énfasis, control de ganancia DAC, filtros de
decimación/interpolación, filtro bandpass de TX, arreglos de AGC en CW… y permite **calibrar
niveles de TX** ("TX codec gain", "Band output gain corr").

- `gdyuldin/x6200_base_patch` → **HTTP 404: no existe**.
- Consecuencia: las features que en el X6100 dependen del base parcheado **no se pueden portar
  desde el código**; requerirían ingeniería inversa del base del X6200 y flashearlo (riesgo alto,
  y puede invalidar la garantía/calibración del equipo).

---

## 6. Veredicto por feature

### 🟢 Verde — la plataforma ya lo permite, es trabajo de software

| Feature | Precondición verificada |
|---|---|
| DX Cluster (spots, columna, filtros) | red (WiFi) ✅ + `vfo_freq_set` ✅ + UI |
| UDP de decodificados (GridTracker/JTAlert) | plain sockets ✅ |
| Banderas `cty.dat`, mapa de QSO | datos + UI ✅ |
| Temas JSON, band plan IARU, aguja S-meter | solo GUI ✅ |
| Grabación de pantalla (AVI) | `fbdev`/DRM + CPU ✅ |
| Aviso de actualización | red ✅ |
| Web configurator ampliado (HTTPS, remotas, backup) | Python/bottle ya en el rootfs ✅ |
| **wfview** (IC-705 en red) | CAT en `/dev/ttyS2` ✅ (misma ruta que el X6100) |
| **BT CAT** (btcat) | misma ruta CAT ✅; depende del amarillo #1 |
| **Auriculares BT** (A2DP + volumen + mute) | `bluez-alsa` en el rootfs ✅ y **misma topología PulseAudio** (`alsa_input.platform-sound.stereo-fallback`) ✅; depende del amarillo #1 |
| Teclados USB/BT + **CW Keyboard** | API de keyer (`key_speed/mode/iambic/tone/vol/train/qsk`) ✅ + HID ✅ |
| **Field Ops SOTA/POTA** | log (`qso_log`) ✅, QTH ✅, red ✅, PTT ✅ |
| **JS8 / WSPR / SSTV / WeFax / NavTex** | captura + reproducción de audio ✅, `ptt_set` ✅, `record_set`/`spmode_set` ✅ |
| **DRM/KMS** para la GUI | kernel y panel ya configurados en el X6200 ✅ |
| GPS / QTH automático | el X6200 ya lo tenía y **lo quitamos nosotros**: restaurarlo es viable |

### 🟡 Amarillo — hay que verificarlo en la radio (minutos)

| Punto | Cómo se verifica | Qué decide |
|---|---|---|
| **#1 Adaptador Bluetooth** | `hciconfig -a` / `bluetoothctl list` / `dmesg \| grep -i blue` | Si aparece → BT CAT y auriculares viables con lo que ya hay. Si no, ver en qué bus está el chip y habilitar `HCIUART` (o el que corresponda) |
| **#2 Audio TX digital** | `pactl list short sinks/sources` + un PTT de prueba | Confirma ruteo y latencia para JS8/WSPR/SSTV/WeFax |
| **#3 Pipeline DRM** | `dmesg \| grep -i drm`, `ls /dev/dri` | Confirma que el motor DRM arranca (y si `/dev/fb0` es la capa de compatibilidad) antes de migrar la GUI |
| **#4 Almacenamiento** | `lsblk`, `df -h /mnt` | Que la partición `DATA` se creó y hay espacio para grabaciones/galería |

### 🔴 Rojo — depende del firmware BASE del X6200 (no hay parche equivalente)

TX Cal (IQ, band output gain, codec gain), CESSB, IF shift, filtro TX, FM pre/de-énfasis,
control de ganancia DAC, `bf16` flow, y el % de batería del base patch.

> Matiz: el X6200 ya tiene su propio **EQ de RX/TX y de micrófono** (que el X6100 no tiene), y su
> firmware de fábrica puede cubrir parte de lo que en el X6100 hubo que parchear. No se puede
> saber sin probar el equipo.

---

## 7. Recomendación

1. **Cerrar los 4 amarillos en la radio** (10 minutos, no requiere flashear nada): son los que
   pueden invalidar o confirmar el plan.
2. **Empezar por lo verde de bajo riesgo**: UDP de decodificados, aviso de actualización, aguja
   S-meter, temas JSON, band plan.
3. **Después** lo que depende de red/audio: DX Cluster, wfview, BT CAT/auriculares, web
   configurator, Field Ops.
4. **Al final** los modos digitales, de a uno (WSPR → SSTV → JS8 → WeFax/NavTex), y DRM/KMS.
5. **No prometer** TX Cal/CESSB/IF shift/filtro TX/FM emphasis hasta que exista un equivalente
   al base patch del X6100 o se revierta el firmware base del X6200.

Lo que **no** cambia: cualquiera de estos ports es código LGPL/GPL compatible, y el trabajo de
integración (audio, PTT, UI) es el mismo que ya hicimos para adaptive tuning y audio
enhancement.
