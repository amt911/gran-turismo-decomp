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
  vblank, en ese hilo del invitado). El tic de un mod apagado se salta.
- `call_guest(ctx, dirección, args, n, &v0)`: llama a una función del juego (hasta 8 argumentos enteros). **Solo dentro del tic**: desde un gancho, con el
  runtime parado o con argumentos malos devuelve distinto de 0 y no ejecuta nada (`host/tests/mod_services_test.cpp`). La función no puede bloquear (un cambio de hilo
  fuera de fibra para el runtime) y se ejecuta con los demás hilos del invitado detenidos en un límite de llamada: solo funciones seguras entre dos llamadas cualesquiera del juego.

## Menú con el mando

`host/pad_menu.cpp` (lógica pura, `host/tests/pad_menu_test.cpp`) filtra la lectura del mando del juego (`sceCtrlReadBufferPositive`, `host/host_hooks.hpp`; el filtro y el tic
solo se instalan en `gt_play` cuando hay al menos un mod instalado). **Acorde: SELECT+START** (variable `GT_PAD_MENU_CHORD`, p. ej. `L+R`, para probar otro): al completarse abre o cierra la página
MODS; arriba/abajo mueven (con repetición a los 18 y cada 4 vblanks), cruz = Intro, izquierda/derecha cambian el valor, círculo cierra. Con el menú abierto
(también si se abrió con F9) el juego no ve botones y ve el stick centrado; un botón que se pulsa con el menú abierto y los del acorde siguen ocultos hasta soltarlos
(ni un Start que pause, ni un círculo suelto al cerrar); uno que ya estaba pulsado al abrir (el acelerador) vuelve a verse al cerrar. Sin acorde y con el menú
cerrado el filtro es la identidad. **Por qué ese acorde:** ver «Acorde elegido» abajo. El overlay del host necesitó el parche `0061` de lcs-recomp (`HostExtraPage::adjust`:
la página distingue Intro/Izquierda/Derecha).

## Acorde elegido (SELECT+START)

Se probó, sin mods, un acorde de 6 vblanks en plena carrera (ruta `cheats`, fotogramas contra una ejecución sin él): SELECT+START, L+R, TRIÁNGULO+CÍRCULO y L+R+SELECT+START
cambian los fotogramas, así que ninguna pareja es «libre». Se eligió SELECT+START porque el filtro oculta los dos botones desde el vblank en que se completa (FACTS «Cheats»),
Select ya es el botón de reset-to-track (que lee el mando después del filtro y no ve nunca ese Select oculto) y Start solo pausa. Límite: si un botón llega más de un vblank antes
que el otro, el primero se filtra al juego (Select: un toque corto que cambia la vista; Start: pausa). `GT_PAD_MENU_CHORD` permite probar otro.

## Comprobar `cheats` y el menú con el mando

```bash
GT_MODS_REPO=../gt-mods tools/mod_cheats_check.sh [--decomp off|on] [--keep-baseline]   # sin mod / mod apagado / mod encendido en reposo: fotogramas idénticos; cámara lenta 0.5x y 0.25x; IA congelada
tools/mod_cheats_check.sh --pad [--decomp off|on]       # ruta cheats_pad: SELECT+START abre el menú, Abajo/Derecha cambian Slow motion y Freeze AI, el acorde lo cierra; sin acelerador mientras está abierto
tools/mod_cheats_check.sh --sabotage-slowmo             # debe FALLAR (la cámara lenta no hace nada)
tools/mod_cheats_check.sh --sabotage-freeze             # debe FALLAR
tools/mod_cheats_check.sh --sabotage-chord              # recompila gt_play con -DGT_PAD_MENU_SABOTAGE_IGNORE_CHORD=ON, debe FALLAR, y lo deja como estaba
tools/mods_together_check.sh                            # los seis mods a la vez (cheats con GT_MOD_CHEATS_SLOWMO=0.5, FREEZE_AI=1)
```

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
tools/mod_camera_check.sh --toggle-off [--toggle-at V]            # el menú apaga el mod a mitad (>= 6 frames orbitados antes, idénticos al run sin mod después)
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

**Select y la cámara.** El juego usa Select para cambiar de vista. El mod engancha la lectura del mando (`0x08C7ABF0`, el stub de importación de
sceCtrlReadBufferPositive, despachado en `--decomp off` y `on`, FACTS): en carrera oculta Select al juego mientras está pulsado; si se suelta antes de
0,5 s, se lo devuelve como un toque de 3 lecturas (la vista cambia igual que sin el mod); si se suelta después, se dispara el reset y la vista NO cambia.
Fuera de carrera (menús, pausa) Select pasa sin tocar. La tecla R no toca el mando.

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
tools/mod_reset_check.sh --sabotage-rates     # no pone a cero las velocidades angulares: debe FALLAR
tools/mod_reset_check.sh --sabotage-newest    # restaura la pose más reciente (fuera de pista): debe FALLAR
tools/mod_reset_check.sh --sabotage-norewind  # sin rebobinado (rewind 0): debe FALLAR
tools/mod_reset_check.sh --without-mod        # sin mod: debe FALLAR (nada se restaura)
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
tools/mod_rollover_check.sh --sabotage-noflag     # el impulso sin la bandera (el recorte de 35° lo frena): debe FALLAR
tools/mod_rollover_check.sh --sabotage-noimpulse  # la bandera sin impulso: la barrera sola no vuelca el coche: debe FALLAR
tools/mod_rollover_check.sh --sabotage-threshold0 # umbral 0: impulso en conducción normal: debe FALLAR
tools/mod_rollover_check.sh --sabotage-flipflag  # la bandera también para la máquina de vuelco (-DROLLOVER_SABOTAGE=4): el coche se hunde: debe FALLAR
tools/mod_rollover_check.sh --without-mod         # física original, el mod solo registra (-DROLLOVER_SABOTAGE=5): nada vuelca, debe FALLAR (con --recover: el coche no está boca abajo al reset)
```

