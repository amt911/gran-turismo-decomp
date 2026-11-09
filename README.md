# gran-turismo-decomp

Decompilación de **Gran Turismo para PSP** (2009), versión europea **UCES-01245 v2.00**: reconstruir el
ejecutable del juego (`EBOOT.BIN`, módulo `PDIAPP`) como **C/C++ legible que compila y corre en otras
plataformas** (PC primero), para poder **modificar el juego** desde su código fuente.

Cómo: el ejecutable se traduce entero a C++ con [PSPRecomp](https://github.com/jessicanataliagta/PSPRecomp)
para que el juego corra en PC desde el principio, y después cada función traducida se sustituye por
código decompilado a mano, comprobando que se comporta exactamente igual que la original. No se busca
un binario idéntico byte a byte (los compiladores exactos de Sony no son públicos).

> **Estado (2026-11-09)**
>
> - **🎮 El juego se juega en PC:** `build/gt_play` (ventana SDL, teclado o mando, sonido y música, partidas
>   guardadas): carreras, contrarreloj, concesionario y garaje. Dibuja con
>   la GPU (Vulkan) o por software. Controles y ajustes: [`docs/COMPILAR.md`](docs/COMPILAR.md) §5b.
> - **🧩 Decompilación:** 16.783 de 23.208 funciones (72 %) ya son C/C++ legible (a mano o con plantilla), comprobado contra el
>   original con el harness diferencial: cada una deja la misma salida y la misma memoria que la recompilada en
>   las rutas de prueba (carrera, concesionario, garaje, opciones, Trial Mountain…) o, si ninguna ruta la alcanza,
>   en casos sintéticos. 1.534 funciones tienen ya su nombre real, cada uno con su evidencia en
>   `config/symbol_addrs.txt`. Barras y mapa abajo; informe: `.venv/bin/python -m tools.progress`.
> - **🔧 Mods nativos** al estilo Dusklight, sin tocar el código del juego: en
>   [amt911/gt-mods](https://github.com/amt911/gt-mods) (pedales analógicos, cámara orbital, distancia de
>   dibujado, volver a la pista, vuelcos, trucos). Menú en juego con F9. Cómo escribirlos: [`docs/MODS.md`](docs/MODS.md).
> - **Siguiente:** seguir decompilando por lotes, descargador de mods dentro del juego, más de 60 fps con
>   interpolación, deformación por choques. Detalle en [`docs/ROADMAP.md`](docs/ROADMAP.md).

## Mapa de lo que queda

<!-- progress:start -->
```text
FUNCIONES   █████████████████████████████░░░░░░░░░░░  72.32 %  16783 / 23208
BYTES       ██████████████████████░░░░░░░░░░░░░░░░░░  55.49 %  2316932 / 4175084
CON NOMBRE  ███░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   6.61 %  1534 / 23208
```
<!-- progress:end -->

![Mapa de la decompilación](docs/progress_map.svg)

Cada rectángulo es una función del ejecutable, con área proporcional a sus bytes: verde = decompilada
a mano, turquesa = decompilada con una plantilla, gris = pendiente. Los grupos son tramos de 256 KiB
del código del juego por dirección, y aparte el SDK y las librerías de Sony (frontera aproximada).
Todas las decompiladas están verificadas contra la original con el harness diferencial.

**Versión interactiva** (pasa el ratón por encima para ver nombre, dirección, tamaño y estado, o busca
una función): [mapa interactivo en línea](https://claude.ai/artifact/V93Bh8ZWDTAUGfWaoZa5hq), o
[`docs/progress/index.html`](docs/progress/index.html) en el navegador tras clonar. Se regenera todo (mapa, página y barras) con `.venv/bin/python -m tools.progress_map`; solo
contiene direcciones, tamaños y nombres propios del proyecto.

## Qué hay y qué no

- **Este repo no contiene datos del juego** — ni ISO, ni ejecutables (cifrados o no), ni assets, ni
  el C++ que genera el recompilador, ni compiladores propietarios. Necesitas **tu propio volcado** del
  UMD.
- El volcado se verifica por hash. El esperado es:

  | Fichero | SHA1 |
  | --- | --- |
  | `Gran Turismo (v2.0) [UCES-01245].iso` | `c84473375e3cd000d8f14c5905e6514389fd6a40` |
  | `EBOOT.BIN` descifrado (ELF) | `8746583fa8298f6447132988a01b236a8c5994b0` |

- **Fuera del alcance**: los scripts Adhoc (ya cubiertos al 100 % por
  [OpenAdhoc](https://github.com/Nenkai/OpenAdhoc)), los assets de `GT.VOL`
  ([GTPSPVolTools](https://github.com/Nenkai/GTPSPVolTools)) y los módulos PRX de Sony.

## Empezar

Guía rápida (Arch Linux; detalle y otros usos en [`docs/COMPILAR.md`](docs/COMPILAR.md)):

```bash
# 0. Una vez: dependencias del sistema
sudo pacman -S --needed git curl unzip 7zip clang cmake ninja uv pkgconf ffmpeg openssl zlib \
                        sdl2 vulkan-icd-loader glslang

# 1. Tu ISO (UCES-01245 v2.00) en game_files/ — se comprueba por hash
cp "/ruta/Gran Turismo (v2.0) [UCES-01245].iso" game_files/

# 2. Herramientas fijadas, extraer el EBOOT y recompilarlo a C++ (todo queda fuera de git)
tools/setup.sh
.venv/bin/python -m tools.extract --full     # --full: también GT.VOL, que el juego necesita
.venv/bin/python -m tools.recomp

# 3. Compilar (≈ 4 min la primera vez; con tope de memoria)
systemd-run --user --scope -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=0 -- \
  sh -c 'cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ && cmake --build build -j8'

# 4. Jugar desde la raíz del repo (ventana, teclado o mando, sonido; partidas en build/savedata/)
build/gt_play
```

Controles por defecto: W / Enter = acelerar y aceptar, S = frenar, A / D = girar, flechas = menús,
Retroceso = atrás, Escape = START, F10 = ajustes, F11 = pantalla completa (mando: gatillos + stick).
`build/gt_native` es la versión sin ventana para pruebas y el harness diferencial.

**Mods nativos** (solo `gt_play`): bibliotecas `.so` con un `mod.toml` en `build/mods/<id>/` (o `GT_MODS_DIR`), cargadas con el
SDK `include/gt_mod/gt_mod_api.h`; `GT_NO_MODS=1` los desactiva. Los mods viven en el repo público
[amt911/gt-mods](https://github.com/amt911/gt-mods); F9 abre su menú en juego (también con el mando: Select + Start).
Comprobaciones en juego: `tools/mod_*_check.sh`. Cómo escribir e instalar mods: [`docs/MODS.md`](docs/MODS.md).

## Para agentes (Claude Code / Codex)

- [`AGENTS.md`](AGENTS.md) es la guía canónica; [`CLAUDE.md`](CLAUDE.md) solo la importa y añade lo
  exclusivo de Claude Code (REA MCP con Ghidra).
- Detalle por tema en [`docs/agents/`](docs/agents/): flujo de decompilación, toolchain, ingeniería inversa,
  testing, verificación en runtime, depuración, orquestación, verificación de PRs.
- Memoria compartida: [`docs/FACTS.md`](docs/FACTS.md) (hechos verificados) y
  [`docs/FINDINGS.md`](docs/FINDINGS.md) (trampas no obvias).

## Créditos y referencias

Trabajo previo de la comunidad en el que se apoya: PSPRecomp (jessicanataliagta) y lcs-recomp
(elmasas), Nenkai (GT Modding Hub, OpenAdhoc, GTAdhocToolchain, GTPSPVolTools), rabbitizer
(Decompollaborate), m2c, ghidra-allegrex (kotcrab), pspdecrypt (John-K), PPSSPP, decomp.me.

Proyecto de investigación sin ánimo de lucro, sin afiliación con Sony Interactive Entertainment ni con
Polyphony Digital. *Gran Turismo* es marca de Sony Interactive Entertainment.
