# Branching and integration policy

The **only permanent development branch** of this repository is `main`. The former `lu2dsx/x6200-rom` branch is frozen and retained temporarily only to consolidate its remaining changes into `main` (see PR #254). Do not push new commits to it, base worktrees or features on it, or target new PRs to it.

For each implementation: fetch `origin/main`; create a dedicated worktree/branch from the verified current SHA; make focused commits; open a pull request targeting `main`; pass the applicable native, harness, QEMU and CI gates; obtain review; then merge. Delete the task branch after merging. Never commit directly to `main`. Keep hardware validation separate and do not claim a release is radio-validated solely from simulation.

Changes from an upstream branch must enter through a separately reviewed integration PR targeting `main`, with explicit compatibility checks for BASE firmware, X6200Control, DSP, settings persistence and TX/RX. Never sync divergent history by force-push or automatic `ours/theirs` conflict resolution.

**Consolidation gate:** Close PR #254 only after all intended legacy commits are incorporated (or explicitly rejected with justification), comparisons against both heads and regression tests are documented, and open PRs/worktrees/automation dependencies on the legacy branch have been migrated. Only then may the legacy branch be deleted. Deleting it prematurely risks losing unpublished work.

## Continuous integration (opt-in gates)

Los gates del CI de este repositorio son **opt-in**: por defecto, un PR **no corre nada**. Los workflows que disparan con `pull_request` (`.github/workflows/ab-testing.yml` — harness de UI A/B: Functional, Quality Contracts, Visual Regression y Performance — y `.github/workflows/boot-rootfs.yml` — guards del prune y gate del arranque) quedan en estado *skipped* en cada PR, salvo que se pidan explícitamente:

- **Agregar la etiqueta `ci` al PR** (ya creada en el repositorio). Al etiquetar, el trigger `labeled` dispara la corrida y los jobs corren (siguen vigentes los filtros de `paths` de cada workflow: la etiqueta no los reemplaza). Quitar la etiqueta (`unlabeled`) también dispara una corrida, que quedará skipped.
- **Correr el workflow a mano** con *Actions → Run workflow* (`workflow_dispatch`), que funciona sin etiqueta.

Motivo: esos jobs no despliegan ni validan el artefacto que se despliega, y costaban ~23 minutos de runner por PR (Performance 13, Visual 6, Contracts 3, Functional 1). El build/despliegue no los usa: `.github/workflows/main.yml` (imagen `sdcard.img`) corre sólo sobre **tags** y `workflow_dispatch`, nunca en PRs.

Una corrida skipped **no bloquea el merge**: el ruleset de `main` no exige esos status checks. Correr los gates cuando el cambio los amerita es responsabilidad del autor/revisor (etiquetar el PR).

**GitHub administration (required for enforcement):** configure the `main` ruleset to prohibit direct pushes and force pushes, require PR reviews and required status checks; restrict or lock pushes to `lu2dsx/x6200-rom` until it is safely deleted. Repository documentation and Actions are not substitutes for server-side branch protection.
