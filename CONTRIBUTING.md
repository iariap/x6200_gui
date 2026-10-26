# Branching and integration policy

The **only permanent development branch** of this repository is `main`. The former `lu2dsx/x6200-rom` branch is frozen and retained temporarily only to consolidate its remaining changes into `main` (see PR #254). Do not push new commits to it, base worktrees or features on it, or target new PRs to it.

For each implementation: fetch `origin/main`; create a dedicated worktree/branch from the verified current SHA; make focused commits; open a pull request targeting `main`; pass the applicable native, harness, QEMU and CI gates; obtain review; then merge. Delete the task branch after merging. Never commit directly to `main`. Keep hardware validation separate and do not claim a release is radio-validated solely from simulation.

Changes from an upstream branch must enter through a separately reviewed integration PR targeting `main`, with explicit compatibility checks for BASE firmware, X6200Control, DSP, settings persistence and TX/RX. Never sync divergent history by force-push or automatic `ours/theirs` conflict resolution.

**Consolidation gate:** Close PR #254 only after all intended legacy commits are incorporated (or explicitly rejected with justification), comparisons against both heads and regression tests are documented, and open PRs/worktrees/automation dependencies on the legacy branch have been migrated. Only then may the legacy branch be deleted. Deleting it prematurely risks losing unpublished work.

## Validación local de PRs (sin CI de GitHub)

Por decisión del owner (2026-10-26), **x6200 no corre GitHub Actions en PRs**: un PR no dispara ningún workflow. La validación de un PR es **local**, con la matriz de checks del harness:

```bash
scripts/apps/x6200/check-matrix.sh <base> <head>
```

La matriz corre, según los paths tocados, lo que sus bloques cubren: el pytest del arnés (`tools/ab_testing`), `x6200-ab themes` y `contracts`, el gate de contraste WCAG sobre la superficie renderizada, los checks estáticos/nativos del firmware y del inventario del arnés, la identidad del build, los temas de la imagen y la sintaxis de los `.sh` tocados. El harness va a hacer fallar un diff de código que no matchee ningún bloque. Los guards del prune del rootfs y el gate de arranque neo-retro **no** están en la matriz todavía: se corren a mano desde `boot-rootfs.yml` (`workflow_dispatch`) hasta que el harness los integre. Cada check reporta `ok`/`FAIL:` con su exit code; lo que no puede correr por falta de entorno se reporta `BLOCKED` y un bloque que no aplica, `SKIP` (nunca "pasó").

Los workflows de GitHub quedan en el repositorio sólo para corridas **manuales** (*Actions → Run workflow*, `workflow_dispatch`): `.github/workflows/ab-testing.yml` (Functional, Quality Contracts, Visual Regression, Performance) y `.github/workflows/boot-rootfs.yml` (guards del prune y gate del arranque). No hay etiqueta que los dispare.

**El merge lo hace el harness exigiendo la salida real de la matriz en la sección `## Validación` del PR.** Motivo del cambio: esos jobs no despliegan ni validan el artefacto desplegado, y costaban ~23 minutos de runner por PR (Performance 13, Visual 6, Contracts 3, Functional 1). El build/despliegue no los usa: `.github/workflows/main.yml` (imagen `sdcard.img`) corre sólo sobre **tags** y `workflow_dispatch`, nunca en PRs, y no se toca.

**GitHub administration (required for enforcement):** configure the `main` ruleset to prohibit direct pushes and force pushes and require PR reviews, **without required status checks** — x6200 PRs run no GitHub CI at all, so a required check would block every merge forever; the validation is the local harness matrix (see above). Also restrict or lock pushes to `lu2dsx/x6200-rom` until it is safely deleted. Repository documentation and Actions are not substitutes for server-side branch protection.
