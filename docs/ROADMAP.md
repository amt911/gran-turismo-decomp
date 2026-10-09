# ROADMAP — gran-turismo-decomp

Estado de cada fase. Se actualiza **en el mismo cambio** que cierra o desbloquea algo (regla de
`AGENTS.md` § Working rules). Cada fase grande pasa por `brainstorming → spec → plan` antes de
implementarse.

**Estrategia (decidida por el usuario el 2026-10-09):** el objetivo es la **decomp** — C/C++ legible
que compila en otras plataformas. Los compiladores exactos de Sony no son públicos, así que no hay
matching byte a byte: **estrategia B**, base recompilada con PSPRecomp + decompilación función a
función verificada de forma diferencial. Detalle en `AGENTS.md` § Strategy.

## Fase 0 — Preparación ✅ (2026-10-09)

- [x] ISO identificada y hasheada; layout del disco inventariado ([FACTS](FACTS.md)).
- [x] `EBOOT.BIN` descifrado con pspdecrypt (a mano, en un directorio temporal) y verificado por SHA1.
- [x] ELF caracterizado: secciones con nombre, sin `.symtab`, C++, `-G0`, tres GCC en `.comment`,
      VFPU escaso, 36 librerías / 329 funciones importadas.
- [x] Ecosistema investigado — [toolchain](agents/toolchain.md).
- [x] **Búsqueda de compiladores**: los exactos (GCC 3.3.6 psp-1.14/1.15, 4.0.4 psp-2.8.2) no aparecen;
      el más cercano (decomp.me `psp-gcc-1.7.1`, GCC 3.3.3) iguala 1 de 2 funciones probadas → no es el
      mismo. Resultado: estrategia B.
- [x] PSPRecomp evaluado: su capa de host ya implementa 151/329 imports de GT.
- [x] `AGENTS.md`, `CLAUDE.md`, `docs/agents/`, FACTS, FINDINGS, `.gitignore`, `.mcp.json`.

## Fase 1 — Extracción reproducible y entorno ⏳

- [ ] `tools/setup.sh`: venv fijado (rabbitizer, pyelftools, m2c); pspdecrypt en el commit fijado;
      PSPRecomp en el commit fijado.
- [ ] `tools/extract.sh`: SHA1 de la ISO → `7z` (sin `GT.VOL`) → pspdecrypt → SHA1 del ELF.
      Idempotente; falla con mensaje claro si el hash no cuadra.
- [ ] `config/checksums.sha1` con los hashes de FACTS.
- [ ] Ghidra 12.1.3 local en `tools/ghidra/` + ghidra-allegrex v21.4; REA con el provider Ghidra
      disponible y el ELF abriendo.
- [ ] PPSSPP instalado; **medir la base de carga real** del módulo (¿`0x08804000`?) y anotarla en FACTS.

## Fase 2 — Base recompilada: GT corre en PC vía PSPRecomp

Necesita spec (perfil GT, qué se reutiliza del perfil VCS / de lcs-recomp, backend gráfico en Linux).

- [ ] Perfil GT en PSPRecomp sobre el ELF descifrado; el C++ generado va a `recomp/` (ignorado).
- [ ] Capa de host: reutilizar los 151 HLE existentes (GE, display, ctrl, hilos, SAS, MPEG…) e
      implementar los que falten a medida que el juego los pida (fuentes `sceLibFont`, `scePsmf`,
      `sceMp3`, diálogos de `sceUtility`, `Kernel_Library`; la red adhoc puede ir como stub).
- [ ] Backend gráfico para Linux (Vulkan/SDL2, como lcs-recomp).
- [ ] Estados de referencia definidos (título, menú, salida de carrera) y comparados con PPSSPP.
- [ ] Medir la vuelta (`debugging.md`) y anotar los tiempos.

## Fase 3 — Harness diferencial

Necesita spec (interoperabilidad con la memoria guest, formato de captura de casos).

- [ ] Poder alternar cualquier función entre su versión recompilada y la decompilada.
- [ ] Captura de entradas/salidas de una función en partidas reales; comparación automática.
- [ ] Informe de progreso: funciones decompiladas y en verde / total, juego vs SDK.
- [ ] Prueba de que el gate puede fallar (romper una función a propósito y verlo en rojo).

## Fase 4 — Decompilación función a función

- [ ] Primeras hojas de librería y utilidades (validan el harness).
- [ ] Subsistemas del juego, por orden de lo que desbloquea nombres (bindings de Adhoc, menús, sonido
      SGX, física).

## Fase 5 — Más adelante

- [ ] Sin parte recompilada: 100 % fuente decompilada.
- [ ] Otras plataformas.
- [ ] Pista de matching opcional si aparecen los compiladores exactos.
