# Pruebas pendientes en el portátil

Lista viva de lo que hace falta probar en el portátil (Ryzen 7 PRO 5850U, gráfica integrada Vega 8).
Cuando una prueba está hecha y sus resultados están en el chat, se borra de aquí.

## Antes de cualquier prueba: actualizar y recompilar

```bash
git pull
tools/setup.sh                         # vuelve a aplicar los parches de lcs-recomp si han cambiado
.venv/bin/python -m tools.recomp
cmake --build build -j8
git log --oneline -1                   # pega esta línea junto con los resultados
```

`config/gt_play.ini` se copia junto a `build/gt_play` al compilar: los cambios de las pruebas se hacen en
**`build/gt_play.ini`** (o en otro fichero con `PSPRECOMP_CONFIG=<ruta>`).

## 1. Pianos y minimapa (arreglo de la PR del parche 0029)

El arreglo anterior (#163) no era la causa: la textura se leía de un render target viejo del menú.

- [ ] Con `NativeGE=true`, en la primera carrera: ¿los pianos rojos y blancos se ven con sus rayas, sin el texto
      "Single Player"?
- [ ] ¿El minimapa de arriba a la izquierda es el del circuito?

## 2. Tiempo de GPU con batería (medición de #164)

Ya tengo x4 enchufado (14 ms de GPU por frame en carrera). Falta el mismo log **con batería**, en la misma carrera:

```bash
GT_PERF_LOG=1 build/gt_play 2>&1 | grep '^\[perf\]' | tee perf_gpu_x4_bateria.txt
```

Déjalo en `my_tests/`. (Las pruebas x2/x1 ya no hacen falta: voy a reducir el coste por píxel y a añadir resolución
automática; te pediré medir de nuevo cuando esté.)

## 3. Velocidad del coche

El reloj del juego va bien (`guest time 1.00x` en tus logs). Para comparar con la consola o con PPSSPP:

- [ ] Con el Peugeot 206 de la primera carrera, ¿cuánto tarda de 0 a 100 km/h (o 0-60 mph) con el
      cronómetro del móvil? Si puedes, lo mismo en PPSSPP con el mismo coche y circuito.
