# Historial y evolución consultados

Consulta: GitHub API vía `gh`, 2026-10-21/22 (UTC). Estado y fechas son los devueltos por la API
en esa consulta; pueden cambiar después. **Cerrado no equivale a validado físicamente; mergeado no
equivale a incluido en el candidato.**

## 1. Issues antecedentes

| Issue | Estado | Cierre (UTC) | Tema | Lectura para este dossier |
|---|---|---|---|---|
| #86 | CLOSED | 2026-10-16T00:11:32Z | build/identidad del artefacto | gate #86 PASS revalidado sobre el artefacto #195 |
| #89 | CLOSED | 2026-10-15T22:30:06Z | doble init S20gui/S95gui | no reaparece en el artefacto (24 init, sin duplicado) |
| #94 | CLOSED | 2026-10-16T00:56:15Z | barrido de apps deja estado cambiado | riesgo de barrido; no se ejecutó barrido completo |
| #111 | CLOSED | 2026-10-16T22:56:24Z | saturación cola de eventos | antecedente, no se re-midió |
| #121 | CLOSED | 2026-10-18T05:59:55Z | cuelgue/pérdida de acceso, lastbreath | F-02/F-05 lo enlazan; no se asume resuelto |
| #131 | CLOSED | 2026-10-19T17:22:41Z | SIGSEGV por corrupción LVGL | antecedente de concurrencia; no se re-midió |
| #152 | CLOSED | 2026-10-19T04:36:19Z | WSPR revienta GUI | correspondería a un barrido de apps BLOCKED |
| #156 | CLOSED | 2026-10-19T14:42:25Z | TX real CB 27 MHz | sin instrumentos en esta campaña; no se revalida |
| #157 | CLOSED | 2026-10-19T17:06:41Z | `pwr`/NFM en control remoto | protocolo QEMU PASS en negativas; hardware pendiente |
| #172 | CLOSED | 2026-10-20T16:41:20Z | socket pisado por doble GUI | no se reprodujo en QEMU (1 GUI) |
| #188 | CLOSED | 2026-10-21T15:40:01Z | DATA vfat/read-only tras apagón | F-01/F-02/F-18 lo enlazan; el código de creación sigue creando vfat |
| #192 | CLOSED | 2026-10-21T18:32:23Z | boot animation | **no** está en `c888308e`; se integró después como #200 → STALE |

## 2. PRs de producto / fixes

| PR | Estado | Merge (UTC) | Commit | Tema |
|---|---|---|---|---|
| #151 | MERGED | 2026-10-19T03:33:38Z | `410c066b` | LVGL draw buffer 128 líneas |
| #153 | MERGED | 2026-10-19T13:06:20Z | `95eebd3b` | snapshots de espectro sin carreras |
| #161 | MERGED | 2026-10-19T17:22:11Z | `57aa2e51` | waterfall incremental |
| #164 | MERGED | 2026-10-19T18:26:20Z | `5f3c52fe` | guardas de igualdad/invalidación |
| #170 | MERGED | 2026-10-20T02:46:59Z | `b318a1f3` | identidad de fuentes embebida |
| #180 | MERGED | 2026-10-20T16:40:48Z | `b20de1d6` | socket remoto sin robar a GUI viva |
| #181 | MERGED | 2026-10-20T17:56:42Z | `f81ef7f9` | DFL inline |
| #184 | MERGED | 2026-10-20T20:23:05Z | `d32b59e8` | espejo LSB/LSB-D |
| #185 | MERGED | 2026-10-20T22:58:41Z | `6666806e` | presets DFL |
| #187 | MERGED | 2026-10-20T23:54:31Z | `23313505` | dial VFO adaptativo |
| #189 | MERGED | 2026-10-21T15:55:50Z | `89a7043e` | paridad local/CI |
| #191 | MERGED | 2026-10-21T17:38:46Z | `e6925db6` | Auto Tune v2 |
| #193 | MERGED | 2026-10-21T18:06:32Z | **`c888308e`** | **candidato**: DFL inline conserva editor al cambiar de modo |
| #200 | MERGED | 2026-10-21T18:32:02Z | `50bf8ab0` | boot animation (posterior al candidato) |
| #202 | MERGED | 2026-10-21T21:10:58Z | `9ed25fa9` | quita u-boot prebuilt |
| #206 | MERGED | 2026-10-21T23:04:03Z | `d4fb57f8` | bootanim v2 (tip STALE) |
| #201 | MERGED | 2026-10-21T20:58:54Z | `230b2063` | evidencia #195 |
| #207 | MERGED | 2026-10-21T22:51:51Z | `ca1698d1` | evidencia #196 |
| #203 | MERGED | 2026-10-21T22:52:51Z | `c41f36b2` | evidencia #197 |
| #208 | MERGED | 2026-10-22T00:06:43Z | `05020942` | evidencia #198 |

## 3. Buildroot

| PR | Estado | Merge (UTC) | Commit | Tema |
|---|---|---|---|---|
| #1 | MERGED | 2026-10-21T16:08:13Z | `d084e287` | fstab/DATA ext4 + S01create_data tolerante (ref #188) |
| #3 | MERGED | 2026-10-21T21:50:38Z | `128d96d` | quita logo de kernel |
| #2 | MERGED | 2026-10-21T21:51:25Z | `f23d14f` | defconfig LIBTOOL/LIBSNDFILE/PULSEAUDIO (resuelve F-11) |

## 4. Incidente `ca650ea` (incertidumbre del diagnóstico)

El commit anterior `ca650ea` documenta una radio inalcanzable 90 s después de desplegar el GUI
`81d087bd…`. La causa quedó pendiente. En esta campaña **no se atribuye** al software ni a la
alimentación sin prueba, y **no se usa** como demostración de una regresión nueva. Se registra
como antecedente no resuelto; la fase B debe capturar boot_id/uptime/LASTBREATH antes de concluir
nada si vuelve a ocurrir una pérdida de acceso.

## 5. Lectura de evidencia histórica

- Los resultados de #195..#198 se midieron sobre `c888308e` + `d084e287` + artefacto con hashes
  propios; **no** se reutilizan corridas de #151..#191 como si fueran del candidato.
- Los antecedentes se citan con SHA/capa/fecha; sirven para contexto y para no duplicar tickets.
- #192 no forma parte del candidato; su implementación (#200) está en el tip y es STALE.
- La ausencia de crashes en QEMU no autoriza a flashear ni demuestra ausencia de defectos.
