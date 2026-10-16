# Pruebas pendientes en el portátil

**Última actualización: 2026-10-16 12:56** — prueba nueva: punto 1 (parche 0048, el temblor).

Lista viva de lo que hace falta probar en el portátil (Ryzen 7 PRO 5850U, gráfica integrada Vega 8).
Cuando una prueba está hecha y sus resultados están en el chat (o en `my_tests/`), se borra de aquí.

## Antes de cualquier prueba: actualizar y recompilar

```bash
git pull
tools/setup.sh                         # vuelve a aplicar los parches de lcs-recomp (hay nuevos: 0046 y 0048)
.venv/bin/python -m tools.recomp
cmake --build build -j8
git log --oneline -1                   # pega esta línea junto con los resultados
grep -c "GT patch 0048" tools/src/lcs-recomp/lcs/host/ge_renderer.cpp   # debe dar más de 0
```

`config/gt_play.ini` se copia junto a `build/gt_play` al compilar: los cambios de las pruebas se hacen en
**`build/gt_play.ini`** (o en otro fichero con `PSPRECOMP_CONFIG=<ruta>`). Comprueba que tiene `NativeGE=true`,
`InternalResolutionMode=Window` y `GpuBudgetMs=10` en `[Rendering]`.

## 1. El temblor de la imagen (parche 0048)

Tu vídeo lo dejó clarísimo: con el coche parado, toda la escena 3D salta entre dos posiciones fotograma a
fotograma (1-2 píxeles), y el marcador no se mueve. Es cosa del propio juego: GT desplaza la cámara medio píxel
en un fotograma sí y en otro no, un truco de antialiasing que en la pantalla del PSP (lenta) se mezclaba en una
sola imagen. En una pantalla moderna se ve como temblor. Ahora se quita ese medio píxel.

- [ ] En la misma contrarreloj del vídeo, con el coche parado, ¿sigue temblando la imagen?
- [ ] Si quisieras compararlo con el temblor original: `KeepTemporalJitter=true` en la sección `[Rendering]`
      de `build/gt_play.ini` lo vuelve a poner.

## 2. Coches en el concesionario

En tu captura del DeLorean no sé qué ves raro (en la comparación automática con el renderizador por software
salen idénticos). ¿Qué es lo que no cuadra: los reflejos de la carrocería, los cristales, las ruedas, el brillo?
Si puedes, una captura del mismo coche en PPSSPP para comparar.

## 3. Rendimiento y sonido con batería

Tu último log (`perf_bateria_0045.txt`): en carrera se saltan 30-50 fotogramas por segundo, el juego espera a la
GPU un 35 % del tiempo y el audio baja a 42.000 Hz en los tramos más duros (de ahí los cortes). Estoy con el
camino rápido de la GE que aprobaste (la primera parte ya está en `main`; la segunda, la grande, en marcha). Te
aviso con otra entrada cuando esté.

## 4. Velocidad del coche

El reloj del juego va bien (`guest time 1.00x` en tus logs). Para comparar con la consola o con PPSSPP:

- [ ] Con el Peugeot 206 de la primera carrera, ¿cuánto tarda de 0 a 100 km/h (o 0-60 mph) con el
      cronómetro del móvil? Si puedes, lo mismo en PPSSPP con el mismo coche y circuito.
