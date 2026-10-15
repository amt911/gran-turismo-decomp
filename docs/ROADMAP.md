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
      Temperature}`, `src/game/unit/unit_system.cpp` (antes `munit.cpp`)), con `GuestFrame` para locales en la pila guest.
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
- [x] **Funciones triviales** (`src/core/trivial.cpp`, `src/core/accessors.cpp`): vacías (37),
      lectores de una global (199), `return this->campo` (46), `return 0` (10), reenvío con
      `this->miembro` (49) y `return (*this)->campo` (16). Los stubs de importación (`.sceStub.text`)
      quedan fuera: parecen vacíos pero el cargador los parchea (FINDINGS). Sabotajes detectados en los
      lectores de globales (199/199) y en los reenvíos. `tools.progress` ya cuenta los registros de un
      argumento. **943 funciones, 0 discrepancias** (`tools.progress`: 939, 4,05 % de funciones,
      1,31 % de bytes).
- [x] **Reenvíos y fábricas** (`src/core/forwarders.cpp`, `src/core/factories.cpp`): llamar y
      devolver (13), guardar la vtable y saltar a la base (23), crear una instancia de la clase en un
      holder (`make_instance`: reservar, construir, iniciar el holder; 46) y asignar desde la fábrica
      (`assign_from_factory`; 56). Sabotajes detectados en todas. En guardar-y-saltar, la base vuelve
      a escribir `*this`, así que el sabotaje se hizo sobre s0. **1.081 funciones, 0 discrepancias**
      (`tools.progress`: 1.077, 4,64 % de funciones, 1,51 % de bytes).
- [x] **Constructores, bucles y conversiones** (`src/core/constructors.cpp`,
      `src/game/adhoc/try_cast.cpp`): constructor base + vtable (44), recorrido de un array cuyo
      destructor de elemento está vacío (47) y `try_cast` (asignar si es instancia de la clase; 2 de
      111: el recompilador no tiene entrada en las otras 109 y el juego no las llama). Sabotajes
      detectados en todas. **1.174 funciones, 0 discrepancias**.
- [x] **Cadenas, destructores y listas libres** (`src/core/chains.cpp`, `destructors.cpp`,
      `containers.cpp`): `first(this); return this` (9), `return next(first(...))` (1), destructores
      vtable + cuerpo/miembro + base o delete (4), `free_list_push` (43). `tree_clear` (44 candidatas)
      no se ejecuta en la carrera. Sabotajes detectados en todas. **1.231 funciones, 0 discrepancias**.
- [x] **Tres recorridos** (`config/pad_routes/`): la carrera, el concesionario (lista, vista 3D y
      ficha de un coche) y récords/garaje/Theatre. Las candidatas de todas las plantillas se pasaron
      por los tres. +69 instancias que solo alcanzan los menús; ninguna registrada empeoró.
      **1.300 funciones, 0 discrepancias en las tres rutas.** Las plantillas están saturadas con esta
      cobertura: lo que sigue es decompilar nativos y funciones a mano.
- [x] **Nativos de los menús** (concesionario y garaje): `MSpecDB.getCarPrice/getCarName/
      getCarColorName/getCarCatalogMs` (el texto de la ficha: "---", "%d", "%d kg"). Sabotajes
      detectados. (`getCarVariation`, `MListBox.getItemCount`, `setClearColorBuffer` e `IsDebugVersion`
      parecían no alcanzarse: era la parada temprana de #53; registrados en la entrada de abajo.) `loadCarArray/loadCourseMap` leen ficheros (no comparables, registro desactivado con el
      motivo); `getCarCatalogLength/Height/Width` comparten cuerpo con `Ms` pero ninguna ruta las llama.
      **1.304 funciones, 0 discrepancias en las tres rutas.**
- [x] **printf de la libc** (`src/libc/printf.cpp`): no es el de newlib; formatea a través de objetos
      «sink» de C++. `sprintf/snprintf/vsprintf/vsnprintf`, los sinks de cadena y acotado (init y
      write), la base del sink y el lector de anchura/precisión. Sabotajes detectados en las 12.
      `calloc/realloc` escritos pero ninguna ruta los llama. **1.316 funciones, 0 discrepancias.**
- [x] **Hojas** (`src/core/leaves.cpp`, `chained.cpp`): funciones de `jr $ra` + una instrucción
      (constante, dirección de miembro, setters, getters de byte/float, borrar campo, devolver
      argumento; 49) y de puntero + un acceso a través de él (getters/setters vía `this->ptr`, dos
      setters; 44). Elegidas con un sondeo de llamadas (4.103 funciones del juego se ejecutan en la
      carrera). Sabotajes detectados en todas. **1.409 funciones, 0 discrepancias en las tres rutas.**
- [x] **Corrección:** desde #50 todas las ejecuciones `--decomp diff` se paraban pronto (una
      dirección mal copiada en un nativo de la ficha, 0x0893244C en vez de 0x08932448) y aun así salían
      con 0: lo verificado en #50–#52 cubría solo hasta esa parada. Arreglada la dirección, y
      `gt_native` sale con **3** si el runtime se para por algo que no sea el límite de tiempo
      (FINDINGS). Repetidas las tres rutas completas: **1.409 funciones, 0 discrepancias**.
- [x] **Funciones calientes a mano** (`src/game/unsorted/hot_leaves.cpp`, 47): las pequeñas que más se
      llaman en la carrera según el sondeo — accesores de arrays y de la entrada actual, banderas,
      el decodificador UTF-8, la reserva alineada que usan las fábricas, un cerrojo anidado sobre
      `heap_lock`, conversión bits→float [0,1)… Con la parada temprana arreglada, también se registran
      `getCarVariation`, `getCarCatalogLength/Height/Width`, `MListBox.getItemCount`,
      `setClearColorBuffer` e `IsDebugVersion`. Sabotajes detectados en todas.
      **1.463 funciones, 0 discrepancias en las tres rutas.**
- [x] **Más funciones calientes** (+37 en `hot_leaves.cpp`): `crc32` de zlib (tabla IEEE estándar),
      una caché hash de 2048 entradas (buscar/insertar), el `append` de un búfer dinámico, la búsqueda en
      una lista u16, accesores vía `this->ptr`, constructores de listas… Sabotajes detectados en las 37.
      **1.500 funciones, 0 discrepancias en las tres rutas.**
- [x] **Plantilla `holder_virtual`** (`src/core/virtuals.cpp`, 13 de 19 candidatas: `if (h && h->obj)
      h->obj->vslot(h->obj, h)`) y +15 funciones calientes a mano (las esquinas de un rectángulo en
      un vector de puntos, la diferencia de ángulos envuelta a [-π, π), un vec3 global…). Sabotajes
      detectados en todas. **1.528 funciones, 0 discrepancias en las tres rutas.**
- [x] **+15 funciones calientes**: el setter de un bloque de parámetros de sonido (valores int16 con
      máscaras de cambiado/puesto, entre `enter`/`leave` que llaman a punteros a función globales),
      la búsqueda en una pila bajo un mutex, lecturas con cerrojo, `a &= ~b` y borrado de conjuntos
      de bits, el bucle que llama a un virtual de cada elemento… Sabotajes detectados en las 15.
      **1.543 funciones, 0 discrepancias en las tres rutas.**
- [x] **+18 funciones calientes**: un búfer circular (avanzar lectura/escritura), una pila de
      elementos de 16 bytes que llama a un virtual al desapilar, quedarse con la distancia mínima,
      copiar estado del renderizador global, una tabla acotada… Sabotajes detectados en las 18.
      **1.561 funciones, 0 discrepancias en las tres rutas.**
- [x] **+15 funciones calientes**: más accesos a los slots de 0x2F8 bytes (tipo, flag, kind), lecturas
      bajo cerrojo, copiar un conjunto de bits, el máximo de dos contadores… Sabotajes detectados en
      las 15 (script genérico: perturba v0/f0 y s0 en las `Void` y con llamada de cola).
      **1.576 funciones, 0 discrepancias en las tres rutas.**
- [x] **+16 funciones calientes**: reinicios de estado (distancias a 1000.0f, ids a 0xFF), una cuenta
      atrás que rota cinco entradas, el recorrido de una cadena de nodos limpiando un bit, la bandera
      calculada con umbrales de 0.9… Sabotajes detectados en las 16.
      **1.592 funciones, 0 discrepancias en las tres rutas.**
- [x] **+23 funciones calientes**, entre ellas **`isInstanceOf`** (0x08A76F20: la clase del objeto por
      su virtual +0x24 y la cadena de padres en +0x38; la usan todas las conversiones de los scripts) y
      un generador aleatorio de 15 bits con las constantes del `rand` de ANSI C. Sabotajes detectados en
      las 23. **1.615 funciones, 0 discrepancias en las tres rutas.**
- [x] **+27 funciones calientes**: enlaces válidos bajo cerrojo, desapilar de una pila de palabras,
      la distancia a lo largo de una dirección (|dot|), una matriz 3x3 identidad, asignar con
      retain/release bajo mutex, conversiones byte→float (/100, ×10)… Sabotajes detectados en las 27.
      **1.642 funciones, 0 discrepancias en las tres rutas.**
- [x] **+30 funciones calientes**: listas de callbacks y de objetos recorridas bajo su mutex, avisos
      que se saltan si están desactivados, colas globales, el reloj de 64 bits, lecturas bajo cerrojo,
      dos `madd` que conservan HI… (`func_08AD96A0` escrita, no se alcanza). Sabotajes detectados en las
      30. **1.672 funciones, 0 discrepancias en las tres rutas.**
- [x] **+19 funciones calientes**: `a |= b` de conjuntos de bits, copias de vec3/vec4 a objetos, un
      setter con retain/release, la clase de un objeto y su virtual +0x10C, un tope `min(f, +0xFC + 1)`…
      Sabotajes detectados en las 19. **1.691 funciones, 0 discrepancias en las tres rutas.**
- [x] **+21 funciones calientes**: destructores y constructores de los nodos de la interfaz (vtables,
      retain/release), un bit por índice (también con índices negativos), un escalado 2D de matriz, la
      mezcla de un float con `func_08B28FCC`… Sabotajes detectados en las 21.
      **1.712 funciones, 0 discrepancias en las tres rutas.**
- [x] **+17 funciones calientes**: lecturas a través de un enlace (dueño en +8, índice en +4) hacia el
      cuerpo de su ranura, una pila de holders con marca de nivel máximo, la fábrica de un objeto de
      0x14 bytes, el par float+bandera de un bloque, accesores por índice con comprobación de rango…
      Sabotajes detectados en las 17. **1.729 funciones, 0 discrepancias en las tres rutas.**
- [x] **+18 funciones calientes** (las más llamadas que quedaban en la carrera): visibilidad por
      virtual y banderas, la tabla de ranuras por fila/columna, copias de vec4 hechas con la VFPU,
      reenvíos a un delegado, el empaquetado y aplicación de un color, un lector de un byte por
      virtual… Sabotajes detectados en las 18. **1.747 funciones, 0 discrepancias en las tres rutas.**
- [x] **+15 funciones calientes**: el puntero con recuento de referencias (copia y asignación), las
      mezclas de vectores de una clase (seis componentes, con la multiplicación de la FPU de la PSP:
      inf × 0 da su NaN), un hash de 4 bytes por tabla, un escalado global antes de una llamada, dos
      recorridos de hijos… Sabotajes detectados en las 15. **1.762 funciones, 0 discrepancias en las
      tres rutas.**
- [x] **+12 funciones calientes**: un paso pseudoaleatorio (`*state = *state * 17 + 17` y su hash),
      el plegado de un ángulo a [-0,25, 0,25] antes de una función de onda, la pertenencia a un rango,
      un despacho entre dos punteros, un byte con signo… Sabotajes detectados en las 12.
      **1.774 funciones, 0 discrepancias en las tres rutas.**
- [x] **+15 funciones calientes**: despachos entre los dos punteros de un par, llamadas virtuales a
      través de un holder, la reserva alineada a 16 bytes (`func_08A5BC14`), dos llamadas a imports
      del sistema, un enlace dueño/índice, una condición de tres bytes… Sabotajes detectados en las
      15. **1.789 funciones, 0 discrepancias en las tres rutas.**
- [x] **+20 funciones calientes**: el recorte de un valor a [0, 1], la suma ponderada x² + k·y², la
      negación de una mezcla, un entero pasado a float, cuatro lectores de floats y tres escritores de
      bytes de un objeto, reenvíos… Sabotajes detectados en las 20. Tres más (llamadas a imports del
      kernel) quedan sin registrar: el harness aislado no puede compararlas.
      **1.809 funciones, 0 discrepancias en las tres rutas.**
- [x] **+29 funciones calientes**: el cálculo de frames por segundo de un objeto (59,94 / g y
      g / 59,94), una pila de palabras (sacar n), dos constructores base (vtable), un contador de
      bytes que limpia un campo al llegar a 0, reenvíos con cerrojo (`func_08B34888`/`func_08B34B24`),
      dos pares locales para `func_08C3F490`… Sabotajes detectados en las 29; una más (import del
      kernel) queda sin registrar. **1.838 funciones, 0 discrepancias en las tres rutas.**
- [x] **+23 funciones calientes**: tres setters de globales, cinco setters a través de un puntero
      miembro, una rejilla de bytes con signo, el tope de la pila de palabras, un halfword big-endian,
      un constructor con vtable que se registra en su dueño, secuencias de tres llamadas… Sabotajes
      detectados en las 23; cinco más (imports del kernel) quedan sin registrar.
      **1.861 funciones, 0 discrepancias en las tres rutas.**
- [x] **+34 funciones calientes**: ocho consultas de una clase a su fuente (+0xC) con su valor por
      defecto (0, 1 o 49), lectores de bytes y floats de un objeto, la cabecera de 13 halfwords
      `{28, 0…}` con `[4] = 1`, un umbral de 0,25… Sabotajes detectados en las 34.
      **1.895 funciones, 0 discrepancias en las tres rutas.**
- [x] **+36 funciones calientes**: campos big-endian (palabra, halfwords con y sin signo), la
      comparación de 32 bytes (`func_08B36170(a, b, 32) == 0`), un contador que solo suma si está
      activo, actualizaciones de objetos globales con guarda, la dirección `1000·i + 332·fila + 0x38`,
      la lista de tipos que devuelven 0 (1, 3, 6, 9)… Sabotajes detectados en las 36.
      **1.931 funciones, 0 discrepancias en las tres rutas.**
- [x] **+37 funciones calientes**: los bits de estilo de una clase, el modo de dos bits en +0x60 (fijar
      y actuar según él), una mezcla en punto fijo 1.15, un predicado de orden por float, la escala ×60,
      un byte pasado a float, varias secuencias de llamadas… Sabotajes detectados en las 37; una más
      queda sin registrar porque el harness nunca ve una llamada suya en las rutas.
      **1.968 funciones, 0 discrepancias en las tres rutas.**
- [x] **+33 funciones calientes** (**pasamos de 2.000**): cuatro constructores con vtable, el recorrido
      de rutas con `/` (saltar separadores, buscar el siguiente), el reconocimiento de las etiquetas
      «3SXt»/«3SXT», un anillo con vuelta, una rejilla de celdas de 28 bytes, liberar el handle de
      +0x98, poner y quitar bits de un conjunto… Sabotajes detectados en las 33; cuatro más quedan
      sin registrar (una llama a un import del kernel, tres no se llaman en las rutas).
      **2.001 funciones, 0 discrepancias en las tres rutas.**
- [x] **+37 funciones calientes**: una lista libre de un solo enlace (sacar y devolver nodos), añadir al
      final de una lista doble, un valor de 64 bits por índice, constructores con vtable, temporales
      construidos y destruidos, un contexto global opcional… Sabotajes detectados en las 37; una más
      (import del kernel) queda sin registrar. **2.038 funciones, 0 discrepancias en las tres rutas.**
- [x] **+39 funciones calientes**: el paso de `rand()` de la biblioteca C (semilla de 16 bits), un CRC-32
      que empieza por el byte más significativo, la marca «SGXD» de los bancos de sonido, insertar
      antes de un nodo en la lista doble, recortar a [mín, máx], constructores con vtable… Sabotajes
      detectados en las 39; una más (import del kernel) queda sin registrar.
      **2.077 funciones, 0 discrepancias en las tres rutas.**
- [x] **+37 funciones calientes**: la reserva redondeada a 64 bytes y alineada (dos asignadores), cinco
      virtuales con dos argumentos a través de un holder, cinco campos de un registro buscado, una
      lista simple con cola, valores de 64 bits por fila, constructores… Sabotajes detectados en las
      37; dos más (imports del kernel) quedan sin registrar.
      **2.114 funciones, 0 discrepancias en las tres rutas.**
- [x] **+38 funciones calientes**: una pila global de nodos, la longitud de una cadena, un bit de un
      flag apuntado, la tabla de bytes indexada con límites (0 < i < 7), un objeto bajo cerrojo, virtuales
      a través de un holder, constructores… Sabotajes detectados en las 38; una más queda sin registrar
      porque el harness no la ve llamada en las rutas. **2.152 funciones, 0 discrepancias en las tres rutas.**
- [x] **+34 funciones calientes**: desenlazar un nodo de la lista doble, la tabla de cadenas indexada
      (desplazamientos tras la cuenta), una consulta con un búfer local de 128 bytes, una segunda pila
      global de nodos, el tope a 50 de un byte, un retardo de cuatro pasos, constructores…
      Sabotajes detectados en las 34; tres más (imports del kernel) quedan sin registrar.
      **2.186 funciones, 0 discrepancias en las tres rutas.**
- [x] **+34 funciones calientes**: añadir al final de una lista global, una tabla rellenada con un
      valor lejano, la banda 1/2/3 de un valor (<4.500, <5.000), dos floats divididos por 19,6, un
      campo big-endian escrito, una llamada a través de un puntero global, constructores… Sabotajes
      detectados en las 34; dos más quedan sin registrar porque el harness no las ve llamadas.
      **2.220 funciones, 0 discrepancias en las tres rutas.**
- [x] **+38 funciones calientes**: constructores con su vtable, un objeto temporal construido y
      usado, cuatro floats guardados y reenviados, lecturas negadas (0, -y, -x), un halfword con signo
      convertido a float, entradas de 0x2F8 bytes de una tabla, «el segundo objeto, si no el primero»…
      Sabotajes detectados en las 38 (una de ellas solo en su salida temprana, sabotaje dirigido); dos
      más quedan sin registrar (una sin llamadas visibles, otra con imports del kernel). Desde este
      lote: lotes de ~80 y las tres rutas en paralelo (`docs/agents/decomp-workflow.md`).
      **2.258 funciones, 0 discrepancias en las tres rutas.**
- [x] **+77 funciones calientes** (primer lote de ~80): la tabla de entradas de 1.088 bytes, virtuales
      de un «holder» con cinco argumentos, constructores que se registran en una lista global,
      destructores con base, un buffer que copia lo pendiente, setters reflejados en un objeto espejo…
      Con el **gate rápido** nuevo (`GT_DECOMP_ONLY` + sin límite de tiempo real, ~5 min) y el sabotaje
      de s0 a la entrada, detectado en las 77 a la primera. Tres más (imports del kernel) quedan sin
      registrar. **2.335 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+78 funciones calientes**: las fábricas `new` de los valores de script (reservar + construir),
      sus destructores, asignaciones a través de un temporal construido, salidas opcionales de
      halfwords, tres virtuales seguidas, copias de 384 bytes… Gate rápido limpio a la primera,
      sabotaje detectado en las 78; dos más (imports del kernel) sin registrar.
      **2.413 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+76 funciones calientes**: difusiones a los «listeners», lecturas y llamadas bajo mutex, miembros reiniciados con un valor nuevo, actualizaciones de flags, destructores… Gate rápido limpio, sabotaje detectado en las 76; tres se saltaron (tabla de saltos, argumentos desconocidos) y una más (imports del kernel) queda sin registrar.
      **2.489 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+66 funciones calientes**: comprobación de una marca de tiempo de 64 bits, setters protegidos, lecturas «vivas» bajo mutex, vectores de palabras, una búsqueda binaria, destructores… Gate rápido limpio, sabotaje detectado en las 66 (se saltaron las de VFPU, tablas de saltos y argumentos desconocidos).
      **2.555 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+70 funciones calientes**: objetos nuevos entregados a un «wrapper», setters de campos protegidos, la velocidad en km/h (3,6 × la longitud del vector), consultas a los «listeners», destructores… Gate rápido limpio, sabotaje detectado en las 70.
      **2.625 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+68 funciones calientes**: estáticos locales con guarda (__cxa_guard), mezclas por progreso, un valor que se acerca a su objetivo con límite de velocidad, búsquedas de entradas por índice, objetos nuevos envueltos… Gate rápido limpio a la primera, sabotaje detectado en las 68.
      **2.693 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+55 funciones calientes**: estáticos con constructor en línea, búsqueda en un conjunto de bits, los estados de render 2D, un suavizado de tres pasos, las clases base del árbol de visualización… El gate rápido pilló dos fallos (una constante float y un v0 heredado del llamado, documentado en FINDINGS); sabotaje detectado en las 55.
      **2.748 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+46 funciones calientes**: mezclas por progreso indexadas, destructores que liberan cadenas (contador atómico), una tabla de seno en punto fijo, copias de vectores, refrescos de «handles»… El gate rápido pilló tres fallos (dos v0 heredados y un erase al que faltaban sus argumentos); sabotaje detectado en las 46.
      **2.794 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+58 funciones calientes**: ajustes escalados con límites, refrescos de estado, matrices de cámara y transformación, un Mersenne Twister, un asignador de ids y bloques de arena.
      **2.852 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+73 funciones calientes**: bloques de estado GU, términos de profundidad de proyección, búsquedas binarias de keyframes y de claves de 64 bits, operadores de valores de script, lectores de claves de texto y destructores de nodos.
      **2.925 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+73 funciones calientes**: lectores de propiedades (el inverso de los visitantes), ajustes de canales de sonido, un hash de texto de 64 bits, una ventana de fundido y operadores de valores.
      **2.998 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+77 funciones calientes**: partes de transformaciones, operadores de división y comparación de valores, recorridos de cadenas y ancestros, una comprobación de rutas absolutas, un parser de enteros con prefijo y una lista de voces libres.
      **3.075 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+68 funciones calientes**: fusión de parámetros de sonido, geometría de ruedas, búsquedas de registros por clave de 64 bits, lectores de propiedades vec4 y constructores de paneles.
      **3.143 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+70 funciones calientes**: montajes de rueda, orientación de triángulos sobre el circuito, términos de suspensión desde los bytes del reglaje, ajustes de voces de sonido, un sufijo de rutas, ejecutores de la máquina de scripts y destructores.
      **3.213 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+74 funciones calientes**: proyección de rectángulos GU, lectores de propiedades de texto (con un helper visit_string compartido), comparaciones de cadenas, constructores de copia y destructores, un reset de cámara, curvas y triángulos.
      **3.287 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+69 funciones calientes**: empaquetado de color, búsquedas en curvas, normalización de ángulos, un escritor de longitud variable, lectores de propiedades float y de cadena, destructores de paneles y clases, setters de llamadas de script, reserva alineada, matrices de reflexión, búsquedas en el registro, espaciado de layout, un helper de directorio de ruta y peticiones de voz.
      **3.356 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+66 funciones calientes**: liberación de shared_ptr, visitas a items, enganche de componentes, redimensionado de vectores de pares, pasos de ajustes, asignación de valores hechos por script, y un sabotaje que suma a s0 en vez de invertirlo (no se anula en llamadas anidadas)
      **3.422 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+55 funciones calientes**: pasadas de dibujo de menús, bisección de curvas, escalado de agarre, desgaste y temperatura de neumáticos, carga aerodinámica, telemetría, sincronización de cuerpos físicos, lectores de propiedades por palabra clave, construcción de paquetes, coste de caminos en rejilla, copias y destructores de paneles, rutas de nodos, un hash de cadenas de 64 bits.
      **3.477 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+71 funciones calientes**: lectores de propiedades de ruta y por palabra clave, ajustes de opciones en coma flotante, la luz de cambio de marcha, modos de cambio, fábricas de objetos de carrera y de holders, constructores y destructores de lectores de texto, registros de manejadores, utilidades de rutas y extensiones, registro de nombres, tamaños de textura.
      **3.548 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+90 funciones calientes**: pasos de respuesta y telemetría del coche, comprobaciones de huecos de coche, puntos de trayectoria, modos de cambio, pools de cuerpos, listas circulares, ajustes booleanos y en coma flotante, destructores y copias de paneles, registro de nombres, pasos de vuelta, tamaños de sonido, banderas bajo cerrojo, ajuste de ángulos.
      **3.638 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+66 funciones calientes**: ayudantes de fuentes y glifos, eventos de widgets, reinicios de feedback y boost del coche, fuerzas de neumáticos, setters de cadenas y ajustes, tamaños de layout, barras de scroll, asignación de shared_ptr y filtros de sonido
      **3.704 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+77 funciones calientes**: setters de bytes por jugador, flags de la tabla de eventos, helpers del bloque seleccionado, comprobaciones de huecos de coche, listas, estado ocupado/pendiente de widgets, constructores con base estática
      **3.781 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+110 funciones calientes**: recorridos de listas y callbacks, constructores/destructores con vtable fija, visitantes de propiedades, actualizacion de conjuntos de bits y registros de slots, copias de objetos de 184 y 36 bytes, relocacion de punteros, lectores de campos big-endian, constantes de tamano y reenvios por tail call
      **3.891 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+94 funciones calientes**: constructores de listas globales y recorridos de cadenas, constructores/destructores que fijan vtable, inicializadores de estaticos locales, reenvios bajo lock, inicializadores globales perezosos, busqueda en tabla big-endian, setters de floats, reenvios por tail call
      **3.985 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+93 funciones calientes**: inicializadores de estaticos locales, constructores con base llamada que fijan vtable, consultas de registros, reenvios bajo lock, setters de floats, recorridos de cadenas, accesores de campos y reenvios por tail call
      **4.078 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+165 funciones calientes**: reenvios por getter de singleton (accesor de estatico con guarda y tail call con el objeto), inicializadores de estaticos locales, setters y lectores de campos big-endian, constructores de listas globales y vtables, reenvios a miembros de objetos, accesores pequenos y reenvios por tail call
      **4.243 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+80 funciones calientes**: ayudantes de punteros inteligentes (crear un holder, comprobar nulo, asignar y liberar), reenvios bajo lock, envoltorios de reservar y construir, destructores con par de miembros, setters de propiedades float, bucles sobre arrays de objetos y constructores con base llamada
      **4.323 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+43 funciones calientes**: ayudantes de punteros inteligentes, lecturas de propiedades y setters de bits, constructores de registros y listas, bucles sobre arrays de objetos, inicializadores y reenvios por tail call
      **4.366 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+41 funciones calientes**: reenvios por tail call y llamada cuyo callee deja intactos los registros necesarios (comprobado en el codigo generado), envoltorios de reservar y construir, bucles de tokens y ayudantes sin holder
      **4.407 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+7 funciones calientes**: reenvios y secuencias de llamadas cuyo uso de registros se probo con el codigo generado de los callee (comprobacion de slot, busqueda de clave, reserva de bloque, destructor con array liberado, consulta de tamano con holder)
      **4.414 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **Imports grabados y reproducidos en el gate** (2026-10-13): el original ejecuta una vez cada import
      que no cambia de hilo (crear/arrancar/despertar hilos, UMD, energía, abrir/posicionar ficheros…) y
      el decompilado debe hacer las mismas llamadas con los mismos argumentos; recibe el efecto grabado.
      32 funciones que antes no se podían comparar ya se comparan.
      **4.446 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+54 funciones calientes**: 49 funciones anteriores registradas de nuevo ahora que el arnes graba y reproduce imports, mas creacion de registros de hilo, bucles de reintento del asignador y busqueda del hilo actual
      **4.500 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **+6 funciones calientes**: busquedas y borrado por hilo sobre un mapa, secuencias de llamadas con locales holder, un setter de seleccion acotada bajo lock, actualizacion de slot con llamadas virtuales
      **4.506 funciones, 0 discrepancias en las tres rutas** (gate completo).
- [x] **Primer subsistema con nombres: RaceParameter** (2026-10-13): structs tipados sobre la memoria
      del juego (`include/gt/guest.hpp`) y los ajustes de carrera con los nombres del propio GT
      (`race_type`, `endless`, `disable_collision`, `launch_speed`…, sacados del registro de atributos de
      los scripts Adhoc) en `src/game/race/race_parameter.cpp`; receta en el workflow para los siguientes
      (CarParameter, SpecDB, Calendar).
- [x] **Segundo subsistema con nombres: CarParameter** (2026-10-13): `include/gt/game/car_parameter.hpp`
      (datos con tres bloques de coche de 384 bytes y la estructura de especificaciones) y
      `src/game/car/car_parameter.cpp`: los accesores del motor llevan el nombre del atributo o método
      de MCarParameter que los llama (`team_id`, `quicktune_tcs`, `color`, `getPP`…). 21 registrados con
      gate; el resto escrito pero sin registrar porque las rutas nunca lo llaman (y `begin_parts_change`, que llega a llamadas que el aislamiento no reproduce).
- [x] **Tercer subsistema con nombres: Calendar** (2026-10-13): `include/gt/game/calendar.hpp` (contador de
      días y anillo de 100 eventos de 148 bytes) y `src/game/calendar/calendar.cpp` con los nativos de
      MCalendar (`getGameDay`, `getEventSize`, `incDate`, `dateToDay`, `dayToWeekDay`, `dayToGameDay`,
      `gameDayToDay`). Solo `getGameDay` y el primer día (2009-10-01) los llaman las rutas, así que son los
      únicos registrados; el resto está escrito y sin registrar. `dayToDate`, `getEvent` y los `put*Event`
      siguen siendo el original.
- [x] **Cuarto subsistema con nombres: SpecDB** (2026-10-13): `include/gt/game/spec_db.hpp` (los registros de
      coche de 32 bytes y de circuito de 28 bytes, códigos de 64 bits = id + número de tabla) y
      `src/game/specdb/spec_db.cpp` con las lecturas simples de MSpecDB (`getCarPrice`, `getCarCategory`,
      `getCourseCondition`, `isDirt`, `canRun`, tamaños de tablas, códigos 32 a 64 bits). 18 funciones
      movidas desde `game_natives.cpp` y `hot_leaves.cpp`, 18 nuevas; 19 registradas. Las cuatro natives de
      texto (`getCarLabel`, `getCourseLabel`, `getCarShortName`, `getTunerCode`) siguen en `text_natives.cpp`
      porque comparten ayudantes con otras.
- [x] **Quinto subsistema con nombres: CarCollection** (2026-10-13): `include/gt/game/car_collection.hpp` (el garaje:
      registros de 34 bytes por coche, dos listas de 100 entradas y 30 favoritos que son `CarParameterHandle`) y
      `src/game/car/car_collection.cpp` con los nativos de MCarCollection (`hasCar`, `getCarCount`,
      `hasFavorite`, tamaños) y sus funciones del motor. Solo se registra lo que se compara por sí solo en el gate.
- [x] **Sexto subsistema con nombres: RaceOperator** (2026-10-13): `include/gt/game/race_operator.hpp` (los datos del
      operador de carrera: 392 bytes por ranura, ventana, serie de circuitos, seis bloques del plan de memoria) y
      `src/game/race/race_operator.cpp` con los nativos de MRaceOperator que ya existían más las lecturas y escrituras
      de sus atributos. Solo se registra lo que se compara por sí solo en el gate.
- [x] **Octavo subsistema con nombres: Organizer** (2026-10-13): `include/gt/game/organizer.hpp` (los datos del
      organizador de sesión: cámara, panel de carrera, buffer de repetición, condición de juego, objetos de pausa) y
      `src/game/race/organizer.cpp` con los 27 nativos de MOrganizer que ya existían y sus funciones del motor. Solo se
      registra lo que se compara por sí solo en el gate rápido o completo.
- [x] **Séptimo subsistema con nombres: CarDriverParameter** (2026-10-13): `include/gt/game/car_driver_parameter.hpp`
      (los 184 bytes del conductor: ayudas, nivel de IA, tipo humano/IA) y `src/game/car/car_driver_parameter.cpp` con los
      accesores de sus 14 atributos, sus métodos y 7 nativos de MCarDriverParameter. Las registraciones genéricas de
      `src/core/chained.cpp` y de `hot_leaves.cpp` que repetían estas direcciones se retiran.
- [x] **Noveno subsistema con nombres: UnitSystem** (2026-10-13): `include/gt/game/unit_system.hpp` (las unidades de
      visualización: velocidad, potencia, par, régimen, temperatura) y `src/game/unit/unit_system.cpp` con los 8 nativos
      de MUnit que ya existían (`munit.cpp` se retira) y las funciones del motor que usan (textos, factores de conversión,
      tabla idioma → sistema de unidades). Solo se registra lo que se compara por sí solo en el gate; `set_unit_system` solo está verificada para el sistema 1 (las otras 13 filas salen de la tabla de saltos del original).
- [x] **Contratos de retorno, segunda parte** (2026-10-14): las 30 funciones registradas `Void`/`Word` cuyos llamadores
      leen un registro que no se comparaba (`tools.contract_audit`) dejan v0 (y v1 en `func_088D9C70`) como el original
      en cada salida y se registran `Word`/`DoubleWord`. `memset` era un falso positivo del escáner (un par lwl/lwr, que ahora cuenta como escritura);
      `func_0892C8B8` se desregistra (nunca comparada). **4.519 funciones, 0 discrepancias en las cuatro rutas.**
- [x] **Lote 57 de funciones calientes** (2026-10-13, `src/game/unsorted/hot_batch_57.cpp`, 89 registradas): vectores de
      2/3/4 floats (suma, resta, producto, punto, longitud², mín/máx), las funciones que devuelven un par de 64 bits,
      los escritores de la lista de dibujo de la biblioteca gráfica, contadores bajo `suspend/resume interrupts` y
      pequeños ayudantes de objetos. Sondeo con `GT_PROBE_SYMBOLS` sobre todas las funciones sin hacer de hasta 200 bytes
      (con `--decomp off`: con `on` el sondeo rompe el arranque). Sabotajes detectados en las 89; `contract_audit` sin
      avisos para el lote. **4.608 funciones (con el resto de `main`), 0 discrepancias en las cuatro rutas.**
- [x] **Lote 58 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_58.cpp`, 85 registradas, más 12 instancias de
      plantilla en `chained.cpp` y `containers.cpp` sacadas de los clústeres de `tools.shapes`): vectores de floats, pares de
      64 bits, escritores de la lista de dibujo y los `lower_bound` / `find` de `std::map` con claves de 32 bits (13 instancias
      en tres cuerpos compartidos). `GT_DECOMP_PTR_*` y `FREE_LIST_PUSH` con constantes decodificadas de cada función. Sabotajes
      detectados en las 97 (las plantillas, saboteando a mano el cuerpo compartido). **4.705 funciones (con el resto de `main`), 0 discrepancias.**
- [x] **Lote 59 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_59.cpp`, 60 registradas): las funciones con llamadas que
      dejó el lote 58 — inicializadores de nodo con salto final (11), envoltorios que construyen un valor en un holder local (11),
      getters float que llaman a una rutina de maquetación (6), borrados de rango de un vector (4), destructores pequeños y
      barajadores de argumentos. Ninguna tiene saltos condicionales (comprobado en el asm), así que el gate ve todos los caminos.
      Los ayudantes comunes (`word`, `put`, `psp_mul`, `kGu*`) pasan a `include/gt/game/hot_common.hpp`.
      Sabotajes detectados en las 60. **4.765 funciones, 0 discrepancias.**
- [x] **Lote 60 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_60.cpp`, 30 registradas + `func_08A61E3C` con v0 corregido):
      funciones con ramas o bucles que las rutas alcanzan. El gate solo prueba los caminos que recorren los casos comparados, así que cada función
      lista en un comentario las ramas que las rutas no toman y que se compararon con el asm a mano (la medición de cobertura,
      `GT_HOT_COVERAGE` en `include/gt/game/hot_common.hpp`, dice qué ramas toman las rutas; la comparación a mano de las demás
      encontró errores en `func_088A9A50`, `func_088A9AB4`, `func_088A9AF0`, `func_088A9CBC`, `func_088A9CFC` y `func_08955428`,
      y la revisión otro que la comparación a mano no vio: `func_08C1CCD0` con key == nodo devuelve 0, no 1 — el 1 del hueco de
      retardo del beq se sobrescribe en el destino).
      Sabotajes detectados en las 30. **4.795 funciones, 0 discrepancias.**
- [x] **Lote 61 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_61.cpp`, 26 registradas): más funciones con ramas o bucles
      (destructores de rangos y listas, comparaciones de claves, envoltorios de holders, copias de cadenas con cuenta, producto vectorial, envoltura
      de un periodo). Cobertura de ramas con `tools.decomp_gate coverage`; cada rama que las rutas no toman se comparó con el asm a mano leyendo
      el DESTINO del salto (no solo el delay slot) y queda listada en el comentario de la función. Sabotajes detectados en las 26.
      **4.821 funciones, 0 discrepancias.**
- [x] **Lote 62 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_62.cpp`, 28 registradas): más funciones con ramas o bucles
      que las rutas alcanzan (llamadas virtuales a través de un miembro, bucles de copia de rangos, `strstr`, un decremento de árbol
      rojo-negro, listas de índices de 16 bits en un pool, escritores de la lista de la GE, producto vectorial). Cobertura de ramas con
      `tools.decomp_gate coverage`; cada rama que las rutas no toman se comparó con el asm a mano leyendo el DESTINO del salto y queda
      listada en el comentario de la función. Tres funciones escritas pero sin registrar (`func_08B83F40`, `func_08B83FA4`, `func_08B84270`):
      todos sus casos llegan a imports de sceSasCore que el arnés no puede grabar (no comparables). Sabotajes detectados en las 28.
      **4.849 funciones, 0 discrepancias.**
- [x] **Lote 63 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_63.cpp`, 33 registradas): copias y rellenos de rangos sobre
      `func_08A6972C` / `func_08A76ABC`, bucles de llamadas virtuales sobre rangos (pasos de 40 / 48 bytes) y listas, el siguiente nodo de un
      árbol rojo-negro, un par de recortes con dos cotas, envoltorios de holders y 10 instancias de una plantilla: el acceso a un contenedor
      vacío estático de función (byte de guarda + destructor registrado para la salida; tabla de constantes por dirección). Cobertura de ramas con
      `tools.decomp_gate coverage`; cada rama que las rutas no toman se comparó con el asm a mano leyendo el DESTINO del salto y queda listada en el
      comentario. Seis funciones escritas pero sin registrar: `func_088A964C` (comparada 0) y `func_08B590D0`, `func_08B58E28`, `func_08B83C84`,
      `func_08B83C2C`, `func_08B8421C` (todos sus casos llegan a stubs de import que el arnés no puede reproducir ni grabar). Sabotajes detectados
      en las 33. **4.882 funciones, 0 discrepancias** (puerta rápida: 33 comparadas; completa: 2.619 comparadas en la carrera).
- [x] **Lote 64 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_64.cpp`, 34 registradas, sondeo de llamadas con las cinco rutas):
      rellenos y copias de rangos (palabras, bytes, medias palabras), sumas a un campo puntero con y sin salto final, delegaciones a métodos
      virtuales de un objeto hijo, accesos de holders comprobados (un holder nulo llama a `func_08A5BB7C` y el código sigue) y un contador con
      `ll`/`sc`. `func_08B86CA8` convierte su llamada final recursiva en bucle (el arnés lee un salto a la propia entrada como retorno). Cobertura
      de ramas con `tools.decomp_gate coverage`; cada rama no tomada se comparó con el asm a mano leyendo el DESTINO del salto. `func_08B93AE8`
      (lower_bound de un árbol) escrita sin registrar: ninguna ruta la llama. Sabotajes detectados en las 34. **4.916 funciones, 0 discrepancias**
      (puerta rápida: 34 comparadas en las cinco rutas; la completa llega a pocas del menú: las rutas dealer/garage/buy/options apenas salen
      del título con todas las funciones instaladas).
- [x] **Lote 65 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_65.cpp`, 34 registradas, sondeo de llamadas con las cinco rutas):
      thunks de punteros a método miembro, bucles «llama a una función en cada elemento de un rango» (pasos de 8, 0x30, 0x40, 0x60), copias de
      elementos que saltan un destino nulo, destructores que sueltan un holder por su vtable (con y sin cola a `operator delete`), borrado recursivo
      de nodos de árbol, siete instancias del paso de acotado de `func_08C4045C` (mismas instrucciones, comparadas una a una) y un relleno de
      `func_08A590CC`. Cobertura de ramas con `tools.decomp_gate coverage`; cada rama no tomada se comparó con el asm a mano leyendo el DESTINO del
      salto (varias rutas solo ven rangos vacíos, punteros no nulos o un holder siempre presente: el comentario de cada función lo dice).
      Escritas sin registrar: `func_08BDADAC` (ninguna ruta la llama) y siete que escriben memoria sin que ningún caso comparado la cambie
      (`func_08C15F9C`, `func_08C4B568`, `func_08C4B740/7A0/800/860`, `func_088824F8`, y las instancias `func_08C40FA4` / `func_08C413DC`).
      Cada `call_guest` / `tail_call` pasa los registros de argumento que el original fija antes del `jal`. Sabotajes detectados en las 34.
      **4.950 funciones, 0 discrepancias** (puerta completa: 4.408 comparadas en las cinco rutas, 542 sin comparar).
- [x] **Lote 66 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_66.cpp`, 39 registradas; sondeo de llamadas con `GT_PROBE_SYMBOLS`
      sobre todas las funciones sin hacer de hasta 70 instrucciones, en las cinco rutas): cuatro familias de la misma forma de instrucciones, cada
      miembro con sus constantes leídas de sus propias palabras (dos copian un par de 8 bytes estático y el que apunta `a2` — y en la variante B
      el de `a3` — a la pila y llaman a un trabajador con punteros a las copias; un «inicializa un handle» de 16 instrucciones; un «llama con una
      constante» de 11), los tres tail calls con holder comprobado (`func_08AA9510/93F0/9100`), `func_08BAFF30`, `func_08C1DA98`, `func_08BB4368`
      (descenso de un árbol por un float, solo con árbol vacío en las rutas: el descenso se comparó con el asm a mano) y `func_08C54A7C`.
      Cobertura de ramas con `tools.decomp_gate coverage`; cada rama no tomada se comparó con el asm leyendo el DESTINO del salto.
      Escritas sin registrar: `func_08B83DE8`, `func_08B83E24`, `func_08B83FF4` (su trabajador es un stub de importación `sceSasCore`: 14 casos
      «no comparable» por ruta, 0 comparados) y `func_08BE6D44`, `func_08C4B3C0` (escriben memoria y ningún caso comparado la cambió).
      Cada `call_guest` / `tail_call` pasa los registros de argumento que el original fija antes del `jal` (t0-t3 incluidos). Sabotajes detectados
      en las 39. **4.989 funciones, 0 discrepancias** (puerta completa: 4.444 comparadas en las cinco rutas, 545 sin comparar).
- [x] **Lote 67 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_67.cpp`, 47 registradas; mismo sondeo de llamadas que el lote 66, ahora
      agrupando por forma de instrucciones con los registros conservados y leyendo las constantes de las palabras de cada miembro): cinco familias de tamaño
      medio — «búsqueda en un mapa de cadenas con cuenta de referencias» (65 instrucciones, 7: límite inferior con `memcmp` y desempate por longitud),
      «getter por puntero a miembro y luego setter con argumentos» (49, 8; el setter es un tail call), la misma con el valor convertido por un thunk
      de `a2` en medio (47, 9 registradas de 10 escritas; cada miembro con su convertidor y sus desplazamientos), «estático local de 52 bytes bajo el guard de C++» (44, 14 de 15) y
      «estático local de 12 bytes enlazado a una lista global» (30, 9). Aprendido: el hash de forma enmascara las direcciones de llamada — los destinos
      externos de cada miembro se comprobaron aparte (tres convertidores distintos en la familia de 47) — y un setter simple deja `v0` como lo dejó la
      llamada anterior. Cobertura de ramas con `tools.decomp_gate coverage`: todas las ramas tomadas salvo «el guard devuelve 0» de los dos estáticos
      (comparado con el asm leyendo el destino). Escritas sin registrar: `func_08C0BC20` y `func_08BC48B4` (escriben memoria y ningún caso comparado la
      cambió). Cada `call_guest` / `tail_call` pasa los registros de argumento que el original fija antes del `jal`. Sabotajes detectados en las 47.
      **5.036 funciones, 0 discrepancias** (puerta completa sobre `main` con el lote 66 integrado: ver el PR #175).
- [x] **Lote 68 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_68.cpp`, 44 registradas; sondeo de llamadas con `GT_PROBE_SYMBOLS`
      sobre todas las funciones sin hacer de hasta 70 instrucciones, en las cinco rutas): termina familias de forma ya decompilada (lotes 59, 63 y 66) con
      el patrón existente y las constantes y destinos de llamada leídos de las palabras de cada miembro, tras comparar cada miembro con su plantilla
      palabra a palabra (solo difieren constantes reubicadas y el destino del jal/j): 7 «pair call A», 6 «pair call B», 6 inicializadores de nodo con tail
      call, 3 pasos de acotado, 6 «llama con una constante» (dos trabajadores distintos) y 16 accesores de un contenedor estático vacío.
      Cobertura de ramas con `tools.decomp_gate coverage`; cada rama no tomada se comparó con el asm a mano (el paso de acotado: ramas de otros límites; el
      contenedor: `func_08B6441C` devolviendo 0). Escrita sin registrar: `func_08C401C4` (escribe memoria y ninguno de sus 49 casos la cambió).
      Cada `call_guest` / `tail_call` pasa los registros de argumento que el original fija antes del `jal`. Sabotajes detectados en las 44.
      **5.080 funciones, 0 discrepancias** (puerta completa sobre `main` con el lote 67 integrado: ver el PR #176).
- [x] **Lote 69 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_69.cpp`, 47 registradas; mismo sondeo de llamadas, 1.475 funciones
      alcanzadas sin hacer en 958 formas): termina miembros de las familias de los lotes 59 y 66 con el patrón existente, tras comparar cada miembro con
      su plantilla palabra a palabra (solo difieren constantes reubicadas y el destino del jal/j, comprobado contra la palabra cruda): 17 «pair call A»,
      14 «pair call B», 8 «init handle» y 8 inicializadores de nodo con tail call. Sin ramas (código lineal): la cobertura confirma que toda región se
      ejecutó. Dejadas fuera por ahora: ~139 miembros más de esas cuatro familias (alcanzados una vez por ruta) y las familias nuevas de 32-36 instrucciones
      con 29-38 miembros alcanzados. Sabotajes detectados en las 47. **5.127 funciones, 0 discrepancias** (puerta completa sobre `main` con el lote 68 integrado: ver el PR #177).
- [x] **Lote 70 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_70.cpp`, 50 registradas; primer lote hecho con `tools.family_members`, PR #178:
      sondeo con `tools.decomp_gate probe` —12.734 funciones sin hacer de hasta 70 instrucciones, cinco rutas con `--decomp off`— y miembros de cada familia
      con las constantes leídas de sus propias palabras y de las reubicaciones del ELF, sin otra diferencia con la plantilla): la familia nueva de 35
      instrucciones «objeto estático local bajo el guard de C++ con constructor, vtable y registro de otro estático» (37 miembros alcanzados, la plantilla
      escrita tras leer `func_08B9FA50` entera a mano) y 13 miembros más del «accesor de contenedor vacío» de 32 instrucciones (los 12 más alcanzados ya
      estaban en el lote 63: la herramienta no contaba las instancias `HOT63_CONTAINER(0x..u);` de un solo argumento, corregido en el PR #178). Cobertura de ramas
      con `tools.decomp_gate coverage`: toda rama tomada salvo «el acquire del guard devuelve 0» (ambas familias) y «`object` nulo» (imposible: es una
      dirección constante), comparadas con el asm leyendo el DESTINO. Cada `call_guest` / `tail_call` pasa los registros de argumento que el original
      fija antes del `jal` (el registrador no lee ninguno: comprobado en sus 8 destinos). Sabotajes detectados en las 50, `contract_audit` 0.
      **5.177 funciones, 0 discrepancias** (puerta completa sobre `main` con los lotes 68 y 69 integrados: ver el PR #180). Fuera por ahora: ~139 miembros de las
      familias de los lotes 59/66 (A 53, B 38, init handle 31, nodo 17: que se excluyeron con `--exclude-ref` mientras los PR #176/#177 estaban abiertos), el resto
      del contenedor (4) y las familias de 36-68 instrucciones con 6-8 miembros.
- [x] **Lote 71 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_71.cpp`, 50 registradas; termina miembros de las familias de los lotes 59/66 con
      `tools.family_members` (sondeo del lote 70, `--exclude-ref` del lote 70): 20 «pair call A», 15 «pair call B», 8 «init handle» y 7 inicializadores de nodo con
      tail call. Las filas se generan con `--cpp` y se releen con `--check` (50 coinciden con sus propias palabras, 0 sin analizar). Sin ramas (código lineal):
      la cobertura confirma que toda región se ejecutó. Cada `call_guest` / `tail_call` pasa los registros de argumento que el original fija antes del `jal`.
      Sabotajes detectados en las 50, `contract_audit` 0. Fuera por ahora: ~89 miembros más de esas cuatro familias (A 33, B 23, init handle 23, nodo 10).
      **5.227 funciones, 0 discrepancias** (puerta completa: 4.639 comparadas en las cinco rutas, 588 sin comparar).
- [x] **Lote 72 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_72.cpp`, 50 registradas; termina miembros de las familias de los lotes 59/66 con
      `tools.family_members` (sondeo del lote 70, `--exclude-ref` del lote 71 / PR #183): 33 «pair call A» (todos los que quedaban) y 17 «pair call B». Las filas se
      generan con `--cpp` y se releen con `--check` (50 coinciden con sus propias palabras, 0 sin analizar, 0 líneas `!!`). Sin ramas (código lineal): la cobertura
      confirma que toda región se ejecutó. Cada `call_guest` pasa los registros de argumento que el original fija antes del `jal`. Sabotajes detectados en las 50.
      **5.277 funciones, 0 discrepancias** (puerta completa sobre `main` con el lote 71 integrado: ver el PR #185).
      Fuera por ahora: 6 «pair call B», 23 «init handle» y 10 inicializadores de nodo (familias de los lotes 59/66).
- [x] **Lote 73 de funciones calientes** (2026-10-14, `src/game/unsorted/hot_batch_73.cpp`, 42 registradas de 46 escritas; termina miembros de las familias de los lotes
      59/63/66/68 con `tools.family_members` (sondeo del lote 70, `--exclude-ref origin/decomp/hot-batch-72`): 6 «pair call B», 23 «init handle», 10 inicializadores
      de nodo con tail call y 7 «clamp step» (26 instrucciones, sin constantes propias). Las filas se generan con `--cpp` y se releen con `--check` (39 coinciden con
      sus propias palabras, 0 sin analizar; las 7 de `clamp_step` no llevan argumentos). Sin ramas salvo `clamp_step`, cuyas ramas no tomadas (upper == 0, lower >= upper,
      x >= upper) se leyeron en el lote 63/68 contra el asm. Cada `call_guest` / `tail_call` pasa los registros de argumento que el original fija antes del `jal`.
      Sabotajes detectados en las 46, `contract_audit` 0. Escritas pero sin registrar: 4 miembros de `clamp_step` (`func_08C40FA4`, `func_08C413DC`, `func_08C41934`,
      `func_08C41A04`: ningún caso comparado cambió memoria, como `func_08C401C4` en el lote 68). Fuera por ahora: 6 accesores de contenedor vacío de 32 instrucciones
      que son una variante (dos estáticos más y otro desplazamiento: plantilla propia tras leer uno entero), el «pair call A» restante y las familias de 47-68 instrucciones.
      **5.319 funciones, 0 discrepancias** (puerta completa sobre `main` con el lote 72 integrado: ver el PR #186).
- [x] **Lote 74 de funciones calientes** (2026-10-15, `src/game/unsorted/hot_batch_74.cpp`, 46 registradas de 55 escritas; sondeo del lote 70, `--exclude-ref origin/decomp/hot-batch-73`):
      6 accesores de contenedor vacío de 32 instrucciones (los de palabras distintas a las del lote 68: otros estáticos y un desplazamiento distinto, guarda + 8 / + 24; leído uno
      entero contra el asm y el resto con la misma plantilla), 8 «target setting» de 68 instrucciones (`func_088492F8`) y 2 de la variante booleana de 62, 5 «getter, luego setter
      con el valor» (47), 3 «static record» (44), 2 «map find», 2 «getter, luego setter con argumentos», 2 «static linked object», 2 «destroy range», 4 «call with a constant»,
      3 «convert, then tail», 1 «holder conversion», 5 «base constructor, luego dos stores» (el segundo store en +16, no +8) y 1 «range erase». Las filas se generan con `--cpp`
      y se releen con `--check` (46 coinciden con sus propias palabras, 0 sin analizar). Ramas no tomadas (`member_function` virtual, `null_check`, `map_find`, el guarda que
      otro construye) leídas contra el asm y listadas en los comentarios. Cada `call_guest` / `tail_call` pasa los registros de argumento que el original fija antes del `jal`.
      Sabotajes detectados en las 46, `contract_audit` 0. Escritas pero sin registrar: 5 «clamp step» y 2 «range erase» (ningún caso comparado cambió memoria) y 2 «holder
      conversion» (su única llamada es «no comparable»). Fuera por ahora: los 4 «organizer» de 37 instrucciones (plantilla en `organizer.cpp`), `func_08C0BC20` y `func_08BC48B4` (sin
      registrar desde el lote 67 por la misma razón) y las familias de 2-3 miembros de menos de 30 instrucciones.
      **5.365 funciones, 0 discrepancias** (puerta completa sobre `main` con el lote 73 integrado: ver el PR #188).
- [x] **Lote 75 de funciones calientes** (2026-10-15, `src/game/unsorted/hot_batch_75.cpp`, 39 registradas de 61 escritas; sondeo del lote 70, `--exclude-ref origin/decomp/hot-batch-73 --exclude-ref origin/decomp/hot-batch-74`):
      hojas de 2 instrucciones (devolver el primer argumento ×6, salto a otra función, `a1 = 0/1` + salto, `a0 += n` + salto, `a0 = *(a0+n)` + salto, constante, vacía),
      «load, then call» (7), «store vtable» (4), «call, then tail» (10 y variantes), «record query» (15, 3), «function-local static» (9), «swap and jump» (2),
      «uso del objeto de un holder» (19), fábricas de objetos (2 + 2) y los 2 «organizer getter» de 37 instrucciones que el lote 74 dejó fuera (helper propio en el lote; la plantilla de `organizer.cpp` vive en un espacio de nombres anónimo).
      Filas generadas con `--cpp` y releídas con `--check` (53 coinciden con sus propias palabras, 0 sin analizar). Ramas no tomadas (objeto nulo, `out` igual al holder local) leídas contra el asm.
      Sabotajes detectados en las 40 comparadas; `func_08B77BF4` queda sin registrar porque `contract_audit` marca tres llamadores que leen v1. 21 escritas sin registrar (compared 0 en las cinco rutas rápidas).
      Fuera: «clamp step», «range erase» y «holder conversion» sin registrar desde el lote 74, `func_08825AE8`/`func_08825B74` (UMD y hilos), los setters de organizer ya listados como compared 0.
      **5.404 funciones, 0 discrepancias** (puerta completa sobre `main` con los lotes 73 y 74 integrados: ver el PR #190).
- [x] **Lote 76 de funciones calientes** (2026-10-15, `src/game/unsorted/hot_batch_76.cpp`, 62 registradas de 73 escritas; elegidas por alcance, no por forma: sondeo del lote 70
      ordenado por llamadas y llamadores ya decompilados, `--exclude-ref` de los lotes 74 y 75): 31 funciones leídas una a una contra el asm (20 de ellas registradas: escritores de la lista de display de la
      biblioteca gráfica, ayudantes de coma flotante, pegamento de coma flotante por software, `func_08B34C1C`, llamadas virtuales a través de un temporal, tablas de bytes) y 5
      familias con plantilla leída entera: «init handle con índice» (18 instrucciones, 10), «pair call D» (19, 9), «relocation loop» (36, 9; tres miembros los llama `func_08AF771C`),
      «handle wrapper» (21, 6) y «guarded static sin tamaño» (35, 8). Las filas de familia se generan con `--cpp` y se releen con `--check` (42 coinciden con sus propias palabras, 0 sin
      analizar). Ramas no tomadas (cobertura LLVM) leídas contra el asm y listadas en los comentarios; `func_08B34C1C` reproduce v0 en cada camino porque el HLE de
      `sceKernelCpuResumeIntr` no lo escribe. `func_08B70F28` devuelve 64 bits (`contract_audit` marcó a los llamadores que leen v1). Sabotajes detectados en las 62, `contract_audit` 0.
      Escritas pero sin registrar (no comparables: el original llega a imports que bloquean o que el harness no reproduce): `func_08AD96A0`, `func_08AEB908`, `func_08AEB930`,
      `func_08B0C650`, `func_08B33A24`, `func_08B52D58`, `func_08B58E28`, `func_08B58E84`, `func_08B590D0`, `func_08B83C84`, `func_08B84270`. Fuera: las funciones con VFPU (no hay
      ayudantes para sus instrucciones) y los envoltorios de `sceSasCore` (mismo motivo que `func_08B83C84`).
      **5.466 funciones, 0 discrepancias** (puerta completa sobre `main` con los lotes 73-75 integrados: ver el PR #192).
- [x] **Lote 77 de funciones calientes** (2026-10-15, `src/game/unsorted/hot_batch_77.cpp`, 64 registradas; familias de forma sin plantilla registrada, del sondeo del lote 70 con `--exclude-ref` del lote 76, `tools.family_members --list --any-template`):
      16 familias, cada una con un miembro leído entero contra el asm a mano (desensamblado con las reubicaciones aplicadas) y el resto comparado palabra a palabra por la herramienta: «holder apply» (58 instrucciones, 9),
      «guarded static con tamaño» (40, 6), «counted assign» (35, 6; el puntero con cuenta de referencias con `ll`/`sc`), «llamada por puntero a función miembro + cola con float» (47, 6), «lower bound en un árbol de
      cadenas» (40, 6), «para cada elemento, llamar un puntero a miembro» (45, 5), «init con vtable + cola» (14, 4), «construir cada elemento» (32, 4), «borrar un subárbol» (31, 4; recursiva por su propia dirección;
      `func_08C00860` es la que más casos recibe: 98 comparados) y las pequeñas «copiar cuatro floats» (13, 3), «handle init» (14, 3), «construir con base» (17, 3), «destructor de dos holders» (19, 2; la forma de
      `func_08C1EB60` del lote 59) y «record call» (19, 3). Las filas se generan con `--cpp` y se releen con `--check` (49 coinciden con sus propias palabras, 0 sin analizar; 15 filas sin argumentos no se comparan por
      columnas, sus palabras ya coinciden con la plantilla). Ramas no tomadas (cobertura LLVM) leídas contra el asm y listadas en los comentarios: las rutas solo comparan `count > 0` en «holder apply», punteros a
      miembro no virtuales, y el contador anterior nulo en «counted assign» (la rama de decremento y la llamada virtual quedan comprobadas solo contra el asm). Sabotajes detectados en las 64, `contract_audit` 0,
      puerta rápida 64/64 comparadas, 0 discrepancias. Fuera: «clamp» (`func_08C401C4` y sus 4 instancias) y «release a vector» (`func_08C4B740` y 3 más): ya escritas y sin registrar en los lotes 65/68/73/74 porque ningún
      caso comparado cambia memoria (la herramienta solo cuenta como hechas las registraciones activas, así que vuelve a listarlas). **5.530 funciones, 0 discrepancias** (puerta completa sobre `main` con los lotes 73-76 integrados: ver el PR #195).
      integrado, 4.825 comparadas; el PR #192 (lote 76) tendrá que rebasar los recuentos al integrarse).
- [x] **Lote 78 de funciones calientes** (2026-10-15, `src/game/unsorted/hot_batch_78.cpp`, 57 registradas; familias de forma sin plantilla registrada, de un sondeo nuevo (`tools.decomp_gate probe`, las cinco rutas, `--decomp off`)
      con `--exclude-ref origin/decomp/hot-batch-77`; antes de leer cada miembro se buscó su dirección en `src/` y en la rama del lote 77): 17 familias, cada una con un miembro leído entero contra el asm a mano
      (desensamblado con las reubicaciones aplicadas, con la base de segmento de cada una) y el resto comparado palabra a palabra por `tools.family_members`: «wrap a» (10 instrucciones, 4), «setter de float» (2, 4),
      «copiar tres floats» (7, 3), «llamada + cola» (11, 3), «pair call» (15, 3), «guarded static de tres ceros» (25, 3), «caja compartida» (25, 3; el bloque de cuenta de referencias de 16 bytes), «llamada por puntero a miembro +
      cola por puntero a miembro» (37, 4; y la variante con argumento extra, 38, 3), «nuevo holder» (37, 3), «borrar un subárbol con claves de cadena» (42, 4; recursiva), «holder pair» (41, 3), «destructor con
      liberación» (43, 3; dos decrementos atómicos con llamadas virtuales), «reservar un vector» (63, 4; y por `malloc`, 61, 6: palabras y pares) y «buscar en un árbol de cadenas» (66, 4). Las filas se generan con `--cpp` y se
      releen con `--check` (46 coinciden con sus propias palabras, 0 sin analizar; 11 filas sin argumentos no se comparan por columnas, sus palabras ya coinciden con la plantilla). Ramas no tomadas (cobertura LLVM)
      leídas contra el asm y listadas en los comentarios: las rutas solo comparan «reservar un vector» con capacidad suficiente (el cuerpo de crecimiento queda comprobado solo contra el asm), el destructor solo con el
      primer contador distinto de 1 (las llamadas virtuales, solo contra el asm) y punteros a miembro no virtuales. Sabotajes detectados en las 57, `contract_audit` 0, puerta rápida 57/57 comparadas, 0 discrepancias.
      Fuera: «clamp», «release a vector» y las demás ya escritas y sin registrar (lotes 65/68/73/74/75), la familia de `func_08B8443C` (llama a imports de `sceSasCore` que el harness no reproduce) y las familias
      de 48-68 instrucciones con pocas llamadas, para otro lote (`func_08877418`/`func_08877D64` (atributo + cadena), `func_0884A3AC`, `func_08BCFDEC`, `func_08BB0F28`, `func_08A1503C`, `func_08BA0D34`).
      **5.587 funciones, 0 discrepancias** (puerta completa sobre `main` con los lotes 73-77 integrados: ver el PR #198).
- [x] **Lote 79 de funciones calientes** (2026-10-15, `src/game/unsorted/hot_batch_79.cpp`, 51 registradas y 2 plantillas escritas sin registrar; primero `tools.family_members --list --limit N` (por defecto 60, 0 = todas: el
      listado se cortaba en 60 familias y las siguientes nunca se vieron), luego el listado completo sobre el sondeo del lote 78 con `--exclude-ref origin/decomp/hot-batch-78`; antes de leer cada miembro se buscó su dirección
      en `src/` y en la rama del lote 78): 17 familias («cadena de punteros a miembro» en tres variantes (7), «liberar un rango de cadenas» (2), «parámetro vector con w por defecto» (3), «parámetro objeto» (3),
      «suma escalada de dos vectores» (2; 88.332 llamadas), «construir dos holders» (2), «crear un nodo» (2), «para cada nodo de una lista» (2), «para cada elemento con paso» (3), «callback compartido» (4) y seis pequeñas)
      más siete funciones sueltas leídas a mano (`func_08C22D58` con 21.961 llamadas, `func_08B879E8`, `func_08BE8B78`, `func_08B9432C`, `func_08966C70`, `func_08C309B4`, `func_08B8A004`). Las filas se generan con `--cpp` y se
      releen con `--check` (31 coinciden con sus propias palabras, 0 sin analizar; 15 filas sin argumentos comparadas con `wdiff` a mano). La puerta rápida cazó un error de lectura (`func_08BE0EB8`: un `bnez` saltaba también
      la escritura `this[2] = 0`; el índice del destino estaba mal contado), corregido antes de registrar. Ramas no tomadas (cobertura LLVM) leídas contra el asm y listadas en los comentarios: las rutas nunca pasan por la cadena
      vacía en las cuatro familias de cadenas, ni por los punteros a miembro virtuales, ni por listas/rangos vacíos. Sabotajes detectados en las 51, `contract_audit` 0, puerta rápida 51/51 comparadas, 0 discrepancias.
      Sin registrar: `func_08BBD380` y `func_08BBEBE4` (las rutas no llaman a esas dos plantillas, solo a sus hermanas). Fuera: las familias `func_0884A3AC`, `func_08BA0D34` y `func_08BD74BC` (pocas llamadas, no leídas en este lote)
      y las funciones con VFPU. **5.638 funciones, 0 discrepancias** (puerta completa sobre `main` con los lotes 73-78 integrados: ver el PR #200).
- [x] **Lote 80 de funciones calientes** (2026-10-15, `src/game/unsorted/hot_batch_80.cpp`, 61 registradas y 4 escritas sin registrar; del sondeo del lote 78 con `--exclude-ref origin/decomp/hot-batch-79`, `tools.family_members --list --limit 0
      --any-template --min-reached 1` y, para los sueltos, todas las funciones alcanzadas sin hermanas de la misma forma, por llamadas; antes de leer cada miembro se buscó su dirección en `src/` y en el lote 79, y las que
      lotes anteriores escribieron y dejaron sin registrar (clamp, release a vector, organizador) no se reescribieron). Grupo 1, familias, cada una con un miembro leído entero contra el asm a mano y el resto comparado por la
      herramienta: «asignación de cadena» (52 instrucciones, `func_08B855E0` con **984.732 llamadas** y `func_08B957D8`), «borrar de un vector de punteros» (50, `func_08B85798` con 14.545 llamadas y `func_08C028F0`), «nuevo
      holder desde un temporal» (43, `func_08B86FB0` con 1.940 llamadas y `func_08C307DC`; `func_08B8705C`, hermana que mueve a3 en vez de a2, con su propia envoltura), «borrar un subárbol con holder» (44, recursiva; solo `func_08C012FC` registrada),
      «reenviar al slot virtual de un miembro» (11, 2), ocho pequeñas de un solo miembro y ocho funciones de familias con un solo miembro alcanzado (26 en total). Grupo 2, 35 sueltas leídas a mano palabra a palabra (`func_089FE57C` con 422.197 llamadas, `func_089294F8`, `func_0887CA00`,
      `func_08C467DC`, `func_088CDCFC`, `func_08A1AAB8`, `func_0892BCC0`, `func_0888A058`, las de «holder» de `0x08C2xxxx`, las de 36-40 instrucciones de `0x088Bxxxx`/`0x088Cxxxx`, dos mezclas de floats y una matriz de
      cuaternión). Las filas se releen con `--check` (18 coinciden con sus propias palabras, 0 sin analizar). La primera puerta rápida cazó tres errores de lectura, corregidos antes de registrar: dos funciones cuya última
      llamada es una hoja que no escribe v0 (`func_089FE57C`, `func_088B46A8`: el v0 de la anterior no pasaba a los registros; ahora toda llamada pasa por `call_keep`, FINDINGS) y `func_08B86FB0`/`func_08C307DC`, que
      mueven a2 y no a3 como la hermana que se había leído. Ramas no tomadas (cobertura LLVM) leídas contra el asm y listadas en los comentarios: las rutas no pasan por objetos nulos en las siete funciones de «holder», ni
      por cadenas compartidas vacías, ni por rangos vacíos o largos en las ordenaciones y los borrados, ni por los punteros a miembro virtuales. Sin registrar por no tener ningún caso comparado que cambie memoria
      (`func_08870448`, `func_08BD2D24`, `func_08BF7980`, `func_08C2B76C`); dejadas fuera tras escribirlas (pocas llamadas y lectura larga): `func_08C1CDCC`, `func_08C21048`, `func_08B8543C`, `func_08BE6674`, `func_08B3A37C`,
      `func_08C217F4`. No leídas: las funciones con VFPU (`func_08B54474`, `func_08B564A8`, `func_089A900C`, `func_08B5A838`, `func_08B5A7A8`, `func_08B580F0`, `func_08B54288`), las que llegan a imports que no se reproducen
      (`func_08B31AB4`, `func_08AF219C`), la familia de `func_08B8443C` (flag de depuración + `sceSasCore`) y las familias de 48-68 instrucciones con pocas llamadas. Sabotajes detectados en las 65 escritas (61 registradas),
      `contract_audit` 0, puerta rápida 61/61 comparadas (2.658 casos, 1.792 que cambian memoria), 0 discrepancias. **5.699 funciones, 0 discrepancias** (puerta completa sobre `main` con los lotes 73-79 integrados: ver el PR #201).
- [x] **Lote 81 de funciones calientes** (2026-10-15, `src/game/unsorted/hot_batch_81.cpp`, 55 registradas y 3 escritas sin registrar; del sondeo del lote 78 con `--exclude-ref origin/decomp/hot-batch-80` (el PR #201),
      `tools.family_members --list --limit 0 --any-template --min-reached 1` (las familias que quedaban ya las había escrito el lote 80, salvo `func_08BE6154`, otro miembro de «nuevo holder desde un temporal») y, para las
      sueltas, todas las funciones alcanzadas sin hermanas de la misma forma, por llamadas; antes de leer cada una se buscó su dirección en `src/` y en la rama del lote 80). Sueltas leídas a mano palabra a palabra con sus
      destinos de salto y ranuras de retardo: la tabla de estados de la biblioteca gráfica (`func_08B54FB8`, **15,8 millones de llamadas**) y tres escritores de lista (`func_08B58D08`, `func_08B58F84`, `func_08B5547C`), la
      libm en float con `ReturnKind::Float` (seno/coseno `func_08B499B8`/`func_08B49CC8`/`func_08B6DEF8`, `func_08B66604`/`func_08B662C4`, log `func_08B847A8`, exp `func_08B84954`, pow `func_08B84A2C`, atan2 `func_08B4A430`),
      matrices y cuaterniones (`func_08B4B114`, `func_08C46184`, `func_08B4C804`/`func_08B4CBDC`, la proyección `func_08B4E360`, los ángulos de Euler `func_08C52868`/`func_08C527C0`), cadenas y libc (`func_08B864EC`, `func_08BEFC38`,
      `func_08B86674`, `strcasecmp` `func_08B7B8C4`, `strchr` `func_08B7BA50`), recorridos con puntero a miembro (`func_08BAA778`, `func_08BAA56C`, `func_08BB0C98`), serializadores sobre getters virtuales, holders, estáticas con
      guarda y cadenas de punteros a miembro. La primera puerta rápida cazó dos errores de lectura (`func_08B54FB8`: un destino de salto contado a mano una palabra corrido; `func_08C436F4`: v1 reutilizado por un `lbu` que llega al
      `jal` por su ranura de retardo) y la revisión de las ramas sin tomar un tercero (los estados 0 y 1 de `func_08B54FB8` dejan v0 = 0): FINDINGS, y herramienta nueva `tools/disasm.py` (destinos resueltos y relocaciones aplicadas,
      con pruebas). `func_08B4A430` pasó a `Float` y `func_08BB0C98`/`func_08BAA56C` a `DoubleWord` (un llamador lee v1: `contract_audit`). Ramas no tomadas (cobertura LLVM) leídas contra el asm y listadas en los comentarios
      de cada función. Sin registrar por no tener ningún caso comparado que cambie memoria: `func_088AA074`, `func_088FA224`, `func_08AF2D20`. No leídas: las funciones con VFPU (`func_08B542AC`, `func_08B498AC`, `func_08B4E734`,
      `func_08B568DC`, `func_08B5A7F0`, `func_08B4ACE0`, `func_08B4A774`, `func_08B543B4`, `func_08B5606C`, `func_08B4F2E4`/`func_08B4F3B0`/`func_08B4F47C`), las que llegan a imports de hilos o mutex (`func_08B1C820`,
      `func_088693CC`, `func_08B59A24`, `func_08B31AB4`, `func_08AF219C`), `func_08B71CE0` (enteros de 64 bits) y las funciones de 48-70 instrucciones con pocas llamadas. Sabotajes (`s0` al entrar) detectados en las 55,
      `contract_audit` 0, `--check` 0 filas malas, puerta rápida 55/55 comparadas (4.493 casos, 2.636 que cambian memoria), 0 discrepancias. **5.754 funciones, 0 discrepancias** (puerta completa sobre `main` con los lotes 61-80 integrados: 4.991 comparadas, 763 sin comparar (carrera 4.731 / concesionario 3.510 / garaje 3.556 / compra 3.466 / opciones 3.614); las 55 de este lote comparadas, 4.511 casos).
- [x] **Lote 82 de funciones calientes** (2026-10-15, `src/game/unsorted/hot_batch_82.cpp`, 57 registradas y 11 escritas sin registrar; del sondeo del lote 78 con `--exclude-ref origin/decomp/hot-batch-81` (el PR #202,
      que añade `tools/disasm.py`, aún abierto: se leyó con una copia suelta de ese script) y sin ninguna dirección que #202 escriba, registre o deje sin registrar; antes de leer cada función se buscó su dirección en `src/` y
      en la rama del lote 81). Las candidatas son las sueltas alcanzadas por las **cinco** rutas, ordenadas por llamadas (`func_088FC214` con 85.258, `func_08B71CE0` con 266.025 en las rutas, `func_08B8543C` con 2,9 millones
      de llamadas), leídas a mano palabra a palabra con los destinos de salto y las relocaciones que imprime `disasm.py` (nunca contados a mano): holders y punteros inteligentes del motor, int a double blando
      (`func_08B71CE0`, `DoubleWord`) y `ceilf` (`func_08B661E8`, `Float`), `strcat`, comparación de cadenas, el reservador de representaciones de cadena, destructores con borrado, inserción y `operator[]` de un árbol con clave
      de cadena, el lector/escritor de un búfer circular, ayudantes de cola y constructores pequeños. La primera puerta rápida se paró en el centinela del harness en las cinco rutas: `func_08B7B2E4` es un `setjmp` y guarda `ra`
      (dentro de un caso, el centinela) en el búfer; un `longjmp` posterior volvía a un centinela sin caso (FINDINGS). `func_08B34CA0` dejaba el `v0` del `CpuSuspendIntr` antes del `tail_call` al stub (FINDINGS); `func_08BE6674`
      olvidaba el `lui v0` del retardo del primer `beqz`. Ramas no tomadas (cobertura LLVM) leídas contra el asm y listadas en los comentarios: en varias funciones las rutas solo pasan por la salida temprana
      (`func_088FC214`, `func_08ACCE04`, `func_08C40EBC`, `func_08C41BE4`: el cuerpo sin verificar, solo leído). Sin registrar: `func_08B59A24`, `func_08ACF390`, `func_08B33E50`, `func_08B0EBF0`, `func_08AD083C`, `func_08ABCE2C`
      (todos sus casos paran en imports que bloquean o cambian de hilo: vblank, `sceIoRead`, un hilo, `sceUtility`), `func_08B7B2E4` (el `setjmp`) y `func_08B44B24`, `func_08BE834C`, `func_08C15E8C`, `func_08BE6674` (escriben memoria
      y ningún caso comparado la cambió). No leídas: VFPU, las que llegan a imports que no se reproducen (`func_08B35418`, `func_08B7771C`, la familia de `func_08B8443C`, hilos) y las de más de 70 instrucciones. Sabotajes detectados
      en las 58 comprobadas (57 registradas), `contract_audit` 0, `--check` 0 filas malas (el lote no tiene envolturas con argumentos), puerta rápida 57/57 comparadas (2.098 casos, 1.459 que cambian memoria), 0 discrepancias.
      **5.811 funciones, 0 discrepancias** (puerta completa sobre `main` con los lotes 73-81 integrados: ver el PR #203).
- [x] **Lote 83 de funciones calientes** (2026-10-15, `src/game/unsorted/hot_batch_83.cpp`, 57 registradas; del sondeo del lote 78 con `--exclude-ref origin/decomp/hot-batch-82` (el PR #203) y sin ninguna dirección
      que #203 escriba, registre o deje sin registrar; antes de leer cada función se buscó su dirección en `src/` y en la rama del lote 82). Candidatas: las sueltas alcanzadas por las rutas, ordenadas por llamadas, sin VFPU,
      sin imports que el harness no reproduce (semáforos, la espera del vblank, `sceSasCore`, hilos, `setjmp`/`longjmp`) y sin «funciones» que son varios puntos de entrada; leídas a mano palabra a palabra con
      `tools/disasm.py` (destinos de salto y relocaciones resueltos por la herramienta, cada hueco de retardo parte de su rama): cadenas y contenedores del motor (añadir, constructor por rango, `rfind`, subcadena,
      el reservador de representaciones, recorridos y destructores de listas y árboles, el borrado de un búfer, una ordenación de valores de 16 bits), la lista de bloques del heap, la pila de registros por hilo,
      estáticos con guarda, matemáticas (interpolación, seno por rango, rotación por cuaternión, selección componente a componente, ayudantes de doble blando) y contadores, piscinas e inicializadores pequeños.
      Ramas no tomadas (cobertura LLVM) leídas contra el asm y listadas en el comentario de cada función; **compradas solo por una salida temprana o una rama lateral** (el cuerpo sin verificar, solo leído):
      `func_08C30E0C` (cuenta 0), `func_08A782D8` (origen vacío), `func_08B0B36C` (solo la salida `*ok = 0`; en la lectura a mano del cuerpo de éxito el `v0` que devuelve estaba mal y se corrigió contra el asm),
      `func_08C49140` (rangos de hasta 16 valores), `func_08B18A58`, `func_08BEEE28`, `func_08B66524` (cuadrante 1), `func_089445D8` y `func_08ADBFE0`. Sabotajes detectados en las 57 (57 registradas), `contract_audit` 0,
      `--check` 0 filas malas (el lote no tiene envolturas con argumentos), puerta rápida 57/57 comparadas (2.156 casos, 1.354 que cambian memoria), 0 discrepancias. **5.867 funciones, 0 discrepancias** (puerta completa sobre `main` con los lotes 73-82 integrados; `func_08A9DD14` queda escrita sin registrar: FINDINGS): ver el PR #204.
- [ ] Más nativos de los 286 que se ejecutan hasta la carrera (`GT_PROBE_SYMBOLS`).

## Jugable — `gt_play` (2026-10-10)

- [x] Segundo ejecutable con los backends reales de lcs-recomp (ventana SDL2, presentación Vulkan,
      teclado/mando, audio), mismo juego y mismo código decompilado que `gt_native`. Llega a la carrera
      (prueba sin pantalla: `SDL_VIDEODRIVER=offscreen` + `GT_PAD`). Controles para GT en
      `config/gt_play.ini`. Spec: `docs/superpowers/specs/2026-10-10-playable-window-design.md`.
- [x] Sonido: la mezcla de GT sale por `sceAudioOutput2` (antes se descartaba) y la música Atrac3+ se
      decodifica desde los datos que el juego entrega en su búfer (FFmpeg atrac3p, trama a trama).
- [x] Audio de los vídeos PSMF (2026-10-13): GT los lee de GT.VOL por el ring buffer de sceMpeg; el
      ATRAC3plus se saca de esos mismos paquetes (FINDINGS).
- [x] Menús e iconos/HUD que faltaban: los sprites se dibujaban antes que los triángulos pendientes
      (parche 0015; FINDINGS). Referencia: PPSSPPSDL en Xvfb.
- [ ] Backend Vulkan de la GE para GT (`NativeGE=true` dibuja mal: escrito para LCS). Spec:
      `docs/superpowers/specs/2026-10-12-gpu-ge-backend-design.md`.
  - [x] **Fase 1 — medir** (2026-10-12): `tools/gpu_gate.sh` pinta cada frame con la GPU y con el
        rasterizador por software a la vez (sin ventana) y `tools/gpu_compare.py` los compara; el gate
        se demostró fallando con un shader saboteado (253/253 frames). Resultado a ×1: pasan 61/841
        frames de la carrera, 47/607 del concesionario y 62/637 del garaje. Causas (FINDINGS): la
        escena 3D no se dibuja, texto con la textura de fuentes obsoleta, pantallas compuestas por
        render-to-texture en negro, y tres modos de blending sin soporte (sobre todo `2·α / 1−2·α`).
  - [x] **Fase 2 — GT se dibuja en la GPU** (2026-10-12): blending general (como el software), destinos
        en espacio de píxeles de la PSP (capa del HUD `0x0CC000` compuesta bien), z-buffer compartido y
        escrituras de profundidad sin test (el coche del concesionario), texturas por contenido (el texto,
        copiado glifo a glifo con block transfers). Gate ×1 (150 s/ruta): carrera 298/307, garaje 272/290,
        concesionario 183/288 frames iguales (eran 61, 62 y 47 en la fase 1).
  - [x] **Rendimiento** (2026-10-12): el present en FIFO bloqueaba ~28 ms el hilo de la GE; con MAILBOX la
        carrera va a 120 vblanks/s, 0 saltados, audio a 44,1 kHz, a ×2 y ×4 en ventana (el software da 102).
  - [x] **Fluidez, resolución y CPU** (2026-10-13): un frame por vblank como la PSP y presentado en el
        instante del vblank (100 % de intervalos en 15-18 ms de juego, 96 % en tiempo real); FIFO sin
        bloquear (X11, Android); resolución interna que sigue a la ventana (`InternalResolutionMode=Window`,
        `MaxInternalScale`); CPU de 17,9 a 7,6 s por minuto de carrera (hilos del rasterizador que giraban
        en vacío y un recorrido de todas las texturas en cada block transfer). Gate ×1 150 s: carrera
        287/292, garaje 274/290, concesionario 167/290 (el concesionario varía entre ejecuciones también en
        `main`: FINDINGS).
  - [x] **Colores y parpadeo** (2026-10-14): los vídeos y los fundidos del logo salían violetas/rosas, el cielo
        de la carrera cian plano y la primera imagen de cada pantalla nueva teñida: la primera vez que se dibujaba una
        textura recién decodificada se pintaba con el color de depuración de LCS (parche 0024). El z-buffer se redondea a
        16 bits como en la GE (parche 0025: un 12 % menos de píxeles que parpadean entre frames en la carrera; escribir la
        profundidad desactiva la prueba temprana: a medir en GPUs integradas). Gate ×1
        200 s de carrera: 1016/1092 frames iguales (1006/1095 antes); 34 s de arranque: 2 fallos (10-11 antes).
  - [x] **GPU lenta = menos imágenes, no juego más lento** (2026-10-14, parche 0026): el final de cada frame
        esperaba a que la GPU acabase el anterior y el juego (que avanza lógica y sonido por frame) esperaba detrás:
        con una GPU de 20-30 ms por frame (integrada a alta resolución) el coche iba a cámara lenta y el sonido a
        ~30 kHz. Ahora un frame que solo dibuja en los dos búferes de pantalla se descarta si la GPU no acaba en
        4 ms (como mucho dos seguidos): con `GT_SIM_GPU_MS=20`, el juego espera un 33 % en vez de un 88 % y el sonido
        sale a 42,6 kHz en vez de 30,1; con una GPU rápida apenas se descartan.
  - [x] **Portátil con batería y fidelidad** (2026-10-14/15, parches 0027-0040; todo en FINDINGS):
        pianos con «Single Player» (destinos de render obsoletos, 0029/0033), escala interna automática por presupuesto
        de GPU (`GpuBudgetMs`, 0033), sin fogonazos negros ni línea abajo (0034/0035), el frameskip ahora se salta
        también el trabajo de la GPU (0036: con 2 núcleos el audio pasa de 32-35 kHz a 44 kHz), letreros lejanos con el
        nivel de mipmap del PSP a cualquier escala (0037), y ~5 veces menos dibujos por frame de carrera (0039: el nivel
        de detalle de cada objeto partía ~300 texturas en ~1.500; grabar el frame pasa de 1,85 a 0,47 ms con 2 núcleos).
        `GT_PERF_LOG` separa el tiempo del hilo de la GE (0038) y `GPU_GATE_LOD0=1` hace que la carrera vuelva a
        comprobar algo en el gate de la GPU. Pendiente: confirmarlo en el portátil (`docs/PRUEBAS_PORTATIL.md`).
  - [ ] Pendiente para igualar al software: reflejos del coche en el concesionario, transiciones de
        pantalla que GT escribe con la CPU (escritura de vuelta, fase 3) y `NativeGE=true` por defecto.
- [x] **Velocidad real** (2026-10-11): el juego, la música y los efectos iban a la mitad de velocidad.
      Dos hilos esperan cada vblank y el ritmo se aplicaba por llamada; ahora se ata el tiempo del
      juego al reloj (1,00×, audio a 44,1 kHz; `GT_PERF_LOG=1` lo mide). Los hilos ociosos del
      rasterizador ya no giran en vacío (mitad de CPU, menos saltos en portátiles) y la ventana
      muestra 60 fps en vez de 30.
- [x] **Frameskip** (2026-10-11): en CPUs modestas los menús y el sonido seguían lentos porque el
      ritmo saltaba vblanks y GT avanza por frame. Ahora un retraso corto se recupera sin saltar, y si
      el PC va tarde no se dibuja ese frame en pantalla (parche 0018). Con 2 núcleos: 121,8 vblanks/s
      (antes 106) y audio a 44,0 kHz (antes 36,7).
- [x] Ventana titulada «Gran Turismo» (parche 0016) y fuentes de la PSP bajadas por `tools/setup.sh`:
      jugar ya no necesita PPSSPP instalado.
- [ ] Probarlo jugando de verdad (el usuario) y ajustar controles.

## Fase 5 — Más adelante

- [ ] Sin parte recompilada: 100 % fuente decompilada.
- [ ] **Mods**: cambios sobre la fuente decompilada (lógica, coches, circuitos, UI) sin tocar el
      binario original; carga de datos modificados de `GT.VOL` con las herramientas de Nenkai.
- [ ] Otras plataformas.
- [ ] Pista de matching opcional si aparecen los compiladores exactos.
