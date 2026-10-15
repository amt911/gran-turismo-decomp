# Pruebas pendientes en el portátil

**Última actualización: 2026-10-15 02:01** — hay pruebas nuevas (puntos 1 a 4).

Lista viva de lo que hace falta probar en el portátil (Ryzen 7 PRO 5850U, gráfica integrada Vega 8).
Cuando una prueba está hecha y sus resultados están en el chat (o en `my_tests/`), se borra de aquí.

## Antes de cualquier prueba: actualizar y recompilar

```bash
git pull
tools/setup.sh                         # vuelve a aplicar los parches de lcs-recomp (hay nuevos: 0036 y 0037)
.venv/bin/python -m tools.recomp
cmake --build build -j8
git log --oneline -1                   # pega esta línea junto con los resultados
grep -c "GT patch 0037" tools/src/lcs-recomp/lcs/host/ge_gpu_backend_vulkan.cpp   # debe dar más de 0
```

`config/gt_play.ini` se copia junto a `build/gt_play` al compilar: los cambios de las pruebas se hacen en
**`build/gt_play.ini`** (o en otro fichero con `PSPRECOMP_CONFIG=<ruta>`). Comprueba que tiene `NativeGE=true`,
`InternalResolutionMode=Window` y `GpuBudgetMs=10` en `[Rendering]`.

## 1. Rendimiento y sonido con batería (parche 0036)

Lo que había: tu log decía `frameskip 26/s` y `GE wait 74 %`, y el audio salía a 13-14 kHz en vez de 44 kHz
(por eso iba a tirones y se cortaba). El frameskip solo se saltaba el dibujo por software: con `NativeGE=true`
el hilo de la GE seguía procesando todos los dibujos de los frames «saltados», así que el procesador nunca
recuperaba el retraso. Ahora un frame saltado de verdad no hace ese trabajo. En el ordenador de sobremesa,
limitado a 2 núcleos para parecerse al portátil, el audio pasó de 32-35 kHz a 44 kHz en carrera.

**Con batería**, la carrera de siempre, unos 30-40 s conduciendo en tercera persona y otros 30 s en primera:

```bash
GT_PERF_LOG=1 build/gt_play 2>&1 | grep -E '^\[perf\]|internal scale' | tee my_tests/perf_bateria_0036.txt
```

- [ ] ¿El sonido sigue a tirones o cortándose? (En el log, el número de `audio ... Hz` debería estar cerca de 44100.)
- [ ] ¿El coche va a su velocidad aunque la imagen vaya a menos fps? (Cuando el procesador no llega, ahora se
      muestran menos imágenes pero el juego no se ralentiza.)
- [ ] ¿Primera persona sigue yendo peor que tercera?
- [ ] Deja el log en `my_tests/perf_bateria_0036.txt`.

## 2. Perfil de energía (una sola comprobación, 1 minuto)

Con batería, KDE/GNOME suelen poner el perfil «ahorro de energía», que limita mucho la frecuencia del procesador.

```bash
powerprofilesctl get                   # apunta lo que sale
```

- [ ] Si sale `power-saver`, repite la prueba 1 con `powerprofilesctl set balanced` (y luego vuelve a dejarlo como
      estaba) y dime si cambia algo. Deja ese log en `my_tests/perf_bateria_balanced.txt`.

## 3. Hilos de decodificación (opcional, 1 minuto)

Hay hilos auxiliares que preparan los vértices en paralelo. En el sobremesa no ayudaban y por eso están
apagados, pero en el portátil con batería quizá sí:

```bash
PSPRECOMP_GE_DECODE_THREADS=2 GT_PERF_LOG=1 build/gt_play 2>&1 | grep -E '^\[perf\]' | tee my_tests/perf_bateria_hilos.txt
```

- [ ] ¿Va mejor o peor que en la prueba 1?

## 4. Letreros y vallas a lo lejos (parche 0037)

La causa: el PSP elige el nivel de detalle de cada textura (mipmap) según su pantalla de 480x272. A resolución
interna x4 la gráfica lo calculaba sobre 4 veces más píxeles y usaba texturas dos niveles más detalladas que la
consola, así que lo lejano parpadeaba y salía con dientes de sierra. Ahora elige el mismo nivel que el PSP.

- [ ] En el mismo sitio de tu captura (salida de la primera carrera), ¿los letreros y vallas del fondo se ven
      estables, sin parpadeo ni dientes de sierra? Si no, una captura como la de antes.

## 5. Parpadeo de la imagen y línea de abajo (parches 0034 y 0035)

- [ ] ¿La imagen sigue parpadeando o vibrando (menús y carrera)?
- [ ] ¿Sigue la línea de 1 píxel de colores abajo del todo (vídeo de inicio, menús)?

## 6. Velocidad del coche

El reloj del juego va bien (`guest time 1.00x` en tus logs). Para comparar con la consola o con PPSSPP:

- [ ] Con el Peugeot 206 de la primera carrera, ¿cuánto tarda de 0 a 100 km/h (o 0-60 mph) con el
      cronómetro del móvil? Si puedes, lo mismo en PPSSPP con el mismo coche y circuito.
