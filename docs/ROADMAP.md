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
- [x] `sceAtrac` completo para GT (`SetDataAndGetID`, `SetData`, `GetAtracID`, `GetNextSample`,
      `GetMaxSample`, `IsSecondBufferNeeded`). La música de GT.VOL está cifrada como los vídeos: hoy
      se "decodifica" silencio llevando toda la contabilidad (muestras, frames, bucles, streaming).
- [x] `sceLibFont` real: lector PGF propio (`host/pgf.cpp`, con tests) + HLE (`host/font_hle.cpp`)
      con las fuentes del firmware del usuario (las de PPSSPP). Sin glifos, la maquetación de texto
      leía un glifo nulo (`NULL+4`) en la pantalla de título (FINDINGS).
- [x] **Menú principal alcanzado** (2026-10-10): título → menú con sus 8 iconos, minutos sin fallos.
- [x] Textos visibles: GT copia cada glifo a una textura pequeña con transferencias de bloque de la
      GE (TRXKICK, ahora en `host/gt_ge_exec.cpp`) y lo dibuja como sprite transformado, que el
      renderer descartaba (parche 0013). Trazador de listas GE: `GT_GE_LOG=<vblank>`.
- [x] **Primera carrera** (2026-10-10): menú → Single Race → coche → pista → parrilla → carrera en 3D.
      Hicieron falta un asignador de bloques que fusiona huecos y respeta las peticiones "altas" (la
      carga de 6,6 MB fallaba con 10,5 MB libres en 1.141 trozos; FINDINGS) y `sceRtc` (reloj
      determinista: 2009-10-01 + tiempo virtual, UTC).
- [x] Ventana en tiempo real, controles y sonido: `gt_play` (sección *Jugable*).
- [x] Cargar partidas: GT rechazaba la suya ("Save data created on another system") porque el HLE de
      `sceUtilitySavedata` no ponía el campo `bind` al cargar (GT exige `bind & 3 == 1`); ahora 1021,
      y el `sfoParam` se guarda y se devuelve como en un PARAM.SFO. `gt_play` conserva las partidas en
      `build/savedata/`; `gt_native` sigue empezando sin partida (reproducible).
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
- [x] Informe de progreso: `tools/progress.py` (con tests) sobre la lista de funciones de Ghidra
      (23.208 funciones, 4,17 MB). Hoy: 8 decompiladas (7 SDK, 1 juego), 0,045 % de los bytes.
- [ ] Investigar las llamadas directas entre unidades que se saltan las sustituciones (FINDINGS).

## Fase 4 — Decompilación función a función

- [x] Primeras hojas de libc (2026-10-10): `memcmp`, `strlen`, `strcmp`, `strchrnul` (+ `memset`), en
      `src/libc/`. Harness: 16/16 casos iguales cada una (strchrnul 4/4: GT la llama poco); sabotaje
      del valor devuelto detectado en las cuatro (exit 2; memcmp 15/16 casos con resultado ≠ 0).
- [x] `memcpy` (`0x375A88`, copia por bloques de 16 B reproducida), `memmove` (`0x375BAC`; el
      harness destapó que hacia atrás devuelve `dest + n`), `add_ref` del motor (`0x257A08`, primer
      código de juego). 32/32 casos (memmove 18/18), sabotajes detectados.
- [x] Llamar a código del juego desde código decompilado: `gt::call_guest` (con tests; para si el
      llamado cambia de hilo) + imports re-ejecutables en el harness (LwMutex, intr, dcache).
      Primeras funciones no hoja: `malloc` (7/7, 5 no comparables por crecimiento del heap) y
      `_malloc_r` (2/2). `calloc`/`realloc` no se llaman en lo alcanzado: siguen recompiladas.
- [ ] Más libc/libm/libstdc++ estática alrededor de `0x331800-0x334000` y en otros bloques.
- [ ] Subsistemas del juego, por orden de lo que desbloquea nombres (bindings de Adhoc, menús, sonido
      SGX, física).
- [x] **Puente Adhoc — nombres:** los registradores de nativos (`0x382FB0` funciones de módulo,
      `0x383108` métodos de clase) dan **1.313 funciones con su nombre real** de script
      (`MLocale::getLanguage`, `MSpecDB::…`) → `config/symbol_addrs.txt` (generado y versionado).
- [x] Modelo de objetos del motor (`src/core/refcount.cpp`): objeto `{vtable, fuerte, débil}`,
      retener/soltar (fuerte y débil), copia, destructor y reasignación del puntero inteligente
      (8 funciones, con saltos-cola donde el original los hace).
- [x] **Fibras** (`host/fiber.cpp`, con test): el código decompilado puede llamar a funciones que
      bloquean (cambio de hilo PSP); se suspende y se reanuda. Hasta el menú: 41 suspensiones.
- [x] Harness: saltos-cola (`gt::tail_call`), muestreo geométrico (el de 1/64 se gastaba en el
      arranque: FINDINGS), modo denso por función. `GT_PROBE_SYMBOLS` cuenta qué funciones con nombre
      se ejecutan (74 nativos Adhoc hasta el menú).
- [x] Primeros nativos Adhoc: los 8 de `MUnit` (`Set/GetCurrentUnitOf{Velocity,Power,Torque,
      Temperature}`, `src/game/adhoc/munit.cpp`), con `GuestFrame` para locales en la pila guest.
- [x] `MRandom::GetValue` (10/10, rama de 2 argumentos); `call_guest` con floats; leer la hora del
      sistema es re-ejecutable en el harness.
- [x] Nativos `Object.toInt/toFloat/defined`, `Math.clamp`, `Module.clearStatic`,
      `MSystemBGM.getContextSize`; ayudantes compartidos de valores Adhoc (`src/game/adhoc/value.*`).
- [x] Harness: aislamiento del kernel en los casos (sin preempción por temporizador ni colas de
      LwMutex; FINDINGS), sin casos anidados, límite de 250 ms, comparación de RAM por bloques y
      búferes reutilizados (antes ~100 ms por caso).
- [x] `Array.resize` (vector de holders: truncar o crecer con `nil`), `MManager.registerClass`;
      módulos con nombre `Array`, `STStructure`, `pdistd`.
- [x] Nativos de UI: `MRootWindow.setFocus`, `MNode.doCopy`, `MRenderContext.pushEvent`,
      `MComposite.appendChild`.
- [x] Harness: el informe dice por qué un caso no fue comparable; estado del kernel re-ejecutable
      (bloques de memoria y prioridades de hilo, guardado/restaurado y comparado); un fallo de la
      versión decompilada cuenta como discrepancia. Hasta el menú: **0 casos descartados**.
- [x] 12 nativos de juego/UI más: `MWatcher.append/remove`, `MOrganizer.startGameObjectLoop/
      setRaceSound/setSoundContext`, `MRaceOperator.setOrganizer`, `MSpecDB.getCarLabelCount/
      getCarCode` (valor de 64 bits), `GameRecord.CreateGameRecordStructure/InitializeGameRecord`,
      `MUpdateContext.getRenderContext`, `MRenderContext.getUpdateContext`. **50 funciones.**
- [x] Harness: mutex de kernel y creación de LwMutex re-ejecutables. Sin comparar aún:
      `MOrganizer.initialize` (crea hilos), `MMenuGameObjectManager.initialize` (lee ficheros).
- [x] Nativos por la ruta de la carrera (Single Race → Apricot Hill), 25 más con ayudantes de patrón
      (`with_self`, `core_call`, `organizer_call`, `car_parameter_call`, `spec_lookup`, `return_long`):
      `MOrganizer.*` (enter/sound/rendering race…), `MCarParameter.*`, `CarSettings.begin/endSetting`,
      `MCarDriverParameter.setDefault`, `Object.toBool/toULong`, `Math.sqrt/abs`,
      `MSpecDB.getCarCategory/getCourseCondition/isDirt`, `MProductInformation.GetParentalLevel`,
      `MSound.stopMusic`, `MTextFace.adjustScale`, `MToolTipFace.remove`,
      `MRootWindow.getFocusedWidget`, `MComposite.countChild`. Diff por la ruta de la carrera:
      **75 funciones, 0 discrepancias**; sabotaje detectado. Sin comparar (registradas pero
      desactivadas): `exitCourse`, `endSession`, `requestLoadingCourse` (duermen/despiertan hilos),
      `beginPartsChange` (lee ficheros), `MMovieFace.unload` y `MRenderContext.finish` (imports no
      re-ejecutables).
- [x] 14 nativos más de la ruta de la carrera: `MOptionMenu.Count/getItem/setIndex`,
      `MIconBox.getItemCount/getItemVisible`, `MComposite.getChild/removeChild/clearWindow`,
      `Object.isInstanceOf`, `MCarParameter.getCarCode`, `MRaceOperator.getTargetSlot/isOnline`,
      `MMusic.isPlaying`, `MCarCollection.hasCar`. **89 funciones, 0 discrepancias** hasta la carrera.
      Sin comparar: `MUpdateContext.Sync` y `MListBox.setItemCount` (imports no re-ejecutables).
- [x] `bsearch` de libc (la función SDK más llamada hasta la carrera: 1,6 millones de llamadas,
      29/29 casos iguales; comparador como 5.º argumento EABI), `Object.getObjectAttribute/
      setObjectAttribute`, `STStructure.define`, `MNode.doInitialize`, `MSystemBGM.stop` (salto de
      cola). **95 funciones, 0 discrepancias** hasta la carrera.
- [x] **El asignador de memoria de libc**: `memalign` (el núcleo de `malloc`) y `free`, ~1,2 M y
      ~1,1 M llamadas hasta la carrera. No es dlmalloc sino un asignador de páginas por clases de
      tamaño (bloques del kernel "PDI:ABLOCK"/"PDI:MBLOCK"; `src/libc/page_heap.hpp`). Diff: 29/29 y
      28/28 en la ruta de la carrera, 2.000/2.000 cada uno en modo denso; sabotajes de las ramas raras
      detectados solo en modo denso (FINDINGS). **97 funciones, 0 discrepancias.**
- [x] **Arreglo de fondo:** las funciones decompiladas a las que solo se llamaba desde su propia
      unidad generada nunca se ejecutaban (el recompilador las enlazaba con `goto`). Parche 0014 de
      lcs-recomp + `tools.recomp`: cada `GT_DECOMP` activo es una frontera que pasa por el despachador
      (FINDINGS). Con ello, las 8 auxiliares del heap de libc (`page_heap.cpp`: lock/unlock, clase de
      tamaño, búsqueda/inserción/borrado en la tabla de páginas, liberar página, pedir bloque) pasan
      de 0 llamadas a millones. **105 funciones, 0 discrepancias** hasta la carrera.
- [x] Los tipos de valor de Adhoc (`src/game/adhoc/values.cpp`): fábricas y constructores de
      entero, float, booleano y entero de 64 bits (con las que todo nativo devuelve su resultado;
      ~15.000 y ~20.000 llamadas las de entero y booleano). **113 funciones, 0 discrepancias.**
- [x] 15 nativos más (MCalendar, MOrganizer de entradas y pilotos, MCarParameter/MCarDriverParameter,
      MRaceParameter, MListBox/MIconBox.getItemWidget, MSound.play) y ayudantes para `std::string`
      de libstdc++ (`to_string`, `string_release`: la cadena COW). **127 funciones, 0 discrepancias.**
      Sin comparar (duermen el hilo con `sceKernelSleepThread`): `MRenderContext.closePage`,
      `MMenuGameObjectManager.start`.
- [x] 31 nativos más de la carrera (MRaceOperator, MOrganizer, MCarParameter, MCarDriverParameter,
      MRaceParameter, MCarCollection.hasFavorite, MController.getConfig, configuración del sistema;
      ayudante `return_cstring` para devolver textos). **158 funciones, 0 discrepancias.** Sin comparar:
      `MRaceOperator.checkIn` (`sceUmdWaitDriveStat`), `allocateResource` (`sceKernelCreateThread`).
- [x] 14 nativos que devuelven texto (`src/game/adhoc/text_natives.cpp`: etiquetas de coches y
      circuitos, nombre corto, código de preparador, idioma, nick, versión, unidades, dinero,
      `Object.toString`); ayudantes `return_string`/`return_c_string`. `sceUtilityGetSystemParamInt/
      String` pasan a ser re-ejecutables en el harness. **172 funciones, 0 discrepancias.**
- [x] 12 nativos de interfaz y sonido (MIconBox/MListBox setItemCount/setItemTemplate,
      MRenderContext.getCommonPage/filterKeyEvent, MEvent.dispatchEvent, MUpdateContext.getPadButtonState,
      MOptionMenu.applyAppend, MNode.getNodePath, MImageFace.getImagePath/Texcoord, MSound.fadeOutMusic).
      **184 funciones, 0 discrepancias.** Sin comparar: `MComposite.clearChild` (duerme el hilo),
      `MMusic.fadein` (`sceKernelWakeupThread`).
- [x] Métodos de String (`toInt/toLong/toULong/upcase/substr`, `src/game/adhoc/string_natives.cpp`),
      `pdistd.AsciiStringHash` y `Module.removeStatic/defineModule/defineAttribute`.
      **193 funciones, 0 discrepancias.**
- [x] **Plantillas:** las funciones "como clase X" (convertir un holder a una clase del motor) son 394
      instancias del mismo código; una función (`src/game/adhoc/as_class.cpp`) + una tabla con su copia
      y su conversión las cubre. Registradas las 74 que la ruta de la carrera llama (todas iguales; el
      resto sigue recompilado hasta que una ejecución las alcance). `GT_DECOMP_AS_CLASS` cuenta en
      `tools.progress` y en las fronteras. **267 funciones, 0 discrepancias.**
- [x] **Plantilla de conversión** (`holder_cast`, a la que saltan las anteriores): 197 instancias con dos
      asignaciones de registros del mismo código (comparador tolerante a registros y a saltos absolutos).
      Registradas las 58 de la ruta de la carrera. **325 funciones, 0 discrepancias.**
- [x] **Thunks de salto** (`src/core/thunks.cpp`): funciones de una sola instrucción `j destino`
      (538 candidatas). Una función (`jump_thunk`) + tabla entrada→destino; `GT_DECOMP_THUNK` cuenta en
      `tools.progress` y en las fronteras. Registradas las 261 que la carrera compara iguales (sabotaje
      `a2 += 4` detectado). **586 funciones, 0 discrepancias** (`tools.progress`: 582 por entrada
      conocida, 2,51 % de funciones, 1,23 % de bytes).
- [ ] Más nativos de los 286 que se ejecutan hasta la carrera (`GT_PROBE_SYMBOLS`).

## Jugable — `gt_play` (2026-10-10)

- [x] Segundo ejecutable con los backends reales de lcs-recomp (ventana SDL2, presentación Vulkan,
      teclado/mando, audio), mismo juego y mismo código decompilado que `gt_native`. Llega a la carrera
      (prueba sin pantalla: `SDL_VIDEODRIVER=offscreen` + `GT_PAD`). Controles para GT en
      `config/gt_play.ini`. Spec: `docs/superpowers/specs/2026-10-10-playable-window-design.md`.
- [x] Sonido: la mezcla de GT sale por `sceAudioOutput2` (antes se descartaba) y la música Atrac3+ se
      decodifica desde los datos que el juego entrega en su búfer (FFmpeg atrac3p, trama a trama).
- [ ] Audio de los vídeos PSMF.
- [x] Menús e iconos/HUD que faltaban: los sprites se dibujaban antes que los triángulos pendientes
      (parche 0015; FINDINGS). Referencia: PPSSPPSDL en Xvfb.
- [ ] Backend Vulkan de la GE para GT (`NativeGE=true` dibuja mal: escrito para LCS).
- [ ] Probarlo jugando de verdad (el usuario) y ajustar controles.

## Fase 5 — Más adelante

- [ ] Sin parte recompilada: 100 % fuente decompilada.
- [ ] **Mods**: cambios sobre la fuente decompilada (lógica, coches, circuitos, UI) sin tocar el
      binario original; carga de datos modificados de `GT.VOL` con las herramientas de Nenkai.
- [ ] Otras plataformas.
- [ ] Pista de matching opcional si aparecen los compiladores exactos.
