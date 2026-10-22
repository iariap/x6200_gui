# Provenance audit — unidad #199

**Fecha:** 2026-10-22 UTC (fase A, sin radio).
**Alcance:** verificar que la evidencia 01..04 es la aceptada, que el candidato/artefacto son los
congelados y que este dossier no introduce resultados nuevos ni toca producción.

Los comandos exactos, cwd, UTC y exit están en [`commands.jsonl`](commands.jsonl); las salidas
crudas en [`logs/provenance/`](logs/provenance/).

## 1. Refs y linaje

| Ref | SHA | Cómo se verificó |
|---|---|---|
| Candidato GUI | `c888308eede71205605c06383432ee7e165f202e` | `git rev-parse c888308e` |
| Integración `assessment/rom-194` | `05020942127222c934d69ed0b37bea5895c5baaa` | `git rev-parse origin/assessment/rom-194` |
| Tip `lu2dsx/x6200-rom` | `d4fb57f8ff8ccbcfdef13fc191cc9977511b6845` | `git ls-remote` |
| Buildroot congelado | `d084e2870a00b64d9c57474e7eceeef80f43f25b` | clon privado en el SHA |
| Buildroot tip | `f23d14f975f2d12466274aeeb4932bd191f6f6e5` | `git rev-parse origin/lu2dsx/x6200-rom` |
| LVGL gitlink | `74d0a816440eea53e030c4f1af842a94f7ce3d3` | `git ls-tree HEAD lvgl` |
| `fuente-md5` | `964193ae9306b5127845cec44a57d52c` | `tools/fuente-md5.sh` |
| Ancestro | `c888308e` es ancestro de `05020942` | `git merge-base --is-ancestor` exit 0 |

## 2. Aceptación por unidad (verificada, no supuesta)

| Unidad | PR | Commit | Merge | Archivos del merge | SHA256SUMS | `sha256sum -c` |
|---|---|---|---:|---:|---:|---|
| #195 | #201 | `be6088d1` | `230b2063` | 81 | 80 | **OK** |
| #196 | #207 | `afa6109b` | `ca1698d1` | 103 | 102 | **OK** |
| #197 | #203 | `f82dbadc` | `c41f36b2` | 816 | 815 | **OK** |
| #198 | #208 | `2986d04d` | `05020942` | 273 | 272 | **OK** |

- Conteo de archivos = `git diff --name-only <merge>^1 <merge> | wc -l`; coincide con las carpetas
  (81/103/816/273) y con los `SHA256SUMS` (uno menos: el propio archivo de sums).
- `sha256sum -c SHA256SUMS` en las cuatro carpetas: **exit 0** en las cuatro
  ([log](logs/provenance/sha256sums.log)); 80/102/815/272 entradas verificadas.
- Fechas de merge/PR: #201 2026-10-21T20:58:54Z; #207 22:51:51Z; #203 22:52:51Z; #208
  2026-10-22T00:06:43Z (GitHub API).
- **Nota:** `gh pr view` devuelve como máximo 100 archivos por PR (por eso #207/#203/#208 figuran
  con 100). El conteo válido es el diff del merge, no el campo `files` de la API.

**Orden real:** `ca1698d1` (#196) es ancestro de `c41f36b2` (#197). El README de #197 se escribió
cuando #196 no estaba integrado; su fila `DEP-196` quedó **superada**. El hueco que permanece es la
ausencia de captura de los 22 diálogos inertes, no la dependencia.

## 3. Artefacto y GUI (revalidados)

Los archivos están en `assessment-194/clone-buildroot/build/images/` (fuera de Git por tamaño).
`sha256sum` y `stat` en [`logs/provenance/artifact-hashes.log`](logs/provenance/artifact-hashes.log):

| Archivo | Bytes | SHA256 | Coincide con #195/#196/#198 |
|---|---:|---|---|
| `sdcard.img` | 571.473.920 | `d0ff4435…` | sí |
| `rootfs.tar` | 242.790.400 | `7c26c76b…` | sí |
| `rootfs.ext2` (`rootfs.ext4`) | 536.870.912 | `9365b52f…` | sí |
| `boot.vfat` | 33.554.432 | `914f07eb…` | sí |
| `zImage` | 5.567.760 | `660c94c6…` | sí |
| `sun8i-r16-x6200.dtb` | 26.544 | `4d73f8a5…` | sí |
| `u-boot-sunxi-with-spl.bin` | 556.112 | `dd96b0ca…` | sí |

GUI extraído de `rootfs.tar` (`tar -xOf`): 13.407.624 B, md5 `1f998c46909307397525c975a712adfd`,
sha256 `7e2e9331c4cbc51727a098996d98e263c45dd57cb8fff8d1f2aea2964c3849c5`; `fstab` de la imagen:
`/dev/mmcblk0p3 /mnt ext4 rw 0 2`
([log](logs/provenance/gui-hash.log)). Gate #86:
`python3 tools/verifica-firmware-port.py --tar rootfs.tar` → **exit 0**, "ELF32 ARM, 13407624 B,
3/3 marcas" ([log](logs/provenance/gate86.log)).

## 4. Diff de producción y STALE

- `git diff --name-only c888308e origin/assessment/rom-194 | grep -v '^docs/'` = **0** archivos:
  la integración solo agregó documentación.
- `git diff --name-only c888308e d4fb57f8 | grep -v '^docs/'` = **25** archivos de producción
  ([log](logs/provenance/tip-diff.log)). **Corrección:** #198 declaró 26; el recuento reproducible
  es 25. No se editó el informe de #198.
- Buildroot: el artefacto se construyó con `d084e287`; el tip `f23d14f` (PR #2/#3) no se usó para
  reconstruirlo. Por eso F-11 (defconfig) está resuelto en el repo pero el artefacto medido sigue
  siendo el del par congelado con la desviación declarada.

## 5. Auditoría documental y enlaces

- `python3 tests/audita-pruebas.py` sobre el repo **sin modificar** `docs/PRUEBAS.md`:
  `builds en la tabla: 235 | secciones: 14 | enlaces: 50` → `OK: estructura y evidencia en orden`,
  **exit 0** ([log](logs/provenance/audita-pruebas.log)). Los 31 candidatos a revisión manual son
  texto histórico, no errores de esta unidad.
- `python3 scripts/check_links.py`: verifica todos los enlaces locales relativos del dossier
  (README, coverage, findings, stale, plan, hardware-remote, ASSESSMENT, PRUEBAS-row).
  Resultado final en [`logs/provenance/check-links.log`](logs/provenance/check-links.log).
- `index.json`, `artifact-manifest.json` y `acceptance-manifest.json` son JSON válidos (se
  regeneran con `scripts/build_manifests.py`).
- `SHA256SUMS` de esta carpeta cubre todos los archivos (excepto él mismo).

## 6. Límites de esta auditoría

- **No** se re-ejecutaron mediciones de 01..04; se verificó su aceptación, hashes y contenido.
- **No** se contactó la radio; la identidad del binario instalado (`3b22a03a…`) es un dato externo
  de `ORCHESTRATION.md`, no revalidado acá.
- **No** se reconstruyó el artefacto; el build del par congelado usó la desviación de `.config`
  documentada por #195/#196.
- **No** se abrió ningún ticket ni se modificó firmware/tests/CI/rootfs/src.
- La validez es solo para `c888308e` + `d084e287` + el artefacto con los SHA de la tabla; ver
  [`stale.md`](stale.md).
