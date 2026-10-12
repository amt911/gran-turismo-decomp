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
