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
sudo pacman -S --needed ppsspp            # para la comparación (paso 6)
sudo pacman -S --needed jdk21-openjdk     # solo si quieres Ghidra (setup.sh --with-ghidra); vale cualquier JDK >= 21
```

Y tu ISO de **Gran Turismo EU (UCES-01245, v2.00)** en `game_files/`. Se comprueba por hash; con otro
volcado los scripts se niegan a seguir.

## 1. Herramientas (≈ 10 s; segunda vez ≈ 0,3 s)

```bash
tools/setup.sh                 # venv de Python, pspdecrypt, framework lcs-recomp + nuestros parches
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
toca.

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
