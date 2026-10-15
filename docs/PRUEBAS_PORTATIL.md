# Pruebas pendientes en el portátil

**Última actualización: 2026-10-15 15:44** — pruebas nuevas: puntos 1 y 2 (parches 0041 y 0042).

Lista viva de lo que hace falta probar en el portátil (Ryzen 7 PRO 5850U, gráfica integrada Vega 8).
Cuando una prueba está hecha y sus resultados están en el chat (o en `my_tests/`), se borra de aquí.

## Antes de cualquier prueba: actualizar y recompilar

```bash
git pull
tools/setup.sh                         # vuelve a aplicar los parches de lcs-recomp (hay nuevos: 0041 y 0042)
.venv/bin/python -m tools.recomp
cmake --build build -j8
git log --oneline -1                   # pega esta línea junto con los resultados
grep -c "GT patch 0042" tools/src/lcs-recomp/lcs/host/ge_gpu_backend_vulkan.cpp   # debe dar más de 0
```

`config/gt_play.ini` se copia junto a `build/gt_play` al compilar: los cambios de las pruebas se hacen en
**`build/gt_play.ini`** (o en otro fichero con `PSPRECOMP_CONFIG=<ruta>`). Comprueba que tiene `NativeGE=true`,
`InternalResolutionMode=Window` y `GpuBudgetMs=10` en `[Rendering]`.

## 1. Letreros lejanos (parche 0041)

Tu captura de las 12:31 tenía razón: no era el nivel de detalle en sí, sino un fallo al subir las texturas
pequeñas. GT guarda los niveles pequeños de sus mipmaps con 32 píxeles de ancho (32x32 → 32x16 → 32x8) y el
programa los leía como si cada nivel fuera la mitad del anterior, así que a partir de ahí la textura salía
desplazada: líneas negras en las letras, y en los carteles de Gran Turismo directamente ruido rojo y azul.

- [ ] En el mismo sitio de tu captura (salida de la primera carrera), ¿los letreros (TRD, elf…) se ven ya
      limpios, sin líneas negras? Si no, una captura como la de antes.

## 2. Rendimiento con batería: lo que falta por medir (parche 0042)

Tu último log enseñó que, incluso en los menús con 6 primitivas, cada frame gastaba **3 ms grabando** para
la GPU (en el sobremesa son 0,3 ms). He quitado dos costes fijos por frame que solo ocurren con ventana real
(una reserva de memoria por cada textura que se sube y varias consultas al sistema de ventanas en cada frame)
y la línea del log ahora separa en qué se va ese tiempo.

Igual que antes, **con batería** (y en modo de energía normal, no «performance»), menús y unos 30-40 s de
carrera:

```bash
GT_PERF_LOG=1 build/gt_play 2>&1 | grep -E '^\[perf\]|internal scale' | tee my_tests/perf_bateria_0042.txt
```

- [ ] ¿Va mejor que la vez anterior (menos tirones, más fps estables)?
- [ ] Deja el log en `my_tests/perf_bateria_0042.txt`: la línea `GE thread` ahora dice
      `record ... [uploads .., draws .., present pass ..]`, y con eso sé qué parte queda.

Lo siguiente que estoy haciendo (aprobado por ti): la caché de geometría en la GPU, para que la CPU no vuelva a
decodificar cada frame los vértices que no cambian (el 99,8 % en carrera). Te aviso con otra entrada aquí.

## 3. Parpadeo y fundidos

- [ ] ¿Sigue parpadeando o vibrando la imagen? Si puedes, di dónde (menús, carrera, al cambiar de pantalla).
- [ ] Los fundidos entre pantallas (elegir carrera, coche, modo) que no van fluidos: ¿se ven a saltos (pocas
      imágenes) o se quedan congelados un momento?

## 4. Velocidad del coche

El reloj del juego va bien (`guest time 1.00x` en tus logs). Para comparar con la consola o con PPSSPP:

- [ ] Con el Peugeot 206 de la primera carrera, ¿cuánto tarda de 0 a 100 km/h (o 0-60 mph) con el
      cronómetro del móvil? Si puedes, lo mismo en PPSSPP con el mismo coche y circuito.
