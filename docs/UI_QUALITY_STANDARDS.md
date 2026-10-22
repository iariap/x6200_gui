# Estándares de calidad de la UI

Qué tiene que cumplir la interfaz, con independencia de contra qué commit se
compare (el "C" de [`docs/AB_TESTING_PLAN.md`](AB_TESTING_PLAN.md): una diff de
píxeles no es un error por sí misma, e imágenes idénticas tampoco implican UI
correcta). Los umbrales versionados viven en
[`tools/ab_testing/quality/`](../tools/ab_testing/quality/); acá se explica el
criterio.

## Puertas (gates)

Cada corrida reporta seis puertas por separado; ninguna se mezcla con otra:

| Puerta | Qué mide | En host |
|---|---|---|
| Functional | que el arnés haya corrido todos los escenarios de A y B | ejecutable |
| Visual Regression | cambios de píxeles no explicados contra la línea base | ejecutable |
| Quality Contract | reglas objetivas sobre el candidato (abajo) | ejecutable |
| Performance | costo de render relativo (píxeles volcados, flushes) | proxy x86 |
| Hardware | comportamiento en la radio real | **BLOCKED** sin equipo |
| Energy | consumo medido | **BLOCKED** sin medición física |

Hardware y Energy quedan **BLOCKED**, nunca PASS por defecto (regla 7 del plan):
el arnés corre en el host, no en la radio. `Performance` es un proxy de costo de
render del x86 bajo tiempo virtual; **no** demuestra rendimiento del Cortex-A7 ni
autonomía, y CPU% nunca se convierte en watts.

## Contratos (reglas sobre el candidato, sin mirar A)

1. **Framebuffer**: toda captura es 800×480 (el layout la da por supuesta).
2. **Contenido por región**: cada componente visible (barra superior, S-meter,
   espectro, frecuencia, cascada, botones) conserva contenido; una región en
   blanco significa un componente ausente, borrado o fuera de pantalla.
3. **Cobertura honesta**: la tabla de cobertura solo usa
   `REAL`/`PARTIAL`/`SYNTHETIC`/`UNSUPPORTED` y declara los componentes que
   corresponden. Un `PARTIAL` nunca se escribe como `REAL` (regla 1 del plan).
4. **Cambios esperados**: la única forma de que un cambio visual deje de ser
   regresión sin ser un fix. Cada uno lleva motivo, autor y **fecha de
   vencimiento**; nunca hay exenciones globales ni permanentes.
5. **Regiones de revisión**: un cambio confinado a una región marcada para
   revisión no falla solo, queda en `REVIEW REQUIRED` para que lo mire una
   persona (un cambio estético, por ejemplo).

## Presupuestos (fijados antes de correr)

En `quality/budgets.yaml`, versionados como el código: editar un umbral para que
una corrida pase es exactamente lo que el plan prohíbe.

- Tolerancia de píxel del diff y tamaño mínimo de caja.
- Área a partir de la cual un cambio se reporta como *major* (informativo).
- Deltas de costo de render aceptables para el candidato.

## Jerarquía de prioridades

Batería > performance > UX (`AGENTS.md` del repo). Todo trade-off queda
documentado en el informe de la corrida.
