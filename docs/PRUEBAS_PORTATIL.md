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

## 1. Pianos y minimapa (arreglo de #163)

Con `NativeGE=true`: Single Player → una carrera.

- [ ] ¿Los pianos rojos y blancos se ven con sus rayas, sin el texto "Single Player"?
- [ ] ¿El minimapa de arriba a la izquierda es el del circuito en el que estás?

## 2. ¿Cuánto tarda la gráfica por frame? (medición de #164)

Con `NativeGE=true`, **enchufado y luego con batería**, en la misma carrera:

```bash
GT_PERF_LOG=1 build/gt_play 2>&1 | tee perf_gpu_x4.txt
```

Pásame solo las líneas `[perf]` de la carrera (unos 30 s conduciendo). Ahora salen dos por cada 5 s: la de
siempre y otra que empieza por `[perf]   GPU … ms/frame`.

Repite con la resolución interna fija a x2 y a x1. En `build/gt_play.ini`:

```ini
InternalResolutionMode=Scale
InternalScale=2      ; luego 1
```

| Prueba | Enchufado | Batería |
| --- | --- | --- |
| Window (x4 a 1080p) | | |
| Scale x2 | | |
| Scale x1 | | |

Con esto sé si la gráfica integrada es el límite (el tiempo de GPU por frame baja mucho con la resolución) o no.

## 3. Parpadeo / vibración con NativeGE=true

- [ ] ¿Vibra la imagen entera (como un temblor de un píxel) o parpadean cosas sueltas (pianos, bordes,
      el HUD)? ¿En qué pantalla o en qué parte del circuito?
- [ ] ¿Pasa también con `InternalScale=1`?

## 4. Velocidad del coche

El reloj del juego va bien (`guest time 1.00x` en tus logs). Para comparar con la consola o con PPSSPP:

- [ ] Con el Peugeot 206 de la primera carrera, ¿cuánto tarda de 0 a 100 km/h (o 0-60 mph) con el
      cronómetro del móvil? Si puedes, lo mismo en PPSSPP con el mismo coche y circuito.
