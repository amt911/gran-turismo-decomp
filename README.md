# gran-turismo-decomp

Decompilación de **Gran Turismo para PSP** (2009), versión europea **UCES-01245 v2.00**: reconstruir el
ejecutable del juego (`EBOOT.BIN`, módulo `PDIAPP`) como **C/C++ legible que compila y corre en otras
plataformas** (PC primero), para poder **modificar el juego** desde su código fuente.

Cómo: el ejecutable se traduce entero a C++ con [PSPRecomp](https://github.com/jessicanataliagta/PSPRecomp)
para que el juego corra en PC desde el principio, y después cada función traducida se sustituye por
código decompilado a mano, comprobando que se comporta exactamente igual que la original. No se busca
un binario idéntico byte a byte (los compiladores exactos de Sony no son públicos).

> **Estado (2026-10-09, noche) — para leer mañana.** Fases 0 y 1 terminadas. Fase 2a en curso, en la
> rama `feat/phase2a-recomp-boot`:
>
> - **Gráficos: todavía nada.** `gt_native` es *headless* (sin ventana ni GPU) a propósito; la primera
>   imagen llega en la Fase 2b. No se ha visto ningún frame.
> - **Lo que sí funciona:** GT entero recompilado a C++ nativo (156 unidades, compila en ~3 min dentro de
>   6 GB de RAM), carga el EBOOT y **ejecuta 30 s seguidos sin fallos (~1,09 millones de llamadas al
>   sistema)**. Las primeras 235 coinciden en orden con PPSSPP (objetivo de la fase 2a: 2.000).
> - **Decompilación empezada (Fase 3):** harness diferencial en `gt_native --decomp diff`; primera
>   función decompilada y probada: `memset` (16/16 casos iguales frente al original).
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
