# Benchmark de prácticas de radios de referencia (transferible, no medido acá)

Objetivo: qué técnicas de mejora de RX son prácticas establecidas y transferibles a X6200. Nada de
esto se midió en nuestro equipo; se listan descripciones públicas de manuales/operadores. No se
afirma que X6200 las implemente igual ni que suenen igual.

| radio/marca | técnicas públicas relevantes (descripción) | transferible a X6200 |
|---|---|---|
| Icom IC-7300/7610 | filtros IF ajustables, twin PBT, notch manual/auto, NR/AN, AGC con tiempos, NB | DFL edges/PBT-like, notch manual, NR nivel moderado, AGC slow/auto |
| Yaesu FT-710/991A | DNR/DNF, NB, contour, IF shift/width, AGC rápido/lento | NR/NB/DNF existentes, AGC/edges |
| Kenwood TS-890S | DSP NR, beat cancel/notch, filtros DSP, AGC, RX EQ | NR/NB/DNF/RX EQ nativos, AGC |
| Elecraft K3/K4 | filtros DSP de ancho variable, APF (CW), NR, notch, AGC | filtros de ancho, APF no disponible (no inventar), NR/notch nativos |
| FlexRadio SmartSDR | filtros/NR/notch/AGC y audio processing en software; EQ | mismo principio: procesamiento nativo controlable; no implica copiar algoritmo |

Fuentes públicas citadas (acceso 2026-11-04):
- Icom IC-7300 manual (mirror): [manualslib IC-7300 p36](https://www.manualslib.com/manual/1106166/Icom-Ic-7300.html?page=36)
- Notas de operador AB4OJ (Icom/Yaesu; tercera parte, no oficial): [IC-7300 notes](https://www.qsl.net/ab4oj/icom/ic7300/7300notes.pdf), [FT-710 notes](https://www.qsl.net/ab4oj/test/docs/ft710notes.pdf)
- Kenwood TS-890S (oficial): [kenwood.com TS-890S NR](https://www.kenwood.com/india/com/amateur/ts-890s/nr/)
- Elecraft K4 Operating Manual (mirror): [manualslib K4 p58](https://www.manualslib.com/manual/2194062/Elecraft-K4.html?page=58)
- Yaesu manual (oficial CN): [yaesu.com.cn PDF](https://yaesu.com.cn/uploads/allimg/20251021/3-2510210A355C5.pdf)

**Técnicas transferibles (sin copiar algoritmos):** selectividad variable antes del demod, notch/ahorro de
portadora, NR/NB de nivel moderado, AGC con tiempos y hang, EQ de recepción para el transductor, y
**control de usuario** de todo eso. **No transferible sin evidencia:** atribuir "inteligibilidad" a un
algoritmo, copiar curvas/valores específicos de otra marca, o afirmar superioridad acústica sin A/B.

**Prácticas de referencia para el A/B:** mismo ancho de banda/volumen, AGC fijo, comparaciones ciegas
cortas, señales limpias/ruidosas/débiles/interferencia/impulsos, y registrar parámetros. Ver `ab-test-plan.md`.
