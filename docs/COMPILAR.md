# Compilar y probar gran-turismo-decomp

Guía para compilar el proyecto desde cero y comprobar hasta dónde llega. Todos los comandos se han
ejecutado en esta máquina (Arch Linux) el 2026-10-09. Si alguno falla, mira antes
[`docs/FINDINGS.md`](FINDINGS.md).

> **Qué esperar hoy (Fase 2a, en curso):** `gt_native` es GT recompilado a código nativo de PC, sin
> ventana, sin gráficos y sin sonido. Arranca, ejecuta el código del juego y se para en la primera
> función del sistema de la PSP que aún no está implementada. El progreso se mide comparando sus
> llamadas al sistema con las de PPSSPP ejecutando el juego original (paso 6). La primera imagen
> llegará en la Fase 2b.

## 0. Requisitos (una vez)

Paquetes de Arch (los que ya tengas se saltan):

```bash
sudo pacman -S --needed git curl unzip 7zip clang cmake ninja uv pkgconf ffmpeg openssl zlib
sudo pacman -S --needed sdl2 vulkan-icd-loader glslang   # para jugar en ventana (gt_play)
sudo pacman -S --needed ppsspp            # solo para la comparación con el original (paso 6); jugar no lo necesita
sudo pacman -S --needed jdk21-openjdk     # solo si quieres Ghidra (setup.sh --with-ghidra); vale cualquier JDK >= 21
```

Y tu ISO de **Gran Turismo EU (UCES-01245, v2.00)** en `game_files/`. Se comprueba por hash; con otro
volcado los scripts se niegan a seguir.

## 1. Herramientas (≈ 10 s; segunda vez ≈ 0,3 s)

```bash
tools/setup.sh                 # venv de Python, pspdecrypt, framework lcs-recomp + nuestros parches,
                               # fuentes de la PSP (de PPSSPP, fijadas por hash)
tools/setup.sh --with-ghidra   # además Ghidra 12.1.3 + ghidra-allegrex en tools/ghidra/ (≈ 550 MB)
```

Todo queda dentro de `tools/` y `.venv/` (ignorados por git). No usa `sudo`.

## 2. Extraer el juego (≈ 2 s)

```bash
.venv/bin/python -m tools.extract --full
```

Debe acabar con `ok: …/extracted/EBOOT.elf (sha1 8746583fa8298f6447132988a01b236a8c5994b0)`.
`--full` incluye `GT.VOL` (1 GB), que el juego necesita para ejecutarse.

## 3. Recompilar el ejecutable a C++ (≈ 30 s)

```bash
mkdir -p recomp/analysis
systemd-run --user --scope -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=0 -- \
  .venv/bin/python -m tools.recomp
```

Genera unos 156 ficheros de C++ en `recomp/generated/`. Es una traducción del código del juego: **no se
sube nunca a git** (está ignorado). Si nada ha cambiado, dice `up to date` y no hace nada.

## 4. Compilar `gt_native` (≈ 3–4 min la primera vez; luego solo lo que cambie)

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
systemd-run --user --scope -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=0 -- \
  cmake --build build -j8
```

Cada fichero generado necesita ~370 MB al compilarse; `-j8` cabe en el techo de 6 GB. Si tienes poca
RAM libre, baja a `-j4`.

## 5. Ejecutarlo (≈ 1 s hoy)

```bash
build/gt_native --max-seconds 30 --trace recomp/analysis/gt.trace
```

Verás algo como:

```text
Loaded extracted/EBOOT.elf (243383 relocations)
Registered 257… generated functions
Running from entry 0x0880410C
Runtime stopped: Missing HLE import <librería>::0x<NID>
Traced 3853 import calls to recomp/analysis/gt.trace
```

La última línea `Runtime stopped:` dice dónde se para: una función del sistema que falta
(`Missing HLE import`) o una instrucción que el recompilador aún no traduce (`Unsupported Allegrex
instruction`).

### Rutas con partida guardada (puerta diferencial)

Las rutas `save_*` arrancan desde una partida terminada del usuario: la carpeta `UCES01245-GAMEDAT` de una PSP o de PPSSPP
(versión europea) va en `game_files/` (nunca en git) y se comprueba con `config/savedata.sha1`; la puerta la copia limpia
para cada ruta. Sin ella, o con otra, la puerta se niega con un mensaje. El HLE de `sceUtilitySavedata` la descifra como
el firmware (modo 5, con la clave que pasa el juego); las claves de KIRK se leen al compilar del checkout de pspdecrypt que
ya baja `tools/setup.sh` (`build/generated/kirk_slot_keys.hpp`) y se comprueban con `config/kirk_slot_keys.sha256`.

### Ver los gráficos (volcado de frames)

`gt_native` no abre ventana: el renderer por software dibuja en la VRAM emulada y, si se lo pides,
guarda cada N frames como imagen PPM:

```bash
GT_FRAME_DUMP=recomp/frames GT_FRAME_EVERY=60 build/gt_native --max-seconds 120
ls recomp/frames            # frame_000060.ppm, frame_000120.ppm, …
magick recomp/frames/frame_001500.ppm logo.png    # o ábrelos con cualquier visor (gimp, feh…)
```

`GT_FRAME_EVERY` vale 60 por defecto (≈ 1 imagen por segundo de juego). Las imágenes son salida del
juego: se quedan en `recomp/` (ignorado por git), **no se suben**.

Sin ventana no hay mando, pero se pueden simular pulsaciones con `GT_PAD="<vblank>:<BOTÓN>,…"`
(cada una dura 6 vblanks salvo que pongas `:<frames>`, p. ej. `20000:CROSS:600` mantiene X 5 s).
Aquí un «vblank» es una llamada del juego a `sceDisplayWaitVblankStart`: GT hace dos por refresco de
pantalla, así que **120 ≈ 1 s**. Botones: `START SELECT CROSS CIRCLE SQUARE TRIANGLE
UP DOWN LEFT RIGHT L R`. Por ejemplo, para saltar el vídeo de introducción:

```bash
GT_PAD="3300:START" GT_FRAME_DUMP=recomp/frames build/gt_native --max-seconds 90
```

Una ruta también puede **esperar al juego en vez de a un tiempo**: `WAIT:seek=0x1cf2a980` detiene el guion hasta que
el juego hace `seek` a ese offset de `GT.VOL` (así carga cada página de menú: ese offset es la entrada de la
página, FACTS § «Page loads as screen signals»), y los tiempos que siguen se cuentan desde ese momento hasta el
siguiente `WAIT`: `WAIT:seek=0x1cf2a980,300:CROSS` pulsa X 300 vblanks después de que cargue la página. Si la espera no
se cumple en 12000 vblanks (`WAIT:...:<timeout>` lo cambia) la ejecución termina con código **97** y un mensaje
`[pad] WAIT ... timed out` (nunca un fallo silencioso). `WAIT:word@0x08XXXXXX=<valor>` espera a una palabra de RAM.
Dentro de un tramo los tiempos son desplazamientos desde el último `WAIT` (no se suman: `300:RIGHT,400:RIGHT`
pulsa a los 300 y a los 400). Los ficheros de `config/pad_routes/` pueden ocupar varias líneas y llevar líneas `#`.
`GT_PAD_LOG=1` imprime cada espera cumplida; `GT_IO_LOG=1` lista los `seek` con su vblank.
Para escribir una ruta: `tools/pad_route.sh <ruta> <segundos> <dir>` la ejecuta con volcado de imágenes y
`python -m tools.frame_sheet seeks|sheet|waits` resume los `seek` por pantalla y hace una hoja de contacto de los
fotogramas (las imágenes son salida del juego: déjalas bajo `recomp/analysis/`).

Los textos necesitan fuentes de la PSP (PGF). `tools/setup.sh` baja las del repositorio de PPSSPP
(etiqueta v1.20.4, comprobadas con `config/ppsspp_fonts.sha256`) a `tools/src/ppsspp-fonts/` (ignorado
por git); no hace falta tener PPSSPP instalado. Otras: `GT_FONT_DIR=/ruta/a/flash0/font`.

Las partidas guardadas van a `recomp/savedata` y se borran al arrancar (cada ejecución empieza igual);
con `GT_SAVEDATA_DIR=/ruta` se conservan. `GT_GE_LOG=<vblank>` vuelca los dibujos de las listas GE del
primer vblank desde ese que envía alguna (texturas, transferencias, estados); con `GT_GPU_DUMP` el registro dice
el vblank de cada frame volcado (`[gpu-dump] frame N vblank V`); `GT_GE_LOG_DETAIL=1` añade, por cada dibujo, el estado
donde suelen esconderse las diferencias (tipo de vértice, luces, función de textura, mapeo, mezcla, test alfa/z).
En `gt_play`, **F8** hace lo mismo con el frame siguiente, sin saber el vblank de antemano: escribe
`ge_trace_vblank<N>.txt` (con el detalle) en el directorio desde el que se lanzó.

Para llegar a una carrera (Single Player → Single Race → coche → pista → salida), unos 3–4 min:

```bash
GT_FRAME_DUMP=recomp/frames GT_FRAME_EVERY=200 \
GT_PAD="3300:START,6600:CROSS,7800:CROSS,9000:CROSS,10200:CROSS,11400:CROSS,12600:CROSS,13800:CROSS,15000:CROSS,16200:CROSS,17400:CROSS,18600:CROSS,19800:CROSS,21000:CROSS" \
build/gt_native --max-seconds 480
```

Las imágenes a partir de `frame_019800.ppm` son la parrilla y la carrera.

`GT_MPEG_LOG=1` explica de dónde sale cada vídeo y si se decodifica; `GT_IO_LOG=1` lista cada
`seek`/`read` de ficheros (para compararlo con el log de PPSSPP). `GT_PROBE=0x…` también muestra el
segundo argumento y, si es texto, la cadena (rutas de fichero, nombres).

## 5b. Jugarlo en ventana (`gt_play`)

`cmake --build build -j8` compila también `build/gt_play` si el sistema tiene SDL2, el loader de
Vulkan y glslang (las cabeceras de Vulkan las baja `tools/setup.sh`). Es el mismo juego que
`gt_native`, pero con ventana (título «Gran Turismo»), teclado/mando y sonido, al ritmo de la PSP.
Se lanza desde la raíz del repositorio (lee `extracted/`):

```bash
build/gt_play
```

- **Teclado:** W / Enter / Espacio = Cruz (acelerar, aceptar) · S / Shift = Cuadrado (frenar) ·
  A / D = girar (stick) · flechas = cruceta (menús) · Retroceso = Círculo (atrás) · F = Triángulo ·
  Q / E = L / R · Escape = START · V = SELECT · F10 = ajustes del host · F11 = pantalla completa.
- **Mando:** gatillo derecho / A = acelerar, gatillo izquierdo / X = frenar, stick izquierdo = girar.
- Ajustes en `build/gt_play.ini` (se copia de `config/gt_play.ini` solo si no existe; después es tuyo: compilar o hacer `git pull` no lo pisa — para volver a la plantilla, bórralo y vuelve a configurar): escala de la ventana,
  FPS, controles. La GE se dibuja con el rasterizador por software (el verificado); el backend Vulkan
  de la GE (`NativeGE=true`) aún no está adaptado a GT.
- **GPU (alta resolución y fluido):** en `build/gt_play.ini` pon `NativeGE=true` en `[Rendering]`. Con
  `InternalResolutionMode=Window` (el valor por defecto) la resolución interna sigue a la ventana: el
  menor múltiplo entero de 480x272 que la cubre (1080p: x4), también al redimensionar o en pantalla
  completa; la terminal escribe `[vulkan] internal resolution ...` en cada cambio. En GPUs modestas
  (móvil, Raspberry Pi) baja `MaxInternalScale` (2 = 960x544). `[window] ... presenter=vulkan` confirma
  que dibuja la GPU. Diferencias conocidas con el software: reflejos del coche en el concesionario y
  alguna transición de pantalla.
- **Ritmo de frames:** `GT_PRESENT_TRACE=1` escribe al salir un histograma del tiempo entre frames
  nuevos, en tiempo del juego (`guest`) y real (`host`); a 60 fps la barra alta debe estar en 15-18 ms.
- **Gate de la GPU** (en desarrollo): `tools/gpu_gate.sh build recomp/analysis/gpu/run` lanza las tres
  rutas sin ventana pintando con la GPU y con el software a la vez, y compara cada frame
  (`tools/gpu_compare.py`; `GT_GPU_STATS=1` cuenta los modos de blending). Hoy falla: ver ROADMAP.
  Los frames de carrera fallan siempre porque el software no aplica el nivel automático de mipmap;
  `GPU_GATE_LOD0=1` hace que la GPU use el nivel 0 como el software, y así la carrera compara todo lo demás.
  Dos ejecuciones no corren los mismos frames (la carga tarda distinto): compara ventanas del mismo contenido.
- `GT_PAD` y `GT_FRAME_DUMP` también funcionan aquí. Sin pantalla: `SDL_VIDEODRIVER=offscreen`.
- **¿Va lento?** `GT_PERF_LOG=1 build/gt_play` escribe cada 5 s en la terminal el ritmo real:
  `guest time 1.00x` es velocidad de PSP; `vblanks` debería rondar 120/s (GT espera dos por refresco);
  `skipped` son vblanks que el PC no llegó a hacer (los menús y el sonido de GT avanzan por frame, así
  que eso se nota como lentitud); `GE wait` es el tiempo que el juego pasa esperando a la GE (alto =
  el juego va a cámara lenta aunque `guest time` diga 1.00x; fíjate también en `audio`, que debería
  rondar 44100 Hz), y `frameskip` los frames que no se dibujaron para no ir lento. Con `NativeGE=true`, la
  terminal dice cada 300 imágenes descartadas porque la GPU no llegaba (`[vulkan] N frames dropped`).
  `GT_SIM_GPU_MS=N` (pruebas) simula una GPU que tarda N ms por imagen; `GT_GPU_NO_DROP=1` no descarta nunca.
- **Frameskip** (activado en `gt_play`; `GT_FRAMESKIP=0` lo quita): si el PC va tarde, no se dibuja
  en pantalla ese frame (como el frameskip de PPSSPP), pero el juego y el sonido siguen a su ritmo y
  la ventana mantiene la última imagen completa. Dibujar es casi todo el coste en CPUs modestas.
- Sonido: efectos, motor y música. `GT_AUDIO_DUMP=<dir>` (también en `gt_native`) escribe la mezcla en
  `<dir>/channel_7.wav`.
- Partidas: se guardan en `build/savedata/` y se cargan al arrancar (`GT_SAVEDATA_DIR` para otra carpeta).
- Pendiente: el audio de los vídeos.

## 5c. Más rápido con PGO (opcional, ≈ 14 min, clang)

`tools/pgo.sh` compila una versión instrumentada, juega sola 220 s de la ruta de carrera (sin ventana, partida
nueva) para medir qué código se usa, y vuelve a compilar `build/` optimizado con ese perfil:

```bash
tools/pgo.sh            # o: tools/pgo.sh build 220   (directorio y segundos de la partida de perfil)
```

En el portátil en power-saver el hilo del juego pasó del 59 % al 39 % de un núcleo y la GE de 508 a 470 ns por
dibujo, con el mismo resultado. El perfil (`recomp/analysis/pgo/gt_play.profdata`) sale de jugar tu copia del juego y
no se sube; tras cambios grandes vuelve a ejecutar el script (un perfil viejo sigue ayudando, solo deja sin optimizar
lo que cambió). Para quitarlo: `cmake -S . -B build -DGT_PGO_PROFILE=` y compilar.

## 6. Comparar con el juego original en PPSSPP

La referencia se graba una vez (30 s, ~190 MB de log; va a disco, no a `/tmp`, que aquí está en RAM):

```bash
systemd-run --user --scope -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=0 -- \
  PPSSPPHeadless "game_files/Gran Turismo (v2.0) [UCES-01245].iso" -l --timeout=30 --graphics=software \
  > recomp/analysis/ppsspp.log 2>&1
```

Y cada vez que pruebes:

```bash
.venv/bin/python -m tools.trace_compare recomp/analysis/ppsspp.log recomp/analysis/gt.trace
```

Te dice cuántas llamadas al sistema coinciden en orden con las del juego original
(`matched N calls (gate: 2000)`) y, si hay, la primera diferencia. La Fase 2a se da por buena con 2.000.

## 7. Tests

```bash
.venv/bin/pytest --cov --cov-fail-under=80   # herramientas Python
ctest --test-dir build                        # tests C++ del host (comparación del harness)
```

## 8. Probar las funciones decompiladas (harness diferencial)

Las funciones ya decompiladas viven en `src/` y se registran solas. Para comprobar que hacen
exactamente lo mismo que el código original del juego:

```bash
build/gt_native --max-seconds 30 --decomp diff
```

Al final imprime una línea `[decomp]` por función: casos comparados, iguales, discrepancias y
cuántos casos cambiaron memoria. Si alguna función difiere, el programa sale con código **2** y
muestra la primera diferencia (registro o byte de RAM, con los argumentos de la llamada).
`--decomp on` usa las versiones decompiladas en lugar de las originales; `off` (por defecto) no las
toca. Para que lleguen casos de más partes del juego, deja que llegue al menú:
`GT_PAD="3300:START" build/gt_native --max-seconds 150 --decomp diff --decomp-cases 32`.

**Las nueve rutas con las que se verifica todo** están en `config/pad_routes/`: `race.pad` (hasta la
carrera, 600 s; espera a las páginas con `WAIT:seek`), `dealer.pad` (concesionarios y ficha de un coche, 300 s), `garage.pad` (récords,
garaje, Theatre y la lista de circuitos, 310 s), `buy.pad` (compra un coche y lo añade a Favoritos,
150 s), `options.pad` (cambia las unidades a CH, ft-lb y km/h, las guarda y abre el concesionario, 240 s),
`drift.pad` (Drift Trial, 200 s), `challenge.pad` (Driving Challenges A-1 hasta la pantalla de resultado, 180 s),
`dirt.pad` (carrera en un circuito de tierra con el menú de pausa y un reinicio, 260 s) y `manual.pad` (Ad Hoc Mode y
el manual de usuario, 190 s). Cada función registrada debe salir sin discrepancias en todas
(`.venv/bin/python -m tools.decomp_gate full TAG` las lanza):

```bash
for r in race:600 dealer:300 garage:310 buy:150 options:240 drift:200 challenge:180 dirt:260 manual:190; do
  GT_PAD=$(grep -v '^#' config/pad_routes/${r%:*}.pad) build/gt_native --max-seconds ${r#*:} --decomp diff \
    | grep -c "mismatched [1-9]"   # 0 en cada ruta
done
```

**Progreso** (funciones decompiladas / total y bytes, juego y librerías por separado):

```bash
# una vez: lista de funciones según Ghidra (≈ 1 min, local, no se sube)
systemd-run --user --scope -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=0 -- \
  env JAVA_HOME=/usr/lib/jvm/java-27-openjdk MAXMEM=3G \
  tools/ghidra/ghidra_12.1.3_PUBLIC/support/analyzeHeadless ghidra gt -process EBOOT.elf -noanalysis \
  -readOnly -scriptPath tools/ghidra_scripts -postScript ExportFunctions.java $PWD/recomp/analysis/functions.tsv
.venv/bin/python -m tools.progress
.venv/bin/python -m tools.progress_map   # treemap (docs/progress_map.svg), página interactiva (docs/progress/index.html) y barras del README
```

## Atajo: todo de una vez

```bash
tools/setup.sh && .venv/bin/python -m tools.extract --full && mkdir -p recomp/analysis && \
systemd-run --user --scope -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=0 -- sh -c '
  .venv/bin/python -m tools.recomp &&
  cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ >/dev/null &&
  cmake --build build -j8' && \
build/gt_native --max-seconds 30 --trace recomp/analysis/gt.trace; \
.venv/bin/python -m tools.trace_compare recomp/analysis/ppsspp.log recomp/analysis/gt.trace
```

## Si algo va mal

| Síntoma | Qué hacer |
| --- | --- |
| `SHA1 mismatch` en extract | La ISO no es la UCES-01245 v2.00 esperada. |
| `patch does not apply` en setup | `rm -rf tools/src/lcs-recomp` y vuelve a ejecutar `tools/setup.sh`. |
| `Generated code missing` en cmake | Falta el paso 3. |
| La compilación va muy lenta o el equipo se queda sin RAM | Baja `-j8` a `-j4`; comprueba RAM libre con `free -h`. |
| `matched 0` en el paso 6 | Falta `recomp/analysis/ppsspp.log` (graba la referencia) o el `gt.trace` es de otra ejecución. |
