# gran-turismo-decomp

Decompilación de **Gran Turismo para PSP** (2009), versión europea **UCES-01245 v2.00**: reconstruir el
ejecutable del juego (`EBOOT.BIN`, módulo `PDIAPP`) como C/C++ que, al recompilarse, produzca **los
mismos bytes** que el original.

> **Estado:** Fase 0 (preparación) terminada. Aún no hay split ni código decompilado. Ver
> [`docs/ROADMAP.md`](docs/ROADMAP.md).

## Qué hay y qué no

- **Este repo no contiene datos del juego** — ni ISO, ni ejecutables (cifrados o no), ni assets, ni
  compiladores propietarios. Necesitas **tu propio volcado** del UMD.
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
2. Pasos de la Fase 0, verificados a mano: [`AGENTS.md` § Commands](AGENTS.md#commands). Los scripts
   `tools/setup.sh` y `tools/extract.sh` llegan en la Fase 1.

## Para agentes (Claude Code / Codex)

- [`AGENTS.md`](AGENTS.md) es la guía canónica; [`CLAUDE.md`](CLAUDE.md) solo la importa y añade lo
  exclusivo de Claude Code (REA MCP con Ghidra).
- Detalle por tema en [`docs/agents/`](docs/agents/): flujo de matching, toolchain, ingeniería inversa,
  testing, verificación en runtime, depuración, orquestación, verificación de PRs.
- Memoria compartida: [`docs/FACTS.md`](docs/FACTS.md) (hechos verificados) y
  [`docs/FINDINGS.md`](docs/FINDINGS.md) (trampas no obvias).

## Créditos y referencias

Trabajo previo de la comunidad en el que se apoya: Nenkai (GT Modding Hub, OpenAdhoc, GTAdhocToolchain,
GTPSPVolTools), splat / rabbitizer / spimdisasm (Decompollaborate, ethteck), objdiff (encounter), m2c,
ghidra-allegrex (kotcrab), pspdecrypt (John-K), PPSSPP.

Proyecto de investigación sin ánimo de lucro, sin afiliación con Sony Interactive Entertainment ni con
Polyphony Digital. *Gran Turismo* es marca de Sony Interactive Entertainment.
