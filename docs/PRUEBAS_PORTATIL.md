# Pruebas pendientes en el portátil

**Última actualización: 2026-10-19 19:45** — prueba nueva: punto 6 (compilar con PGO, más rápido).

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

## 6. Compilación optimizada con PGO (opcional, ≈ 14 min)

En tu portátil en power-saver (medido por SSH desde /tmp) el hilo del juego bajó del 59 % al 39 % de un núcleo con
esta compilación. Después de actualizar y compilar como siempre:

```bash
tools/pgo.sh          # compila, juega sola 220 s sin ventana para medir, y recompila build/ optimizado
```

- [ ] ¿Notas el juego más suelto en power-saver? ¿Se calienta o gasta menos batería?

## 7. Tirones con la luz nocturna de KDE (y fotogramas atados a la pantalla)

Con la **luz nocturna** activada, KWin recompone la ventana del juego en cada fotograma en vez de mostrarla
directamente, y en power-saver no llega: la pantalla enseña una imagen de cada dos (30 Hz reales), aunque el juego
entregue 60. Se nota en el texto que se desplaza y en los coches que adelantan. Desactívala mientras juegas (icono de
luz nocturna en la bandeja → suspender). Medido en tu portátil en power-saver: con ella, 9 de cada 10 imágenes fuera
de su refresco; sin ella, 12 de 5103.

Además el juego ahora sigue los refrescos de la pantalla (60,05 Hz en tu panel) en vez de los 59,94 Hz de la PSP: va
un 0,18 % más rápido y ya no se cruza con la pantalla cada 9 s (el sonido lo absorbe sin cortes). `GT_DISPLAY_LOCK=0`
vuelve al reloj de la PSP para comparar.

- [ ] Con la luz nocturna apagada y en power-saver, ¿van fluidos el texto del menú y la carrera?
- [ ] ¿Oyes algún chasquido o cambio de tono en la música tras unos minutos?
