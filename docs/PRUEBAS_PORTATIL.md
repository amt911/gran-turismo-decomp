# Pruebas pendientes en el portátil

**Última actualización: 2026-10-19 18:10** — prueba nueva: punto 5 (fluidez en power-saver tras el arreglo del frameskip).

Lista viva de lo que hace falta probar en el portátil (Ryzen 7 PRO 5850U, gráfica integrada Vega 8).
Cuando una prueba está hecha y sus resultados están en el chat (o en `my_tests/`), se borra de aquí.

## Antes de cualquier prueba: actualizar y recompilar

```bash
git pull
tools/setup.sh                         # vuelve a aplicar los parches de lcs-recomp (hay nuevos: 0046, 0048 y 0049)
.venv/bin/python -m tools.recomp
cmake --build build -j8
git log --oneline -1                   # pega esta línea junto con los resultados
grep -c "GT patch 0049" tools/src/lcs-recomp/lcs/host/ge_renderer.cpp   # debe dar más de 0
```

`config/gt_play.ini` se copia junto a `build/gt_play` al compilar: los cambios de las pruebas se hacen en
**`build/gt_play.ini`** (o en otro fichero con `PSPRECOMP_CONFIG=<ruta>`). Comprueba que tiene `NativeGE=true`,
`InternalResolutionMode=Window` y `GpuBudgetMs=10` en `[Rendering]`.

## Medido por SSH (2026-10-19)

Con tu permiso las pruebas de rendimiento las lanzo yo en el portátil (contraseña, sin instalar claves). Resultado: en
modo de energía **power-saver** la CPU va limitada a 1,9 GHz y en carrera se saltaban 7-9 fotogramas/s (el hilo de la
GE al 80 %); en modo **performance** la carrera va fluida (0,3 saltados/s). La gráfica nunca fue el límite. Detalle en
`docs/FINDINGS.md`.

## 4. Velocidad del coche

El reloj del juego va bien (`guest time 1.00x` en tus logs). Para comparar con la consola o con PPSSPP:

- [ ] Con el Peugeot 206 de la primera carrera, ¿cuánto tarda de 0 a 100 km/h (o 0-60 mph) con el
      cronómetro del móvil? Si puedes, lo mismo en PPSSPP con el mismo coche y circuito.

## 5. Fluidez en modo power-saver (arreglo del frameskip, PR #236)

En power-saver el juego se saltaba fotogramas aunque le sobraba tiempo: GT espera cada vblank desde dos hilos y el
segundo llegaba "tarde" por el trabajo hecho entre medias, que a 1,9 GHz pasa de 1 ms. Ya está arreglado: en tu
portátil (medido por SSH desde /tmp, sin tocar nada) la carrera pasó de 3,3-3,6 fotogramas saltados por segundo (picos
de 18) a 0,12-0,15, y el juego espera a la GE la mitad (8 % en vez de 15 %).

Después de `git pull`, `tools/setup.sh`, `.venv/bin/python -m tools.recomp` y `cmake --build build -j8` (desde este
cambio, compilar ya no pisa tu `build/gt_play.ini`; comprueba que sigue con `NativeGE=true`), en power-saver:

- [ ] ¿Va fluido en menús y en carrera? ¿Notas algún tirón?
- [ ] Si quieres dejar el log: `GT_PERF_LOG=1 build/gt_play 2>&1 | grep -E '^\[perf\]' | tee my_tests/perf_powersave.txt`
      (la línea nueva `vblank lag` dice cuántas veces el juego llegó tarde a un fotograma).
