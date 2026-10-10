# gran-turismo-decomp

Decompilación de **Gran Turismo para PSP** (2009), versión europea **UCES-01245 v2.00**: reconstruir el
ejecutable del juego (`EBOOT.BIN`, módulo `PDIAPP`) como **C/C++ legible que compila y corre en otras
plataformas** (PC primero), para poder **modificar el juego** desde su código fuente.

Cómo: el ejecutable se traduce entero a C++ con [PSPRecomp](https://github.com/jessicanataliagta/PSPRecomp)
para que el juego corra en PC desde el principio, y después cada función traducida se sustituye por
código decompilado a mano, comprobando que se comporta exactamente igual que la original. No se busca
un binario idéntico byte a byte (los compiladores exactos de Sony no son públicos).

> **Estado (2026-10-10, madrugada) — para leer mañana.**
>
> - **🎉 ¡Llega al MENÚ PRINCIPAL!** `gt_native` (sin GPU, renderer por software) pinta: aviso legal →
>   «Sony Computer Entertainment presents» → Polyphony Digital → aviso de autoguardado → logo GT →
>   **vídeo de introducción completo** → pantalla de título (el coche) → **menú principal de Gran
>   Turismo** con sus 8 iconos. Corre minutos sin fallar.
> - **Verlo tú:** `GT_PAD="3300:START" GT_FRAME_DUMP=recomp/frames build/gt_native --max-seconds 120`
>   guarda una imagen por segundo y pulsa START para saltar el vídeo ([`docs/COMPILAR.md`](docs/COMPILAR.md)
>   §5). Las imágenes no se suben al repo. Necesita las fuentes del firmware que trae PPSSPP
>   (`/usr/share/ppsspp/assets/flash0/font`, o `GT_FONT_DIR=…`).
> - **Pendiente visible:** los textos del menú salen en blanco/vacíos; sin sonido (la música va
>   "en silencio"); sin ventana (solo imágenes).
> - **Decompilación (Fase 3-4):** 25 funciones decompiladas y probadas contra el original con el
>   harness diferencial (libc: `memset`, `memcmp`, `memcpy`, `memmove`, `strlen`, `strcmp`,
>   `strchrnul`, `malloc`, `_malloc_r`; motor: el recuento de referencias y el puntero inteligente;
>   scripts: los 8 nativos de `MUnit`, las unidades de velocidad/potencia/par/temperatura).
>   **1.313 funciones ya tienen su nombre real** (los nativos que usan los scripts Adhoc:
>   `config/symbol_addrs.txt`). Informe: `.venv/bin/python -m tools.progress`.
> - **Probarlo tú:** [`docs/COMPILAR.md`](docs/COMPILAR.md). Detalle en [`docs/ROADMAP.md`](docs/ROADMAP.md).

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

1. Pon tu ISO en `game_files/` (está en `.gitignore`).
2. Sigue [`docs/COMPILAR.md`](docs/COMPILAR.md): herramientas, extracción, recompilación, compilación de
   `gt_native`, ejecución y comparación con PPSSPP, paso a paso.

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
