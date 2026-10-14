# Pruebas pendientes en el portátil

**Última actualización: 2026-10-14 22:00** — hay pruebas nuevas (puntos 1 y 2).

Lista viva de lo que hace falta probar en el portátil (Ryzen 7 PRO 5850U, gráfica integrada Vega 8).
Cuando una prueba está hecha y sus resultados están en el chat (o en `my_tests/`), se borra de aquí.

## Antes de cualquier prueba: actualizar y recompilar

```bash
git pull
tools/setup.sh                         # vuelve a aplicar los parches de lcs-recomp (hay nuevos: 0031-0033)
.venv/bin/python -m tools.recomp
cmake --build build -j8
git log --oneline -1                   # pega esta línea junto con los resultados
grep -c "GT patch 0033" tools/src/lcs-recomp/lcs/host/ge_gpu_backend_vulkan.cpp   # debe dar más de 0
```

`config/gt_play.ini` se copia junto a `build/gt_play` al compilar: los cambios de las pruebas se hacen en
**`build/gt_play.ini`** (o en otro fichero con `PSPRECOMP_CONFIG=<ruta>`). Si tenías un `build/gt_play.ini` editado
de antes, comprueba que tiene `InternalResolutionMode=Window` y `GpuBudgetMs=10` en `[Rendering]` (es lo nuevo
por defecto) y `NativeGE=true`.

## 1. Pianos con "Single Player" (parche 0033, mergeado a las 22:00)

La causa real: la textura de los pianos está en VRAM donde el menú había dibujado "Single Player", y el backend
seguía leyendo esa imagen vieja de la GPU. Ahora una zona solo cuenta como imagen de la GPU si la GE ha dibujado
en ella en el último segundo.

- [ ] Con `NativeGE=true`, en la primera carrera: ¿los pianos rojos y blancos se ven con sus rayas, sin el texto
      "Single Player"? (Si sigue saliendo, una captura como la de antes, por favor.)

## 2. Rendimiento con batería (parches 0031-0033: menos trabajo de GPU y resolución automática)

Ahora, con `InternalResolutionMode=Window`, la resolución interna baja sola cuando la gráfica tarda más de
`GpuBudgetMs` (10 ms) por frame y vuelve a subir cuando sobra. En la terminal sale una línea
`[vulkan] GPU ... ms per frame ... internal scale x4 -> x3` cada vez que cambia.

**Con batería**, la misma carrera de siempre, unos 30-40 s conduciendo:

```bash
GT_PERF_LOG=1 build/gt_play 2>&1 | grep -E '^\[perf\]|internal scale' | tee my_tests/perf_bateria_auto.txt
```

- [ ] ¿Va fluido en carrera? ¿El audio va bien (sin retraso)?
- [ ] ¿Se nota la imagen menos nítida cuando baja la escala? (Si molesta, se puede subir `GpuBudgetMs`.)
- [ ] Deja el log en `my_tests/perf_bateria_auto.txt`.

## 3. Velocidad del coche

El reloj del juego va bien (`guest time 1.00x` en tus logs). Para comparar con la consola o con PPSSPP:

- [ ] Con el Peugeot 206 de la primera carrera, ¿cuánto tarda de 0 a 100 km/h (o 0-60 mph) con el
      cronómetro del móvil? Si puedes, lo mismo en PPSSPP con el mismo coche y circuito.
