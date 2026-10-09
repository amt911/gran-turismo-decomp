# ROADMAP — gran-turismo-decomp

Estado de cada fase. Se actualiza **en el mismo cambio** que cierra o desbloquea algo (regla de
`AGENTS.md` § Working rules). Cada fase grande pasa por `brainstorming → spec → plan` antes de
implementarse.

## Fase 0 — Preparación ✅ (2026-10-09)

- [x] ISO identificada y hasheada; layout del disco inventariado ([FACTS](FACTS.md)).
- [x] `EBOOT.BIN` descifrado con pspdecrypt (a mano, en un directorio temporal) y verificado por SHA1.
- [x] ELF caracterizado: secciones con nombre, sin `.symtab`, C++, `-G0`, tres GCC en `.comment`,
      VFPU escaso, 36 librerías importadas.
- [x] Ecosistema investigado (splat, rabbitizer, objdiff, m2c, decomp.me, ghidra-allegrex, PPSSPP,
      PSPRecomp, trabajo de Nenkai) — [toolchain](agents/toolchain.md).
- [x] `AGENTS.md`, `CLAUDE.md`, `docs/agents/`, FACTS, FINDINGS, `.gitignore`, `.mcp.json`.

## Fase 1 — Extracción reproducible ⏳

- [ ] `tools/setup.sh`: venv con `splat64[mips]`, `rabbitizer`, `spimdisasm` fijados; pspdecrypt en el
      commit fijado; binutils Allegrex de pspdev; objdiff-cli.
- [ ] `tools/extract.sh`: SHA1 de la ISO → `7z` (sin `GT.VOL`) → pspdecrypt → SHA1 del ELF → volcado de
      los segmentos en bruto que usará splat. Idempotente; falla con mensaje claro si el hash no cuadra.
- [ ] `config/checksums.sha1` con los hashes de FACTS.
- [ ] Ghidra 12.1.3 local en `tools/ghidra/` + ghidra-allegrex v21.4; REA con el provider Ghidra
      disponible (`binary_session`) y el ELF abriendo.
- [ ] PPSSPP instalado; **medir la base de carga real** del módulo (¿`0x08804000`?) y anotarla en FACTS.

## Fase 2 — Identificar el compilador 🔒 (bloquea el matching)

- [ ] Huella de codegen: biblioteca (zona con FDEs, desde `0x356960`) frente a código de juego.
- [ ] Compilar funciones hoja con cada compilador PSP de decomp.me; anotar resultados en FACTS.
- [ ] Buscar fuentes GPL de los parches Allegrex de Sony para GCC 3.3.6 / 4.0.4.
- [ ] **Decisión del usuario** si nada reproduce el codegen: decomp funcional (no matching, verificada
      en runtime) o pausa.

## Fase 3 — Split + build byte-idéntico desde asm

- [ ] Config de splat (`platform: psp`) sobre los segmentos en bruto, enlazado en base 0.
- [ ] Separar librerías (NIDs con psplibdoc, libc/libstdc++, SGX) del código de juego.
- [ ] Build con ninja que reproduce los segmentos; `ninja check` en verde al 0 % de C.
- [ ] `objdiff.json` + informe de progreso con dos números (juego / SDK).
- [ ] Medir la vuelta (`debugging.md`) y anotar los tiempos.

## Fase 4 — Decompilación función a función

- [ ] Primeras funciones hoja de librería (para validar flags del compilador).
- [ ] Subsistemas del juego, por orden de lo que desbloquea nombres (Adhoc bindings, menús, sonido).

## Fase 5 — Opcional, más adelante

- [ ] Build desplazable (shiftable) + verificación en PPSSPP.
- [ ] Pista de port a PC (¿PSPRecomp?) — proyecto aparte, nunca sustituye al gate.
