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
- [x] El vídeo va a ~16 fps de juego (PPSSPP: 30): mirar el ritmo. **Hecho (2026-10-22):** el manejador de vblank de GT
      se ejecutaba en cada espera de vblank (dos hilos esperan cada una); ahora una vez por vblank y el vídeo va a ~30 fps
      (FINDINGS).
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
- [x] **Once recorridos que esperan al juego** (2026-10-20): `WAIT:seek`/`WAIT:word` en vez de tiempos fijos; nuevas rutas
      drift, challenge, challenge2, dirt, manual y circuit (carrera urbana, pausa y repetición). El gate completo acaba
      cada ruta tras su guion: **5.377 funciones comparadas, 0 discrepancias**.
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
- [x] **Lote 84 de funciones calientes** (2026-10-15, `src/game/unsorted/hot_batch_84.cpp`, 47 registradas y 9 escritas sin registrar; del sondeo del lote 78 con `--exclude-ref origin/decomp/hot-batch-83` (el PR #204) y sin ninguna
      dirección que #204 escriba o registre; antes de leer cada función se buscó su dirección en `src/` y en la rama del lote 83). La reserva de sueltas calientes está casi agotada (las ~80 primeras por llamadas están hechas,
      escritas sin registrar, son stubs de imports o llevan VFPU), así que el lote toma el escalón siguiente: funciones que las rutas llaman pocas veces (2 a 50 llamadas por ruta), sin VFPU, sin imports que el harness no reproduce,
      sin variadicas (`func_08C46D74` guarda `a2-t3` en el marco) y sin puntos de entrada múltiples (`func_08B207A4`, `func_08ABEA40`); leídas a mano con `tools/disasm.py`. Grupos: el cluster de objetos de menú
      (`func_0880B5E8` … `func_0886xxxx`, constructores, la cadena con cerrojo `func_0880BA98` → `func_0886978C` → `func_0886E008`), cadenas (`operator+`, `find`, `find_first_not_of`, el reservador `func_08B644BC`), estáticos
      con guarda, vectores y floats pequeños, destructores y holders. Siete funciones alcanzables solo desde otra función comparada se registran `GT_DECOMP_NESTED` (contract_audit 0). Tres lecciones nuevas en FINDINGS: un
      constructor que deja el `s0` de entrada en su sitio hace que un callee guarde otra palabra (`func_0880BF70`: 306 bytes de RAM, con la misma secuencia de llamadas; se resolvió trazando los dos despachos), una cola a una
      función `Void` anidada se lleva su `v0` (`func_08881C38`, sin registrar) y el total «comparadas» de la completa no cayó en cientos. Ramas no tomadas (cobertura LLVM) leídas contra el asm y listadas en el comentario de cada
      función; **comparadas solo por una salida temprana o una rama lateral**: `func_08A1F520` (solo op 0) y `func_08BDD7D8` (solo el rango vacío) quedan SIN registrar por eso; `func_0886E008` (solo flag != 0) y `func_08BF8A7C`
      (solo la respuesta 1) se mantienen con la parte no verificada anotada. Sin registrar: `func_08A1F520`, `func_08BDD7D8`, `func_08881C38`, `func_08B6E4FC`, `func_08B6E268`, `func_0887BC90`, `func_08875038`, `func_0886C31C`,
      `func_089C4C20`. Sabotajes detectados en las 50 comprobadas con la primera lista (las 47 registradas incluidas), `contract_audit` 0, `--check` 0 filas malas (el lote no tiene envolturas con argumentos), puerta rápida 47/47
      comparadas, 0 discrepancias. **5.914 funciones, 0 discrepancias** (puerta completa sobre `main` con los lotes 73-83 integrados: ver el PR #206).
- [x] **Lote 85 de funciones calientes** (2026-10-15, `src/game/unsorted/hot_batch_85.cpp`, 48 registradas; del sondeo del lote 78 con `--exclude-ref origin/decomp/hot-batch-84` (el PR #206) y sin ninguna dirección
      que #206 escriba, registre o deje sin registrar; antes de leer cada función se buscó su dirección en `src/` y en la rama del lote 84). El fondo de sueltas calientes está casi agotado (las que quedan son VFPU,
      de varias entradas o enredadas), así que este lote toma funciones medianas (10-60 instrucciones) que las rutas alcanzan pocas veces y llamadas de funciones ya decompiladas, leídas a mano palabra a palabra con
      `tools/disasm.py`: cargadores de ajustes (bytes de la tabla 0x08CDE508 a flotantes), borrados de vectores y búsquedas en tablas, una inserción en árbol, constructores con estáticos protegidos, cadenas de reenvío con
      llamadas finales, y reservas/inicializaciones de piscinas. No registradas (el comentario y la nota del registro dicen por qué): `func_08B1C590` (todos sus casos llegan a una espera de hilo),
      `func_08A4D508`, `func_08AD5460` y `func_08B32228` (llegan a un import o un sleep que el harness no reproduce), `func_08BBB6F8`, `func_08C49B00` y `func_08C1CE44` (sus casos no cambian memoria) y
      `func_08BEE82C`, `func_08BDA64C` y `func_08947A30` (todas las llamadas comparadas toman la salida temprana: el bucle, la ordenación y la fórmula no corren). Comparadas por su camino principal con un camino lateral leído solo
      del asm (cada comentario lista lo que los casos no toman): `func_08C48120`, `func_08BB3AE4`, `func_08B64CD0`, `func_08B93F38`, `func_08B5D1E0`, `func_088A79AC`, `func_08C40554`, `func_08A62D08`, `func_08ADDE30`,
      `func_08B1AEAC` y `func_08AD97D8`. Sabotajes detectados en las 48 (48 registradas), `contract_audit` 0, `--check` 0 filas malas, puerta rápida 48/48 comparadas (444 casos, 393 que cambian memoria), 0 discrepancias.
      **5.962 funciones, 0 discrepancias** (puerta completa sobre `main` con los lotes 73-84 integrados: ver el PR #210).
- [x] **Lote 86 de funciones calientes** (2026-10-15, `src/game/unsorted/hot_batch_86.cpp`, 51 registradas y 24 escritas sin registrar; del sondeo del lote 78 con `--exclude-ref origin/decomp/hot-batch-85` (el PR #210) y sin ninguna
      dirección que #210 escriba, registre o deje sin registrar; antes de leer cada función se buscó su dirección en `src/` y en la rama del lote 85). Funciones de 10-70 instrucciones que las rutas alcanzan, sin VFPU, sin
      llamadas a los stubs de imports (>= 0x08C70000), sin bucles sin fin ni variadicas ni puntos de entrada múltiples, leídas a mano con `tools/disasm.py`. Grupos: constructores con estáticos protegidos y registros de clase
      (`func_08A29588`, `func_08A3D394`, `func_08AA6D68`, `func_08AB6AD0`, `func_08892A7C`, `func_08894574`, `func_088FEA94`, `func_088F5670`, `func_0886E7A8`), temporales de cadena con la liberación con cuenta de referencias
      (`func_0899CD50`, `func_0897FDE0`, `func_08A6BBE8`, `func_089AF5E8`, `func_08A17710`), destructores (`func_0886A804`, `func_088742C8`, `func_08882DC0`, `func_0887104C`, `func_0886E980`) y la actualización de un
      objeto de menú (`func_08869444`, `func_0886968C`, `func_08B03F10`, hasta 26.754 llamadas). Cada función fija los registros `s0-s7`/`fp` como los tiene el original durante las llamadas (`SavedRegs`; FINDINGS del lote 84) y
      pasa todo por `call_keep`. Tres lecciones nuevas en FINDINGS: el centinela de la liberación de la cadena compartida queda en `v0`, `--check` toma por fila de tabla cualquier `void func_X(...) { w(rt, ctx, N); }`
      (un número de bit de `ext`/`ins` va como argumento de plantilla), y un padre registrado se traga las llamadas comparadas de sus descendientes decompilados (`func_0880BEF8`: la completa «comparadas» cayó 55 sin cambio
      de comportamiento; queda escrita sin registrar). Ramas no tomadas (cobertura LLVM) leídas contra el asm y listadas en el comentario de cada función; **comparadas por su camino principal con un camino lateral leído solo
      del asm**: `func_089E18BC` y `func_089E19BC` (solo el modo > 0; el modo 0 y el < 0 solo leídos), `func_08AE7E28` (un solo nodo con coincidencia y q[0] == 6), `func_088D3424` (el bloque final con el `+ 3.0` y
      `func_088D2D10`), `func_0886B600` (el bucle que vacía la cola), `func_08894574` (la inicialización del estático), `func_08BEBECC` (la forma virtual del puntero a miembro), `func_088D9BBC` (`clear` y la cola) y
      `func_0886AD04`. Sin registrar: ocho por no comparar nada (`func_088733F4`, `func_0889844C`, `func_088A34BC`, `func_08B95080`, `func_08B95120`, `func_08BC1FE8`, `func_08C34270`, y `func_0880BEF8` por lo dicho),
      cuatro que llegan a un import o una espera que el harness no reproduce (`func_088A5E5C`, `func_0892FCE0`, `func_08A13844`, `func_08AC8FDC`), nueve comparadas solo con una salida temprana o el rango vacío (`func_08900350`,
      `func_08BE78E8`, `func_08C246E4` y los borrados de vector `func_08C49A28`, `func_08C49C28`, `func_08C49D08`, `func_08B89338`, `func_08B895E0`, `func_08C0DBBC`) y tres sin ningún caso que cambie memoria (`func_08B043D8`,
      `func_08872E18`, `func_08C49930`). Sabotajes detectados en las 51 (51 registradas), `contract_audit` 0, `--check` 0 filas malas, puerta rápida 51/51 comparadas (654 casos, 569 que cambian memoria), 0 discrepancias.
      **6.013 funciones, 0 discrepancias** (puerta completa sobre `main` con el lote 85 integrado: ver el PR #212).
- [x] **Lote 87 de funciones calientes** (2026-10-16, `src/game/unsorted/hot_batch_87.cpp`, 46 registradas y 14 escritas sin registrar; del sondeo del lote 78 con los lotes 85 y 86 ya en `main`; antes de leer cada función se buscó su
      dirección en `src/` y en las ramas abiertas: ninguna). Funciones de 10-70 instrucciones que las rutas alcanzan, sin VFPU, sin llamadas a los stubs de imports (>= 0x08C70000; se dejaron fuera `func_08AC45F4`, que termina
      saltando a uno, y la envoltura variádica `func_08C46D74`), sin bucles sin fin ni puntos de entrada múltiples (ni las dos conmutaciones por tabla de saltos `func_08AEFB40` y `func_088A0DD4`), leídas a mano con
      `tools/disasm.py` (la única tabla de saltos, D_08C8B3E8, leída del ELF). Grupos: destructores y desmontajes con la liberación del puntero compartido y la destrucción de arrays (`func_0887ED48`, `func_0886DE00`,
      `func_088AF1D0`, `func_088A11F8`, `func_08BD67B8`, `func_08BE46A8`, `func_089EF8A8`, `func_08ABF590`, `func_0889A4F0`, `func_08BAEF98`, `func_08BE45C4`, `func_08BC0808`, `func_08BAC9EC`, `func_08BE0724`), accesores
      con «holder» y temporales de cadena de las clases de script (`func_08A83474`, `func_08A83AC0`, `func_08A52F9C`, `func_08A20EC0`, `func_089ADE38`, `func_089C0360`, `func_0880C1DC`, `func_088797F8`, `func_0887894C`,
      `func_08A57D74`, `func_08A1503C`, `func_08A2A724`, `func_08993FCC`, `func_08A0BF5C`…), constructores con estáticos protegidos y sencillos (`func_0889DA58`, `func_0892BE4C`, `func_08809650`, `func_08ACFA08`, `func_08922CF8`,
      `func_089E6520`, `func_0894CD28`), el crecimiento de un vector (`func_08C4A4AC`), una división tipo `strtok` (`func_08ABD5EC`) y lectores pequeños (`func_0894D934`, `func_088D2E78`, `func_08915DEC`, `func_08898D30`).
      Cada función fija `s0-s7`/`fp` como el original durante las llamadas (`SavedRegs`) y pasa todo por `call_keep`. Dos lecciones nuevas en FINDINGS: una puerta rápida en una máquina cargada «no compara» lo que las rutas
      alcanzan en una tranquila (carga 57 en 24 núcleos: 17 de 48 comparadas; con los tiempos de las rutas duplicados, 43 de 60), y los descendientes tragados por un padre registrado se pueden registrar `GT_DECOMP_NESTED` (el
      sabotaje no los fija dentro de sus 4 rondas: una segunda ejecución con solo ellos sabotajeados). Más un caso de la regla del retardo (`func_0880D478` tomó la asignación por la respuesta anterior: un `move` en el hueco
      de retardo de un `jal` ve el `v0` de ANTES de esa llamada). **Comparadas por su camino principal con un camino lateral leído solo del asm** (cada comentario lista lo que los casos no toman): `func_08A57D74` (modo 0),
      `func_08A20EC0` y `func_089C0360` (su segunda mitad), `func_089ADE38` (la mitad que escribe), `func_08A52F9C` (n > 0), las liberaciones de contadores de `func_08BC0808`, `func_08BAC9EC`, `func_08BD67B8`, `func_08BE45C4`,
      `func_08BAEF98` y `func_0889A4F0`, `func_088A23EC` (la llamada final), `func_08ABD5EC` (todos los tokens), `func_0881D450` (un par distinto de -1:-1), `func_08C4A4AC`, `func_08BE0724` y `func_0894D934` (qué entradas
      de la tabla usaron los casos). Sin registrar: dos padres cuyo único `delete` por ruta de menú destruye un objeto miembro con todo su árbol decompilado y se traga las llamadas comparadas de sus descendientes
      (`func_0880D564`, `func_0880D478`), ocho cuyos casos paran en un cerrojo, un sleep, un arranque de hilo o un import que el harness no reproduce (`func_0883B600`, `func_0897BFEC`, `func_089C1368`, `func_089C151C`,
      `func_08ABAFCC`, `func_08AC2BD8`, `func_08BCFEF8`, `func_08BD787C`), tres sin ningún caso que cambie memoria (`func_0888CFCC`, `func_08ABECA0`, `func_08B8ECAC`) y `func_08B9355C` (solo se comparó con el nodo nulo).
      Sabotajes detectados en las 46 (43 en la primera ejecución y las 3 anidadas restantes en una segunda), `contract_audit` 0, `--check` 0 filas malas (el lote no tiene filas de tabla), puerta rápida 46/46 comparadas
      (250 casos, 246 que cambian memoria, más 20 casos anidados de las cinco registradas `GT_DECOMP_NESTED`), 0 discrepancias. **6.059 funciones, 0 discrepancias** (puerta completa sobre `main` con los lotes 85 y 86
      integrados: 5.048 comparadas, 7 menos que `main` (5.055): 34 funciones ya decompiladas dejan de contarse (callees de las nuevas, tragados por sus casos) y 27 nuevas se alcanzan (22 comparadas y 5 anidadas); las otras 19 nuevas solo las alcanza la puerta rápida sin tope; ver el PR del lote 87).
- [x] **Lote 88 de funciones calientes** (2026-10-16, `src/game/unsorted/hot_batch_88.cpp`, 55 registradas y 16 escritas sin registrar; del sondeo del lote 78 con el lote 87 ya en `main`; antes de leer cada función se buscó su
      dirección en `src/` y en las ramas abiertas: ninguna). Funciones de 10-70 instrucciones que las rutas alcanzan, sin VFPU, sin llamadas a los stubs de imports (>= 0x08C70000), sin bucles sin fin, tablas de saltos ni
      puntos de entrada múltiples (`func_08B207A4` comparte sus bytes con otros tres puntos de entrada; `func_08AC7EDC` es el bucle de un hilo), leídas a mano con `tools/disasm.py`. Grupos: ayudantes de bits, rangos y tablas
      (`func_08B1CA60` un escritor de bits, `func_08B2068C` una posición cíclica o acotada, `func_08BB1A00` el descenso de un montículo, `func_08BFA640` la recursión de una ordenación estable, `func_08C4FF60` un cifrado de
      flujo, `func_08B71DC8` doble a entero con saturación, `func_08B8A8A8`, `func_08AD5CE8`, `func_08AE9F98`…) y la familia de `reserve` de vectores (`vector_reserve<Shift>`: `func_08B893E8`, `func_08B894E4`; `func_08B904D8` con el
      otro asignador; `func_08B8AF08` la copia de rango); constructores con estáticos protegidos (`func_088710DC`, `func_0887F4FC`, `func_0886F5FC`, `func_08BA3718`), la búsqueda de clave de tipo (`type_key_cast`:
      `func_08BB9C24`, `func_08BB9E4C`) y nativos de las clases Adhoc: lectores, escritores y tablas de registro sobre «holders» y cadenas temporales (`func_0880CE2C`, `func_0880F470`, `func_0881EAD0`, `func_0881FDEC`,
      `func_088213BC`, `func_08A25FAC`, `func_08A213C0`, `func_0883C4B4`, `func_08855C68`, `func_08994994`, `func_0899EF08`, `func_089D30D4`…) con cuatro envoltorios de propiedad (`property_accessor`, `property_pair_b`,
      `bool_property_a`, `bool_property_b`; las filas se comprueban con `family_members --check`: 8 filas, 0 malas). Cada función fija `s0-s7`/`fp` como el original durante las llamadas (`SavedRegs`), pasa todo por `call_keep`
      y deja `v0` (y `v1`/`f0` donde un llamador los puede leer) como el original en cada camino. **Comparadas por su camino principal con un camino lateral leído solo del asm** (cada comentario lista lo que los casos no toman):
      la segunda ronda de `func_08B1CA60`, los caminos cíclico y de acotación de `func_08B2068C`, el recorrido y la recursión de `func_08969F64` (solo con la bandera a 0), el camino principal de `func_088A1BC8` (la ranura virtual
      0x7C respondió siempre 0), el descenso profundo de `func_08BB1A00`, las mitades recursivas de `func_08BFA640` (con `GT_DECOMP_DENSE`: 58 llamadas, 3 casos que cambian memoria), los casos de saturación de
      `func_08B71DC8` y, de las propiedades, la mitad que las rutas no tomaron (`func_0880F470`, `func_08835E1C`, `func_08828ABC`, `func_088342F4`, `func_0884A5CC`, `func_0884A6DC`, `func_08994994`, `func_0899EF08`,
      `func_089D30D4`). `func_088710DC` solo se ejecuta dentro de los casos de su padre decompilado `func_0887F4FC` (su única llamada por ruta se traga las comparadas del descendiente), así que se registra `GT_DECOMP_NESTED`
      (`contract_audit` 0; la puerta lo cuenta en 6 casos anidados iguales). Sin registrar: cuatro cuyos casos paran en un cerrojo (`func_0897AEE4`, `func_08A4AE9C`, `func_08BCF9E4`, `func_08A8712C`), dos que llegan a un import
      que el harness no reproduce (`func_08AB9B04` sceUtility, `func_08A37510` sceIoRead), nueve sin ningún caso que cambie memoria (`func_088285AC`, `func_088286B8`, `func_08828C8C`, `func_088341EC`, `func_088941A0`,
      `func_088D7158`, `func_0883365C`, `func_08845BA8`, `func_0898BAB4`: se compararon los lectores, no los escritores ni la copia) y `func_088A1A3C` (ninguna ruta la llamó). Puerta rápida con los tiempos de siempre en una máquina tranquila (carga 1-5): la primera con 56 funciones comparó 51, la segunda con 61 comparó 58, y la final con las 55 registradas, 55 de 55 (399 casos, 217 que cambian memoria,
      más 6 casos anidados de la registrada `GT_DECOMP_NESTED`), 0 discrepancias; sabotajes detectados en las 55 (53 en la primera ronda y las 2 restantes en las dos siguientes), `contract_audit` 0, `--check` 0 filas malas.
      **6.114 funciones, 0 discrepancias** (puerta completa: 5.070 comparadas, 20 más que `main` (5.050), 0 discrepancias; 43 de las 55 se comparan en la puerta completa en tiempo real y las otras 12 solo las alcanza la rápida sin tope).
- [x] **Lote 89 de funciones calientes** (2026-10-16, `src/game/unsorted/hot_batch_89.cpp`, 67 registradas y 1 escrita sin registrar; antes de leer cada función se buscó su dirección en `src/` y en las ramas abiertas: ninguna).
      El sondeo del lote 78 se agotó: de las 1.177 funciones que alcanzó quedaban 48 de 10-70 instrucciones sin hacer y 23 tenían VFPU (9) o llamadas a los stubs de imports >= 0x08C70000 (14), así que quedaban 25 candidatas. Se hizo un
      sondeo nuevo (`tools.decomp_gate probe b89p`, las cinco rutas, `--decomp off`) de las 1.403 funciones sin hacer de 71-120 instrucciones: alcanzó 281, 264 sin VFPU, stubs ni saltos por registro. Sus grupos de forma (misma firma que
      `tools.shapes`) tienen dos familias enormes de LA MISMA función de 75 instrucciones en dos asignaciones de registros: una función de registro guiada por una tabla (se construye un «holder», se reserva un objeto de 0x38 bytes y cada
      par {nombre, valor} de una tabla de `.data` pasa a `func_08B86E98`; cada miembro se llama una vez por ruta), con 46 + 10 miembros y ninguno hecho antes. Se escribió una plantilla (`register_table`, variante a y b, 56 funciones, cada una con
      las constantes de su propia fila: texto, principio y fin de la tabla y cadena de aserción, sacadas de sus palabras reubicadas; `tools.family_members --check`: 58 filas —56 más las 2 de `float_property`— coinciden, 0 malas, 0 sin
      analizar). Además 12 funciones sueltas leídas a mano con `tools/disasm.py`: tres «re-enlazar hijo» (`func_0886E9FC`, `func_088702AC`, `func_088713A4`, con `bind_child` compartido y llamada final), `func_0887D4EC` (dos objetos con
      contadores de referencia; los `ll`/`sc` se leen como carga y almacenamiento), `func_088DAEB0`, `func_088A1854` (cuatro flotantes y llamada final), `func_0888B4E0` (dos bucles sobre una tabla de medias palabras), `func_088D5ED0` (un par de
      64 bits en orden de bytes invertido: `wsbw`/`wsbh`) y los nativos de propiedad `func_0898CC60` y `float_property` (`func_089A0F88`, `func_089A1084`). Leídas y dejadas para otro lote (con coma flotante o flujo enredado): `func_08877354`,
      `func_0887871C`, `func_088DE24C`, `func_0890778C`, `func_089191DC`, `func_0892063C`, `func_089281E4`, `func_08951304`, `func_0896B4D4`, `func_08AC800C`, `func_08AF8AB0`, `func_08B1F2B8`, `func_08C053E8`. Sin registrar:
      `func_08B0A458` (ningún caso comparado cambia memoria: los cinco, uno por ruta, tomaron caminos que no escriben). Cada función fija `s0-s7`/`fp` (y `$f20`) como el original durante las llamadas (`SavedRegs`, `SavedFloat`), pasa todo
      por `call_keep` y deja `v0` (y `v1`/`f0` donde un llamador los puede leer) como el original en cada camino. **Comparadas por su camino principal con un camino lateral leído solo del asm** (cada comentario lista lo que los casos no toman,
      con `tools.decomp_gate coverage`): el objeto de registro o de clase nulos de `register_table`, el segundo objeto de `func_0887D4EC`, la liberación del hijo anterior en las tres de «re-enlazar» (y todo el camino de creación de
      `func_088713A4`), el lector o el escritor que las rutas no llamaron (`func_0898CC60` solo el lector, `float_property` solo el escritor) y `func_088D5ED0` con la bandera distinta de cero. Puerta rápida (cinco rutas sin tope, carga 2-10 por
      mis propios sondeos y compilaciones: no es una máquina tranquila): 67 de 67 comparadas (las 56 de la tabla, un caso por ruta, todas cambian memoria), 0 discrepancias; sabotajes detectados en las 67 en la primera ronda,
      `contract_audit` 0, `--check` 0 filas malas.
      **6.181 funciones, 0 discrepancias** (puerta completa: 5.076 comparadas, 4 más que `main` (5.072), 0 discrepancias; solo 6 de las 67 se comparan en la puerta completa en tiempo real —`func_0886E9FC`, `func_088713A4`, `func_0898CC60`,
      `func_088D5ED0`, `func_089A0F88`, `func_089A1084`, 33 casos— y las otras 61, entre ellas las 56 de la tabla, solo las alcanza la rápida sin tope).
- [x] **Lote 90 de funciones calientes** (2026-10-16, `src/game/unsorted/hot_batch_90.cpp`, 48 registradas y 4 escritas sin registrar; antes de leer cada función se buscó su dirección en `src/` y en las ramas abiertas: solo el borrador #214 de especificación, ningún código).
      Se parte del sondeo `b89p` del lote 89 (las 1.403 funciones sin hacer de 71-120 instrucciones; 281 alcanzadas, 264 sin VFPU, stubs ni saltos por registro): tras el lote 89 quedaban 225 sin hacer. Sus grupos de forma (misma firma que `tools.shapes`) son
      pequeños (el mayor tiene 7 miembros), así que el lote son seis plantillas de grupo y funciones sueltas leídas a mano con `tools/disasm.py`. Plantillas: `hint_insert` (4: inserción con una posición de pista en una estructura enlazada, cada miembro con sus dos
      llamadas en su fila), `assign_counted` (7 copias idénticas: un puntero con contador de referencias sustituido por otro; los `ll`/`sc` se leen como carga y almacenamiento), `call_member_and_release` (3 copias idénticas), `owner_release_a` (5) y
      `owner_release_b` (2) con la palabra de vtable de cada miembro, `string_tree_insert` (2) y `refresh_then` (2, la segunda termina con otra llamada); `tools.family_members --check`: 15 filas coinciden, 0 malas, 0 sin analizar (las 9 filas sin argumentos no se
      pueden comprobar así: se miraron sus dos plantillas con `family_members TEMPLATE`, sin ninguna línea `!!`). `vector_reserve_scaled` son tres formas de la reserva de un vector con elementos de 6, 240 y 84 bytes (el contador sale de un desplazamiento y un
      producto de 32 bits por el inverso modular; no es un grupo, sin filas). Además 21 funciones sueltas: inicializadores campo a campo (`func_088E8184`, `func_08AF1FC0`, `func_08B2F31C`, `func_088B4938`, `func_08B451A0`, `func_08B7A13C`, `func_088E90F0`, `func_08979B08`,
      `func_0888A9B8`, `func_0889F1DC`, `func_08BE449C`, `func_08AE2D38`), `func_08BB6BFC` (inserción en un anillo de bloques con fusión), `func_088D53FC` (copia de 0x82 bytes), `func_089C6370`, `func_0881CDC4` y `func_08A6D10C` (holders y cadenas temporales),
      `func_089B139C` (tres vectores vaciados), `func_08B03A48` (cuatro tablas que se reinician al llenarse), `func_08C48004` y `func_08B5240C` (empaquetado de campos de bits). Sin registrar: `func_08BC0DFC` (su único caso comparado no escribió; las otras seis
      copias sí), `func_08BA9498` y `func_08C070FC` (borrado y asignación de un vector: con 3.000 casos densos por ruta se compararon todas las llamadas y ninguna cambió memoria) y `func_08B48A74` (todos sus casos paran en `kernel isolation (sleep would block)`).
      Leídas y dejadas: `func_08B0ACDC` (padre de dos funciones de este lote: registrada se tragaría sus llamadas comparadas), `func_0883C988`/`func_0883CCC8` (pasan un `a1` que la llamada anterior deja indefinido), `func_08B7C598` y `func_089D5690` (no escriben) y las que llaman
      a los stubs de imports desde 0x08C70000 o tienen el flujo enredado. Cada función fija `s0-s7`/`fp` como el original durante las llamadas (`SavedRegs`), pasa todo por `call_keep` y deja `v0` (y `v1`/`f0` donde un llamador los puede leer) como el original en cada camino; la pila
      muerta no se reproduce. **Comparadas por su camino principal con caminos laterales leídos solo del asm** (cada comentario lo lista, con `tools.decomp_gate coverage`): los caminos de liberación de `assign_counted`, `call_member_and_release` y `owner_release_*`, la rama
      de «clave ya presente» de `string_tree_insert`, la parte de coma flotante de `refresh_then` (el byte `this[0x1E9]` siempre fue 0), la primera vez del estático protegido de `func_08BB6BFC` y los caminos de fallo de `func_08AE2D38`. Puerta rápida (cinco rutas sin tope): 48 de 48
      comparadas, 0 discrepancias; sabotajes detectados 48 de 48 en la primera ronda; `contract_audit` 0 (dos registros tienen un llamador que lee `v1`, ninguno es `GT_DECOMP_NESTED`: `func_08C011A0` y `func_08C0DEF8` lo guardan y releen un byte).
      **6.229 funciones, 0 discrepancias** (puerta completa: 5.094 comparadas, 19 más que `main` (5.075), 0 discrepancias; 30 de las 48 se comparan en la puerta completa en tiempo real y las otras 18 solo las alcanza la rápida sin tope; carga de la máquina 8-11 por trabajos de otros agentes).
- [x] **Lote 91 de funciones calientes** (2026-10-16, `src/game/unsorted/hot_batch_91.cpp`, 47 registradas y 5 escritas sin registrar; antes de leer cada función se buscó su dirección en `src/` y en las ramas abiertas: solo el borrador #214 de especificación, ningún código).
      Se parte del sondeo `b89p` (71-120 instrucciones): tras los lotes 89 y 90 quedaban 177 alcanzadas y sin hacer, de las que 28 las habían leído y dejado esos lotes, 7 tienen VFPU, 17 llaman a los stubs de imports o saltan por registro y unas 25 son de coma flotante pesada. `tools.family_members --list --any-template`
      no encuentra ningún grupo nuevo (solo los dos ya diferidos), así que el lote son 43 funciones leídas una a una con `tools/disasm.py` y dos plantillas de dos miembros cada una, encontradas leyendo: `vector_assign<Shift>` (`func_08B8AD88` con elementos de 2 bytes y `func_08BFBB38` con 4: `this = other` de un vector; el desplazamiento es un campo de instrucción, así que es argumento de plantilla y
      los tres destinos son la fila) y `vector_insert_grow` (la mitad que reasigna de `func_08C1ABE0` y `func_08C25730`, con las cuatro constantes de cada una en su fila; las mitades con hueco son distintas). Muchas son nativos de scripts, constructores de registro (cinco pares nombre/callback), recorridos de árbol o vector y similares; trece tienen nombre Adhoc en `config/symbol_addrs.txt`, pero los comentarios
      conservan `func_X` y solo dicen lo que hace el asm. `tools.family_members --check` no lee ninguna fila de este archivo (los argumentos de plantilla no se emparejan y las filas de `vector_insert_grow` son argumentos de una llamada interna), así que **nueva herramienta `tools.literal_check`** (con prueba, 98 % de cobertura): cada literal
      hexadecimal de cada función debe aparecer en el desensamblado de esa función (inmediatos, direcciones resueltas, destinos de llamada, el valor de un par `lui`/`ori` o de un `lui` suelto): 52 funciones, 0 literales que falten (una sola excepción manual, `0x08AD6AB8:0x3C`); mutando dos literales a mano los encuentra. Sin registrar: `func_089B439C` (todos sus casos paran en `kernel isolation (LwMutex contention)`),
      `func_089CA118` (el stub `sceIoRead`), `func_08A235F8` (el stub `sceSasCore::0x33D4AB37`), `func_08A243FC` (`kernel isolation (sleep would block)`) y `func_08B666E8` (`calls 0` en las cinco rutas: sus aciertos del sondeo vienen de llamadas que el despachador no ve). Leídas y dejadas: `func_08ABEAEC`, `func_08ACCFF8`, `func_0884085C`, `func_088D7794`, `func_08877B68` y las de coma flotante pesada.
      La primera puerta rápida encontró un fallo real: `func_0899CE20` leía el `addu s1, v0, zero` del hueco de retardo de un `jal` como el resultado de la llamada (se ejecuta antes). **Caminos leídos solo del asm** (cada comentario los lista, con `tools.decomp_gate coverage` y revisados a mano): la parte de `func_08A9FFD0` tras `func_08A69530` (solo corrió el retorno temprano), el bucle de
      `func_08A4A960` (nunca corrió), las mitades con hueco de `func_08B9A5C4`, `func_08C1ABE0` y `func_08C25730`, el modo 0 de `func_08A10468`, el otro modo de `func_08A6F704`, tres de los cuatro caminos de `func_08B20A78`, el producto con `0x3E8E38E4` de `func_0894D484` y la rama `0x101` de `func_08988BB8`. Puerta rápida (cinco rutas sin tope): 47 de 47 comparadas, 0 discrepancias;
      sabotajes 47 de 47 detectados en la primera ronda; `contract_audit` 0 nuevos (siguen los dos del lote 90). **6.276 funciones, 0 discrepancias** (puerta completa: 5.087 comparadas, 6 menos que `main` (5.093), 0 discrepancias; 37 de las 47 se comparan en la puerta completa en tiempo real y las otras 10 solo las alcanza la rápida sin tope; 47 funciones que `main` comparaba ya no se comparan porque corren dentro de los casos de estas
      (sus llamadas ya no se cuentan aparte, FINDINGS lote 86) y 40 son nuevas, cuentas por dirección de otra pasada que no cuadran al uno con las 5.087; la verificación del PR dio 5.088; carga de la máquina 5-8).
- [x] **Lote 92 de funciones calientes** (2026-10-16, `src/game/unsorted/hot_batch_92.cpp`, 50 registradas y 12 escritas sin registrar; antes de leer cada función se buscó su dirección en `src/` y en las ramas abiertas).
      Se parte del sondeo `b89p` (71-120 instrucciones): tras el lote 91 quedaban 130 alcanzadas sin registro activo: 25 que los lotes 89-91 ya habían escrito o leído y dejado, 2 con VFPU, 8 que llaman a los stubs de imports o saltan por registro, 21 de coma flotante pesada y 12 leídas y dejadas aquí (`func_08849B8C` y `func_08B8B33C`,
      `func_08A92588`, `func_08B00E90`, `func_08BDABCC`, `func_08BDBB44`, `func_088A6F74` es un bloque de varias funciones tras un salto, `func_088BBA00`, `func_08BB1D9C`, `func_08C46E38`, `func_08BD4724` no escriben memoria propia, `func_08C07E24` repite la asignación de vector del lote 90). `tools.family_members --list --any-template` no encuentra ningún grupo; leyendo se vio que
      `func_08BB2340` y `func_08BB24B0` son la misma función dos veces (solo cambia el orden de los operandos de dos comparaciones y los saltos propios) y que `func_08BB2660` comparte con ellas el cuerpo de liberación (`release_pooled`). Total 62 funciones leídas una a una con `tools/disasm.py`, 50 registradas
      y 12 sin registrar: `func_088FEB9C` (`calls 0`), `func_0898BDB4`, `func_088890AC`, `func_088F9F00`, `func_088F024C` y `func_0888AD94` (sus casos comparados, 1-4, no cambiaron memoria), `func_088F05BC`, `func_088E6250` y `func_08BB2340` (la rápida y 3.000 casos densos en la ruta de carrera comparados iguales, ninguno escribió), `func_08ACECFC` (stub `sceUtility::0x2A2B3DE0`), `func_0882F7A0` (`sleep would block`) y `func_0897AAB4` (`LwMutex contention`).
      `func_089064D8` y `func_0895B454` se registran `GT_DECOMP_NESTED`: solo se alcanzan dentro de los casos comparados de `func_088FC45C` y `func_088A2074` (con los 48 registros juntos `func_0895B454` pasaba de 7 llamadas a 0: lo tragaba su padre). La primera pasada de la puerta rápida encontró tres fallos de `v0` en rutas que el original deja con un valor que el C no ponía (`func_08B2E434` y `func_08A0F344` devuelven una dirección que dejan el stub y la tabla de saltos, `func_088721DC` el puntero que acaba de cargar).
      Puerta rápida (cinco rutas sin tope): todas las registradas comparadas al menos una vez con 0 discrepancias y al menos un caso que cambia memoria; sabotajes 50 de 50 detectados (3 rondas); `contract_audit` 0 nuevos; `literal_check` 62 funciones, 0 literales que falten (ninguna excepción manual); `family_members --check` no lee ninguna fila. Caminos leídos solo del asm: cada comentario los lista (`tools.decomp_gate coverage`); los más débiles son las mitades con hueco de `func_08BB13D4` y `func_08C3EBD8`,
      la tabla de máscaras de `func_08918170` y los avisos de puntero nulo. **6.326 funciones, 0 discrepancias** (puerta completa: 5.079 comparadas, 9 menos que `main` (5.088), 0 discrepancias; los descendientes de los padres registrados corren dentro de sus casos y ya no se cuentan aparte, FINDINGS lote 86).
- [x] **Lote 93 de funciones calientes** (2026-10-16, `src/game/unsorted/hot_batch_93.cpp`, 44 registradas y 4 escritas sin registrar; antes de leer cada función se buscó su dirección en `src/` y en las ramas abiertas: ningún PR abierto y las dos ramas sin fusionar solo la nombran en comentarios).
      El sondeo de 71-120 instrucciones (`b89p`) se agotó: tras el lote 92 quedaban 80 alcanzadas sin registro activo y solo `func_08B0ACDC` (el padre de dos funciones del lote 90) se podía escribir. Se hizo un sondeo nuevo, `tools.decomp_gate probe b93p`, sobre las 557 funciones sin hacer de 121-170 instrucciones (las cinco rutas con `--decomp off`): alcanzó 192.
      `tools.family_members --list --any-template` solo encuentra un grupo (3 miembros); la nueva opción `tools.shapes --skeleton` (3 pruebas; agrupa por clase de instrucción, sin registros, inmediatos ni desplazamientos de salto) junta además las funciones que el compilador repartió en dos asignaciones de registros: siete familias. 26 de las 44 registradas son instancias de nueve plantillas con las constantes de cada miembro en su fila
      (el analizador de 154 instrucciones cuatro veces, `scan_block`; el manejador de banderas en dos formas, `flag_dispatch_a/b`, más sus variantes `notify_then_gate_b` y `object_call_c`; `assign_holder`; y la inserción de `count` copias en un vector doce veces con elementos de 1, 2, 4 y 16 bytes, `vector_fill_insert`, `fill_insert_bytes` y `fill_insert_scalar`, que comparten `fill_insert_grow` con el desplazamiento, el límite y el asignador en su fila);
      las otras 18 se leyeron una a una con `tools/disasm.py`: constructores, nativos de scripts, un escritor de lista de pantalla (`func_08B544B0`) y similares. Sin registrar: `func_089B2014` y `func_089B2324` (nativos que reúnen argumentos en un vector: todos sus casos paran en `kernel isolation (sleep would block)` / el stub `sceDisplay::0x46F186C3`, compared 0),
      `func_088EFA1C` (su único caso comparado no cambió memoria) y `func_08A17818` (sus 8 casos comparados no cambiaron memoria: el singleton ya existía). Leídas y dejadas: `func_0883C988` / `func_0883CCC8` (pasan un `a1` que la llamada anterior deja sin definir), `func_08C0E3B8`, `func_08C4BB10`, `func_08BF1768`, `func_08B22694` (`mtlo` / `madd`) y las de saltos probables enredados.
      La primera pasada de la puerta rápida encontró tres fallos: los cuatro miembros de `scan_block` (la ranura virtual tras el bucle lee `a1`, que el original pone en el hueco de retardo de un `bne`), `func_08896780` (`func_0892B968` / `func_0892BB10` toman `a0` = `this[0x6C]`, cargado 30 instrucciones antes) y `func_08BFB68C` (el `sll v0, s3, 2` entre dos llamadas es el `v0` que deja la función); una auditoría de argumentos de cada llamada con dirección literal encontró uno más en un camino que ninguna ruta recorre (`func_08BF9B6C` toma el final del rango en `a2`). FINDINGS lote 93.
      Puerta rápida (cinco rutas sin tope): 44 de 44 comparadas, 0 discrepancias y al menos un caso que cambia memoria en cada una; sabotajes 44 de 44 detectados (la primera ronda fijó 37; la ruta de carrera la mató el tiempo límite con la máquina cargada por otro agente, así que las 7 funciones que solo ella alcanza se sabotearon aparte y se fijaron todas); `contract_audit` 0 nuevos; `tools.family_members --check` no lee ninguna fila;
      `literal_check` 48 funciones, 0 literales que falten (33 `--allow`, todos desplazamientos escritos como base + paso en el asm o el bit 31 de `ins`/`bgez`; ninguno en un camino sin recorrer). Caminos leídos solo del asm: cada comentario los lista (`tools.decomp_gate coverage`); los más débiles son el bucle y el recorrido de tabla de `func_08896780`, el bucle de copia de solapamiento de `fill_insert_bytes` (ningún miembro movió elementos),
      las mitades con hueco de tres de los cinco `vector_fill_insert`, el segundo literal y las salidas del bucle de `scan_block` y el camino sin separador de `func_08A6F4D8`. **6.370 funciones, 0 discrepancias** (puerta completa: 5.079 comparadas, 1 más que `main` (5.078), 0 discrepancias; 34 de las 44 se comparan en la puerta completa en tiempo real y las otras 10 solo las alcanza la rápida sin tope; 35 funciones que el lote 92 comparaba ya no se comparan (corren dentro de los casos de las nuevas, FINDINGS lote 86, o la ruta en tiempo real ya no las alcanza) y 35 son nuevas: 34 de este lote y `func_0881EAD0` (cuentas por dirección de otra pasada, que no cuadran al uno con la diferencia de totales); carga de la máquina alta por gates de otros agentes).
- [x] **Lote 94 de funciones calientes** (2026-10-16, `src/game/unsorted/hot_batch_94.cpp`, 32 registradas y 3 escritas sin registrar; antes de leer cada función se buscó su dirección en `src/` y en las ramas abiertas: el único PR abierto, #223, es un cambio de GPU).
      Se parte del sondeo `b93p` (121-170 instrucciones): tras el lote 93 quedaban 150 alcanzadas sin registro. `tools.shapes --skeleton` las reparte en 149 grupos y solo el par de `args_to_vector` (que el lote 93 dejó sin registrar) tiene dos miembros, así que no hay familias de partida: se leyeron una a una con `tools/disasm.py`, primero las que menos FPU, tablas de saltos y llamadas a stubs tienen. La lectura encontró cuatro familias: la asignación de un vector de registros de 0x30 y 0x28 bytes (`vector_assign`: `func_08BECF40`, `func_08BF4CF0`), el analizador de flujo que rellena 2 o 4 floats
      (`parse_floats`: `func_089765C0`, `func_089B31F0`, hermano de `scan_block` del lote 93) y las tres inserciones con relleno que el lote 93 dejó leídas (`func_08C4BB10`, `func_08BF1768`, `func_08C0E3B8`, con la `fill_insert_grow` del lote 93 copiada y una fila cada una). El resto son constructores (`func_0897B108`, `func_088D671C`, `func_08BACF7C`, `func_0886CE44`, `func_088A4D18`, `func_08BD5870`, `func_088A0BD8`), devoluciones de estructura por valor (`func_08B32B5C`, `func_088D2320`, `func_0881B0D8`),
      nativos de scripts con cadenas (`func_089F37AC`, `func_08A2982C`, `func_089E9F90`, `func_089AF6A0`, `func_08A18270`, `func_08C4DA5C`, `func_08A1F328`), inserciones y asignaciones de vectores (`func_08C16984`, `func_08C3D298`, `func_08C0CECC`, `func_089DDA80`) y similares (`func_08933C5C`, `func_08AE6620`, `func_08B22694`, `func_088D1858`). Sin registrar: `func_088FE08C` (su única llamada alcanzada para en `kernel isolation (LwMutex contention)`), `func_08A1F5D4` (todos sus casos paran en el stub `sceLibFont::0xA834319D`, compared 0 en las cinco rutas)
      y `func_0888C2B8` (comparada una vez, en la ruta de opciones de la pasada de cobertura, y no alcanzada en las siguientes: su sabotaje no se detectó). Leídas y dejadas: las de coma flotante pesada o tablas de saltos, las de mapas, asignador y cerrojos (`func_08A7A3D4`, `func_08A535A4`, `func_08BB6964`, `func_08A13BE0` es un bucle de hilo) y los nativos de varios modos.
      La primera pasada de la puerta rápida: todas sin discrepancias salvo `func_08A1F328`, que falló en todos sus casos (`Guest memory access outside PSP RAM/EDRAM`): la vista del desensamblado con los reloads de registros guardados ocultos escondía un `lw $s0, 4($sp)` real (FINDINGS lote 94; nueva herramienta `tools.sp_loads`, 3 pruebas, que lista los `lw $sN, off($sp)` sin guardado previo: en las 35 funciones solo ese y 11 ya modelados).
      Puerta rápida (cinco rutas sin tope): 32 de 32 comparadas, 0 discrepancias y al menos un caso que cambia memoria en cada una; sabotajes 32 de 32 detectados en la primera ronda (los 33 sabotajes incluían `func_0888C2B8`, el único no fijado, que se quitó del registro); `contract_audit` 0 nuevos (las dos del lote 90 con `v1`); `tools.family_members --check` no lee ninguna fila; `literal_check` 35 funciones, 0 literales que falten (7 `--allow`: los anchos `ins`/`ext` 0x3F y 0x7F de `func_089DDA80`, los 0xED bytes de la copia de `func_08B32B5C` y `func_08B22694`, y tres constantes `lui`/`ori` sobre dos registros distintos).
      Caminos leídos solo del asm: cada comentario los lista (`tools.decomp_gate coverage`); los más débiles son las mitades con hueco de `func_08C4BB10`, `func_08BF1768`, `func_08C0E3B8` y `func_08C3D298` (solo corrió la que reasigna), los caminos de copia de `vector_assign` (solo corrieron orígenes vacíos), la copia por mitades y el tercer bloque alineado de `func_088D671C`, y los tipos de `func_08C4DA5C` distintos del que corrió.
      **6.402 funciones, 0 discrepancias** (puerta completa de esta rama: 5.113 comparadas, 24 más que `main` (5.089), 0 discrepancias; 21 de las 32 se comparan en la puerta completa en tiempo real y las otras 11 solo las alcanza la rápida sin tope; contando por dirección en los registros de las dos pasadas completas, 4.991 del lote 93 y 5.025 de esta, hay 34 más: 21 nuevas de este lote, 22 más de otros lotes por el tiempo de la pasada y 9 menos, de las cuales 7 son callees que ahora corren dentro de los casos de las nuevas (`func_088EF524`, `func_089099F4`, `func_08A6BA44`, `func_08A6BBE8`, `func_08BD3650`, `func_08BD3F30`, `func_08C3D09C`, FINDINGS lote 86); los dos totales cuentan distinto y no cuadran al uno; carga de la máquina alta por las pasadas de otro agente).
- [x] **Lote 95 de funciones calientes** (2026-10-16, `src/game/unsorted/hot_batch_95.cpp`, 33 registradas y 4 escritas sin registrar; antes de leer cada función se buscó su dirección en `src/` y en las ramas abiertas: no hay ningún PR abierto).
      Tras el lote 94 quedaban 111 funciones alcanzadas del sondeo `b93p` (121-170 instrucciones) sin `func_X` en `src/`; `tools.shapes --skeleton` no encuentra ninguna familia entre ellas y lo que queda son tablas de saltos, recursión, saltos probables enredados, coma flotante y llamadas a stubs que bloquean. Como el grupo está casi seco se hizo un sondeo nuevo (`tools.decomp_gate probe b95p`, 171-230 instrucciones: 348 sin hacer, 104 alcanzadas en las cinco rutas) y `--skeleton` encontró una familia: `hinted_insert`, una inserción con pista de 182 instrucciones en un árbol rojo-negro con claves de cadena,
      cinco copias que solo difieren en las dos funciones que llaman (`func_08BF70B4` y cuatro miembros; `tools.family_members --check` lee las cinco filas y coinciden con sus palabras), más su hermana con clave (palabra, cadena), `func_08C34C58` (45.104 llamadas), que se leyó aparte. De las 111 se escribieron 31 (27 registradas), leyéndolas una a una con `tools/disasm.py`, `tools.sp_loads` y la nueva `tools.arg_reads`: tres escritores de vectores de elementos de 4, 16 y 40 bytes (`func_08BEFA18`, `func_08C13A68`, `func_08BF56E4`),
      dos nativos de número a texto (`func_08A26744`, `func_08A26940`, con `number_text_tail`), dos constructores de matrices (`func_0899BEC0`, `func_0899CB44`), una búsqueda en registro con comparaciones virtuales (`func_08C29370`), nativos de cadenas con celdas y titulares, código de punteros contados (`func_089170E0`, `func_08888988`, con `release_counted`), un clasificador (`func_08B29070`), un destructor (`func_089F4140`) y similares. Sin registrar: `func_089C9F24` (todos sus casos paran en el stub `sceIoRead`), `func_089CA474` (`kernel isolation (LwMutex contention)`) y `func_08A31244` (el stub `sceWlanDrv::0x0C622081`), sin ningún caso comparado en las cinco rutas, y
      `func_08A7A3D4` (93 casos comparados en la puerta rápida y 3000 en una pasada densa de la ruta de carrera con 15.907 llamadas: ninguno cambió memoria, todas las llamadas salen pronto). Leídas y dejadas: `func_0895E3F4` y `func_08ADC4E0` (tablas de saltos), las ordenaciones y reequilibrios recursivos (`func_08B8BF44`, `func_08C496A4`, `func_08B5F444`, `func_08BB2CFC`), `func_08B3177C` (varios bloques con salto final), los mapas con punteros compartidos (`func_08A535A4`, `func_08AACAF8`), `func_08AF0B78` (anillo con semáforos), `func_088A66A4`, `func_0891179C`, `func_0895F7C0`, `func_08A6A40C` y los nativos de varios modos (`func_08A77940`, `func_08C23158`).
      Las puertas rápidas por tandas (20, 25, 3, 5 y 1 funciones, según se iban escribiendo) dieron 0 discrepancias a la primera: ninguna función falló en su primera pasada (en la primera, tres no se compararon porque sus llamadas paran en stubs o en un cerrojo y se dejaron sin registrar). La puerta completa sí encontró un fallo, y no era del lote: `func_08A568EC` (de `hot_leaves.cpp`) devolvía el resultado de su llamada donde el original devuelve `*a` (el `lw v0, 0(s1)` del test) cuando `*a` ya está puesto, un camino que la pasada completa en tiempo real alcanzó una vez en la ruta de opciones (1 discrepancia en 5.133 comparaciones); corregido en esta rama.
      Puerta rápida (cinco rutas sin tope, con la pasada de cobertura): 33 de 33 comparadas, 0 discrepancias y al menos un caso que cambia memoria en cada una salvo `func_08B29070`, que no escribe (49 casos iguales); sabotajes 33 de 33 detectados en la primera ronda; `contract_audit` 0 nuevos (las dos del lote 90 con `v1`); `tools.family_members --check` 5 filas iguales; `literal_check` 37 funciones, 0 literales que falten (3 `--allow`: la mitad alta 0x08D70000 que `lui v0, 0xA` deja en `v0` en `func_08A6E974`, y los desplazamientos 0x20 y 0x24 de la base del elemento por defecto de `func_08BF56E4`); `tools.arg_reads` marca seis llamadas, todas inofensivas.
      Caminos leídos solo del asm: cada comentario los lista (`tools.decomp_gate coverage`); los más débiles son `func_08B29070` (solo corrieron las clases con `lo` < 100: las demás y todo el árbol de 64 bits salen del asm), `func_08858090` (solo corrió la delegación en `func_08855990` con `count` < 2), `func_08819170` (`count` fue siempre 3), `func_089170E0` (`count` fue siempre 1 y no se liberó ningún bloque contado), `func_08C34C58` y las filas de `hinted_insert` (los brazos de igualdad de palabra y de comparación de cadena y casi todos los de vecinos), `func_089936C4` (todos los objetos opcionales estaban), `func_08888988` (las salidas tempranas y el par nulo) y `func_08A82864` (el brazo que encuentra un nodo y no inserta).
      **6.435 funciones, 0 discrepancias** (puerta completa de esta rama, segunda pasada tras corregir `func_08A568EC`: 5.136 comparadas, 27 más que `main` (5.109), 0 discrepancias; contando por dirección frente a la pasada completa del lote 94 son 5.048 frente a 5.025: 23 más = 36 nuevas (30 de este lote; las otras tres, `func_08858298`, `func_089170E0` y `func_08C33C44`, solo las alcanza la puerta rápida, y 6 de otros lotes por el tiempo de la pasada) menos 13 que dejan de compararse (corren dentro de los casos de las nuevas o la ruta en tiempo real ya no las alcanza); los dos totales cuentan distinto y no cuadran al uno; en la primera pasada completa, con la discrepancia de `func_08A568EC`, fueron 5.133).
- [x] **Lote 96 de funciones calientes** (2026-10-16, `src/game/unsorted/hot_batch_96.cpp`, 32 registradas y 5 escritas sin registrar; antes de leer cada función se buscó su dirección en `src/` y en las ramas abiertas: no hay ningún PR abierto, los aciertos son solo llamadores).
      Tras el lote 95 quedaban de los sondeos `b93p` (121-170 instrucciones) y `b95p` (171-230) 80 + 98 funciones alcanzadas sin `func_X` en `src/`; `tools.shapes --skeleton` no encuentra ninguna familia exacta entre las 178 (las parejas casi iguales, 0,80-0,89, son variantes de la inserción con relleno cuyos bucles de movimiento de elementos difieren y se dejaron). Se leyeron una a una con `tools/disasm.py`, `tools.sp_loads` y `tools.arg_reads` y se escribieron 37, empezando por el código de coma flotante sin saltos y las cadenas de llamadas poco profundas (lo difícil, tablas de saltos, recursión, VFPU y las rutinas de `libm` con tablas en `.rodata`, queda para después):
      hojas de coma flotante (`func_088CB9B0`, `func_08954898`, `func_08B49F68` una tangente con reducción de rango, `func_08C46A5C` matriz 4x4 por vector, `func_08B4FC34` interpolación esférica de cuaterniones), curvas y seguidores (`func_089109A0`, `func_08926EA0`, `func_089F6748`, `func_089AA708`), maquetación (`func_08A17104`, `func_08A18494`, `func_08A4D79C`, `func_0880E1E0`, `func_08812A8C`, `func_08813044`) y actualizaciones por fotograma (`func_088705D4`, `func_08956B20`, `func_088AA230`, `func_08AD2B50`, `func_08AD6EBC`, `func_089AAD34`, `func_088AB884`, `func_08905300`, `func_089230A4`, `func_0896A9C4`, `func_089ACCA0`, `func_0894CA04`, `func_0894F1C8`, `func_08A07E28`, `func_0889E01C`, `func_0893B494`, `func_08956930`).
      Sin registrar (5): `func_088616A8`, `func_0889B810`, `func_0893CBCC`, `func_0895B8C4` y `func_0895C140` se compararon (98, 87, 83, 83 y 83 casos en la puerta rápida y 3.000 en una pasada densa de la ruta de carrera de 11.810-16.430 llamadas) con 0 discrepancias y ni un caso cambió memoria (actualizan estado solo en condiciones que las rutas guionizadas nunca alcanzan), así que los caminos que escriben quedan sin prueba y se dejan escritos pero sin registrar.
      Primera pasada: 24 funciones, 22 iguales y dos discrepancias solo en `v0` (`func_089230A4` y `func_0895C140`: la constante que deja un test, FINDINGS); la segunda, con 32, una sola: `func_08AD6EBC` fallaba en los 98 casos porque su llamada al reenviador `func_08AD6A00` (`j func_08C3E4E8`, una ordenación parcial de tres argumentos) pasaba dos (FINDINGS; `tools.arg_reads` ahora sigue los `j` de los reenviadores y marca esa llamada). Tres revisores de solo lectura contrastaron con el asm los caminos que ninguna ruta toma y hallaron tres fallos más, ya corregidos: la salida temprana de `func_0889E01C` (su `v0` es el `andi` del test, 0), el `slt` con signo de `func_0894CA04` (un `halfword` contra el inicio con signo) y `func_089F6748` (el valor de las dos llamadas virtuales del medio se vuelve a leer de memoria tras cada llamada).
      Puerta rápida (cinco rutas sin tope, con la pasada de cobertura): 32 de 32 comparadas, 0 discrepancias y al menos un caso que cambia memoria en cada una salvo las dos funciones puras de coma flotante `func_08B49F68` y `func_089109A0`; sabotajes 32 de 32 detectados en la primera ronda; `contract_audit` 0 nuevos (las dos del lote 90 con `v1`; `func_08956B20` salió marcada —un llamador decompilado usa su resultado, el `v0` del salto final— y se registró como Word); `tools.family_members --check` 0 filas (0 incorrectas); `literal_check` 37 funciones, 0 literales que falten (3 `--allow`: 0x51EB851F de `func_0894CA04`, un par `lui`/`ori` en dos registros, y el cerrojo 0x08CF4A2C de `func_089AAD34` y `func_089AA708`, que el asm construye con `lui s0, 2` + `addiu a0, s0, -0x7AD4`); `tools.arg_reads` marca tres llamadas (más una línea que cita una de ellas en un comentario), todas inofensivas.
      Caminos leídos solo del asm: cada comentario los lista (`tools.decomp_gate coverage`); los más débiles son `func_08956930` (la máquina de estados del byte de banderas: solo corrió el brazo de la primera llamada), `func_0896A9C4` (solo corrió el modo 2: los modos 1 y 3 salen del asm), `func_089AAD34` (el bloque de tolerancias y el del cerrojo), `func_0889E01C`, `func_088705D4` (la llamada del bloque y los dos ajustes de ángulo), `func_089109A0` (solo las búsquedas en tabla de la forma 0), `func_08AD6EBC` (los brazos de `o[0x1E]` y `o[0x21]`), `func_088AB884` (las escrituras del mejor índice) y `func_08B4FC34` (solo corrió el brazo genérico: t == 0, t == 1, d < 0, 1 < d y s == 0 salen del asm).
      **6.467 funciones, 0 discrepancias** (puerta completa de esta rama: 5.168 comparadas, 36 más que `main` (5.132), 0 discrepancias; 31 de las 32 funciones de este lote se comparan en la puerta completa en tiempo real y `func_08954898` (un inicializador de una sola llamada) solo la alcanza la rápida sin tope; las otras 5 de diferencia vienen de otros lotes por el tiempo de la pasada y los dos totales no cuadran al uno).
- [x] **Lote 97 de funciones calientes** (2026-10-16, `src/game/unsorted/hot_batch_97.cpp`, 27 registradas (una anidada) y 7 escritas sin registrar; antes de leer cada función se buscó su dirección en `src/`: quedaban 141 de las 178 de los sondeos `b93p` y `b95p`, contando las 5 que el lote 96 dejó sin registrar).
      Se escribieron 34, de las más regulares y más llamadas: ventanas de dígitos y de reloj (`func_0880E400`, `func_08811E30`, `func_08812720`, `func_08812CEC`), actualizaciones por fotograma (`func_08883B7C`, `func_0896A540`, `func_08974A14`, `func_08969D2C`, `func_089491A0` un suavizado de medidor, `func_0897D118` el contador de un botón mantenido, `func_08992320`, `func_089ABBB0`, `func_089BEF60`, `func_089FB990`, `func_08AF1524` el dibujado de capas), el cursor de un menú con tabla de saltos (`func_0895E3F4`), código de coma flotante (`func_08AE0244` fase más corta, `func_08904E08`), el escritor tipado de un flotante (`func_08C4D4EC`, doce identificadores de tipo), las inicializaciones de una sola vez y los constructores (`func_088EE6C0`, `func_08928E24`, `func_08915714`, `func_08B5BB0C`, `func_08B610C0`, `func_08B8F080` las estáticas con guarda) y `func_0890E41C`. Lo difícil (cadenas con copia al escribir, 64 bits y coma flotante por software, `alloca` con punteros inteligentes, VFPU) queda para después.
      Sin registrar (7): `func_08AF0B78` y `func_08AEB4CC` esperan un semáforo (`sceKernelWaitSema`, que no es registrable: 89 de 89 y 88 de 99 casos «no comparables»); `func_08B04D98` construye un objeto cuya llamada final rellena elementos nuevos de un vector con memoria de pila sin inicializar (11 de 11 casos distintos, FINDINGS); `func_08860890`, `func_08B3177C`, `func_08C496A4` y `func_0889AD18` se compararon (99 + 3.000, 94 + 3.000, 17 + 204 y 85 + 506 casos; los densos son pasadas de la ruta de carrera con `GT_DECOMP_DENSE`) con 0 discrepancias y ningún caso cambió memoria (`func_08C496A4` solo recibe rangos de menos de 17 elementos, así que su partición no se ejecuta).
      Cambio de host: `sceRtcGetCurrentClockLocalTime` (0xE7C27D1B) es ahora una importación grabable (`kRecordableNids`), así que `func_08812CEC`, que llama al stub ella misma, se compara (17 de 17 iguales; antes «no comparable» en todos). `func_08B610C0` se llama solo dentro de los casos de `func_08B5BB0C` (en solitario se compara 1 de 1): se registra anidada (`HOT97N`) y su verificación es «anidada en 1 caso igual» (FINDINGS).
      Primera pasada: 20 funciones, una discrepancia de RAM en `func_0880E400` (`w[0xE8] = 5`, el C++ escribía 6); la segunda, con 28: `func_08915714` fallaba en `v0` (la cota del bucle `addiu v0, zero, 4` deja 4) y `func_08B04D98` en todos sus casos. Tres revisores de solo lectura contrastaron con el asm todos los caminos que ninguna ruta toma y hallaron un fallo más, ya corregido: las cuatro salidas tempranas de `func_089ABBB0` dejaban el último `v0` de una llamada en vez del valor que deja el asm.
      Puerta rápida (cinco rutas sin tope, con la pasada de cobertura): 27 de 27 comparadas, 0 discrepancias y al menos un caso que cambia memoria en cada una salvo `func_08904E08` (solo en su pasada densa: 8 de 327 llamadas) y `func_08B610C0` (anidada); sabotajes 27 de 27 detectados (26 en la primera ronda, `func_08B610C0` en la segunda); `contract_audit` 0 nuevos (las dos del lote 90 con `v1`; `func_08904E08` se registra como Float y `func_08B8F080` como Word, que sus llamadores leen); `tools.family_members --check` 0 filas; `literal_check` 34 funciones, 0 literales que falten (6 `--allow`: los campos `ext`/`ins` 0xF, 0xF0, 0xF00 de `func_0896A540` y los desplazamientos 0x9640, 0x9644, 0x9648 de `func_08B3177C`, que el asm obtiene de `lui` + un desplazamiento negativo); `tools.arg_reads` marca seis llamadas, todas inofensivas (`func_08B58874` con un modo que solo lee a0, `func_08A00AB0` que lee los a0/a1 de la entrada, las dos de `func_08AD83B8` que es variádica, `func_089AAC8C` que lee el a1 que conserva el contexto desde la entrada).
      Caminos leídos solo del asm: cada comentario los lista (`tools.decomp_gate coverage`); los más débiles son `func_08B8F080` (las estáticas con guarda: solo corrió la inicialización de la primera llamada), `func_0895E3F4` (varios estados del cursor), `func_0897D118` (los contadores del botón mantenido), `func_08C4D4EC` (solo algunos de los doce tipos), `func_08904E08` y `func_089491A0` (brazos raros de coma flotante) y `func_08AF1524` (los brazos de la cámara y la superposición).
      **6.494 funciones, 0 discrepancias** (puerta completa de esta rama: 5.180 comparadas, 11 más que `main` (5.169), 0 discrepancias; `func_088EE6C0`, `func_08904E08`, `func_08928E24` y `func_08C4D4EC` solo las alcanza la puerta rápida sin tope).
- [x] **Lote 98 de funciones calientes** (2026-10-19, `src/game/unsorted/hot_batch_98.cpp`, 33 registradas y 2 escritas sin registrar; sondeos `b98p` (231-300 instrucciones) y `b98s` (las 7.866 funciones sin registrar de 20-120 instrucciones: 43 alcanzadas) además de los restos de `b93p`/`b95p`; antes de leer cada función se buscó su dirección en `src/`).
      Se escribieron 35: borrador de 11 de un agente interrumpido (releído contra el asm: dos errores, ver abajo), una familia de tres redimensionadores de vector con descriptor (`func_08BED9A0`, `func_08C0D538`, `func_08C11D08`), flotantes calientes (`func_088C4BB0`, `func_088C50D4` una búsqueda en tabla, `func_08960D4C`, `func_08977904`, `func_08AD41B4`, `func_088A0628`, `func_08AFD0EC`), un árbol de llamadas por fotograma (`func_089C54B8`, `func_088A1D84`, `func_088D3E98`, `func_088D414C`), destructores y holders (`func_0889ED00`, `func_088829A0`, `func_0896E380`, `func_08A539AC`) y arranques (`func_088D78EC`, `func_088DFB68`, `func_0893BA0C`). Sin registrar: `func_0888E81C` (una llamada por ruta, 0 o 1 caso comparado, ninguno cambia memoria) y `func_0893B824` (comparada 0).
      Errores: primera puerta, `func_0894A15C` (el callee `func_08949B04` lee `f12` y el C++ no se lo pasaba) y `func_0880E6EC` (25.0f escrito como 100.0f): FINDINGS; `func_08881A68` devolvía el puntero en lugar de `index < 2` cuando `obj` es nulo. Tres revisores de solo lectura de los caminos sin tomar: `func_08AE78E8` (`n[0x40]` recibe la escala, no 0), `func_088DFB68` y `func_08881A68` (orden de lectura antes de una llamada virtual).
      Puerta rápida (cinco rutas sin tope, con cobertura): 33 de 33 comparadas, 0 discrepancias; sabotajes 33 de 33 detectados en la primera ronda; `contract_audit` 0 nuevos. **6.527 funciones, 0 discrepancias** (puerta completa: 5.174 comparadas, 0 discrepancias; 27 de las 33 nuevas se comparan en tiempo real, el resto solo en la puerta rápida).
- [x] **Lote 99 de funciones calientes** (2026-10-19, `src/game/unsorted/hot_batch_99.cpp`, 36 registradas y 3 escritas sin registrar; sondeo `b99p` de las 14.157 funciones recompiladas de hasta 420 instrucciones que ningún fichero de `src/` define ni registra: 668 alcanzadas por las cinco rutas, 463 de 20 o más instrucciones. El filtro de lotes anteriores («la dirección no aparece en `src/`») ocultaba las funciones que solo llama código ya decompilado: FINDINGS).
      Se escribieron 39: escritores de estado de la biblioteca gráfica (`func_08B58874` 9,7 millones de llamadas en el sondeo, `func_08B52578`, `func_08B58218`, `func_08B5596C`, `func_08B52B70`), álgebra en coma flotante (`func_08B6D564`, `func_08B49D98`, `func_08C468E4`, `func_08B509EC`, `func_089A932C`, `func_089DA5C0`), árboles y vectores (tres inserciones únicas de un árbol rojo-negro con claves de cadena contada, dos asignaciones de vector, `func_08BEF574`, `func_08C3E4E8`), rutas y cadenas (`func_08B7C830` strncpy, `func_08B7C6BC` strncmp, `func_08B7BB6C` strcpy, `func_08B23928`, `func_08B23FA0`, `func_08B2E314`), árboles de llamadas (`func_08C20B4C` y su gemela `func_08C23E60`, `func_08C238F0`, `func_089B684C`, `func_08A7D24C`, `func_08AAACF4`, `func_0896A254`, `func_08AE69F4`, `func_0886AAA4`, `func_08AA1D7C`, `func_08C053E8`, `func_088B29FC`). Sin registrar: `func_08AE1D6C` (las rutas solo llegan a la salida con `total <= 0`: el cuerpo del bucle no corrió en 84 casos), `func_08B57968` (109 casos comparados, todos por un brazo que escribe valores que ya estaban; cuatro brazos sin correr) y `func_08978080` (comparada 0: su llamada final alcanza el import `sceDisplay`).
      Errores: segunda puerta, `func_0886AAA4` (`v0` distinto: la ranura virtual no escribe `v0` y el original devuelve la dirección de la vtable; se añadió `call_virtual`, FINDINGS). Tres revisores de solo lectura de los caminos sin tomar no encontraron errores de lógica; sí diferencias de registros de paso (`a1`/`a3`/`v1`/`f0`), corregidas las baratas.
      Puerta rápida (cinco rutas sin tope, con la pasada de cobertura): 36 de 36 comparadas, 0 discrepancias; sabotajes 36 de 36 detectados en la primera ronda; `contract_audit` 0 nuevos (las tres inserciones del árbol se registran como `DoubleWord`: sus llamadores leen `v1`); `tools.family_members --check` 3 filas, 0 malas; `tools.literal_check` 39 funciones, 14 literales «ausentes» que son máscaras o desplazamientos aritméticos del asm (`ins`/`ext`, `0x18 + 1`, `0x28 = 8 * 5`, los campos de `func_08B58218`); `tools.sp_loads` 15 lecturas de callee-saved desde el marco, todas modeladas como locales; `tools.arg_reads` 3 líneas inofensivas (`func_08B36170` lee `t3` solo para compararlo, `func_08AD83B8` variádica).
      Caminos leídos solo del asm: cada comentario los lista; los más débiles son `func_08C20B4C` y `func_08C23E60` (los avisos de nulos y el bucle de la cadena), `func_08B58874` (los modos 0x78 y 0x8002), `func_089B684C` (los bloques segundo y tercero), `func_08B23928` (el cuerpo del bucle de componentes), `func_08B2E314` y `func_08B5596C` (varias ramas de la tabla de saltos) y `func_08C3E4E8` (la rama sin intercambio).
      **6.563 funciones, 0 discrepancias** (puerta completa: 5.201 comparadas, 0 discrepancias; 32 de las 36 nuevas se comparan en tiempo real, `func_0886AAA4`, `func_08AA1D7C`, `func_08AAACF4` y `func_08BE9D90` solo en la puerta rápida).
- [x] **Lote 100 de funciones calientes** (2026-10-19, `src/game/unsorted/hot_batch_100.cpp`, 10 registradas y 30 escritas sin registrar; sondeo `b100p` de las 14.121 funciones recompiladas de hasta 420 instrucciones que ningún fichero de `src/` define ni registra: 638 de 20 o más instrucciones alcanzadas por las cinco rutas).
      Se escribieron 40: el reequilibrado de un árbol rojinegro (`func_08B5F2B8`), un analizador de texto con `*` (`func_08A4D57C`), un getter de cuaternión (`func_088AA074`), empaquetado de ranuras virtuales (`func_088FA224`), añadido de componentes de ruta (`func_08B23A70`), rellenos y copias de rangos, asignaciones de vector, la familia de `func_08C401C4` y otras. Registradas: `func_08A4D57C`, `func_08B5F2B8`, `func_088AA074`, `func_088FA224`, `func_08B19F10`, `func_08B23A70`, `func_08C34270`, `func_08C401C4`, `func_08C40FA4`, `func_08C413DC`. `func_08A9DD14` queda sin registrar (la revisión del PR #237: apaga ~330 funciones de arranque, FINDINGS; la caída de 5.201 a 4.915 comparadas era eso, no la carga).
      Sin registrar (línea `HOT100` comentada con el motivo): 10 alcanzan importaciones `sce*` en todos los casos (comparadas 0), 18 se compararon solo por los caminos vacíos (ningún caso cambió la memoria; ni una pasada densa de la ruta de carrera) y `func_08B7B2E4` es un `setjmp` que el harness no puede comparar (FINDINGS).
      Puerta rápida (cinco rutas sin tope, con cobertura): 39 funciones instaladas, 29 comparadas, 0 discrepancias; las 11 registradas comparan; sabotajes 11 de 11 detectados en la primera ronda; `contract_audit` 0 nuevos. **6.573 funciones, 0 discrepancias** (puerta completa sin `func_08A9DD14`: ver el PR #237).
      Caminos leídos solo del asm, los más débiles primero: `func_08A4D57C` (todo el camino de `*`: números, par con coma, escalas, copia del prefijo), `func_08C34270`, `func_08B23A70` (las salidas tempranas y el caso «no cabe»), `func_08B19F10` (el byte ya activo), `func_08C40FA4` / `func_08C413DC` (cuerpo compartido con `func_08C401C4`), `func_088AA074` (objeto nulo).
- [x] **Lote 101 de funciones calientes** (2026-10-19, `src/game/unsorted/hot_batch_101.cpp`, 49 registradas y 11 escritas sin registrar; sondeo `b101p` de las 13.968 funciones recompiladas de hasta 300 instrucciones que ningún fichero registra, filtradas a las que escriben memoria o son reenvíos y getters puros, sin importación `sce*` en los cinco saltos siguientes y sin definir ya en `src/`).
      Se escribieron 60: los reenvíos con salto final más llamados del juego sin decompilar (`func_08B4B864` 134.690 llamadas sondeadas, `func_08B77BF4` 97.882, `func_08B4BAA8` 86.154, `func_08C31304`, `func_08C3131C`, `func_0892C8B8`...), getters y setters pequeños, el borrado y reequilibrado de un árbol rojinegro (`func_08B5F444`), una inserción ordenada en una lista de bloques libres (`func_08B2BF20`), copias de estructuras (`func_088D4824`, `func_08897470`, `func_088D176C`, `func_0894DB6C`), dos reubicaciones de punteros (`func_08909CB4`, `func_08903958`), un `memchr`, una comparación sin mayúsculas y una búsqueda de `short` (`func_08C46E38` y su gemela `func_08BB890C`).
      Sin registrar (línea `HOT101` comentada con el motivo): 5 comparadas 0 (todos los casos «no comparables»: sus llamadas alcanzan importaciones `sce*`; dos de ellas terminan en `jr t9` por una ranura virtual, que un recorrido estático no ve), 5 que escriben memoria sin que ningún caso la cambie y `func_08B93AE8` (sus 6 casos tenían el árbol vacío).
      Puerta rápida (cinco rutas sin tope, con cobertura): 50 funciones instaladas, 50 comparadas, 0 discrepancias; sabotajes 50 de 50 detectados en la primera ronda (una de ellas, `func_08B93AE8`, quedó después sin registrar; los 5 que cambiaron al final, otra vez: 5 de 5); una pasada densa de la carrera para `func_08B5F444` (3.000 casos, todos iguales y todos cambian la memoria) porque la cobertura la deja con la mayoría de las ramas sin tomar; `contract_audit` 0 nuevos (tres reenvíos que devuelven `v1` se registran como `DoubleWord`). **6.622 funciones, 0 discrepancias** (puerta completa: 5.218 comparadas y 1.404 sin comparar frente a 5.208 / 1.365 de `main`: 18 de las 49 nuevas se comparan en tiempo real, las otras 31 solo en la puerta rápida).
      Caminos leídos solo del asm, los más débiles primero: `func_08B7C598` (ningún caso con mayúsculas; las salidas por final de cadena y por `n`), `func_08B5F444` (casi todas las ramas del reequilibrado salvo las que la pasada densa cubre sin informe), `func_08935B88` (toda la segunda búsqueda), `func_08909CB4` y `func_08903958` (cada caso tenía todas las tablas llenas), `func_088D176C` y `func_0894DB6C` (solo direcciones alineadas), `func_088D4824`, `func_0892BF7C` (cuenta <= 0), `func_08B2BF20` (nunca avanza por la lista) y los reenvíos con `jr t9` por la ranura virtual (`func_08860058`, `func_0885E794`).
- [x] **Lote 102 de funciones calientes** (2026-10-19, `src/game/unsorted/hot_batch_102.cpp`, 42 registradas y 12 escritas sin registrar; sondeo `b101p` releído tras el lote 101: de las 225 funciones alcanzadas que ningún fichero define ni registra, hasta 300 instrucciones, solo 62 no tienen VFPU ni una importación `sce*` en los cinco saltos siguientes; `tools.pool` guarda ahora ese recorte).
      Se escribieron 54: reenvíos con salto final y recorridos de listas (`func_08B207A4`, `func_08891C54`, `func_08C4CB24`, `func_08C4CADC`, `func_088A0DD4`), la asignación de punteros contados (`func_088AAAE8`, `func_088AAC74`, `func_0890673C`, `func_08873EA0`), tres bucles de ordenación introsort (`func_08C3EEE0`, `func_08BDBB44`, `func_08BDABCC`), conversiones de tablas de bytes a flotantes (`func_089594D8`, `func_0895871C`, `func_0895EEF8`), física y geometría en flotantes (`func_089572C4`, `func_088D1BD0`, `func_0895F7C0`, `func_08951304`, `func_089101F0`), un buscador de nombres en tabla ordenada (`func_08ADC4E0`), un manejador de mensajes (`func_088B4F3C`), una búsqueda-o-creación en árbol (`func_08A92588`, 744.457 llamadas en la puerta rápida), la inserción en un vector (`func_08C0A8A0`) y el relleno de entradas de una tabla de 56 bytes (`func_08AE9CD8`).
      Sin registrar (línea `HOT102` comentada con el motivo): 10 comparadas 0 (todos los casos «no comparables»: sus llamadas alcanzan importaciones `sce*` que el harness no puede registrar: `func_08BD74BC`, `func_08B35418`, `func_08AEB850`, `func_08B2DBF0`, `func_0897BB68`, `func_08B098A8`, `func_08825AE8`, `func_0885E940`, `func_08B7771C` y `func_08B6C020`, que las rutas nunca llaman) y 2 que escriben memoria sin que ningún caso la cambie (`func_0888879C`, `func_08B00E90`; ni una pasada densa de la carrera de 3.000 casos).
      Puerta rápida (cinco rutas sin tope, con cobertura): 44 funciones instaladas, 41 comparadas, 0 discrepancias; sabotajes 41 de 42 detectados en la primera ronda y la que faltaba (`func_08891C54`, alcanzada por un caso en dos de cuatro pasadas) detectada sola en la segunda; `contract_audit` 0 nuevos. **6.664 funciones, 0 discrepancias** (puerta completa en tiempo real: 5.226 comparadas y 1.438 sin comparar frente a 5.213 / 1.409 de `main` medidos con el mismo árbol y las 42 desinstaladas: 28 de las 42 nuevas se comparan, pero 20 llamadas que antes se comparaban sueltas (`func_088FAFD8`, `func_08946734`, `func_08BD31D4`...) ahora corren anidadas dentro de los casos de sus nuevos llamadores decompilados; suelo `config/decomp_gate_floor.txt` 5.226).
      Caminos leídos solo del asm, los más débiles primero: `func_08C3EEE0`, `func_08BDBB44` y `func_08BDABCC` (todo el bucle: los casos comparados tenían menos de 17 elementos), `func_08BB1D9C` (solo rangos vacíos), `func_089D5690` (todo tras la primera comparación), `func_08ADC4E0` (todo tras un acierto: el `switch` de tipos), `func_08A77940` (modos 2 y 3), `func_088B4F3C` (casi todos los tipos de mensaje), `func_08C0A8A0` (inserción sin realocar, crecimiento de un vector no vacío y la liberación), `func_08A92588` (el brazo del centinela y la construcción), `func_089281E4`, `func_08B31AB4` (el brazo del estado 4), `func_089191DC`, `func_089101F0`, `func_088BBA00`, `func_08AE9CD8`, `func_0890778C` y los helpers de contador (`func_08873EA0`, `assign_counted`).
- [x] **Lote 103: importaciones desbloqueadas y 43 funciones** (2026-10-20, `src/game/unsorted/hot_batch_103.cpp`, 31 nuevas registradas de 33 escritas, más 12 funciones aparcadas en lotes anteriores como «no comparables» que se registran de nuevo con su código original; 6.664 → 6.708).
      El recorte estático de importaciones (`tools.pool --stubs`) marcaba `sceKernelDelayThread` en 140 de 277 candidatas; se midió en dinámico (una puerta rápida sobre 74 registros aparcados) y los bloqueos reales eran `LwMutex`/`sleep` (se quedan fuera), `sceIoRead`, `sceDisplay` y los semáforos, `sceSasCore`, la memoria de particiones y `sceIoClose`. Se leyó el HLE de cada una y pasan a `host/import_lists.hpp` (con `host/tests/import_lists_test.cpp`, visto fallar): 40 grabables (semáforos y banderas de evento con rechazo dentro de un caso cuando bloquearían o despertarían, particiones, `sceKernelDeleteThread`, `sceIoClose`, subinterrupciones, toda la unidad `sceSasCore`) y 2 repetibles (`sceKernelPrintf`, `sceWlanGetEtherAddr`). `sceKernelCreateMutex` se probó y se revirtió: la tabla de mutex está en el estado de repetición y `func_08B6441C` falló la puerta completa.
      Nuevas: los cuatro ajustadores de efectos de `sceSasCore`, los seis constructores que registran nombres, el insertar-con-pista del árbol (tres tipos de clave), el asignador y el liberador del montón, la dirección MAC, el gancho de subinterrupciones, destructores con liberación de objetos compartidos y el inicializador de registro big-endian `func_088D5AFC`. Sin registrar: `func_08B34B2C` (101 de 101 casos duermen) y `func_088B0520` (pasada densa de 3.000 casos sin lista no vacía).
      Puerta rápida (cinco rutas sin tope): 44 instaladas, 43 comparadas en una pasada y todas en otra, 0 discrepancias; sabotajes 31 de 31 en la primera ronda y las 13 re-registradas pinadas por fichero (`func_08A235F8` solo tras quitar un `[[maybe_unused]]` que ocultaba la función al sabotaje). **6.708 funciones, 0 discrepancias** (puerta completa: 5.270 comparadas, suelo 5.226 → 5.270).
      Caminos leídos solo del asm, los más débiles primero: `func_08AE2160` (el bucle nunca corrió), `func_08909D98` (el último objeto compartido siempre nulo: sin la llamada final), `func_08B34B2C`/`func_088B0520` (sin registrar), `func_08B59A24` (solo los casos sin espera de vblank), `func_088D8B20` (un único caso), `func_08A17A28` (toda la búsqueda en la lista), `func_08BFEBAC`/`func_08C1EEEC`/`func_08B940D8` (solo los brazos con la pista en el encabezado o con clave menor), `func_08B33C0C`, `func_08B2BFDC` y `func_08B2CE7C` (los caminos de error).
      Revisión del PR #251: `__sceSasSetVoice` pasa a comparar sus cinco argumentos (máscara `0x1F`; la HLE lee también la dirección del VAG, su tamaño y el bucle) y `func_08B59A24` vuelve a quedar sin registrar (lo bloquea la espera de vblank, que sigue prohibida en un caso): 6.707 funciones.
- [x] **Lote 135** (2026-10-24): el primer lote de código que solo alcanzan las rutas desde la partida del usuario:
      59 funciones y 3 thunks (lectura de la partida: números de longitud variable, palabras big-endian, el cifrado XOR
      de GT sobre sus datos, cadenas y mapas deserializados; natives de script de fechas y campos de 16 bits; el
      formateo de fecha y hora con sus dos órdenes; la conversión de segundos a fecha; el control de selección de una
      lista; y varios constructores y getters). Como los padres decompilados ocultan a sus hijos, se comparó en cinco
      pasadas (cada una solo con las que faltaban): 62 de 62 comparadas, 0 discrepancias, sabotaje 62 de 62.
- [x] **Lote 136** (2026-10-24): 6 funciones más que solo alcanzan las rutas desde la partida: física de sus coches
      (la fuerza de la suspensión de una rueda, tres parámetros de los ajustes del coche, la tabla de desarrollos de la
      caja de cambios, la clasificación de la tracción y el paso de un estado con cuatro canales que se acercan a su
      fila) y un lector de un registro versionado (`func_08AAB0B4`: un mapa de cadenas a nodos por debajo de la versión
      13, una lista de cadenas leída en otra posición del flujo desde la 14). Comparadas por separado (los padres decompilados las ocultan): 6 de 6, 0 discrepancias, sabotaje 6
      de 6; las ramas de versión que no recorre ninguna ruta, revisadas contra el asm. Puerta completa: 16 rutas, 7.359
      funciones, 5.681 comparadas, 0 discrepancias.
- [x] **Lote 212** (2026-10-29): funciones sueltas de cuarenta y siete instrucciones escritas a mano (grupos de forma idéntica por sustitución, constructores y destructores, recorridos de listas, copias, llamadas virtuales): 101 filas, 89 verificadas; las 12 restantes no reciben casos. Total: 14.796 funciones.
- [x] **Lote 211** (2026-10-29): funciones sueltas de cuarenta y seis instrucciones escritas a mano (grupos de forma idéntica por sustitución, constructores y destructores, recorridos de listas, copias, llamadas virtuales): 94 filas, 65 verificadas; las 29 restantes no reciben casos. Total: 14.707 funciones.
- [x] **Lote 210** (2026-10-29): funciones sueltas de cuarenta y cinco instrucciones escritas a mano (grupos de forma idéntica por sustitución, constructores y destructores, recorridos de listas, copias, llamadas virtuales): 88 filas, 68 verificadas; las 20 restantes no reciben casos. Total: 14.642 funciones.
- [x] **Lote 209** (2026-10-29): funciones sueltas de cuarenta y cuatro instrucciones escritas a mano (grupos de forma idéntica por sustitución, constructores y destructores, recorridos de listas, copias, llamadas virtuales): 101 filas, 81 verificadas; las 20 restantes no reciben casos. Total: 14.574 funciones.
- [x] **Lote 208** (2026-10-29): funciones sueltas de cuarenta y tres instrucciones escritas a mano (grupos de forma idéntica por sustitución, constructores y destructores, recorridos de listas, copias, llamadas virtuales): 69 filas, 51 verificadas; las 18 restantes no reciben casos. Total: 14.493 funciones.
- [x] **Lote 207** (2026-10-29): funciones sueltas de cuarenta y dos instrucciones escritas a mano (grupos de forma idéntica por sustitución, constructores y destructores, recorridos de listas, copias, llamadas virtuales): 110 filas, 83 verificadas; las 27 restantes no reciben casos. Total: 14.442 funciones.
- [x] **Lote 206** (2026-10-29): funciones sueltas de cuarenta y una instrucciones escritas a mano (grupos de forma idéntica por sustitución, constructores y destructores, recorridos de listas, copias, llamadas virtuales): 78 filas, 48 verificadas; las 30 restantes no reciben casos. Total: 14.359 funciones.
- [x] **Lote 205** (2026-10-29): funciones sueltas de cuarenta instrucciones escritas a mano (grupos de forma idéntica por sustitución, constructores y destructores, recorridos de listas, copias, llamadas virtuales, coma flotante con el NaN de la PSP): 131 filas, 95 verificadas; 35 no reciben casos y una (`func_08B05F9C`) queda fuera por la carga útil de un NaN. Total: 14.311 funciones.
- [x] **Lote 204** (2026-10-29): funciones sueltas de treinta y nueve instrucciones escritas a mano (grupos de forma idéntica por sustitución, constructores y destructores, recorridos de listas, copias, llamadas virtuales): 92 filas, 65 verificadas; las 27 restantes no reciben casos. Total: 14.216 funciones.
- [x] **Lote 203** (2026-10-29): funciones sueltas de treinta y ocho instrucciones escritas a mano (grupos de forma idéntica por sustitución, constructores y destructores, recorridos de listas, copias, llamadas virtuales): 109 filas, 75 verificadas; las 34 restantes no reciben casos. Total: 14.151 funciones.
- [x] **Lote 202** (2026-10-29): funciones sueltas de treinta y siete instrucciones escritas a mano (grupos de forma idéntica por sustitución, constructores y destructores, recorridos de listas, copias, llamadas virtuales): 101 filas, 81 verificadas; las 20 restantes no reciben casos. Total: 14.076 funciones. Además, `memcmp` (libc) trata la cuenta como con signo, como el original.
- [x] **Lote 201** (2026-10-29): funciones sueltas de treinta y seis instrucciones escritas a mano (grupos de forma idéntica por sustitución, constructores y destructores, recorridos de listas, copias, llamadas virtuales, cálculo en coma flotante con el NaN de la PSP): 104 filas, 78 verificadas; las 26 restantes no reciben casos. Total: 13.995 funciones.
- [x] **Lote 200** (2026-10-29): funciones sueltas de treinta y cinco instrucciones escritas a mano (grupos de forma idéntica por sustitución, constructores y destructores, recorridos de listas, copias, llamadas virtuales): 124 filas, 103 verificadas; las 21 restantes no reciben casos. Total: 13.917 funciones.
- [x] **Lote 199** (2026-10-29): funciones sueltas de treinta y cuatro instrucciones escritas a mano (grupos de forma idéntica por sustitución, constructores y destructores, recorridos de listas, copias, llamadas virtuales): 126 filas, 72 verificadas; las 54 restantes no reciben casos. Total: 13.814 funciones.
- [x] **Lote 198** (2026-10-28): funciones sueltas de treinta y tres instrucciones escritas a mano (familias de operadores virtuales generadas por plantilla, grupos de forma idéntica por sustitución, constructores, recorridos de listas, conversiones de float): 157 filas, 99 verificadas; las 58 restantes no reciben casos. Total: 13.742 funciones.
- [x] **Lote 197** (2026-10-28): funciones sueltas de treinta y dos instrucciones escritas a mano (familias de operadores sobre valores de script generadas por plantilla, búsquedas binarias y lineales, constructores y destructores con cadenas compartidas, copias de bloques, rand() con multiplicación acumulada, ulp de double): 180 filas, 100 verificadas; las 80 restantes no reciben casos. Total: 13.643 funciones.
- [x] **Lote 196** (2026-10-28): funciones sueltas de treinta y una instrucciones escritas a mano (copias de punteros compartidos con recuento atómico, destructores de cadenas, recorridos de árboles, distancias mínimas en float, mapeos de bits de estado): 112 filas, 86 verificadas; 0x08927A0C coincide pero su sabotaje no se detecta y queda comentada; las 25 restantes no reciben casos. Total: 13.543 funciones.
- [x] **Lote 195** (2026-10-28): funciones sueltas de treinta instrucciones escritas a mano (envoltorios de errno, destructores de vectores y cadenas, llamadas a miembros por puntero, comparación de cadenas, búsquedas en tablas): 113 filas, 90 verificadas; las 23 restantes no reciben casos. Total: 13.457 funciones.
- [x] **Lote 194** (2026-10-28): funciones sueltas de veintinueve instrucciones escritas a mano (destructores de cadenas con recuento de referencias, envoltorios de errno, recorridos de listas, inicializadores estáticos, copias de estructuras con vtable): 152 filas, 119 verificadas; las 33 restantes no reciben casos. Total: 13.367 funciones.
- [x] **Lote 193** (2026-10-28): funciones sueltas de veintiocho instrucciones escritas a mano (búsquedas por nombre en tablas, recorridos de listas enlazadas, búsquedas binarias de cotas, mapeo de códigos de error, conversiones de entero sin signo a float): 128 filas, 104 verificadas; las 24 restantes no reciben casos. Total: 13.248 funciones.
- [x] **Lote 192** (2026-10-28): funciones sueltas de veintisiete instrucciones escritas a mano (destructores con miembros de función, inicializadores estáticos protegidos, iteradores de listas, inserciones ordenadas de claves de 64 bits, copias sin inicializar de estructuras): 120 filas, 93 verificadas; las 27 restantes no reciben casos. Total: 13.144 funciones.
- [x] **Lote 191** (2026-10-28): funciones sueltas de veintiséis instrucciones escritas a mano (destructores de cadenas con recuento de referencias, copias sin inicializar, búsquedas binarias en tablas, lectura de hexadecimal y decimal, cerrojos ll/sc): 147 filas, 123 verificadas; las 24 restantes no reciben casos. Total: 13.051 funciones.
- [x] **Lote 190** (2026-10-28): funciones sueltas de veinticinco instrucciones escritas a mano (constructores y destructores, copias sin inicializar de vectores, un CRC de 64 bits por tabla, cerrojos ll/sc, comparación de enteros grandes, redondeo de float hacia cero): 151 filas, 122 verificadas; las 29 restantes no reciben casos. Total: 12.928 funciones.
- [x] **Lote 189** (2026-10-28): funciones sueltas de veinticuatro instrucciones escritas a mano (rotaciones de árbol rojo-negro, ordenaciones por inserción, búsquedas binarias, borrado de listas y árboles, multiplicaciones de 64 bits): 162 filas, 138 verificadas; 0x088DE874 queda comentada por diferir. Total: 12.806 funciones.
- [x] **Lote 188** (2026-10-28): funciones sueltas de veintitrés instrucciones escritas a mano (copias y rellenos de arrays, desenganche de árboles, parsers de dígitos y varints, inicialización del Mersenne Twister, llamadas virtuales con retorno de 64 bits): 154 filas, 129 verificadas; 0x08B3EA88 (trunc.w.s desbordado) y 0x08B053E8 quedan comentadas por diferir. Total: 12.668 funciones.
- [x] **Lote 187** (2026-10-28): funciones sueltas de veintidós instrucciones escritas a mano (bucles de espera, decodificación LEB128, sumas adler, rellenos y copias de arrays, recorridos de observadores con retorno): 162 filas, 141 verificadas. Total: 12.539 funciones.
- [x] **Lote 186** (2026-10-28): funciones sueltas de veintiuna instrucciones escritas a mano (bloqueo de weak_ptr con CAS, búsquedas hacia atrás en cadenas, árboles, constructores, llamadas virtuales con retorno de 64 bits): 191 filas, 165 verificadas. Total: 12.398 funciones.
- [x] **Lote 185** (2026-10-28): funciones sueltas de veinte instrucciones escritas a mano (copias de shared_ptr con incremento atómico, árboles de búsqueda, conversiones de float, destructores con vtable): 205 filas, 171 verificadas; 0x08875D60 queda comentada (difiere en memoria escrita por su llamada virtual). Total: 12.233 funciones.
- [x] **Lote 184** (2026-10-28): funciones sueltas de diecinueve instrucciones escritas a mano (constructores y destructores con vtable, recorridos de listas de observadores, búsquedas binarias, copias de bloques, ayudantes de float y de 64 bits): 197 filas, 165 verificadas. Total: 12.062 funciones.
- [x] **Lote 183** (2026-10-28): funciones sueltas de dieciocho instrucciones escritas a mano (asignaciones con
      temporal, constructores y destructores, recorridos de listas, aritmética de 64 bits, ayudantes de float): 182
      filas, 154 verificadas. Total: 11.897 funciones.
- [x] **Lote 182** (2026-10-28): funciones sueltas de diecisiete instrucciones escritas a mano (constructores y
      destructores, asignaciones con temporal, cajas de 8 y 16 bytes, búsquedas, copias, ayudantes de float): 199 filas,
      189 verificadas. Total: 11.743 funciones.
- [x] **Lote 181** (2026-10-28): funciones sueltas de dieciséis instrucciones escritas a mano (constructores y
      destructores, búsquedas binarias, intercambio de rangos, llamadas que guardan s0/s1, ayudantes de float): 143
      filas, 129 verificadas. Total: 11.554 funciones.
- [x] **Lote 180** (2026-10-28): funciones sueltas de quince instrucciones escritas a mano (constructores y
      destructores con vtable, cajas de 8 bytes, llamadas virtuales, copias de rangos, ayudantes de float): 148 filas,
      130 verificadas. Total: 11.425 funciones.
- [x] **Lote 179** (2026-10-28): funciones sueltas de catorce instrucciones escritas a mano (constructores, llamadas
      que guardan a0 en s0/s1, búsquedas en árboles, copias de rangos, ayudantes de float): 169 filas, 159
      verificadas. Total: 11.295 funciones.
- [x] **Lote 178** (2026-10-28): funciones sueltas de trece instrucciones escritas a mano (constructores, llamadas
      que guardan a0 en s0, búsquedas, copias de rangos, `div` con divisor cero como Allegrex): 155 filas, 132
      verificadas. Total: 11.136 funciones.
- [x] **Lote 177** (2026-10-28): funciones sueltas de doce instrucciones escritas a mano (llamadas que guardan a0 en
      s0, búsquedas con valor por defecto, copias de rangos, constructores, ayudantes de float): 149 filas, 132
      verificadas. Total: 11.004 funciones.
- [x] **Lote 176** (2026-10-28): funciones sueltas de once instrucciones, segunda mitad (copias y rellenos de
      rangos que saltan destinos nulos, `remove_copy`, llamadas que guardan a0 en s0): 60 filas, 54 verificadas.
      Total: 10.872 funciones.
- [x] **Lote 175** (2026-10-28): funciones sueltas de once instrucciones escritas a mano, primera mitad (llamadas
      que guardan a0 en s0, constructores con etiqueta de tipo, llamadas virtuales y de cola, ayudantes de float): 110
      filas, 99 verificadas. Total: 10.818 funciones.
- [x] **Lote 174** (2026-10-28): funciones sueltas de diez instrucciones escritas a mano (llamadas que guardan a0 en
      s0, llamadas virtuales, rellenos con guarda, cocientes y restos, ayudantes de float): 135 filas, 127 verificadas.
      Total: 10.719 funciones.
- [x] **Lote 173** (2026-10-28): funciones sueltas de nueve instrucciones escritas a mano (llamadas que preparan
      argumentos o ajustan el resultado, llamadas de cola con guarda, rellenos y copias, ayudantes de float): 130
      filas, 121 verificadas. Total: 10.592 funciones.
- [x] **Lote 172** (2026-10-28): funciones sueltas de ocho instrucciones escritas a mano (llamadas que preparan un
      argumento o ajustan el resultado, llamadas de cola con guarda, recorridos de cadena, ayudantes de float): 110
      filas, 98 verificadas. Total: 10.471 funciones.
- [x] **Lote 171** (2026-10-28): funciones sueltas de siete instrucciones escritas a mano (llamadas que preparan un
      argumento, getters de float con condición, contadores y copias de vectores): 106 filas, 102 verificadas. Total:
      10.373 funciones.
- [x] **Lote 170** (2026-10-28): funciones sueltas de seis instrucciones escritas a mano (getters y setters, pruebas de
      bit, cargas con guarda y llamadas de cola): 111 filas, 105 verificadas. Total: 10.271 funciones.
- [x] **Lote 169** (2026-10-28): funciones sueltas de cinco instrucciones escritas a mano (getters y setters,
      campos de bits, contadores, pruebas y llamadas de cola): 103 filas, 98 verificadas. Total: 10.166 funciones.
- [x] **Lote 168** (2026-10-27): funciones sueltas de cuatro instrucciones escritas a mano (getters, setters, pruebas,
      ayudantes de float y llamadas de cola): 96 filas, 91 verificadas. **Se pasan las 10.000 funciones**: 10.068.
- [x] **Lotes 166-167** (2026-10-27): funciones sueltas de dos y tres instrucciones escritas a mano una a una
      (llamadas de cola que preparan un argumento, getters, setters y pruebas de bandera): 123 filas, 113 verificadas.
      Total: 9.977 funciones.
- [x] **Lote 165** (2026-10-27): catorce envoltorios pequeños más (direcciones de globales, llamadas de cola con
      argumentos intercambiados o leídos de campos, borrados de dos a cuatro palabras, almacenes y banderas): 67 filas,
      las 67 verificadas. Total: 9.864 funciones.
- [x] **Lote 164** (2026-10-27): formas pequeñas: miembros pendientes de siete macros del núcleo (`set_byte`,
      `clear_field`, `return_pair`, `ptr_set_float`, `ptr_set_field`, `set_two_fields`, `holder_virtual`) y nueve envoltorios
      nuevos de dos a cinco instrucciones (llamadas de cola con constantes, con `this` ajustado o con argumentos
      relativos; tablas, almacenes dobles): 76 filas, 67 verificadas. Total: 9.797 funciones.
- [x] **Lote 163** (2026-10-27): casos sintéticos tras cuatro rutas más (garaje con partida, circuito, compra y
      opciones): 36 filas comentadas quedan verificadas (16 + 10 + 6 + 4). `GT_SYNTH_ROUTE` acepta varias rutas a la
      vez, y los veredictos repasan con todas. Total: 9.730 funciones.
- [x] **Lote 162** (2026-10-26): casos sintéticos tras la carrera (`GT_SYNTH_ROUTE=race`). Tras el concesionario los
      nativos de script no tenían sus objetos de carrera y el original fallaba. De 431 filas comentadas, 49 comparan
      ahora y quedan verificadas. Total: 9.694 funciones.
- [x] **Lotes 160-161** (2026-10-26): más miembros de ocho familias ya escritas (lotes 67, 74, 75, 77, 99, 135 y
      `hot_leaves`) y `bound_node` (el nodo de llamada enlazada del lote 104, 67 miembros): 175 filas, 43 verificadas.
      Casi todos son nativos de scripts grandes que con entradas inventadas no dan casos comparables. Total: 9.645
      funciones.
- [x] **Lote 159** (2026-10-26): siete familias pequeñas con plantilla con nombre (`held_chain`, `call_with_ref`,
      `offset_call`, `copy_words`, `field_virtual_tail`, `constant_tail`, `member_assign`): 95 filas, 74 verificadas.
      Total: 9.602 funciones.
- [x] **Lote 158** (2026-10-26): más miembros de seis familias ya escritas (`return_constant`, `member_address`,
      `load_then_call`, `init_once` y dos de `hot_leaves`/lote 135): 98 filas, 86 verificadas. `family_rows` se salta
      los miembros que ya tienen cuerpo en `src/` con su registro comentado (había generado tres segundos cuerpos).
      Total: 9.528 funciones.
- [x] **Lote 157** (2026-10-26): dos familias grandes, `guarded_zero` (60 miembros más de una plantilla del lote 78) y
      `register_object` (un objeto con su vtable que se entrega a un registro, 63): 123 filas, las 123 verificadas.
      `family_rows` escribe bien los valores negativos (antes salía `0x-0006C94u`) y los enteros con signo (`-1`, no
      `4294967295`). Total: 9.442 funciones.
- [x] **Lotes 155-156** (2026-10-26): `field_into_frame`, `slot_present`, `float_name_assign`, `named_member_refresh`
      (un `std::string` COW en la pila entregado a un virtual y liberado) y dos familias de propiedades de script
      (`managed_byte_property`, `flagged_byte_property`: leen o escriben un byte del objeto según el modo): 103 filas,
      74 verificadas. Total: 9.319 funciones.
- [x] **Lotes 153-154** (2026-10-26): `deref_word` (dos desplazamientos), `held_object_call`, `virtual_on_second`,
      `name_assign`, `virtual_name_assign`, `ptmf_chain` (un puntero a método cuyo resultado recibe otro, en llamada de
      cola) y `guarded_init` (inicialización de un estático con guarda): 107 filas, 71 verificadas. Total: 9.245 funciones.
- [x] **Lote 152** (2026-10-26): diez familias cuya plantilla es una función con nombre (de RaceOperator, del
      organizador y de CarDriverParameter), escritas como envoltorios con parámetros (`deref_byte`, `deref_store_byte`,
      `deref_signed_byte`, `deref_byte_set`, `deref_swapped`, `deref_store_swapped`, `deref_plus_tail`,
      `deref_field_tail`, `deref_field_call`, `held_member_call`): 134 filas, 119 verificadas. Total: 9.174 funciones.
- [x] **Lote 151** (2026-10-26): más miembros de familias ya escritas en `src/core` (getters de campo, de float y de
      byte, `deref`, `field_plus`, `deref_float`, `call_return_self`, `clear_byte`) y de los casts de `as_class`:
      208 filas, 112 verificadas; los thunks, forwarders y destructores no tienen caso comparable (sus destinos fallan
      con entradas aleatorias en el original) y quedan comentados. `family_rows` lee argumentos
      `static_cast<std::uint32_t>(N)`. Total: 9.055 funciones.
- [x] **Lote 150** (2026-10-26): seis familias pequeñas más (`store_half`, `call_with_zero`, `fill_halves`,
      `build_then_apply`, `choose_tail`, `reset_then_base`): 31 filas, 30 verificadas. Total: 8.943 funciones.
- [x] **Lote 149** (2026-10-26): seis familias pequeñas más (`held_call`, `fields_call`, `virtual_float`,
      `forward_field`, `virtual_pass`, `byte_at`): 45 filas, 31 verificadas. Total: 8.913 funciones.
- [x] **Lote 148** (2026-10-26): cuatro familias más (`element_at`, `fill_handles`, `destroy_range`, `copy_range`):
      42 filas, 35 verificadas. `family_rows` acepta un argumento que ocupa varias posiciones que siempre coinciden
      (el paso de origen y destino de una copia). Total: 8.882 funciones.
- [x] **Lote 147** (2026-10-26): las 181 llamadas virtuales en línea de funciones cuyo asm solo usa `jalr $v0` pasan
      a `call_indirect` (un virtual vacío devuelve su propia dirección en el original).
- [x] **Lote 146** (2026-10-26): con los objetos vivos como entradas sintéticas (#304) se reactivan las filas de los
      lotes 138-145 que no tenían caso comparable: 332 verificadas (sabotaje 332 de 332). `pair_virtual` deja v0 como el
      original ante un virtual vacío, y `tools.synth_batch` ya no da por buena una fila con discrepancia ni una ejecución
      que no terminó. Total: 8.847 funciones.
- [x] **Lote 145** (2026-10-26): cuatro familias más (`pair_virtual`, `destroy_and_delete`, `new_handle`, `facet`): 54
      filas, 29 verificadas. `family_rows` resuelve ahora un argumento ambiguo con la única posición que cambia entre
      miembros. Total: 8.515 funciones.
- [x] **Lote 144** (2026-10-26): tres familias más con plantilla nueva (`handle_from`, `name_reset`,
      `release_and_destroy`), ya con `tools.synth_batch`: 73 filas, 63 verificadas (sabotaje 63 de 63), 10 sin caso
      comparable. 6 miembros con otro desplazamiento quedan fuera: `family_rows` los rechaza. Total: 8.486 funciones.
- [x] **Lote 143** (2026-10-26): cuatro familias más, tres con plantilla nueva (`tag_header`, `tag_and_construct`,
      `adjust_and_jump`, el thunk que ajusta `this` por la base virtual) y la de devolver el primer argumento. 56 filas:
      41 comparadas sin discrepancias, sabotaje 35 de 41. Total: 8.423 funciones.
- [x] **Lote 142** (2026-10-26): las primeras familias sin ningún miembro decompilado: se decompila la plantilla a mano
      como envoltorio con parámetros (`make_native_method`, el registro de un método nativo de Adhoc; `reset_list`;
      `reset_member`; `tag`) y el resto sale de `tools.family_rows`. 100 funciones verificadas con casos sintéticos
      (64 casos por función: con 8, el registro de nativos casi nunca daba un caso comparable; `synth --cases N`).
      Las que no tienen caso comparable o cuyo sabotaje no se detecta quedan comentadas. Total: 8.388 funciones.
- [x] **Lote 141** (2026-10-25): 121 miembros más de siete familias (`visit_property`, cadenas de llamadas y cinco
      envoltorios de los lotes 63, 75, 78 y 79), generados con `tools.family_rows`, que ahora también hace filas de
      envoltorio y rechaza tres casos que antes habrían dado filas falsas (FINDINGS). 252 filas: 136 comparadas sin
      discrepancias, sabotaje 121 de 136; las demás quedan con el registro comentado. Total: 8.288 funciones.
- [x] **Lote 140** (2026-10-25): 24 miembros más de dos familias de envoltorio (`register_names`, `handle_wrapper`),
      verificados con casos sintéticos (27 comparados, sabotaje 24 de 27: los 3 sin detectar quedan comentados). Cuatro
      familias más se revirtieron: su plantilla no era de la familia que usa ese envoltorio (FINDINGS). Los barridos
      sintéticos encontraron 5 fallos más en código ya verificado por rutas: los 6 indicadores de CarDriverParameter
      (`> 0`), `over_entry_max`, `func_08B212B4` (con signo donde el original no), y dos casos de registros sobrantes o
      lecturas adelantadas; quedan 7 sin explicar. Las entradas sintéticas ya no cambian con cada lote. Total: 8.167.
- [x] **Lotes 138 y 139** (2026-10-25): los primeros lotes de código que ninguna ruta alcanza, verificados con casos
      sintéticos. Son los demás miembros de familias de una sola forma cuya plantilla ya estaba decompilada: thunks,
      funciones vacías, devolver cero, reenvíos, cabeceras de destructor, llamar-y-devolver (lote 138, 523) y las
      familias con macro de registro (`AS_CLASS`, `CAST`, `DESTROY`, `DESTROY_MEMBER`, `MAKE`, `ASSIGN_MAKE`,
      `STRIDE_WALK`, `GETTER`, `CONSTRUCT`, `SET_FIELD`; lote 139, 252). Las filas salen de las palabras de cada
      miembro (`tools.family_members`, y `tools.family_rows` para colocar cada argumento como en la fila de la
      plantilla). 244 miembros sin ningún caso sintético comparable quedan comentados. Sabotaje sintético: 775 de 775.
      Corrigió `store_and_jump` (v0 = la tabla, como el original). Total: 8.143 funciones.
- [x] **Casos sintéticos** (2026-10-25, spec `2026-10-25-synthetic-cases-design.md`): como las rutas ya casi no
      alcanzan código sin decompilar, la misma puerta recibe casos generados: al final de la ruta `dealer`, cada función
      instalada se llama con argumentos y una zona de memoria inventados (punteros entre sí, enteros pequeños, flotantes,
      funciones triviales reales), cada función en un proceso hijo. Barrido completo de las 7.368 funciones: 75 s, 4.001
      con casos comparables, 3.995 iguales. Encontró un fallo real (los indicadores de RaceParameter usaban `> 0` en vez
      de `!= 0`) y varias derivas en caminos que ninguna ruta toma (registros sobrantes, lecturas adelantadas); quedan 6
      sin explicar (FINDINGS). `tools.decomp_gate synth` y `sabotage --synth`; el sabotaje por casos sintéticos queda
      probado en dos funciones. Siguiente: decompilar funciones que ninguna ruta alcanza y verificarlas así.
- [x] **Lote 137 y ruta `save_garage`** (2026-10-25): con el lote 136 las 16 rutas ya no alcanzaban nada de juego sin
      decompilar (solo el `longjmp` del SDK, la entrada del módulo y la de un hilo). Lo que falta se alcanza con rutas
      nuevas: `save_garage` recorre el garaje de la partida en sus cuatro vistas (últimos 100, últimos conducidos,
      favoritos, por marca), añade un coche a favoritos y abre la ficha y las especificaciones de otro. Entran 9 funciones:
      `MIconBox.setItemWidget`, `MCarCollection.getRecentlyRodeCars` / `getMyCarTypeCountEachMaker` / `listupFavorite` /
      `getOneCarMax`, `MSpecDB.getTunerLabel` y tres hojas. `getTunerLabel` difería en 6 de 6 casos hasta reservar su
      búfer antes de la llamada que da el nombre, como el original (corregido también en `getTunerCode`). Sabotaje 10 de
      10. Puerta completa: 17 rutas, 7.368 funciones, 5.711 comparadas (+30), 0 discrepancias; suelo subido a 5.711.
      Pendiente para una ruta de repeticiones: en las pruebas, el menú de pausa en carrera no movió el cursor con la
      cruceta (CROSS sí respondía); sin explicar todavía (¿lee el stick analógico?).
- [x] **Rutas desde una partida guardada** (2026-10-24, spec `2026-10-23-savedata-routes-design.md`): el HLE de
      `sceUtilitySavedata` descifra las partidas reales de PSP como el firmware (modo 5, con la clave que pasa el juego;
      `tools/savedata.py` es la referencia, `host/psp_savedata_crypto` la versión C++ sobre libcrypto). La partida del
      usuario (Día 25) es una entrada más, comprobada por hash y copiada limpia por ruta; cuatro rutas `save_*`
      (Trial Mountain, Driving Challenges, Drift Trial, concesionarios) alcanzan 95 funciones que las rutas de partida
      vacía no. Encontraron enseguida un fallo real: `func_0895C394` (lote 123) leía el tipo de tracción donde el original
      lee el byte `+0x12` (rama que solo toma un coche con tracción total). Puerta completa: 16 rutas, 5.662 comparadas,
      0 discrepancias.
- [x] **Ruta Trial Mountain y puerta más fiable** (2026-10-23): ruta 12 (`trialmtn`, carrera en Trial Mountain).
      Arreglos de la puerta: el margen tras el menú principal pasa de 1700 a 2300 vblanks en las 12 rutas (con carga
      se perdía la primera pulsación: exit 97 aleatorio), y la puerta rápida y la sonda ejecutan como mucho 6 rutas a la
      vez, como la completa (12 a la vez pasaban MemoryHigh y todas se atascaban). 14 funciones ya decompiladas que la
      puerta completa nunca comparaba quedan verificadas en la rápida (14 comparadas, 0 discrepancias, sabotaje 14/14).
- [x] **Lote 134** (2026-10-23): el registro de los resultados al acabar una carrera (los récords de cada modo) y
      la copia de un puntero compartido de la biblioteca 0x08Bxxxxx con sus cuentas de uso (cuatro subidas cuando el
      dueño tiene bloque de control). Aparcados el cargador de archivos PAC y una función nativa de script, que las
      rutas no comparan. Sabotajes 2 de 2. **7.291 funciones, 5.631 comparadas, 0 discrepancias (puerta final).**
- [x] **Lote 133** (2026-10-23): el envío de eventos del juego a los scripts (switch de 40 tipos que convierte cada
      campo en un valor de script: bytes, palabras, 64 bits, flotantes y cadenas), la búsqueda de vecinos de un nodo
      del trazado (mismo grupo: anterior y siguiente; otros grupos: los extremos respecto al centro, con márgenes y
      filtro de ángulo), el desempaquetado de siete secciones de un perfil y 19 de los 30 analizadores de secciones que
      llama la carga de perfiles del lote 131 (copias, escalados por porcentaje con las divisiones por multiplicación
      del compilador, conversiones flotante→entero sin signo). Aparcados 11 analizadores de secciones opcionales que
      las rutas no cargan. Sabotajes 22 de 22. **7.289 funciones, 5.635 comparadas, 0 discrepancias (puerta final;
      dos pasadas previas perdieron una ruta cada una por un WAIT con la máquina cargada).**
- [x] **Lote 132** (2026-10-23): registros de clases de script que el juego hace al arrancar, generados desde el
      asm: cuatro tablas de métodos (86, 16, 13 y 11 + 7 con una constante entera), cinco definiciones de clase con
      sus atributos (19, 25, 5, 8 y 7: objetos tipados con su tabla virtual, valores simples y «cajas» encadenadas) a
      través de un `ClassBuilder` común, y siete clases seguidas con su tabla de métodos. Se recupera `MUnit` (lote 127,
      aparcada): la puerta rápida sí la compara. Sabotajes 11 de 11. **7.267 funciones, 5.632 comparadas, 0
      discrepancias (puerta final).**
- [x] **Lote 131** (2026-10-22): siete funciones grandes. Los avisos de cada fotograma de la carrera (860
      instrucciones: diez pasadas sobre los coches que reúnen claves y avisan al oyente por sus ranuras virtuales:
      vueltas, meta, tiempo límite), el lector de valores de script desde un flujo de bytes (tabla de saltos de 16
      tipos y registros recursivos), el volcado de las secciones de un perfil (27 secciones por desplazamientos de 64
      bits), dos tablas de métodos de clases de script (30 y 32), una de 61 miembros (métodos y propiedades) y el
      constructor/destructor estático de una unidad con 94 símbolos globales. Aparcadas tres sin casos comparables
      (contactos de colisión, la selección de un `partial_sort` y la apertura de un vídeo PSMF). Sabotajes 7 de 7.
      **7.256 funciones, 5.631 comparadas, 0 discrepancias (puerta final).**
- [x] **Lote 130** (2026-10-22): el cargador de bytecode de una función de script (542 instrucciones): versión del
      flujo, nombre, parámetros y capturas con sus índices, campos del código según la versión (8, 9, 11, 12) y cada
      instrucción creada por su opcode (el despachador de 67 entradas del lote 122). Comparada en las 11 rutas.
      Sabotajes 1 de 1. **7.249 funciones, 5.630 comparadas, 0 discrepancias (puerta final).**
- [x] **Lote 129** (2026-10-22): la llamada a función de la máquina de scripts (518 instrucciones): marco de llamada
      con el estado guardado, locales nuevas, argumentos a parámetros con valores por defecto, variádicos en un array,
      `self` y capturas, y el marco apilado antes de ejecutarlo. Comparada en las 11 rutas. Sabotajes 1 de 1.
      **7.248 funciones, 5.631 comparadas, 0 discrepancias (puerta final).**
- [x] **Lote 128** (2026-10-22): la actualización de un frame de carrera (525 instrucciones, la más llamada del pool):
      reloj de 64 bits, sesión en red, subpasos de física con su tiempo en ms para el mundo, la cámara y el sonido,
      cada coche y su piloto, visores de repetición, estado de la carrera y el contador de frames con su límite.
      Comparada en las 11 rutas, 0 discrepancias. Sabotajes 1 de 1.
      **7.247 funciones, 5.629 comparadas, 0 discrepancias (puerta final).**
- [x] **Lote 127** (2026-10-22): 2 funciones (teclas de una vista de lista, con paso doble al mantener la tecla, y
      la propiedad de script de la condición de juego "MGameCondition": un tipo y tres enteros, lectura y escritura).
      1 aparcada (el registro de los 28 métodos del módulo "MUnit": ninguna ruta lo compara). Sabotajes 2 de 2.
      **7.246 funciones, 5.673 comparadas, 0 discrepancias (puerta final).**
- [x] **Lote 126** (2026-10-22): 2 funciones de la interfaz (recolector recursivo de las entradas del foco, que
      recorta sus rectángulos al recorte activo, y polilínea gruesa con tapas al principio y al final). 1 aparcada (la
      curva de par del motor: ninguna ruta la compara). Una discrepancia de v0 corregida: la llamada final por la tabla
      virtual no toca v0, que queda con la dirección de la función como en el original. Sabotajes 2 de 2.
      **7.244 funciones, 5.682 comparadas, 0 discrepancias (puerta final).**
- [x] **Lote 125** (2026-10-22): 7 funciones de la interfaz y de la ordenación del foco (reconstrucción de los ocho
      valores de una vista a partir de sus nombres, teclas de una vista con el foco —códigos con valores de keysym de
      X11—, movimiento del foco a la entrada siguiente o anterior, y cuatro piezas de la ordenación de libstdc++
      instanciadas con ese comparador de 0x4C bytes: `__introsort_loop`, `__final_insertion_sort`, `__insertion_sort`,
      `__unguarded_linear_insert`). 2 aparcadas (la lista de pistas de música y la partición: ninguna ruta las
      compara). `func_08BF5B28` se quitó del lote: ya estaba registrada como recorrido de zancada y el arranque rechazó
      el doble registro. Sabotajes 7 de 7. **7.242 funciones, 5.662 comparadas, 0 discrepancias (puerta final).**
- [x] **Lote 124** (2026-10-22): 5 funciones de 400 a 700 instrucciones de la interfaz (lector de atributos de color
      de un estilo, rectángulo redondeado, maquetado de un tiempo de vuelta en glifos, renderizado de números, paso de una
      vista con desplazamiento). 0 aparcadas. Dos discrepancias corregidas leyendo mejor el asm: un bucle en el que el
      compilador entra con `j` a su mitad descarta el primer token, y un `beq` tras un `jal` compara la respuesta de la
      llamada, no una constante (ver FINDINGS). Sabotajes 5 de 5. Una ruta de la primera puerta final se colgó (exit 97)
      con la máquina cargada; la repetición pasó entera.
      **7.235 funciones, 5.662 comparadas, 0 discrepancias (puerta final).**
- [x] **Lote 123** (2026-10-22): 10 funciones de 400 a 1.300 instrucciones (paso de física del mundo, diferencial
      con autoblocante, embrague y fuerzas de los neumáticos, temblor y cámara de repetición, render del coche con sombra,
      maquetado de cajas de la interfaz, inserción con pista en un árbol de valores de script, copia de ajustes, división
      de 64 bits con signo). 1 aparcada (resto de 64 bits sin signo: ninguna ruta la llama). Cuatro discrepancias
      corregidas: dos v0 y dos llamadas que leen registros que el original había cargado antes (f12 y a1, ver FINDINGS).
      Sabotajes 10 de 10. La puerta corre ahora 6 rutas a la vez (cada una ocupa ~3 núcleos; con 8 se perdían rutas).
      **7.230 funciones, 5.659 comparadas, 0 discrepancias (puerta final).**
- [x] **Lote 122** (2026-10-22): 8 funciones de 360 a 680 instrucciones del coche y la cámara (test de segmento contra
      triángulos de colisión, suspensión, ayuda de conducción, control de tracción, ABS y fuerza de tracción, movimiento
      de la carrocería, factores de tracción y freno por rueda, despachador de 67 entradas de los scripts). Una
      discrepancia de v0 (la llamada final no toca v0: cada salida deja el del original); 0 aparcadas. Sabotajes 8 de 8.
      Tres puertas finales perdieron una ruta cada una (ver FINDINGS); la cuarta, limpia.
      **7.220 funciones, 5.626 comparadas, 0 discrepancias (puerta final).**
- [x] **Lote 121** (2026-10-22): 6 funciones de alto uso en las rutas (div(), división de 64 bits sin signo,
      envoltorio SVID de powf, lectura con búfer de un flujo, intérprete de bits y valores, mezclador de voces). 1
      aparcada (la lectura con búfer: sus casos tocan imports no comparables). Sabotajes 5 de 5. Una ruta de la puerta
      final se atascó por carga ajena (un test de otra sesión sin `nice`); repetida, limpia.
      **7.212 funciones, 5.642 comparadas, 0 discrepancias (puerta final).**
- [x] **Lote 120** (2026-10-22): 6 funciones de 320 a 380 instrucciones escogidas por llamadas en las rutas (matriz
      de balanceo de la cámara, tabla ordenada de un script, partículas, rig de cámara con tres interpolaciones, reparto de
      tracción de las cuatro ruedas, banderas de un registro). Una discrepancia de v0 (los setters de float conservan el
      v0 del original); 0 aparcadas. Sabotajes 6 de 6. Abierto: `func_0899CB44` (lote 95) difiere 3 bytes en
      `0x09C7AC97` en algunas puertas (3 de 9), sin funciones decompiladas anidadas en el caso.
      **7.207 funciones, 5.617 comparadas, 0 discrepancias (puerta final).**
- [x] **Lote 119** (2026-10-22): 3 funciones de 134 a 346 instrucciones (historial de posiciones con VFPU, matriz de
      balanceo de la cámara interior, actualización de la cámara de repetición); otras 5 escritas quedan aparcadas
      (constructores y registros de módulos que corren una vez: el gate no compara ningún caso suyo). Una discrepancia de
      v0: `func_08967530` solo devuelve f0, así que v0 conserva el desplazamiento calculado antes. Las rutas en tiempo
      real se colgaban por las pruebas de otra sesión aun con `nice 19`: el slice del gate tiene ahora
      `CPUWeight=10000` y la puerta final pasó con carga 27-36. Sabotajes 3 de 3 con `--full`.
      **7.201 funciones, 5.618 comparadas, 0 discrepancias.**
- [x] **Lotes 115-118** (2026-10-22): 61 funciones de 185 a 315 instrucciones (inserciones en vectores, asignadores,
      libm `expm1f`, multiplicación de dobles por software, recorridos de escena), escritas por cuatro agentes y revisadas
      contra el asm (115-116 por un revisor aparte: un `erase` sin a2; 117-118 solo por sus autores). 37 quedan aparcadas
      (sin casos comparables) y 2 más por discrepar: `func_08867268` (la rama de +0x9C leída al revés) y
      `func_08BA996C` (v0 del camino de inserción in situ). Dos puertas perdieron una ruta por carga ajena (`cargo` de
      otra sesión a carga 30): desde entonces las tareas en tiempo real cogen el cerrojo de CPU compartido
      (`tools/cpu_lock.sh`). Sabotajes 14/14, 13/13, 15/15, 19/19 con `--full`.
      **7.198 funciones, 5.633 comparadas, 0 discrepancias.**
- [x] **Lotes 113-114** (2026-10-21): 34 funciones de 141 a 185 instrucciones escritas por dos agentes en paralelo y
      revisadas por un tercero contra el asm (7 fallos corregidos antes de la puerta: seis copiaban en el hueco de retardo
      de un `jal` el resultado de la llamada siguiente en vez de la anterior; uno no pasaba a2 a un `erase`). 16 quedan
      aparcadas. La primera puerta perdió la ruta `challenge` con la máquina a carga 27 (sola pasa); una puerta final dio
      una discrepancia de 3 bytes en `func_0899CB44` (lote 95) que no se repitió ni sola ni con el lote (dos rutas
      dirigidas y la puerta siguiente). Sabotajes 16 de 16 y 18 de 18 con `--full`.
      **7.137 funciones, 5.644 comparadas, 0 discrepancias.**
- [x] **Lote 112** (2026-10-21): 36 funciones de 81 a 140 instrucciones (inserciones en vectores, envoltorios SVID de
      libm, conversión de 64 bits a float, árboles de cadenas, recorridos de listas de dibujo, separadores de miles). 22
      quedan aparcadas: el gate completo no compara ningún caso suyo. Cuatro dejaban v0 distinto: el crecimiento de un
      vector en línea termina con v0 = bloque + bytes; corregido en las cuatro copias. Sabotajes 36 de 36 con `--full`.
      **7.103 funciones, 5.645 comparadas, 0 discrepancias.**
- [x] **Lote 111** (2026-10-21): 52 funciones de 61 a 100 instrucciones (rutas de ficheros, contadores de cartas de
      carrera, listas de la biblioteca estándar, envoltorios de libm). 25 quedan aparcadas: el gate completo no compara
      ningún caso suyo. Seis dejaban v0 distinto en salidas tempranas (el valor que leyó su última prueba); corregido.
      La ruta de opciones se colgó en tres puertas seguidas también en reposo: las 11 rutas a ~0,5 GB cada una pasaban el
      `MemoryHigh` del slice; la puerta completa limita ahora a 8 rutas a la vez (`MAX_PARALLEL_ROUTES`, FINDINGS) y
      tarda 12 min en vez de 22. 60 funciones ya verificadas dejan de contarse aparte (corren dentro de los casos de
      sus nuevos padres registrados, como en el lote 86). Sabotajes 52 de 52 con `--full`.
      **7.067 funciones, 5.627 comparadas, 0 discrepancias.**
- [x] **Lote 110** (2026-10-21): 29 funciones de 41 a 60 instrucciones (formato de tiempos, iteradores de listas
      enlazadas, envoltorios de libm, copias de registros). 14 quedan aparcadas. Dos funciones dejaban v0 distinto al
      final (la última carga del asm); corregido. Dos puertas completas perdieron una ruta cada una por una espera de
      lectura de disco en los menús (`WAIT seek … timed out`); la repetición en reposo pasó entera.
      **7.015 funciones, 5.628 comparadas, 0 discrepancias.**
- [x] **Lote 109** (2026-10-21): 57 funciones de hasta 40 instrucciones que alcanzan las 11 rutas (objetos de valor de los
      scripts, envoltorios bajo cerrojo, listas y vectores de la biblioteca estándar, lectores de bits). 26 quedan aparcadas:
      el gate completo no compara ningún caso suyo. Sabotajes 57 de 57 (35 en la puerta rápida, 22 con `--full`).
      **6.986 funciones, 5.612 comparadas, 0 discrepancias.**
- [x] **Lote 108** (2026-10-21): 33 funciones de 15 a 22 instrucciones que alcanzan las 11 rutas (llamadas virtuales,
      liberación de objetos compartidos, envoltorios con marco). 3 quedan aparcadas: el gate completo no compara ningún
      caso suyo. Sabotajes 30 de 30 (19 en la puerta rápida, 11 con `--full`).
      **6.929 funciones, 5.586 comparadas, 0 discrepancias.**
- [x] **Lote 107** (2026-10-21): 67 funciones pequeñas que solo alcanzan las 11 rutas (accesores, envoltorios de llamada,
      reenvíos virtuales, copias de vectores, intercambios de bytes) y las 15 aparcadas del lote 106, verificadas con el
      nuevo `sabotage --full` (rutas completas). 14 más quedan aparcadas: el gate completo no compara ningún caso suyo.
      **6.899 funciones, 5.556 comparadas, 0 discrepancias.**
- [x] **Lote 106** (2026-10-21): el resto de la librería gráfica (la caché de modelos, los emisores de vértices con UV y
      colores — `func_08B53D2C`, la más llamada — y el despachador de luces), las 6 hojas VFPU alcanzadas en el resto del
      juego y 20 accesores triviales que solo alcanzan las rutas nuevas (familias de `src/core`, con `field_plus`,
      `deref_float` y `set_byte` nuevas). 15 más quedan aparcadas: solo las alcanzan las rutas completas, así que su
      sabotaje no se puede fijar. **6.817 funciones, 5.481 comparadas, 0 discrepancias.**
- [x] **Lote 105 — la librería gráfica y la VFPU** (2026-10-21): 36 funciones de la zona 0x08B40000–0x08B70000 (matrices,
      rotaciones, seno/coseno, transformaciones, test de volumen de vista, comandos GE), 29 con VFPU. Primeras funciones VFPU
      del proyecto: `include/gt/vfpu.hpp` da cada operación VFPU con la semántica exacta del recompilador, y el harness compara
      la VFPU (#258). Rápida 36/36, sabotaje 36/36; completa: **6.780 funciones, 5.437 comparadas, 0 discrepancias**.
- [x] **Lote 104** (2026-10-20): 22 funciones nuevas (19 registradas) y 18 aparcadas que vuelven: sus bloqueos (imports de
      `sceSasCore`, rutas que no las alcanzaban) ya no existen. Tres imports más: `sceDisplaySetFrameBuf` grabable (máscara `0x0F`)
      y dos constantes reproducibles (`sceHprmIsHeadphoneExist`, una llamada de `sceFont`). Puerta rápida 33/33 comparadas, sabotaje
      33/33; puerta completa con las 11 rutas: **6.744 funciones, 5.403 comparadas, 0 discrepancias** (suelo 5.403).

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
- [x] Fotogramas atados a la pantalla (2026-10-22): los vblank siguen los refrescos reales (VK_KHR_present_wait,
      `host/display_phase.cpp`); el audio absorbe el 0,18 % de más (parche 0055). Los tirones del portátil eran la luz
      nocturna de KDE (FINDINGS).
- [x] Colinas traslúcidas en Trial Mountain (2026-10-23): los factores de mezcla "doble alfa" de la GE se calculaban
      mal en los dos renderizadores (2(1 - A) en vez de 1 - 2A); con el alfa 0x80 de GT las capas del fondo se sumaban
      al cielo. Parche 0056, test `ge_blend` (FINDINGS).
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

- [ ] **Sin emulación de ningún tipo** (decidido por el usuario el 2026-10-15; AGENTS.md § End state). Spec aprobado el
      2026-10-19: `docs/superpowers/specs/2026-10-19-no-emulation-design.md` (orden 1 → rutas deterministas → 2 → 3 → 4,
      planificador cooperativo primero, código del SDK de Sony sustituido por equivalentes del sistema). Por orden, con el gate verde en cada paso:
  - [ ] Sin parte recompilada: 100 % fuente decompilada.
  - [ ] Sin memoria emulada: las direcciones de la PSP pasan a tipos de C++ (structs, punteros, clases), por subsistema.
  - [ ] Sin interfaces de la PSP: el renderizado del juego llama a la API gráfica del sistema en vez de construir listas de
        la GE, y las bibliotecas de Sony (HLE) pasan a ser la capa de plataforma, como en un port de PC.
- [ ] **Mods**: cambios sobre la fuente decompilada (lógica, coches, circuitos, UI) sin tocar el
      binario original; carga de datos modificados de `GT.VOL` con las herramientas de Nenkai.
- [ ] Otras plataformas.
- [ ] Pista de matching opcional si aparecen los compiladores exactos.
