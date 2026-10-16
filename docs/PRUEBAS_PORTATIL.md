# Pruebas pendientes en el portátil

**Última actualización: 2026-10-16 21:00** — repetir el punto 3 con la carrera ya en marcha; quitados 1 y 2 (hechos).

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

## 3. Qué hilo se satura en el portátil (repetir, con la carrera ya en marcha)

Gracias por `hilos_bateria.txt`. Los primeros 55 s eran menús y cargas (hilos al 5-17 %) y la carrera empezó justo en
la última ventana (juego 63 %, GE 46 %), así que la carrera se quedó fuera. Lo de `MaxInternalScale=2` (algo más
fluido) apunta a que la gráfica también cuenta. Hace falta la carrera entera:

1. Arranca el juego con el log de siempre, en una terminal:
   ```bash
   GT_PERF_LOG=1 build/gt_play 2>&1 | grep -E '^\[perf\]|internal scale' | tee my_tests/perf_bateria_hilos.txt
   ```
2. **Cuando ya estés corriendo en la pista** (no en el menú), en otra terminal:
   ```bash
   .venv/bin/python -m tools.thread_cpu --name gt_play --every 5 --count 12 | tee my_tests/hilos_carrera.txt
   ```
3. Sigue en carrera el minuto que tarda (con `MaxInternalScale` como lo tengas normalmente; apunta cuál).

- [ ] Deja `my_tests/hilos_carrera.txt` y `my_tests/perf_bateria_hilos.txt`, con batería.

## 4. Velocidad del coche

El reloj del juego va bien (`guest time 1.00x` en tus logs). Para comparar con la consola o con PPSSPP:

- [ ] Con el Peugeot 206 de la primera carrera, ¿cuánto tarda de 0 a 100 km/h (o 0-60 mph) con el
      cronómetro del móvil? Si puedes, lo mismo en PPSSPP con el mismo coche y circuito.
