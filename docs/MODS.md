# Mods nativos para `gt_play`

Un **mod** es una biblioteca compartida (`.so`) más un `mod.toml`, cargada por `gt_play` al arrancar. El mod engancha
**funciones del juego por dirección** (la dirección de ejecución PSP = vaddr del ELF + `0x08804000`), lee y escribe la memoria
del invitado y puede llamar a la función original. Solo `gt_play` carga mods; `gt_native`, la puerta diferencial, `--decomp diff` y
los casos sintéticos nunca los cargan. Los mods del autor viven en el repo privado `gt-mods`
(`analog-pedals`: gatillos/teclas analógicos; `orbit-camera`: cámara orbital con el stick derecho, estilo GT7; `reset-to-track`: un botón devuelve el coche del jugador a la pista; ver su README).

## Dónde y cómo se carga

- Directorio: `<build>/mods/` junto a `gt_play`, o el de `GT_MODS_DIR`. Cada mod es una subcarpeta con `mod.toml` y su biblioteca.
- `gt_play` imprime `[mods] loaded: ... skipped: [...]` si el directorio tiene algo. Un mod se omite (con motivo) si falta
  `mod.toml`, no se puede leer, `api` no es la del host, no abre la biblioteca, falta `gt_mod_init`, su init
  devuelve distinto de 0 o repite el `id` de otra carpeta. Dos mods con un gancho en la misma dirección detienen el arranque
  (código 3) nombrando ambos.
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
Solo teclado (como el panel F10). No se editan los ajustes del mod ni se descargan bibliotecas en caliente.

Un mod con `enabled = false` **se carga igualmente** (su `gt_mod_init` se ejecuta) y queda inactivo, para poder encenderlo
sin reiniciar. Los activados se cargan primero: si uno desactivado engancha una dirección que ya engancha otro, sale como
`ERROR` ("hook conflict") y el juego arranca; dos mods activados en la misma dirección siguen parando el arranque. El
`mod.toml` se escribe en `mod.toml.tmp` y se renombra encima, así un corte nunca lo deja a medias.

Para verificar sin pulsar teclas: `GT_MOD_MENU_TEST_TOGGLE=<id>@<segundos>` hace que un hilo de `gt_play` llame al mismo `toggle`
del menú pasado ese tiempo (`[mods] <id> -> OFF`).

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


## Construir e instalar `reset-to-track`

Mismo esquema (README de `gt-mods`; copiar `reset-to-track/` con `libreset_to_track.so`, `enabled = true`; también desde el menú F9). Botón: Select
del mando mantenido 0,5 s (o la tecla `reset_keys`, por defecto R); el coche vuelve a la pose buena más reciente con al menos 2,5 s de antigüedad
(las cuatro ruedas en asfalto, derecho, en movimiento) y arranca desde parado. Como Select también cambia la vista de cámara del juego, cada reset
cambia la vista una vez. Comprobación en juego (ruta `reset`: el coche se sale al final de la recta y choca; el botón lo da `GT_MOD_RESET_FAKE=<paso>`,
sin mando; log del mod en `recomp/analysis/mod_reset_check`):

```bash
GT_MODS_REPO=../gt-mods-reset tools/mod_reset_check.sh [--decomp off|on]   # sin pulsar: fotogramas idénticos al run sin mod; con botón: 4 ruedas en asfalto, up.z > 0.9, desde parado, sin deriva, conduce 600 pasos
tools/mod_reset_check.sh --sabotage-rates    # no pone a cero las velocidades angulares: debe FALLAR
tools/mod_reset_check.sh --sabotage-newest   # restaura la pose más reciente (fuera de pista): debe FALLAR
tools/mod_reset_check.sh --without-mod       # sin mod: debe FALLAR (nada se restaura)
tools/mod_reset_check.sh --toggle-off        # el menú apaga el mod antes del botón: ni un reset, fotogramas idénticos
```
