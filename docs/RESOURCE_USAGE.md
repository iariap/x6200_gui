# Consumo de recursos del X6200: mediciones y decisiones

Medido el 2026-10-11 sobre la imagen del ROM alternativo (`x6200_20261011_221133.img`).

## El equipo

| | |
|---|---|
| SoC | Allwinner **A33** (`sun8i-a33.dtsi`, misma familia que el R16 del marketing del X6200) |
| CPU | **4x Cortex-A7** (`CONFIG_SMP=y`) |
| RAM | **512 MB** DDR3 (inferido del rango reservado en el DTS: ver abajo) |
| Almacenamiento | microSD (particiones BOOT + rootfs + DATA) |
| Pantalla | 800x480 por **fbdev** (`CONFIG_FB_SIMPLE`, `CONFIG_DRM_SUN4I`); el render de LVGL es por software |

## Hallazgo principal: 128 MB reservados para una GPU que no se usa

El DTS del X6200 reserva un *pool* de **CMA de 128 MB** (25 % de la RAM) marcado como
`linux,cma-default`, atado al nodo de la Mali GPU:

```dts
reserved-memory {
    mali_cma: cma@4a000000 {
        size = <0x8000000>;              /* 128 MB */
        alloc-ranges = <0x4a000000 0x8000000>;
        linux,cma-default;               /* todas las reservas CMA sin pool propio caen aca */
    };
};
```

- El firmware **no usa la GPU**: dibuja con fbdev/software, y el driver de la Mali no está
  compilado (`CONFIG_DRM_MALI_DISPLAY` es el de *display*, no el del GPU).
- Al ser `linux,cma-default`, cualquier asignación CMA sin pool propio toma memoria de ahí, así
  que **no conviene borrarlo a ciegas**: el primer paso seguro es **reducirlo** (por ejemplo a
  32 MB) y probar en la radio que el display y el DSP siguen estables.
- Con 128 MB liberados se gana ~3 veces lo que cuesta el web configurator en Python (ver abajo).

> Dato lateral: ese rango (0x4a000000 + 128 MB = 0x52000000) no entra en 256 MB, así que la RAM
> del equipo es de **512 MB**.

## El web configurator en Python

Medido ejecutando el **Python de la imagen bajo `qemu-arm-static`** (mismo binario ARM, así que
el RSS vale; los tiempos no, porque qemu no modela la velocidad del A33):

| Estado | RSS |
|---|---|
| Intérprete solo | 22,6 MB |
| + `bottle` + `bottle-sqlite` | 34,1 MB |
| + la app completa importada | 36,7 MB |
| **Servidor corriendo y sirviendo páginas** | **45,9 MB** |

En disco: 4,1 MB `libpython` + 18,8 MB de stdlib + 1,3 MB de site-packages ≈ **24 MB** (de un
rootfs de 240 MB). Es decir, el web configurator cuesta ~9 % de la RAM y prácticamente 0 % de CPU
cuando está ocioso (`poll`, sin bucles).

### ¿Conviene reescribirlo en C?

**No.** El trabajo por página es trivial (unas consultas SQLite y un template) y es una interfaz
de gestión que se usa de vez en cuando, no un camino caliente. Reescribir 2.100 líneas de Python
+ 15 plantillas + SQLite en C sería un proyecto de semanas, con riesgo alto y sin beneficio
visible, y rompería la sincronización con el fork upstream del webserver.

Lo que sí conviene, en orden de impacto:

1. **Reducir el pool de CMA de 128 MB** (hasta 3x lo que cuesta Python). Requiere prueba en la
   radio: el display y el DSP usan DMA.
2. **No tener el webserver residente**: arranca en `S96webserver` y queda ocupando 46 MB toda la
   sesión. Alternativas: prenderlo/apagarlo desde Ajustes, o que salga solo tras N minutos sin
   pedidos. Es un cambio chico y va directo con la prioridad de batería del proyecto.
3. Micro-optimizaciones (opcionales, ganan disco más que RAM): no instalar módulos de stdlib que
   no se usan, `python3 -OO`, y compilar a `.pyc` (ya está).

### Regla general

Lo que sea **camino caliente** (audio, video, control en vivo) va en C/C++ — así está hecho el
servidor de control remoto (`remote_server.cpp`) y el CAT. Python queda solo para la interfaz de
gestión, que es donde su costo no se nota.
