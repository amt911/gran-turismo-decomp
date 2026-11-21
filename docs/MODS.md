# Mods nativos para `gt_play`

Un **mod** es una biblioteca compartida (`.so`) más un `mod.toml`, cargada por `gt_play` al arrancar. El mod engancha
**funciones del juego por dirección** (la dirección de ejecución PSP = vaddr del ELF + `0x08804000`), lee y escribe la memoria
del invitado y puede llamar a la función original. Solo `gt_play` carga mods; `gt_native`, la puerta diferencial, `--decomp diff` y
los casos sintéticos nunca los cargan. Los mods del autor viven en el repo privado `gt-mods`
(`analog-pedals`: gatillos/teclas analógicos; `orbit-camera`: cámara orbital con el stick derecho, estilo GT7; `draw-distance`: plano lejano de la carrera x `far_scale`, comprobación `tools/mod_drawdist_check.sh`; `reset-to-track`: un botón devuelve el coche del jugador a la pista; `rollover`: el coche puede volcar, comprobación `tools/mod_rollover_check.sh`; `cheats`: cámara lenta y IA congelada en carrera, filas en el menú F9, comprobación `tools/mod_cheats_check.sh`; ver su README).

## Dónde y cómo se carga

- Directorio: `<build>/mods/` junto a `gt_play`, o el de `GT_MODS_DIR`. Cada mod es una subcarpeta con `mod.toml` y su biblioteca.
- `gt_play` imprime `[mods] loaded: ... skipped: [...]` si el directorio tiene algo. Un mod se omite (con motivo) si falta
  `mod.toml`, no se puede leer, `api` no es la del host, no abre la biblioteca, falta `gt_mod_init`, su init
  devuelve distinto de 0 o repite el `id` de otra carpeta (o engancha dos veces la misma dirección).
- **Varios mods en la misma dirección se encadenan** (desde 2026-11-07; antes paraba el arranque): el primero cargado es el de
  fuera, su `call_original` ejecuta el siguiente mod activo y el último, la función del juego; un mod apagado se salta. Todos los
  mods deben funcionar juntos: `tools/mods_together_check.sh` los instala todos activados y recorre la ruta `chase`.
- Un mod con `enabled = false` **se carga e inicializa igualmente pero queda inactivo**: sus ganchos pasan directo a la función
  original y ningún código del mod se ejecuta (así se puede activar desde el menú sin instalar ganchos desde otro hilo). Su
  `gt_mod_init` sí corre (puede abrir un mando, leer ajustes). En el log sigue saliendo como `skipped [dir: disabled]`.
- `GT_NO_MODS=1` desactiva todo (los scripts de verificación lo ponen: `gpu_gate.sh`, `ge_profile.sh`, `gpu_perf.sh`, `pgo.sh`).

## Menú de mods en el juego (F9)

En una ventana de `gt_play`, **F9** abre la página **MODS** del panel de ajustes del host (la de F10 no cambia). Una fila por mod
con su nombre y `ON` / `OFF`; los que no se pudieron cargar salen como `ERROR` con el motivo recortado y no se pueden activar.
Arriba/Abajo eligen, Izquierda/Derecha/Intro cambian, F9 o Escape cierran (la página enseña 5 filas y se desplaza). El cambio vale
**al instante**: un mod apagado deja pasar sus ganchos a la función original (cada llamada lee el estado una sola vez al entrar,
así que un cambio a mitad de llamada no la deja a medias) y se guarda en su `mod.toml` (`enabled = true|false`, respetando
comentarios y demás claves). Si no se puede escribir el fichero, la fila muestra `SAVE FAILED` pero el cambio sigue aplicado.
Sin mods instalados dice `NO MODS INSTALLED`. Con `GT_NO_MODS=1`, `--decomp diff` o casos sintéticos no hay mods que mostrar.
Teclado y mando (ver «Menú con el mando»). No se editan los ajustes del mod ni se descargan bibliotecas en caliente. Un mod puede añadir sus propias filas (API v2).

Un mod con `enabled = false` **se carga igualmente** (su `gt_mod_init` se ejecuta) y queda inactivo, para poder encenderlo
sin reiniciar. Los activados se cargan primero (son los eslabones de fuera de cada cadena). El
`mod.toml` se escribe en `mod.toml.tmp` y se renombra encima, así un corte nunca lo deja a medias.

Para verificar sin pulsar teclas: `GT_MOD_MENU_TEST_TOGGLE=<id>@<segundos>` hace que un hilo de `gt_play` llame al mismo `toggle`
del menú pasado ese tiempo (`[mods] <id> -> OFF`).

## Cheat device (SELECT+START / F7)

Desde 2026-11-12 (spec `docs/superpowers/specs/2026-11-11-cheat-device-design.md`, aprobada) **SELECT+START** (mando) y **F7**
(teclado) abren el **cheat device**: un menú por categorías (Race, Car, Money & garage, Camera, **Mods**, **Controls**, Settings; las
vacías no salen). Se abre en la **lista de categorías**, con el cursor en Race (desde 2026-11-16; antes la primera vez caía en **Mods**, la lista de mods ON/OFF y sus filas de API v2, y los trucos de **Race** no se veían); luego vuelve a la página y fila del último cierre. Arriba/Abajo eligen,
Izquierda/Derecha/Intro (Cruz) cambian o entran, Escape/Retroceso/Círculo vuelven atrás (en la raíz cierran). Se ven 8 filas.

**Panel propio, aparte de F9** (2026-11-13, spec `docs/superpowers/specs/2026-11-13-cheat-device-panel-design.md`, parche lcs 0068):
el device es un panel del host con su aspecto (marco y cabecera ámbar con la ruta, fila elegida sobre barra azul, pie con
`X OK   O BACK   <> CHANGE`), dibujado arriba a la derecha por el mismo camino que el overlay (SDL, Vulkan, DX12). **No toca la
entrada**: el mando real sigue llegando al filtro del host, que se lo oculta al juego; el teclado deja de ir al juego mientras está
abierto y las flechas/Intro/Escape mueven el panel. Antes era una página del overlay F9, y lcs pone el mando a cero con el overlay
abierto: con un mando real (Xbox One por BT) el device se abría con SELECT+START pero no se podía mover ni cerrar (las rutas de
prueba inyectan el mando por debajo de lcs y no lo vieron; docs/FINDINGS.md). **F9** vuelve a ser la página MODS de siempre (mods,
actualizar, descargar) y **F10** los ajustes del host; abrir uno cierra el panel y viceversa. F8 sigue siendo la traza GE de lcs.

**Fluidez y tamaño** (2026-11-16, parche lcs 0070): la imagen del menú se dibuja en **su propio hilo**, nunca en el que presenta
(redibujarla entera costaba ~13 ms en el sobremesa y paraba el juego en cada pulsación: "va lento al moverse por las opciones");
se sube a la GPU **solo cuando cambia** (antes, 4 MB por fotograma con el menú abierto); y se dibuja **al tamaño de la pantalla**
(40 % del ancho, como mucho el 80 % del alto: nítido en 1080p y en 4K). El dibujo es además ~2x más rápido, píxel a píxel igual.

**Aspecto nuevo** (2026-11-13, parche lcs 0069): el panel, **F9 (MODS)** y **F10 (ajustes)** comparten marco, colores y letra:
cuerpo azul marino casi negro con borde fino y esquinas redondeadas, **azul** del juego (subrayado de la cabecera, última parte de
la ruta, fila elegida con barra de 4 px) y **rojo** para los rechazos (caja "Refused: …" 3 s bajo la cabecera) y el icono Círculo; píldoras
ON verde / OFF gris / UPDATE-INSTALL ámbar / ERROR rojo; texto en **Barlow** (SIL OFL; `tools/setup.sh` la baja a `tools/src/fonts`,
con stb_truetype en `tools/src/stb`; sin la fuente vuelve al 5x7 y lo dice una vez en el log). Las filas son datos (`ui_row.hpp`):
ON/OFF = interruptor, `NN%` = barra, `‹ valor ›` = elección (flechas azules en la fila elegida), acción = chip con su atajo
(`L + Triangle`, `Ctrl + N`), `›` = submenú; las filas de texto de los mods v4 se clasifican por su texto, sin cambiar los mods. Cabecera:
`CHEAT DEVICE`, ruta `Cheats › Race` y `3 / 6`; pie: botones del mando dibujados (Select / Back / Change) y la combinación de abrir
(`SELECT + START · F7`); al capturar: "Press the new combination" y "hold to cancel". F9 y F10 llevan una tira `SETTINGS  MODS`.
Se rasteriza al doble (1040x1024) y se presenta arriba a la derecha al 40 % del ancho (mínimo 520). `build/ui_panel_dump <dir>` (o
`ctest -R ui_panel_dump`) escribe los PNG del panel (solo texto de la interfaz).

- **Controls**: una página por acción con *Keyboard*, *Pad*, *Clear* y *Reset to default*. Intro en *Keyboard*/*Pad* captura la
  próxima tecla / combinación (la unión de los botones pulsados, hecha al soltarlos; un botón solo mantenido 30 vblanks es "mantener";
  Círculo mantenido 30 vblanks cancela). **Un atajo ya usado se rechaza** y el motivo sale 3 s en la cabecera (nunca "gana el último");
  un botón de carrera solo, sin otro ni tiempo de mantener, también se rechaza. `device.open` (F7 / SELECT+START) no puede quedarse
  sin ninguno.
- **`bindings.toml`** junto a la carpeta `mods/`: solo los atajos distintos del defecto (`<acción>.key = "Ctrl+N"`,
  `<acción>.pad = "L+TRIANGLE"`, `"SELECT@30"` = mantener 30 vblanks); se escribe atómico (tmp + rename). Las líneas de mods que hoy
  no están cargados se conservan.
- **Lo que ve el juego**: los botones de cualquier atajo del mando se retienen como los del acorde (hasta 8 vblanks; un atajo de
  mantener, su tiempo): si el atajo se completa el juego no los ve; si no, los recibe tarde con su duración real. Las teclas nunca
  llegan al juego.
- Sin ningún mod de API ≥ 2 cargado el host no lee el mando ni el teclado (identidad con el juego intacto) y no hay panel: los
  mods se encienden y apagan desde F9 (página MODS).

### API v4 (acciones y entradas por categoría)

Un mod que declara `api = 4` en su `mod.toml` tiene además, solo durante `gt_mod_init`:

- **API 6** (2026-11-16) `action_held(self, id)`: 1 mientras la combinación de la acción `id` del mod está pulsada (todos sus botones del mando, o su tecla con sus modificadores); el nitro de `cheats` 0.6.0 dura lo que el botón. **Los atajos del mando solo actúan en carrera** (un paso del mundo en los últimos 30 vblanks): en los menús sus botones llegan al juego tal cual (con Arriba/Abajo asignados, los menús iban lentos).
- `add_action(self, id, label, category, fn, default_key, default_pad)`: una acción `<mod>.<id>` en `category` (`Race`, `Car`,
  `Money & garage`, `Camera`, `Settings`), con atajos por defecto en la sintaxis de `bindings.toml` (`""` = ninguno). Intro en el menú
  o su atajo la ponen en cola y `fn(self, ctx)` corre en el siguiente tic de vblank (como un tic: `call_guest` permitido), una vez por
  vblank, nunca con el mod apagado. Un defecto que otra acción ya tiene no es error: la acción queda sin ese atajo y el log nombra al dueño.
- `add_entry(self, category, id, label, activate)`: una fila (mismo contrato que `add_menu_row`) en una categoría en vez de en Mods.

## API v2 (filas de menú, tic por vblank, `call_guest`)

`include/gt_mod/gt_mod_api.h`. La tabla v2 **añade campos al final** de `GtModApi`; el prefijo v1 no cambia. `GT_MOD_API_VERSION` sigue valiendo 1 (la
versión base) y se añade `GT_MOD_API_VERSION_2`. El cargador acepta `api = 1` o `api = 2` en el `mod.toml` y entrega a cada mod una tabla cuyo `version`
es lo que declaró: un mod v1 (binario o recompilado) sigue comprobando `api->version != GT_MOD_API_VERSION` y carga igual; los cinco mods anteriores
no cambian. Las funciones v2 **rechazan** (devuelven distinto de 0 y dicen por qué en stderr) a un mod que declaró `api = 1`.
- `add_menu_row(self, id, label_fn, activate_fn)`: una fila en la página MODS tras las de los mods. Solo durante `gt_mod_init`. `label_fn(self, out, capacidad)`
  escribe el texto; `activate_fn(self, dirección)` recibe Intro (0), Izquierda (-1) o Derecha (+1). Se rechaza un `id` vacío, de más de 31 caracteres, funciones
  nulas o el mismo `id` dos veces **en el mismo mod** (otro mod puede usar el mismo `id`: las filas se ordenan por orden de carga y conviven, sin «gana el último»).
  Las dos funciones las llama el host desde otros hilos y varios a la vez (teclado en el bombeo de SDL, mando en una lectura del juego, la etiqueta al
  dibujar): estado en atómicos, nunca tocar memoria del invitado ni llamar al juego desde ellas. La fila de un mod apagado dice `<id> mod OFF` y no hace nada.
- `on_vblank(self, fn)`: un tic por mod (solo en `gt_mod_init`), una vez por vblank del juego (`host/gt_kernel.cpp`, el primer `sceDisplayWaitVblank*` que ve cada
  vblank, **en el hilo del invitado que llegue primero**, no en un hilo «principal»). El tic de un mod apagado se salta.
- `call_guest(ctx, dirección, args, n, &v0)`: llama a una función del juego (hasta 8 argumentos enteros). **Solo dentro del tic**: desde un gancho, con el
  runtime parado o con argumentos malos devuelve distinto de 0 y no ejecuta nada (`host/tests/mod_services_test.cpp`). La función no puede bloquear (un cambio de hilo
  fuera de fibra para el runtime) y corre en la pila y con una copia del contexto del hilo del tic. **Qué está parado y qué no:** los hilos del invitado se planifican de forma
  cooperativa en un solo hilo del SO, así que ninguno corre durante el tic; pero uno aparcado (en un cambio de hilo, una llamada HLE) puede estar **a mitad de una función**, también de una
  decompilada (su fibra queda suspendida ahí), y el hilo del GE es un hilo real del SO que sigue leyendo memoria del invitado. Por eso: getters, o setters seguros entre dos instrucciones
  cualesquiera del propio código del juego. **Probado en el juego:** el tic de `cheats` llama al lector de la bandera de vuelco (`0x0894CCB8`, solo lee) y la comprobación exige `status 0, v0 0`;
  ningún setter se ha probado todavía (la pieza 2 será la primera).
- `is_menu_open(self)`: distinto de 0 mientras el cheat device (panel) o el overlay F9/F10 del host está abierto (cualquier versión de api). `vblank_index(self)`: el contador de vblanks del juego en el último tic
  (se actualiza aunque el mod esté apagado): sirve para saber que un estado quedó viejo sin mirar el reloj de pared (que va a otro ritmo que el tiempo virtual del juego).

## Menú con el mando

`host/pad_menu.cpp` (lógica pura, `host/tests/pad_menu_test.cpp`) filtra la lectura del mando del juego (`sceCtrlReadBufferPositive`, `host/host_hooks.hpp`). **Cuándo se instala:** el filtro y el tic
solo se instalan en `gt_play` cuando algún mod cargado (activo o apagado: se enciende con el propio acorde) registró filas de menú o un tic (API v2). Sin ninguno (sin mods, `GT_NO_MODS`, solo mods de API 1) el mando y el
vblank son los del juego, sin tocar, y F9/F10 siguen como antes. **Acorde: SELECT+START** (variable `GT_PAD_MENU_CHORD`, p. ej. `L+R`): al completarse abre o cierra la página
MODS; arriba/abajo mueven (con repetición a los 18 y cada 4 vblanks), cruz = Intro, izquierda/derecha cambian el valor, círculo cierra. **Con el filtro instalado, el overlay abierto (también el F10, y también si se abrió con el teclado)
oculta el mando al juego**: ni botones ni stick; antes, con F9 abierto, el coche seguía recibiendo el mando. Los mods que inyectan entrada propia deben mirar `is_menu_open` (`analog-pedals` ya lo hace y deja de conducir con el menú abierto). Un botón que se pulsa con el menú abierto y los del acorde siguen ocultos hasta soltarlos
(ni un Start que pause, ni un círculo suelto al cerrar); uno que ya estaba pulsado al abrir (el acelerador) vuelve a verse al cerrar.
**Retención del acorde (nadie pulsa dos botones en el mismo vblank):** un botón del acorde pulsado solo se retiene al juego `hold_back` = 8 vblanks (~130 ms); si el resto del acorde llega dentro de esa ventana el juego
no ve ninguno de los dos; si se suelta dentro, el juego recibe la pulsación tarde y con su duración real (un toque de Select sigue siendo un toque para reset-to-track: cambia la vista; una pulsación larga llega con 8 vblanks de retraso y suelta con la suelta real);
si la ventana se agota, pasa desde entonces. Coste: Select y Start llegan al juego 8 vblanks tarde. Los demás botones nunca se retrasan. Con el menú cerrado y sin acorde, el resto del filtro es la identidad. El overlay del host necesitó el parche `0061` de lcs-recomp
(`HostExtraPage::adjust`: la página distingue Intro/Izquierda/Derecha).

## Acorde elegido (SELECT+START)

Se probó, sin mods, un acorde de 6 vblanks en plena carrera (ruta `cheats`, fotogramas contra una ejecución sin él): SELECT+START, L+R, TRIÁNGULO+CÍRCULO y L+R+SELECT+START
cambian los fotogramas, así que ninguna pareja es «libre». Se eligió SELECT+START porque el filtro oculta los dos botones desde el vblank en que se completa (FACTS «Cheats»),
Select ya es el botón de reset-to-track (que lee el mando después del filtro y no ve nunca ese Select oculto) y Start solo pausa. Límite: si un botón llega más de un vblank antes
que el otro (ventana de 8 vblanks), el primero llega al juego (Select: un toque corto que cambia la vista; Start: pausa); dentro de la ventana no se filtra nada. `GT_PAD_MENU_CHORD` permite probar otro.

## Comprobar `cheats` y el menú con el mando

```bash
GT_MODS_REPO=../gt-mods tools/mod_cheats_check.sh [--decomp off|on] [--keep-baseline]   # sin mod / mod apagado / mod encendido en reposo: fotogramas idénticos; cámara lenta 0.5x y 0.25x; IA congelada
tools/mod_cheats_check.sh --pad [--decomp off|on]       # ruta cheats_pad: SELECT+START abre el menú, Abajo/Derecha cambian Slow motion y Freeze AI, el acorde lo cierra; sin acelerador mientras está abierto
tools/mod_cheats_check.sh --sabotage-slowmo             # debe FALLAR (la cámara lenta no hace nada)
tools/mod_cheats_check.sh --sabotage-freeze             # debe FALLAR
tools/mod_cheats_check.sh --replay                      # (cheats 0.4.0) ruta `circuit`, cámara lenta 0.5x + gravedad lunar desde el paso 700: el mod graba sus controles por paso y los reproduce en el replay; emparejados por pasos desde que el coche del jugador arranca, mismos controles y cada coche (jugador e IA) a <= 0,5 m del vivo
tools/mod_cheats_check.sh --sabotage-norecord           # no graba nada (el replay va sin trucos): --replay debe FALLAR
tools/mod_cheats_check.sh --pad-off                     # Slow motion OFF -> 0.5x -> 0.25x -> OFF con el mando: la velocidad vuelve a ser normal tras OFF
tools/mod_cheats_check.sh --pad-reset                   # con reset-to-track también activo y el acorde pulsado escalonado (SELECT, START 3 vblanks después): reset-to-track no ve ningún Select
tools/mod_cheats_check.sh --toggle-off                  # F9 apaga el mod a mitad de carrera: calla y todos los fotogramas idénticos al run sin mod
tools/mod_cheats_check.sh --slow-only --runs 3          # mide la dispersión de la cámara lenta (líneas MEASURE) para fijar las tolerancias
tools/mod_cheats_check.sh --pad --sabotage-hide         # recompila gt_play con -DGT_PAD_MENU_SABOTAGE_NO_HIDE=ON: el juego sigue viendo el mando con el menú abierto, debe FALLAR
tools/mod_cheats_check.sh --sabotage-chord              # recompila gt_play con -DGT_PAD_MENU_SABOTAGE_IGNORE_CHORD=ON, debe FALLAR, y lo deja como estaba
tools/mods_together_check.sh                            # los seis mods a la vez (cheats con GT_MOD_CHEATS_SLOWMO=0.5, FREEZE_AI=1)
```

## API v3: estado del juego con nombre (spec `docs/superpowers/specs/2026-11-10-named-mod-sdk-design.md`)

Los mods no deben llevar direcciones de PSP: piden el estado al host por su nombre y el host guarda las direcciones (con su
evidencia en `host/game_state.cpp` y FACTS). Un mod que lo usa declara `api = 3` en su `mod.toml`. Disponible:

- `money_get(self, ctx, &valor)` / `money_set(self, ctx, valor)` — el saldo en Cr (0..999.999.999); devuelven no-cero sin
  partida cargada. FACTS «Money and cars».

- `race_cars(self, ctx, world, out, capacidad)` — los coches de la carrera (`GtCar`: handle, índice, rol 1 humano / 2 IA / 3 el humano en su replay;
  sin huecos; el replay re-simula la carrera, así que un mod que cambia la física debe actuar también en él, como en vivo);
  `world` es el a0 del enganche del paso de mundo.
- `player_car(self, ctx, world, &coche)` — el coche humano en vivo (0 si hay exactamente uno).
- `car_state` / `car_set_state(self, ctx, coche, GtCarState*)` — posición, base, velocidad, giro, ángulos y pedales
  (FACTS «Car body state»); escribir solo desde el enganche del paso de mundo o el tick.

Siguen (spec, pasos 3-4): puntos de enganche con nombre, cámara.

## API v5: puntos de enganche con nombre, impacto y mallas (spec `docs/superpowers/specs/2026-11-07-crash-deformation-design.md`)

Un mod que declara `api = 5` usa solo llamadas con nombre (sin direcciones del guest). Evidencia junto a cada una:

| Llamada | Qué hace | Evidencia |
| --- | --- | --- |
| `hook_point(GT_HOOK_WORLD_STEP)` | paso físico de cada coche (`0x0894DD9C`, a0 = el mundo) | FACTS "Car body state" |
| `hook_point(GT_HOOK_MESH_DRAW)` | la hoja que dibuja un blob (`0x08B550A8`, a0 = blob) | FINDINGS "GE blob census" |
| `car_impact(world, car, out)` | cambio de velocidad del paso (`K+0x6CC..`) en ejes del cuerpo (filas `K+0x1C/28/34`), dirección de contacto = -dv, `off_track` si una rueda lee `0x300`, salto de la velocidad de guiñada (`K+0x6F8`), caja del cuerpo | FACTS "Collision code ... NOT FOUND" (aproximación por dv); "Rollover body box" |
| `mesh_info`, `mesh_vertex`, `mesh_set_vertex` | blob = BASE, VADR, VTYPE, PRIM, RET + vértices; formatos `0x122` (paso 14, s16 en +8), `0x130` y `0x112` (paso 12, +6) | spec de deformación, hallazgo del spike 2026-11-07 |
| `mesh_copy`, `mesh_release` | copia el blob en memoria del asignador del juego (`0x08B35DA0` / `0x08B35254`, solo desde el tic) con la VADR reescrita | `host/mesh_blob.cpp`, prueba `mesh_blob` |
| `draw_mesh_blob`, `draw_mesh_redirect` | el blob que se va a dibujar y su sustituto (a0) | spec |
| `draw_car_model`, `model_blobs` | coche -> contenedor del modelo (uno por coche) y sus partes que son mallas simples | FACTS "Car to model link" |
| `model_runs`, `run_vertex`, `run_set_vertex` | todas las tiradas de vértices del modelo (también las partes con varias VADR), leer / escribir un vértice en sitio | FACTS "Container per car, parts with several vertex arrays" |
| `on_switch_off` | una función que corre una vez, en el primer tic tras apagar el mod desde el menú (para deshacer lo escrito) | `host/mod_services.cpp` |
| `blob_runs`, `draw_matrix`, `run_normal`, `run_set_normal` | las tiradas de un blob cualquiera (también las copias en VRAM que ve el gancho de dibujo), la última matriz `WORLDN/WORLDD` de la lista (en el gancho `GT_HOOK_MESH_DRAW`: es la misma para todas las partes del coche: su caja) y las normales s8 | FACTS "VRAM copies of the model parts", "Car draw matrix" |

`car_impact` compara con la llamada anterior: llámala una vez por paso. Pruebas: `ctest --test-dir build -R "game_state|mesh_blob|mod_services"`.

### Comprobar `deformation` (`tools/mod_deformation_check.sh`)

Ruta `config/pad_routes/deformation.pad` (la de `rollover` más tres pulsaciones de SELECT: vista de persecución), renderizador por software, `--decomp off|on`, `--others` (con los otros seis mods activos). Cuatro ejecuciones: línea base (mod instalado y apagado), con el mod, con el mod y un abollado de fuerza cero (`depth_per_ms = 0`) y el mod apagado otra vez. Pasa solo si: un golpe en el muro y ninguno antes del paso 1500; los fotogramas antes del golpe son idénticos byte a byte a los de la ejecución de fuerza cero; después del golpe alguno difiere de ella en >= 200 píxeles; y el mod apagado da todos los fotogramas idénticos byte a byte a la línea base. Deben FALLAR: `--without-mod` (sin golpe), `--sabotage-zero` ("changed after hit") y `--sabotage-threshold0` (golpes en conducción normal).

**Por qué los fotogramas previos al golpe se juzgan contra la ejecución de fuerza cero y no contra una sin mod (medido 2026-11-15):** con el mod ENCENDIDO (incluso con abollado cero) 122 de 584 fotogramas anteriores al golpe (desde el vblank 13890; con `--decomp on`, 121 de 555 desde el 13050) difieren de una ejecución sin mod en 3 a 77 píxeles, creciendo despacio (la carrera se desplaza respecto al tiempo real). La causa son los ganchos del mod (dibujo del coche, paso del mundo, tic), no el abollado: contra la ejecución de fuerza cero son 0 de 584; con el mod apagado todos son idénticos a la línea base (la ejecución `identity`); con los otros seis mods encendidos el desplazamiento es 0 de 584. La comprobación imprime el desplazamiento como línea `info:`.

## API v7: lo que necesita un mod de cuerpo rígido (spec `docs/superpowers/specs/2026-11-18-rollover-physics-design.md`)

Un mod que declara `api = 7` recibe las llamadas anteriores más estas cuatro; las funciones v7 rechazan (no-cero y aviso en stderr) a un mod que declaró `api < 7`.
Pruebas: `ctest --test-dir build -R "game_state|tilt_free|mod_loader"`; evidencia en FACTS «Rollover physics research (2026-11-18)».

| Llamada | Qué hace | Evidencia |
| --- | --- | --- |
| `car_dimensions(world, car, out)` | distancia entre ejes, vía, posición de los ejes sobre el origen del cuerpo y radio, del registro del coche (`word(car+0xC)`: `+0x18`, `+0x1C`, `+0x40`, `+0x48`, `+0x08`). Lleva `world` (el a0 del paso) como `car_impact`: el handle es `K` y el registro cuelga del objeto coche | FACTS «Rollover body box» |
| `car_wheel_contacts(world, car, out[4])` | por rueda: punto de la consulta de carretera (x, y en el marco Z-up: y = -z del registro), altura del suelo, tipo de superficie (`+0x2D`), `valid` (byte `+0x2C` = 0: hay suelo, también el terreno fuera de pista) y la carga de la rueda (`K+0x70+0x118*i+0x54`; 0 con la rueda en el aire) | FACTS «Road query and the wheel contact records» |
| `car_set_pose(car, posición, base, velocidad, velocidad_angular)` | escribe un pose entero y coherente: posición, filas de la base (adelante, derecha, arriba; zurda), los tres ángulos de Euler `+0x6FC/700/704` **derivados de las filas** (el juego reconstruye la base desde ellos: `0x0893C3E4`), velocidad, velocidad escalar `|v|` y **cero en los cinco campos de velocidad angular**. Rechaza (sin escribir) un handle malo, un valor no finito o filas que no son una base ortonormal (1e-2). `velocidad_angular` solo se comprueba: los campos de giro del juego no son componentes en ejes del cuerpo (FACTS «Rollover body box») y cero es el único valor demostrado que deja el pose en paz en el paso siguiente | `host/game_state.cpp` `euler_to_rows` / `rows_to_euler` (ida y vuelta probada con 34 112 ternas, polos incluidos) |
| `tilt_free(car, on)` | levanta (o restaura) el recorte de ±35° para ese coche: el host engancha el lector de la bandera (`0x0894CCB8`) por cada mod `api >= 7` y devuelve 1 solo a los lectores del paso de cuerpo (`0x08956DC8`) de un coche liberado: el recorte y el ajuste de Euler, **nunca** la máquina de vuelco latente ni el paso de contacto (misma lógica que usó rollover 0.4.0, ahora en el host). Se deshace al apagar el mod | FACTS «Rollovers» |

El mod no toca nunca `+0x6FC` ni `0x08CF2F08`. `GT_HOOK_CAR_DRAW` permite reescribir el pose justo antes de dibujar (la velocidad se deja como la dejó el paso, para que el golpe contra la barrera siga visible en el siguiente).

## API v8: dibujo 2D sobre el juego y gradación de color (spec `docs/superpowers/specs/2026-11-18-mod-draw-api-design.md`)

Un mod que declara `api = 8` puede dibujar y colorear la imagen sin tocar la lista GE (el 7 es el SDK de física de vuelcos).
Un mod `api = 8` recibe también las llamadas v7.

| Llamada | Qué hace |
| --- | --- |
| `on_overlay(self, fn)` | (solo en `gt_mod_init`, una por mod) `fn(self, ctx)` corre una vez por vblank del juego en el hilo del tic, después de los tics de todos los mods; `ctx` lee estado y permite `call_guest`. Un mod apagado en el menú no dibuja |
| `draw_text(x, y, tamaño, 0xRRGGBBAA, texto)` | texto UTF-8 (hasta 95 bytes) con la fuente de la UI del host; `(x, y)` = esquina superior izquierda de la línea, `tamaño` = alto de línea |
| `draw_rect(x, y, w, h, rgba)`, `draw_line(x0, y0, x1, y1, ancho, rgba)` | rectángulo con bordes suavizados / segmento con extremos redondos |
| `scene_grade(r, g, b, brillo, saturación)` | `c' = clamp(mezcla(luma(c·(r,g,b)), c·(r,g,b), saturación)·brillo)` sobre la imagen del juego (no sobre lo dibujado con `draw_*` ni sobre los menús del host); `(1,1,1,1,1)` = neutro = nada (fotogramas idénticos byte a byte). Valores finitos, acotados a 0..4; los grados de varios mods se multiplican; se quita al apagar el mod |

- Espacio lógico 480×272 (origen arriba a la izquierda) escalado al rectángulo de imagen del juego; el dibujo se rasteriza en el host
  a la resolución real de salida (nítido a cualquier tamaño) en el hilo de imagen del parche 0070, y se sube a la GPU solo cuando cambia.
  Orden de dibujo = orden de llamadas, como máximo 4096 órdenes por pasada; coordenadas no finitas descartan la orden. Se dibuja **sobre
  el juego y bajo F9/F10 y el panel del cheat device**. Llamar a `draw_*` fuera de `on_overlay` se ignora (se avisa una vez).
- Lo que se dibuja se queda hasta la siguiente pasada: una pasada que no dibuja nada borra la capa del mod.
- Ventana Vulkan: la gradación se aplica entera en el shader de presentación (incluye el renderizador por software, que presenta por la misma
  vía); ventana SDL por software: solo `tinte·brillo` (máximo 1, sin saturación); los volcados (`GT_FRAME_DUMP`) llevan la gradación entera y la capa.
  La imagen graduada incluye el HUD del propio juego (FACTS "Scene grade").
- Parches lcs: `0071-mod-draw-layer`, `0072-scene-grade`. Código: `host/mod_services.cpp` (lista, gradación por mod), `host/play_backends.cpp` (enlace con la ventana y los volcados).

### Comprobar el dibujo (`tools/mod_draw_check.sh`)

Mod de prueba `tools/mod_draw_probe/probe.cpp` (no se distribuye). Ruta `chase`, volcados de fotogramas. Cuatro ejecuciones: línea base (sonda instalada y apagada),
`idle` (encendida sin dibujar: idéntica byte a byte en los fotogramas del mismo cuadro de juego), `draw` (rectángulo rojo exacto, línea verde exacta, rectángulo azul
semitransparente, texto blanco en su caja) y `grade` (`scene_grade(0.5, 0.5, 0.5, 1, 1)`: la mitad de brillo píxel a píxel, ±2). Sabotajes que **deben fallar**:
`--sabotage-flush` (`-DGT_DRAW_SABOTAGE_NO_FLUSH=ON`: la lista no llega a la ventana) y `--sabotage-grade` (`-DGT_GRADE_SABOTAGE_IGNORE=ON`).
Pruebas unitarias: `ctest --test-dir build -R "mod_services|mod_draw_raster"` (`mod_draw_raster_tests bench` mide el coste de 200 primitivas).

## `mod.toml`

```toml
id = "analog-pedals"        # obligatorio, no vacío, único entre carpetas
name = "Analog pedals"
version = "0.1.0"
library = "libanalog_pedals.so"   # obligatorio, no vacío, relativo a la carpeta del mod
api = 1                     # obligatorio: debe ser GT_MOD_API_VERSION del host
enabled = false             # por defecto false

[settings]                  # pares clave = valor que el mod lee con api->config(self, "clave")
deadzone = "0.05"
```

Subconjunto de TOML: cadenas entre comillas sin escapes, enteros, `true`/`false`, comentarios `#`, una sola tabla `[settings]`.
Una clave repetida (en la raíz o en `[settings]`) es un error con su número de línea.

## El SDK

`include/gt_mod/gt_mod_api.h`: ABI C estable, sin tipos del runtime. El mod exporta `int gt_mod_init(const GtModApi *, GtMod *)`
(0 = bien) y registra ganchos con `api->hook(self, direccion, fn)`. Dentro de un gancho: `gpr`/`set_gpr`, `read8/16/32/f`,
`write8/16/32/f`, `call_original`, `config`, `log`. Un gancho decide si llama al original antes, después o nunca.

## Límite: solo ven las llamadas que pasan por el despachador

Un gancho sustituye la función en la tabla de direcciones del runtime. El código recompilado solo mira esa tabla cuando llama a una
función **fuera de su unidad**; una llamada que el recompilador dejó dentro de la unidad (un `goto`) no pasa por ella y el gancho
**nunca se ejecuta** (`0x08952B68` desde `0x0894DD9C` con `--decomp off`: 0 aciertos; ver `docs/FINDINGS.md`). Comprobar un punto:

```bash
GT_NO_MODS=1 GT_PROBE=0x08948B78,0x0894DD9C tools/pad_route.sh race 230 recomp/analysis/probe_off --decomp off   # y de nuevo con --decomp on
# stderr: "[probe] 0x... hits=N"; exigir N > 0 en los dos modos
```

(`pad_route.sh` usa `gt_native`, que también lee `GT_PROBE`; con `gt_play` usa `GT_PAD=... build/gt_play --max-seconds ...` como
`tools/mod_pedals_check.sh`.) Con `--decomp on` una sonda sobre una función decompilada marca 0 (la decompilada sustituye a la sonda),
pero los mods se instalan después y sí la envuelven. Regla del repo: no engancharse a código que nunca se despacha; trabajar sobre
su entrada.

## Construir e instalar `analog-pedals`

Instrucciones y ajustes en el README de `gt-mods` (`cmake -S . -B build -DCMAKE_CXX_COMPILER=clang++`, copiar `analog-pedals/` con
`libanalog_pedals.so` a `<build>/mods/`, `enabled = true`). Comprobación en juego, desde este repo:

```bash
tools/mod_pedals_check.sh [--decomp off|on]        # PASS = los pedales del jugador salen a 0.5 / 0.3
tools/mod_pedals_check.sh --sabotage-brake         # debe FALLAR
tools/mod_pedals_check.sh --toggle-off [--decomp on]   # el menú apaga el mod a mitad de carrera: inyecta antes, nada después
tools/mod_pedals_check.sh --disabled               # enabled = false al arrancar: mismo `script done at vblank` que --without-mod
tools/mod_pedals_check.sh --replay               # ruta circuit_analog (sin acelerador digital): PASS = la repetición reproduce el 0.6 analógico y el coche se mueve (mod >= 0.2.0; 0.1.0 FALLA)
# sabotaje del menú: cmake -DGT_MOD_SABOTAGE_IGNORE_ACTIVE=ON, recompilar gt_play, --toggle-off --sabotage-active debe FALLAR
```

## Construir e instalar `orbit-camera`

Mismo esquema (README de `gt-mods`; copiar `orbit-camera/` con `liborbit_camera.so`, `enabled = true`; también se activa desde el menú F9).
Comprobación en juego, desde este repo (ruta `chase`: la carrera más tres pulsaciones de SELECT que cambian de vista; la última cae en la
cámara trasera con el coche visible; un run sin mod y uno por ángulo `GT_MOD_CAMERA_FAKE`, frames a disco en `recomp/analysis/mod_camera_check`):

```bash
tools/mod_camera_check.sh [--decomp off|on] [--yaws "90 180"]   # yaw 0 (control de determinismo, siempre se ejecuta) idéntico al run sin mod; yaw != 0: 3D distinto, dígitos del HUD iguales, coche centrado
tools/mod_camera_check.sh --sabotage                              # pivote en el ojo: debe FALLAR
tools/mod_camera_check.sh --without-mod                           # sin mod: debe FALLAR (nada cambia)
tools/mod_camera_check.sh --toggle-off [--toggle-at V]            # el menú apaga el mod a mitad (>= 6 fotogramas orbitados antes; después, a partir de 600 vblanks (la imagen se asienta: no hay borrado de color) cada fotograma emparejado por cronómetro con uno del run sin mod es idéntico byte a byte fuera de la ventana del coche y del reloj del HUD (abajo a la derecha), y como mucho un 3 % de la ventana del coche distinto; >= 30 y la mitad de los utilizables deben emparejar)
# (--toggle-off vuelca todos los presentes, --every 2, salvo que se dé --every: los fotogramas se emparejan por el cronómetro de vuelta, tolerando un paso de juego de desfase)
```

## Comprobar `draw-distance`

Mismo esquema (ruta `chase`, frames a disco en `recomp/analysis/mod_drawdist_check`; ver `docs/FACTS.md` § Draw distance):

```bash
tools/mod_drawdist_check.sh [--decomp off|on]     # draw-distance: scale 1.0 = fotogramas idénticos; 2.5 = cambia solo el fondo lejano
tools/mod_drawdist_check.sh --sabotage            # near también escalado: debe FALLAR (composición)
tools/mod_drawdist_check.sh --without-mod         # debe FALLAR (nada cambia)
tools/mod_drawdist_check.sh --toggle-off          # el menú apaga el mod a mitad de carrera: idéntico a partir de ahí


## Construir e instalar `reset-to-track`

Mismo esquema (README de `gt-mods`; copiar `reset-to-track/` con `libreset_to_track.so`, `enabled = true`; también desde el menú F9). Botón: Select
del mando mantenido 0,5 s (`hold_ms`) o la tecla `reset_keys` (R). El coche vuelve a la pose buena más reciente con al menos `rewind_ms` (2,5 s) de
antigüedad y arranca desde parado. «Buena» = las cuatro ruedas en asfalto, derecho (up.z > 0.9), más de 5 m/s, giro de guiñada <= 0,6 rad/s y
velocidad alineada con el morro (cos > 0,9: ni de lado ni marcha atrás). El anillo (una pose cada 15 pasos, 24 poses) se vacía si el coche salta
más de 15 m en un paso (aparición, nuevo intento, F9 apagado/encendido), si pasan más de 2 s sin pasos, o al cambiar de coche; nunca se restaura una pose de
más de 8 s.

**Regla de recta hacia delante (mod 0.2.0, API 1).** El coche arranca desde parado siguiendo el rumbo de la pose; una pose «buena» que cruza la pista en diagonal hacia el borde
(4/4 asfalto, derecha, tranquila) lo devolvía fuera de pista (3 de 4 restauraciones de la reproducción). El anillo se recorre en busca de la pose más reciente con `rewind_ms` de antigüedad cuya recta
(rumbo `-sin yaw, cos yaw`, tolerancia lateral 2,5 m) es seguida por el PROPIO camino del coche durante `ahead_m` metros (40; un coche que arranca parado recorre ~38 m en 4 s); como toda pose del
anillo tiene las 4 ruedas en asfalto, ese camino es asfalto. Sin ninguna, la de recta más larga de al menos 15 m (horquillas); sin ninguna, no restaura nada (`NOPOSE` en el log). Una pose restaurada
queda fijada (se puede volver a ella). No se consulta la carretera ni se llama al juego: una primera versión llamaba a la consulta de carretera del juego con `call_guest` desde el tic y no era neutra
para los fotogramas (docs/FINDINGS.md); los hechos de esa consulta están en docs/FACTS.md.

**Select y la cámara.** El juego usa Select para cambiar de vista. El mod engancha la lectura del mando (`0x08C7ABF0`, el stub de importación de
sceCtrlReadBufferPositive, despachado en `--decomp off` y `on`, FACTS): en carrera oculta Select al juego mientras está pulsado; si se suelta antes de
0,5 s, se lo devuelve como un toque de 3 lecturas (la vista cambia igual que sin el mod); si se suelta después, se dispara el reset y la vista NO cambia.
Fuera de carrera (menús, pausa) Select pasa sin tocar. La tecla R no toca el mando.

**Con la API 4 (reset-to-track 0.3.0).** El botón es la acción `reset` del cheat device (categoría Race; por defecto tecla R y Select mantenido 30 vblanks,
`SELECT@30`, reasignables en Controls). Ya no lo lee el mod: el host retiene Select mientras puede ser el comienzo de esa pulsación larga, se lo devuelve al juego
como toque tardío con su duración real si se suelta antes, y lanza la acción en el tic de vblank si se completa (el juego no ve ese Select). Con `GT_PAD_LOG=1` el
host escribe `[padmenu] PADSEL down|up step=<vblank> tap=<0|1>` (tap = el juego vio la pulsación), que la comprobación lee igual que las líneas del mod API 1.
Los fotogramas de referencia de la comprobación salen con el mismo mod instalado y **desactivado** (el device está activo igual y retiene Select/Start igual),
no con la carpeta de mods vacía: ver docs/FINDINGS.md.

**Límites conocidos.** (1) Marcha, rpm y la posición previa del coche no se restauran (no se localizaron); el cambio automático relanza bien desde parado
(la comprobación exige > 15 m/s con el acelerador pisado después del reset). (2) Vueltas y tiempo: el cronómetro y el contador de vueltas siguen su curso
(no se rebobinan; medido con un reset a mitad de vuelta: HUD 0'24.000 -> 0'27.000 en 3 s de juego, vuelta 1/2 sin cambio); un reset que cruce la línea de meta no se pudo probar (ninguna ruta completa una vuelta) y podría contar la vuelta dos veces: no hay bloqueo.
(3) Fantasma y repetición: en Time Trial el fantasma se graba con el coche real, así que un reset deja un salto en el fantasma/la repetición del jugador;
no se ha comprobado cómo reacciona. (4) Sin geometría de pista: se restaura donde el coche estuvo, no el punto más cercano al asfalto.

Comprobación en juego (ruta `reset`: el coche se sale al final de la recta y choca; un SELECT real de la ruta dispara el reset; log del mod en
`recomp/analysis/mod_reset_check`; cada ejecución toma su propio turno del bloqueo de CPU):

```bash
GT_MODS_REPO=../gt-mods-reset tools/mod_reset_check.sh [--decomp off|on]   # sin pulsar: fotogramas idénticos y el mod funcionó; con SELECT: 4 ruedas en asfalto, up.z > 0.9, desde parado, calmado, mismo rumbo, >= 15 m/s con acelerador, >= 1,5 s de recorrido atrás, Select oculto sin toque
tools/mod_reset_check.sh --tap                # las 3 pulsaciones cortas de la ruta chase: toques repetidos, misma vista de cámara que sin el mod
tools/mod_reset_check.sh --toggle-off         # el menú apaga el mod antes del botón: ni un reset, el mod funcionó antes y calló después, fotogramas idénticos
OUT=recomp/analysis/mod_reset_check_cheats WITH_CHEATS=1 tools/mod_reset_check.sh --reset-only|--tap   # con el mod cheats instalado también (menú del mando activo, Select llega 8 vblanks tarde): restaura igual / el toque sigue cambiando la vista
tools/mod_reset_check.sh --multi              # ruta reset_multi (cruce en diagonal y cuatro SELECT): cada reset deja al coche 240 pasos (4 s) en 4/4 asfalto sin girar, y cada RESTORE lleva `run=` >= 15 m (la regla eligió la pose)
tools/mod_reset_check.sh --multi --sabotage-lookahead-off   # la regla apagada, pick antiguo (-DRESET_SABOTAGE=5; RESTORE run=-1): debe FALLAR (un reset saca al coche de la pista y no usa la regla)
tools/mod_reset_check.sh --sabotage-rates     # no pone a cero las velocidades angulares: debe FALLAR
tools/mod_reset_check.sh --sabotage-newest    # restaura la pose más reciente (fuera de pista): debe FALLAR
tools/mod_reset_check.sh --sabotage-norewind  # sin rebobinado (rewind 0): debe FALLAR
tools/mod_reset_check.sh --without-mod        # sin mod: debe FALLAR (nada se restaura)
tools/mod_reset_check.sh --tap --sabotage-notap   # el toque no se devuelve al juego: debe FALLAR (API 1: -DRESET_SABOTAGE=4 en el mod; API 4: recompila gt_play con -DGT_PAD_MENU_SABOTAGE_NO_REPLAY=ON, y lo deja como estaba al salir)
```

## Construir e instalar `rollover`

Mismo esquema (README de `gt-mods`; copiar `rollover/` con `librollover.so`; `enabled = false` por defecto, también desde el menú F9). El juego no deja
volcar: `0x0893E0FC` recorta los dos ángulos de inclinación a ±35° y anula sus velocidades salvo que la palabra `0x08CF2F08` (que lee `0x0894CCB8`) sea
distinta de cero; nada del binario la escribe. El mod engancha esa lectura y devuelve 1 mientras está activo (no escribe memoria: al apagarlo en F9 el
juego vuelve a leer 0). Sola no basta (los muelles enderezan el coche): en un golpe lateral fuerte (cambio de velocidad en el eje lateral del coche,
`K+0x28`, mayor que `threshold` en UN paso de física) le da una velocidad de balanceo `K+0x6F4` hacia el lado golpeado, `gain` × exceso (máx. `cap`),
mantenida 40 pasos con caída lineal. Ajustes: `style = realistic|arcade` (arcade: umbral a la mitad, ganancia ×1,5), `threshold` (3,0 m/s), `gain` (2,5),
`cap` (12 rad/s). En conducción normal el cambio lateral es ~0,2 m/s por paso: no escribe nada. Saltos del coche (> 3 m por paso: reset-to-track, aparición)
no cuentan como golpe. README del mod: `gt-mods/rollover/README.md`.

**La máquina de vuelco latente.** Con la bandera puesta, `0x08956930` también despierta una máquina de estados «coche volcado» que nadie activó nunca
(byte `K+0x74B`, cuenta atrás `K+0x74C = 120`): mientras el coche está de lado se salta todo el pipeline de fuerzas, así que el coche **se hunde a
través de la carretera** (medido: de 6,8 a 2,4 m con la pista a ~5,5 m) y al soltarse **sube de golpe ~6,5 m**; la cuenta atrás (`0x08956E9C`) no tiene
llamante. Detalle y evidencia en FACTS («Rollovers»). Por eso el mod solo da la bandera (1) a los lectores del paso de movimiento del cuerpo `0x08956DC8` (solo el recorte de inclinación y el ajuste de ángulos de Euler; `0x08945F6C` queda fuera aunque corre dentro: con la bandera puesta cambia los fotogramas en `--decomp on`, FACTS) y
el valor real del juego (0) a todos los demás, la máquina de vuelco incluida: en `--decomp off` se distingue por `ra` (dirección de retorno, `0x0893E5AC` recorte,
`0x08956BF0` ajuste); en `--decomp on` el `ra` es un número de sitio y vale el alcance dinámico de un gancho en `0x08956DC8` (en off ese alcance no cubre a los
llamados: medido). Una primera versión daba 1 a todos: con ella el coche en la parrilla ya difería unos píxeles del juego original antes de cualquier golpe (rutas de
contacto y ajuste de ángulos con la bandera puesta, no el recorte); con la bandera estrecha, una partida con el mod y sin golpe es idéntica byte a byte al juego sin mod.
Con eso el coche da vueltas completas sin hundirse.

**Límites conocidos.** No hay colisión de techo en el juego: el coche siempre se recupera (no se queda boca abajo); el recorte se levanta para todos los coches (los de la IA también, aunque no reciben impulsos y ninguno
superó 35° en las pruebas; no está demostrado para todas las pistas); solo el jugador recibe impulsos y se le busca como único humano (con dos humanos, nadie); el balanceo es una velocidad mantenida, no una simulación del golpe (calibrada con un solo choque contra la barrera).

Comprobación en juego (ruta `rollover`: el coche se sale al final de la recta y choca contra la barrera en diagonal; log del mod en
`recomp/analysis/mod_rollover_check`; cada ejecución toma su propio turno del bloqueo de CPU):

```bash
GT_MODS_REPO=../gt-mods-rollover tools/mod_rollover_check.sh [--decomp off|on]   # sin mod / mod desactivado: fotogramas idénticos; con mod: conducción normal sin impulso, un impulso en el choque, inclinación > 35° hacia el lado golpeado, máquina de vuelco dormida (+0x74B = 0), sin saltos de altura, termina derecho o boca abajo parado
tools/mod_rollover_check.sh --toggle-off          # el menú apaga el mod antes del choque: el mod funcionó antes y calló después, TODOS los fotogramas idénticos al run sin mod (off y on)
tools/mod_rollover_check.sh --recover             # rollover + reset-to-track en la ruta `rollover_reset`: el coche está boca abajo, aún rodando, cuando se dispara el reset; el juez de reset-to-track pasa (sin su ventana de calma: tras una voltereta los muelles oscilan ~15 pasos) y la velocidad de balanceo se calma a los 20 pasos
tools/mod_rollover_check.sh                      # (rollover 0.2.0, caja de carrocería) además: juez `box` (esquina más honda de la caja del coche que vuelca >= -0,25 m bajo el suelo, origen < `MAX_TUMBLE_RISE` = 1,6 m sobre el suelo (tools/mod_rollover_check.py), ningún toque de la caja con el coche derecho) y una ejecución `nobox` (body_box = false): las líneas de estado antes del primer impacto son idénticas byte a byte con y sin caja, y sin caja el cuerpo se hunde > 0,5 m
tools/mod_rollover_check.sh --sabotage-nobox      # body_box apagado (-DROLLOVER_SABOTAGE=6; el log sigue midiendo): el cuerpo se hunde 1,02 m, debe FALLAR el juez `box`
tools/mod_rollover_check.sh --sabotage-pushupright # la caja empuja a un coche derecho (suelo 0,6 m alto, esquinas del suelo siempre activas): deben FALLAR `box` (toques derecho) y `boxcmp` (líneas antes del impacto)
tools/mod_rollover_check.sh --sabotage-nocap      # box_kick_cap 20: debe FALLAR el límite de subida
tools/mod_rollover_check.sh --replay             # (rollover 0.3.0) ruta `circuit` con un golpe falso en el paso 600 (GT_MOD_ROLLOVER_FAKE=600:9): el replay re-simula la carrera desde el pad grabado, así que el mod actúa también en él; `REPLAY start`, el impulso en el mismo paso en vivo y en el replay, el coche vuelca en ambos y el del replay no se aleja > 0,5 m del vivo
tools/mod_rollover_check.sh --replay --sabotage-liveonly  # el mod se salta el replay (como hasta 0.2.0): debe FALLAR
tools/mod_rollover_check.sh --frames [--without-box]  # un fotograma cada 3 vblanks alrededor del choque (--frames-window DESDE:HASTA, por defecto automática: de 60 vblanks antes del primer impulso a 300 después) en $OUT/tumble[_nobox]/frames, para mirarlos
tools/mod_rollover_check.sh --sabotage-noflag     # el impulso sin la bandera (el recorte de 35° lo frena): debe FALLAR
tools/mod_rollover_check.sh --sabotage-noimpulse  # la bandera sin impulso: la barrera sola no vuelca el coche: debe FALLAR
tools/mod_rollover_check.sh --sabotage-threshold0 # umbral 0: impulso en conducción normal: debe FALLAR
tools/mod_rollover_check.sh --sabotage-flipflag  # la bandera también para la máquina de vuelco (-DROLLOVER_SABOTAGE=4): el coche se hunde: debe FALLAR
tools/mod_rollover_check.sh --without-mod         # física original, el mod solo registra (-DROLLOVER_SABOTAGE=5): nada vuelca, debe FALLAR (con --recover: el coche no está boca abajo al reset)
```


### Comprobar `rollover` 0.5.0 (estilo `realistic`, `tools/mod_rollover_check.sh --sim|--drop|--sim-replay`, juez `tools/mod_rollover_sim_check.py`)

Los modos de simulación leen el log del mod (`GT_MOD_ROLLOVER_LOG=1`: líneas `ST`, `TAKEOVER`, `HIT`, `SIM`, `HANDBACK`, `RESET`). `--sim`: ruta `rollover`, el choque contra la barrera: sin toma de control antes del choque, una en el choque por un golpe, presupuesto de energía (la energía total nunca supera la de justo tras el impulso: sin saltos ni «peonza»), ningún punto del casco más de 5 cm bajo el suelo, `|w_z|` acotada con el coche de lado, termina quieto o devuelto, y el pose que el juego reporta al inicio de cada paso es el escrito (dentro de un paso de movimiento: `kd`). `--drop`: caída falsa de 2 m (`GT_MOD_ROLLOVER_DROP=m100:2.0`): el primer rebote ≤ 1,2·e²·h, el coche se desliza hasta parar (rozamiento), se devuelve y el acelerador lo vuelve a mover. `--sim-replay`: ruta `circuit` con golpe falso (`GT_MOD_ROLLOVER_FAKE=m30:9`): misma toma en vivo y en el replay, poses a menos de 0,5 m. `--others`: con los otros seis mods activos. Cada sabotaje debe FALLAR: `--sabotage-restitution` y `--sabotage-nofriction` (con `--drop`), `--sabotage-nopose` y `--sabotage-notakeover` (con `--sim`). Los modos antiguos corren el mod con `style = "arcade"`. Con la CPU en `powersave` la ruta necesita ~410 s (por defecto `--seconds 480`).

## Construir e instalar `weather`

README de `gt-mods` (`weather/`, `api = 8`, `enabled = false` por defecto). Spec `docs/superpowers/specs/2026-11-18-weather-mod-design.md`. Usa `scene_grade` (hora del día: presets `noon` / `afternoon` / `sunset` / `dusk`, o `progressive` por los pasos de física
del coche humano), `on_overlay` + `draw_line` (lluvia: rayas en el espacio 480 x 272, deterministas por vblank, desplazadas con la guiñada del coche) y, solo con `grip_loss` > 0 (apagado por defecto), `car_set_state`. Filas `Time of day` y `Rain`
en la página Race del dispositivo de trucos.

La comprobación aprovecha que el volcado de fotogramas aplica el grade (API 8): un fotograma con grade se **predice** del fotograma base de la misma presentación con la fórmula del host
(`c' = clamp(mix(luma(c * tinte), c * tinte, saturación) * brillo)`) y se compara píxel a píxel; dos ejecuciones de la misma ruta son deterministas (los fotogramas del mismo present son idénticos byte a byte).

```bash
GT_MODS_REPO=../gt-mods-weather tools/mod_weather_check.sh [--decomp off|on] [--others]   # off / idle (noon, sin lluvia: fotogramas idénticos) / dusk (la predicción) / progressive (el brillo baja sin subir) / rain (rayas sobre el grade de lluvia)
tools/mod_weather_check.sh --dusk --sabotage-nograde     # el grade se calcula y se registra pero no se aplica: debe FALLAR
tools/mod_weather_check.sh --rain --sabotage-norain      # la lluvia no dibuja nada: debe FALLAR
tools/mod_weather_check.sh --dusk --without-mod          # debe FALLAR
```
Límites: el grade también tiñe el HUD (FACTS «Scene grade»); no se investigaron las tablas de niebla / luz por pista del juego (fase 2 de la spec) ni se baja el plano lejano con lluvia; el recorte de agarre no está comprobado en el juego.

## Construir e instalar `mechanical-damage`

README de `gt-mods` (`mechanical-damage/`, `api = 5`, `enabled = false` por defecto; no necesita ninguna función nueva del host: usa `car_impact`, `car_state` / `car_set_state` y el punto `GT_HOOK_WORLD_STEP`).
Los golpes fuertes (los mismos `car_impact` y el mismo `StepFilter` que usa deformation, con un umbral más alto) dañan tres piezas según dónde caen: delantero = motor y dirección, trasero = transmisión,
costados = dirección. Efectos sobre el coche humano (en vivo y en su repetición): hasta -40 % de empuje del motor, un tirón constante hacia un lado (velocidad de guiñada) y un tope de velocidad si el daño es grave.
El host guarda ahora **un `ImpactTracker` por mod** (`car_impact` compara con la llamada anterior: con uno global, el segundo mod que lo pedía en un paso veía `dv = 0`; deformation y mechanical-damage a la vez perdían golpes; `host/tests/mod_services_test.cpp`).
El daño de motor reduce la aceleración (no la velocidad máxima de arrastre: ahí la ganancia neta ya es ~0); el tope de velocidad es lo que la baja. El tope se alcanza a 0,5 m/s por paso (un salto de velocidad de un paso se leería como un golpe frontal).
Fila en el dispositivo de trucos (categoría Car, `Damage: engine X% ...`, Intro repara) y acción `mechanical-damage.repair`.

**Hallazgos de la investigación (paso 0 de la spec, medidos 2026-11-19, ruta `rollover`, `--decomp off`; FACTS «Mechanical damage»):**
- La palanca `+0x530` (pedal) **no sirve** para escalar la potencia: escrita (escalada) antes del paso con `car_set_state` el coche acelera exactamente igual (velocidad idéntica a 3 decimales durante 300 pasos). El juego rampa
  el pedal ENTRE pasos (1/15 por paso hacia su objetivo) y el paso deriva su propio acelerador efectivo (`+0x534`, `0x0894FD00..0x0894FDE0`); además un pedal escalado y dejado puesto se compone (0,62^n lo clavó en 0,175).
- Lo que sí funciona, igual que el nitro de cheats: escribir velocidad y `speed` **antes** del paso (se integra). El mod resta antes de cada paso `(1 - escala) x` la ganancia que dio el motor en el paso anterior
  (medida desde donde empezó ese paso, así no se compone): la ganancia neta por paso es `escala` veces la original.
- Un sesgo constante en la velocidad de guiñada (`rates[4]`, `+0x6F8`) sumado tras el paso persiste (la guiñada medida sube con la velocidad: ~0,043 rad/s a 11 m/s con daño completo y `yaw_gain` 10).
- El tope de velocidad, escrito tras cada paso, se mantiene (la velocidad medida queda en el tope + ~0,04 m/s).

```bash
GT_MODS_REPO=../gt-mods-damage tools/mod_damage_check.sh [--decomp off|on] [--others]   # off / idle (sin golpe: fotogramas idénticos antes del primero) / front (empuje) / repair / rear (tope) / right, left (tirón)
tools/mod_damage_check.sh --replay                    # ruta circuit con un golpe falso: el daño se reconstruye en la repetición (mismo golpe, misma velocidad)
tools/mod_damage_check.sh --toggle-off                # F9 apaga el mod tras un golpe: no registra nada después
tools/mod_damage_check.sh --without-mod               # debe FALLAR (sin HIT, nada cambia)
tools/mod_damage_check.sh --sabotage-scale0           # effect_scale = 0: el golpe se registra pero no cambia nada: debe FALLAR
tools/mod_damage_check.sh --sabotage-liveonly         # -DDAMAGE_SABOTAGE=1: ignora la repetición: con --replay debe FALLAR
```
Los golpes son falsos (`GT_MOD_DAMAGE_FAKE=m30:25:f`: 25 m/s desde delante, 30 pasos tras pisar el acelerador; `b` trasero, `r` derecho, `l` izquierdo) porque el golpe real de la ruta (la barrera, paso ~1850) llega demasiado tarde para medir su efecto.

## Descargador de mods (en construcción)

Spec: `docs/superpowers/specs/2026-11-08-mod-downloader-design.md`. Hecho el núcleo (sin interfaz todavía):
`host/mod_install.hpp` (índice de la release de gt-mods, orden de versiones, `sha256` y tamaño, desempaquetado seguro del
`.tar.gz` a `<mods>/.staging/<id>/`, conservar `enabled` y los ajustes del usuario al actualizar, cambio por renombrado) y
`host/mod_fetch.hpp` (descarga solo HTTPS con libcurl; `GT_MODS_INDEX_URL` cambia el índice). Pruebas:

```bash
ctest --test-dir build -R "mod_install|mod_fetch"
GT_NET_TEST=1 build/mod_fetch_tests    # descarga la release real e instala analog-pedals en un directorio temporal
```

**En el juego (F9, página MODS):** debajo de los mods y de sus filas sale `GET MODS  ENTER: CHECK`. Enter descarga el
índice de la última release de gt-mods; cada mod sale con su versión y `INSTALL`, `UPDATE <instalada>`, `UP TO DATE`,
`DOWNLOADING`, `RESTART TO LOAD` o `ERROR <motivo>`. Enter sobre `INSTALL`/`UPDATE` lo descarga, lo verifica y lo instala;
se carga al reiniciar el juego (un mod nuevo llega desactivado, como dice su `mod.toml`: actívalo en la misma página).
Sin red, la fila dice `ERROR` y el juego sigue. Comprobación sin teclado:

```bash
GT_MODS_DIR=/tmp/vacio GT_MODS_GET_TEST=analog-pedals build/gt_play --max-seconds 40
# [mods] get index: GET MODS  6 AVAILABLE
# [mods] get analog-pedals: Analog pedals ... 0.2.0 RESTART TO LOAD
```
