# Pruebas pendientes en el portátil

**Última actualización: 2026-10-15 23:59** — pruebas nuevas: puntos 1 a 4 (parches 0043, 0044 y 0045).

Lista viva de lo que hace falta probar en el portátil (Ryzen 7 PRO 5850U, gráfica integrada Vega 8).
Cuando una prueba está hecha y sus resultados están en el chat (o en `my_tests/`), se borra de aquí.

## Antes de cualquier prueba: actualizar y recompilar

```bash
git pull
tools/setup.sh                         # vuelve a aplicar los parches de lcs-recomp (hay nuevos: 0043, 0044 y 0045)
.venv/bin/python -m tools.recomp
cmake --build build -j8
git log --oneline -1                   # pega esta línea junto con los resultados
grep -c "GT patch 0045" tools/src/lcs-recomp/lcs/host/ge_gpu_backend_vulkan.cpp   # debe dar más de 0
```

`config/gt_play.ini` se copia junto a `build/gt_play` al compilar: los cambios de las pruebas se hacen en
**`build/gt_play.ini`** (o en otro fichero con `PSPRECOMP_CONFIG=<ruta>`). Comprueba que tiene `NativeGE=true`,
`InternalResolutionMode=Window` y `GpuBudgetMs=10` en `[Rendering]`.

## 1. Letreros lejanos con rayas blancas (parche 0045)

Tenías razón en que seguía mal, y no era la textura: era geometría. Los carteles negros de «GRAN TURISMO» tienen
una cara delantera negra y una trasera blanca en el mismo plano. El PSP no dibuja la cara de atrás (la «descarta»),
pero nuestro camino por GPU nunca descartaba caras traseras, así que la blanca se peleaba con la negra en la
profundidad y ganaba a rayas verticales. Además, el panel de abajo de esos carteles se veía como un tablero blanco
opaco cuando en el juego original se ve a través. Ahora se descartan como en el PSP.

- [ ] En la salida de la primera carrera (tu captura de las 19:10), ¿los carteles negros se ven limpios, sin rayas?

## 2. La línea de abajo (parche 0044)

Encontrada: las imágenes a pantalla completa (el aviso de autoguardado, el vídeo, los menús) salen de una textura de
512x512 de la que solo se usa la parte de arriba (480x272). Justo debajo de esa parte está la paleta de colores de la
propia imagen. A resolución normal nunca se lee, pero a resolución aumentada el filtro mezclaba un poco de esa fila
en la última línea de la pantalla: la línea de color que cambia.

- [ ] ¿Ha desaparecido la línea de abajo (vídeo de inicio, aviso de autoguardado, menús)?

## 3. Menús con coches (parche 0043)

GT pide muchas texturas con un nivel de detalle fijo y el camino por GPU las dibujaba con el nivel automático
(texturas más borrosas que en la consola). En la comparación automática con el renderizador por software, el
concesionario ha pasado de coincidir en 175 de 588 imágenes a 498 de 498, y el garaje a 596 de 596.

- [ ] ¿Se ven bien los coches en el concesionario y el garaje? (si ves algo raro, una captura)

## 4. Animaciones de las transiciones y temblor lateral

En mi máquina, a velocidad completa, las transiciones (por ejemplo el fundido al pasar de la selección de circuito
a sus opciones) existen y son iguales en el renderizador por software y en la GPU. Sospecho que en el portátil se
las come el salto de fotogramas (frameskip) cuando el juego va lento. Para comprobarlo:

```bash
GT_FRAMESKIP=0 build/gt_play
```

- [ ] Con `GT_FRAMESKIP=0` (irá más lento), ¿aparecen las animaciones al volver al menú de las tres casillas y OK?
- [ ] El temblor lateral: ¿dónde lo ves (menús quietos, carrera, vídeo)? En mi máquina los fotogramas de los menús
      quietos salen idénticos uno tras otro, así que si se mueve en un menú quieto es algo de la ventana. Prueba
      también con `UpscaleFilter=Bilinear` en la sección `[Display]` de `build/gt_play.ini`: ¿cambia algo?
      Si puedes, un vídeo corto con el móvil ayuda mucho.

## 5. Rendimiento con batería

Sigue pendiente: el siguiente paso (que te enseñaré antes de programarlo) es un camino rápido para los dibujos
típicos de GT, porque el coste ahora es fijo por cada dibujo (~660.000 por segundo en carrera). Con estos parches,
de paso, la GPU dibuja menos (las caras traseras ya no se pintan).

- [ ] Si te apetece, otro log con batería para comparar:
      `GT_PERF_LOG=1 build/gt_play 2>&1 | grep -E '^\[perf\]|internal scale' | tee my_tests/perf_bateria_0045.txt`
      (la línea `GE wait` ahora dice también dónde espera: `queue`, `drawsync`, `listsync`).

## 6. Velocidad del coche

El reloj del juego va bien (`guest time 1.00x` en tus logs). Para comparar con la consola o con PPSSPP:

- [ ] Con el Peugeot 206 de la primera carrera, ¿cuánto tarda de 0 a 100 km/h (o 0-60 mph) con el
      cronómetro del móvil? Si puedes, lo mismo en PPSSPP con el mismo coche y circuito.
