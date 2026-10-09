# ROADMAP — gran-turismo-decomp

Estado de cada fase. Se actualiza **en el mismo cambio** que cierra o desbloquea algo (regla de
`AGENTS.md` § Working rules). Cada fase grande pasa por `brainstorming → spec → plan` antes de
implementarse.

**Estrategia (decidida por el usuario el 2026-10-09):** el objetivo es la **decomp** — C/C++ legible
que compila en otras plataformas — y, a través de ella, **poder hacer mods**. La recompilación es un
paso intermedio, no el producto. Los compiladores exactos de Sony no son públicos, así que no hay
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

- [x] `tools/setup.sh`: venv fijado (rabbitizer, pyelftools, pytest); pspdecrypt y PSPRecomp en sus
      commits fijados; Ghidra 12.1.3 + ghidra-allegrex v21.4 locales con `--with-ghidra`.
- [x] `tools/extract.py` (no `.sh`: tiene lógica con tests): SHA1 de la ISO → `7z` (sin `GT.VOL`) →
      cabecera `~PSP` → pspdecrypt → SHA1 del ELF. Idempotente; errores claros.
- [x] `config/checksums.sha1` con los hashes de FACTS.
- [x] Ghidra local analiza el ELF (cargador PSP, lenguaje Allegrex, 23.208 funciones, VFPU ok).
- [x] REA: provider Ghidra disponible, **pero REA 6.2.0 no admite ELF MIPS** → no abre el EBOOT;
      se usa Ghidra directamente (FINDINGS).
- [x] PPSSPP 1.20.4 instalado; base de carga medida: **`0x08804000`** (FACTS § Runtime).

## Fase 2 — Base recompilada: GT corre en PC vía PSPRecomp

Dividida en **2a** (headless, comparada con PPSSPP) y **2b** (gráficos, audio, mandos). Spec y plan de
2a en `docs/superpowers/`.

### Fase 2a — en curso (2026-10-09)

- [x] Framework lcs-recomp fijado + 10 parches nuestros (`tools/patches/lcs-recomp/`): bugs del
      generador, instrucciones Allegrex/VFPU que faltaban, semillas de punteros a código, observador de
      imports.
- [x] `tools/recomp.py`, `extract --full`, `trace_compare.py` (con tests); `gt_native` compila y enlaza.
- [x] Kernel HLE propio (`host/gt_kernel.cpp`, base MIT de lcs-recomp) + funciones que GT necesita.
- [x] Corre 30 s sin colgarse; ~82.000 llamadas al sistema; **235 coinciden en orden con PPSSPP**.
- [ ] Puerta: 2.000 llamadas coincidentes y 30 s sin fallos. Pendiente: puntero nulo en `0x08ADB53C`,
      divergencia en `sceGeListEnQueue` (llamada 235).
- [x] Orden de despertar en el vblank: las muestras del mando llegan *después* del vblank (periodo real
      de 59,94 Hz). Los cambios de hilo coinciden con PPSSPP hasta el nº 22 (antes 19).
- [ ] Siguiente diferencia de planificación: tras la sub-interrupción de vblank que registra GT, PPSSPP
      reanuda el hilo interrumpido ("left interrupt") y nosotros cambiamos de hilo. Aparcado: el harness
      (Fase 3) no necesita igualdad con PPSSPP, compara dentro de nuestra propia ejecución.

### Fase 2b-lite — primeras imágenes (2026-10-10)

- [x] Renderer GE por software de lcs-recomp (`ge_renderer.cpp`) enlazado sin GPU; volcado de frames
      con `GT_FRAME_DUMP` (COMPILAR §5).
- [x] Sub-interrupciones de VBlank: todas las habilitadas, no solo la 15 de LCS (FINDINGS). El hilo de
      render avanza y el juego muestra aviso legal, «SCE presents», Polyphony, autoguardado, logo GT.
- [x] VFPU `lvl.q/lvr.q/svl.q/svr.q` (parche 0012); `sceNpDrmSetLicenseeKey`.
- [x] Vídeo de intro: `scePsmf` propio (`host/psmf*.cpp`, con tests) y decodificación H.264 desde el
      ring buffer (los vídeos de GT.VOL están cifrados en disco). Se ve entero.
- [x] Mando simulado headless: `GT_PAD`. START salta el vídeo → pantalla del logo GT.
- [ ] Música del menú: `sceAtrac` (`SetDataAndGetID`, `GetNextSample`…). Aquí se para hoy.
- [ ] El vídeo va a ~16 fps de juego (PPSSPP: 30): mirar el ritmo.
- [ ] `sceDisplaySetMode` espera un vblank en PPSSPP (divergencia nº 236 de la traza).

- [ ] Perfil GT en PSPRecomp sobre el ELF descifrado; el C++ generado va a `recomp/` (ignorado).
- [ ] Capa de host: reutilizar los 151 HLE existentes (GE, display, ctrl, hilos, SAS, MPEG…) e
      implementar los que falten a medida que el juego los pida (fuentes `sceLibFont`, `scePsmf`,
      `sceMp3`, diálogos de `sceUtility`, `Kernel_Library`; la red adhoc puede ir como stub).
- [ ] Backend gráfico para Linux (Vulkan/SDL2, como lcs-recomp).
- [ ] Estados de referencia definidos (título, menú, salida de carrera) y comparados con PPSSPP.
- [ ] Medir la vuelta (`debugging.md`) y anotar los tiempos.

## Fase 3 — Harness diferencial (2026-10-10)

Spec: `docs/superpowers/specs/2026-10-10-phase3-diff-harness-design.md`.

- [x] `gt_native --decomp off|on|diff`: cada función decompilada se compara con su original dentro de
      nuestra propia ejecución (foto de CPU + RAM, original aislada, decompilada, comparación, y el juego
      sigue con el resultado original).
- [x] Gate demostrado fallando (sabotaje de `v0`: 16/16 discrepancias, código de salida 2).
- [x] Primera función **probada**: `memset` (`src/libc/memset.cpp`): 16/16 casos iguales, 11 cambian
      memoria; reproduce su rareza de cola (escribe un byte de más). Sabotajes de `v0` y de escritura
      detectados (código de salida 2).
- [x] Caída de arranque arreglada (semáforo con máximo 0): **30 s completos sin fallos, ~1,09 M llamadas
      al sistema**.
- [ ] Informe de progreso: funciones decompiladas y en verde / total, juego vs SDK.
- [ ] Investigar las llamadas directas entre unidades que se saltan las sustituciones (FINDINGS).

## Fase 4 — Decompilación función a función

- [x] Primeras hojas de libc (2026-10-10): `memcmp`, `strlen`, `strcmp`, `strchrnul` (+ `memset`), en
      `src/libc/`. Harness: 16/16 casos iguales cada una (strchrnul 4/4: GT la llama poco); sabotaje
      del valor devuelto detectado en las cuatro (exit 2; memcmp 15/16 casos con resultado ≠ 0).
- [ ] Más libc/libm/libstdc++ estática alrededor de `0x331800-0x334000` y en otros bloques.
- [ ] Subsistemas del juego, por orden de lo que desbloquea nombres (bindings de Adhoc, menús, sonido
      SGX, física).
- [ ] **Puente Adhoc (siguiente):** recorrer los 334 inicializadores de `.ctors`, anotar qué módulo
      `pdistd::*` registra cada uno (mapa de partida en FACTS), nombrar con evidencia y decompilar los
      registros y los nativos pequeños con el harness.

## Fase 5 — Más adelante

- [ ] Sin parte recompilada: 100 % fuente decompilada.
- [ ] **Mods**: cambios sobre la fuente decompilada (lógica, coches, circuitos, UI) sin tocar el
      binario original; carga de datos modificados de `GT.VOL` con las herramientas de Nenkai.
- [ ] Otras plataformas.
- [ ] Pista de matching opcional si aparecen los compiladores exactos.
