# Benchmark técnico — inteligibilidad SSB en parlante interno (patrones transferibles)

Fuentes oficiales/técnicas revisadas (no se copiaron presets ni código):
- Icom IC-7300: NR DSP, AGC, filtros y ajustes de recepción; NR "muy efectivo" pero con artefactos si
  se abusa; AGC/filtro afectan inteligibilidad ([notas AB4OJ IC-7300](https://www.qsl.net/ab4oj/icom/ic7300/7300notes.pdf), [manual p.36](https://www.manualslib.com/manual/1106166/Icom-Ic-7300.html?page=36)).
- Yaesu FT-710: NR/AGC/filtros y audio RX configurables ([notas AB4OJ FT-710](https://www.qsl.net/ab4oj/test/docs/ft710notes.pdf), [manual Yaesu](https://yaesu.com.cn/uploads/allimg/20251021/3-2510210A355C5.pdf)).
- Elecraft K4: cadena DSP RX con NR/filtros/AGC y audio de recepción orientado a operador
  ([K4](https://elecraft.com/collections/1_2023-price-changes1/products/k4-transceiver), [K4D](https://elecraft.com/products/k4d-f)).

Patrones transferibles (sin copiar implementación):
- **Adoptar:** NR conservador y conmutable; AGC/nivelador lento que no "bombea"; filtro de banda de
  audio para el parlante pequeño (evitar extremos); continuidad de estado y sin clipping digital.
- **Evitar:** NR agresivo/metálico, compresión rápida con pumping, EQ inventada "para parlante",
  presets nuevos por defecto, procesar dos veces, dependencia de audio de terceros.
- **No transferible offline:** calibración acústica del parlante interno del X6200; sin grabación
  autorizada/medición acústica no hay claim perceptual.
