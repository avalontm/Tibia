# Cola de dibujo del viewport

## Evidencia

- `Tibia.exe`, SHA-256
  `fe1172d0ff2e3024b78125184a4ebcdbb0489b9629ff744184ca130033d6321a`.
- `FUN_004ddb70` (`ev_43c71df6830d28087e691486948b2bfad249f450f3ff89fb3aab5851238594e5`)
  recorre X de `0` a `17` como bucle exterior y Y de `0` a `13` como bucle
  interior; empuja Y y luego X antes de llamar `FUN_004d90c0`, que recibe
  `(X,Y)`.
- `FUN_004d90c0` (`ev_7991357d1fcb4dea098c09fdfc772be918d067b4dd45dada413e0a31b4ca8c54`)
  valida ese rango, calcula la posición con tiles de `32 x 32`, descarta tiles
  fuera del rectángulo `480 x 352` con un margen de un tile y consulta la celda
  cacheada solo después de la prueba de visibilidad. Si la celda no está
  submitted, dibuja la diferencia de sus objetos y después marca la celda.
- `FUN_004d5700` (`ev_722813c7b580626f7270f9d48752ff672836ae3a101ce039275fe7ebd3beb93f`)
  transforma las coordenadas del viewport al almacenamiento circular del
  MapWindow sumando X/Y a las coordenadas centrales guardadas en `this+0x25cc`
  y `this+0x25d0`: columna `(center_x + X) mod 16`, fila
  `(center_y + Y) mod 12`, y dirección de celda
  `base + (columna * 12 + fila) * 0x8C`. `FUN_004c3150`
  (`ev_c298a3920d0690e7fe9caf0b2a6738e6a73106d2e20ca085003e76670ab07a7f`)
  entrega esos centros y el piso central al MapWindow.
- `FUN_004d4b20` (`ev_83be908b82805462d5b9db79e9b224894de43971855a12fff5a42cf0999f61b6`)
  inicializa el origen de cada celda de caché como `(columna*32,fila*32)` con
  índice lineal `columna*12+fila`. En `FUN_004ddb70` (`ev_7a7e7b6faffcf01d802af4430ba70f0fae82d4ddf334812d0c9329119d06be04`)
  el ciclo pone a cero el contador actual y la marca submitted de las 192 celdas;
  conserva el contador anterior para detectar cambios.
- La búsqueda completa de bytes `ev_899d07c638d24a63a408cf8a29dd641bc7b09143d2672e7aaa3a35b799400d35`
  ubica accesos con desplazamiento `0x25CC` en `FUN_004d5700` y `FUN_004ddb70`;
  la búsqueda equivalente `ev_51fe1f1265070e807d2ada797f11970400230a5151c717aa99b145e0324f5d05`
  ubica `0x25D0` en ambas. `FUN_004ddb70` inicializa los centros mediante
  `FUN_004c3150`; `FUN_004d5700` los suma a las coordenadas locales antes de
  aplicar los módulos del anillo.
- `FUN_004d9930` (`ev_c2cf8f47dd9d34890f1dbcfb000277a5ddc7aa90a27ee0cac096530bbaf0a0f4`)
  recibe `(X, Y, selector, offset_x, offset_y)`, aplica la misma prueba de
  visibilidad y actualiza la celda. Cuando `current_count == cached_count`,
  dibuja y registra solo el objeto nuevo. Cuando difieren y el registro no
  coincide, vuelve a dibujar los objetos actuales y el nuevo en la superficie
  cacheada. Si la celda ya fue enviada, dibuja el objeto nuevo directamente en
  el viewport. Al llegar a diez objetos, envía la celda antes de añadir el
  objeto siguiente.
- Para selector `0`, la rutina elige el sprite directo con selector `7`; para
  otros valores usa la ruta texturizada con el selector recibido. En el camino
  directo al viewport el recorte lo realiza la superficie; en el camino
  texturizado la rutina limita el draw con el rectángulo destino de la celda
  intersectado con el viewport.
- `FUN_004da350` (`ev_1609895e69c403e6d407a5c9081aede35e28b564d1a0fa2718e0c52f4735432d`)
  resuelve un objeto de `Tibia.dat` recorriendo X, luego Y y luego capa, todas
  con índices desde uno. Omite sprite ID cero. La posición de cada celda usa
  `ancla + 32 - índice * 32 - extensión`; los patrones X/Y/Z dependen de las
  coordenadas entregadas, y la fase depende del temporizador dividido entre 5
  y el número de fases del objeto.
- `FUN_004da350` (`ev_bddfb1ff25254d0fe8cd60bb1574fada0af9b00ab921354bd0d7d463de10e322`)
  calcula los patrones base como `world_x % pattern_x + 1`,
  `world_y % pattern_y + 1` y `floor % pattern_z + 1`; la fase base es
  `(FUN_00502800() / 5) % animation_frames + 1`. Luego sustituye los patrones
  de objetos con atributos 5, 11/12 o 18 usando argumentos adicionales.
  `render/object_sprite_layout` reproduce las variantes; el nuevo
  `resolve_map_item_sprite_layout` obtiene las coordenadas y el piso del tile y
  deja sin resolución cualquier variante que requiera un dato auxiliar ausente.
  `FUN_00502800` devuelve `DAT_0075d444` y `FUN_00502810` lo incrementa una vez
  por llamada. La ventana original registra el timer 1 a 100 ms en
  `FUN_004bdee0`; su `WM_TIMER` llega a `FUN_004bbb50` y luego a
  `FUN_0044cdc0`, que hace el incremento. El host configura el mismo intervalo
  y avanza `animation_clock` una unidad al recibir ese timer, por lo que el
  divisor 5 significa cinco ciclos de 100 ms, no cinco milisegundos ni cinco
  frames de renderizado. Evidencia: `FUN_00502800` `ev_a8143283d4d1a6366a55af629d92480e1a51f69f53908c6dcf46938bb2c5c093`,
  `FUN_00502810` `ev_d6902054f08e5f630ea528003d974f1a87343e0aff836249b1153a7dac6fcd0a`,
  `FUN_0044cdc0` `ev_d96c1bd9875242dc341a0b6c8b0d96c07875d19af86413bbd552a3c17920b514`,
  `FUN_004bbb50` `ev_e0ac86a04c87d550bf5dc7da866ce57ca705b1d5d69498172bc9c3dec660f557`
  y `FUN_004bdee0` `ev_430542079f8d0c264d5af47a71d666af16e2eb2b8775ee64d8b3114a18c648cb`.
- `FUN_004da350` (`ev_e2b11f2864027444d5d35b97adabc8106f3c794a23766cdc5b39f254ed32aef8`)
  entrega a `FUN_004d9f60` cada sprite resuelto, la posición derivada, el límite
  de filtro recibido por el objeto y el indicador `fases > 1`. El módulo
  `render/object_sprite_router` compone las ubicaciones de
  `resolve_object_sprite_layout` con `route_object_sprite`, pasando también los
  offsets de cámara explícitos; una ruta no resuelta detiene la lista en ese
  sprite, conservando los resultados anteriores.
- `FUN_004d9930` (`ev_c2a3cb63cd9db49d0203ceb60fd4f4460d005843fa3642b9731d8645d3f03ddb`)
  envía los sprites estáticos al modo 4 de la interfaz gráfica mientras la celda
  no se haya enviado; si ya fue enviada, dibuja el sprite en modo 2 y limita el
  destino al rectángulo de la celda cuando cruza el viewport. `FUN_004d90c0`
  (`ev_920fbde3c730ef3491e6cd403c442448db3356b709a793a6c89e53133e50e5fa`)
  termina los sprites pendientes de una celda antes de emitirla al viewport.
  `FUN_004d9f60` (`ev_04ef39dfc59a545708f44aa63f34578ad9ff616152134bd028ad85d93be379fc`)
  vacía las celdas intersectadas antes de dibujar un sprite que omite la caché;
  usa el draw normal o la variante con máscara según sus argumentos.
- Corrección de la interpretación anterior: los cuatro argumentos adicionales
  de `FUN_0049e1e0` no son una región fuente UV, sino el rectángulo de recorte en
  la superficie de destino. `FUN_004d9930` pasa el origen de la celda y 32×32
  para dibujar en caché; cuando la celda ya se envió, pasa la intersección de la
  celda con el viewport. `TileDrawRequest` conserva ahora `clip_x/y/width/height`;
  `make_viewport_draw` calcula ese recorte desde el tile y lo intersecta con el
  viewport. `MapWindowDirectSpriteRenderer` dibuja el sprite completo y aplica
  el recorte mediante `GraphicsRenderer::draw_clipped`.
- `FUN_0049e1e0` (`ev_f5140e58b3b079949eedc1828584164a8ae99e7b76e11a53720440328b572498`)
  pasa al backend la posición destino del sprite completo y el rectángulo que
  limita el dibujo. `GraphicsRenderer::draw_clipped` reproduce el recorte con
  scissor en Direct3D 9/OpenGL y con intersección de píxeles en DirectDraw.
  `MapWindowTileSurfaceRenderer` implementa la composición de las superficies
  de celda y reproduce los eventos de caché y viewport en orden; `render_frame`
  lo conecta al ciclo de `game_world`. Mantiene las texturas GPU de las 192 celdas y de
  `Tibia.pic`; solo vuelve a subir una celda cuando su superficie cambia o se
  reconstruye.
  La pasada nativa `FUN_004ddb70` calcula la luz por celda mediante
  `FUN_004d4880` dentro del recorrido de pisos (`ev_a3cbfcb1cc1607dde5c269116469595ca1c3731cc2b54ea98b03baba3acf34a3`).
  Por eso C++ conserva la iluminación en frames parcialmente resueltos cuando
  mapa, piso y grilla ya están disponibles, en vez de omitirla por el estado
  interno de una ruta de objeto.
- `FUN_004db291` (`ev_595b21a07a9e65a196870e48131e66f5c325e2c9d52df98c2927b7524390b4ec`)
  acumula candidatos de criatura de las celdas vecinas visibles y los reordena
  comparando primero la coordenada Y y después X. El intercambio ocurre incluso
  con claves iguales; `render/creature_render_order` conserva ese recorrido
  pairwise y mantiene unido el índice de origen de cada candidato.
- `FUN_004e9a00` (`ev_fe1dc037dd635dd24f464b57b957a028dcd1045ef14670e0ac517fa681c22e55`)
  valida índices de celda, capa, patrón y fase como valores uno-basados antes
  de resolver el sprite en el arreglo del objeto.
- `FUN_004ec320` (`ev_b57b7bef300f19eb60daee4e87b22e896967934468be444c0d5ed20acc079633`)
  lee el campo `+0x1C` del registro DAT seleccionado; en el layout cargado por
  `FUN_004ea100` este es el número de fases. `FUN_004da350` activa el parámetro
  de ruta especial cuando ese valor supera 1.
- `FUN_004d9f60` (`ev_d9a5c8573e7a7ce66be540e79ffc118d4ef2cf167f34e259c15fb72361a15be8`)
  divide cada sprite de 32×32 entre una, dos o cuatro celdas según su alineación.
  Para objetos con más de una fase primero somete las celdas tocadas y después
  dibuja el sprite en el viewport; para objetos estáticos añade el sprite a cada
  celda con sus offsets relativos. Si no hay ninguna celda aceptada no dibuja.
- `FUN_004d47a0` (`ev_86eeba4bc792f4499a587d45619b1db8883b19a5d05ac43561dd01ef445ce924`)
  valida X en `[0,17]`, Y en `[0,13]`, lee el valor de la tabla en
  `this + 0x25DC + (X * 14 + Y) * 4` y acepta cuando ese valor es menor o igual
  al límite que recibe.
- `FUN_004ddb70` inicializa esa tabla en cero y, para cada celda `(X,Y)`, recorre
  los pisos desde el máximo activo hacia 1. Usa `FUN_004c97b0` para recuperar el
  objeto de índice cero del piso y guarda el primer índice cuyo objeto DAT tiene
  el atributo 31; si no lo encuentra conserva cero. `FUN_004c97b0`
  (`ev_5a407b550aa847ccaf842703267adbec2d957b80bef4a9b463214fee7c4daf31`)
  recupera ese objeto desde la celda; `FUN_004c3d50` y `FUN_004c3eb0`
  (`ev_b48aa00e24dec1775fe30d3547e9e62e384020a514f2a9a3d25fd9560fc840ac`,
  `ev_ef45438f22f6a7108c889f79f472120edd475bfb9e48fc14c9d4bac6af460a3e`)
  confirman que el índice cero es una entrada del arreglo de objetos de la celda.
- `FUN_004ea730` (`ev_5c3b884ac146c88a9892535ec7640ab9473747b8bd13fce4a53426cbae254d8d`)
  devuelve `property_id == 13` para el tipo nativo 99 (criatura), sin consultar
  la tabla DAT. Así, las consultas de propiedades 0, 2, 15 y 23 usadas para
  calcular el piso activo dan falso en criaturas; marcarlas como no resueltas
  interrumpía el cálculo y podía dejar el viewport sin su grilla de filtro.
- El mapeo de un tile a `(X,Y,piso)` usa `floor_delta = center_floor - tile.floor`,
  `X = tile.x + 8 - center_x - floor_delta`, `Y = tile.y + 6 - center_y - floor_delta`,
  y el piso relativo `7 - center_floor + floor_delta` en la superficie superior
  o `floor_delta + 2` bajo tierra. `viewport_map_position` centraliza esta
  conversión que antes se repetía al construir la grilla de filtro.
- La rutina `FUN_004c9180` (`ev_58a8810cd71a817945f6d3c02c3abc83ca2d7c7b78edde50aa8948931f9ca1c4`)
  indexa las celdas con límites X=18, Y=14 y Z=8; el máximo activo usado para
  poblar la tabla se calcula antes en `FUN_004ddb70` a partir del entorno del
  jugador.
- El pseudocódigo de `FUN_004ddb70` (`ev_cdb46fc7ca477efb54cf2665b27b702402eb2c613c2c4fd8bd5dd78677810ba6`, páginas de offsets 6000–8000)
  inicializa el máximo en 7, obtiene el piso central de `FUN_004c3180`
  (`ev_7a8b364228dfa328a04b952ec9fbc9d5c518f21b2817593285ce92ef7d356a47`) y recorre
  la cruz de coordenadas X `[7,9]`, Y `[5,7]`, centrada en `(8,6)`. Si
  `FUN_004d2e90` no encuentra atributo 15 en la pila del piso central, revisa
  pisos superiores y sus coordenadas desplazadas. Bajo la condición de que el
  piso previo siga debajo del máximo actual, reduce ese máximo al piso previo
  cuando los objetos examinados cumplen las combinaciones observadas de
  atributos 0/2 y ausencia de 23.
- `FUN_004d2e90` (`ev_3e0f981b23d4e9b5e87c36d0919fbf8635a5a44c06a01ceea8c6cb9686fcb4d6`)
  itera la pila de una celda y devuelve verdadero si llega al final sin encontrar
  atributo 15. `FUN_004c9cb0` (`ev_1c85bab2ab4a937f5926c730a38b9fc8f896a4898ec24f3153ddf70c7dd3fc9a`)
  incrementa el índice global de la pila y recupera el registro siguiente.
- `FUN_0049dff0` (`ev_0f8082e28f5b8ab2af9e50b870c1020b43d12badc8fe6b298fd48ebd8be5547d`)
  dibuja un ID de sprite no nulo sobre la superficie indicada, usando una celda
  fuente de 32×32 del atlas de sprites.
- `FUN_004ef990` (`ev_3cf02177bfe90f6987eb487fe09159c4f025b7535c634912dc4dd8247c593f41`)
  selecciona los patrones para objetos con el flag 5 mediante el valor adicional
  recibido; las bandas observadas son `<2`, `2`, `3`, `4`, `[5,9]`, `[10,24]`,
  `[25,49]` y `>=50`, y después limita ambos ejes a las dimensiones del objeto.
- `FUN_004efca0` (`ev_9b354d94f1e07a17d28b83dc804b7611760702aae38e2e023c6401715c55b760`)
  selecciona patrones de objetos con flag 11 o 12 usando un valor de subtipo.
  `FUN_004e8550` (`ev_a39845ff49003b91b72d266e33268d1eb759c33d60789a43308cb16d292ad3a1`)
  convierte subtipos 0–15 a índices de patrón; los demás valores producen 1.
- `FUN_004eff70` (`ev_b2c0cc14ab60c7f96187760394fb2fe89d6a4d49cdaf9f33e430236053756024`)
  selecciona el patrón horizontal de objetos con flag 18 para los selectores
  `0x13` y `0x14`, según el número de patrones disponibles; el eje vertical queda
  en 1. Otros selectores dejan el eje horizontal en 1.
- `FUN_004e8e70` (`ev_80735f6e2153dbad20944ef2df52d157f27d8acc89d57f2f023cd7e62a5a752d`)
  y `FUN_004e90c0` (`ev_01671554c8f321a42fa7c4c51172ab07fbcd54c0091ad063a4da6a5a5adf5e31`)
  leen los dos componentes de extensión del atributo 25 del objeto.
- `FUN_004ea100` (`ev_2b832f0cd7a28932f9b3c5973a57c781965d100041e7de31dfdcd791336ff2cb`)
  lee dos palabras de 16 bits para el atributo 25 y las conserva en campos de
  32 bits sin extensión de signo. `resolve_object_sprite_extent` mantiene esos
  valores como magnitudes sin signo y devuelve cero en ambos ejes solo cuando
  el atributo está ausente.

## Implementación

`queue_visible_map_tiles` reproduce el orden de recorrido X/Y, devuelve un
resultado por cada posición del viewport y no consulta el proveedor para las
celdas fuera de pantalla. Las celdas visibles se delegan a
`queue_visible_tile`, que produce las solicitudes de sprite directo o textura,
respeta el contador de caché y marca la celda como enviada. `map_cache_coordinate`
reproduce la transformación al anillo circular y devuelve columna, fila e
índice lineal. Su sobrecarga con `DecodedMap` calcula los offsets de anillo a
partir del centro con módulos 16 y 12. `MapWindowTileCache` crea esas 192 celdas
con sus orígenes, conserva los objetos y el contador cacheado entre frames, y
expone un proveedor que calcula el índice desde el centro del `DecodedMap`.
`begin_frame` reinicia únicamente el contador actual y la marca submitted.
`viewport_map_position` devuelve X/Y y piso relativos para recorrer los tiles
con la misma transformación que usa el cálculo de filtro.
`add_tile_render_object` reproduce las ramas de inserción,
la reconstrucción de la lista cacheada cuando divergen los contadores, el límite
de diez objetos y el envío directo al viewport de objetos posteriores. Cada
solicitud declara el destino y el rectángulo de recorte destino. El campo
`reset_cache` de los lotes ordena limpiar la superficie antes de reproducir una
lista cacheada reconstruida, incluso cuando queda vacía.

La comparación directa de `FUN_004d90c0`
(`ev_2aee166871eeabad42966591161b9d6dd0a68d406a6d2a9b70e089f7fd91d6f2`)
y `FUN_004d9930`
(`ev_ec9e72ea72930d60aa63939dc429c8f75de2699ebb767ce4dbceeb276b75b522`)
confirma el rango de celdas `X=0..17`, `Y=0..13`, el descarte fuera de los
límites `(-32,480) × (-32,352)`, y la coordenada de pantalla calculada como
`tile * 32 - camera_offset`. El original agrega al caché los objetos anteriores
a la presentación de la celda; después de someterla, dibuja los objetos nuevos
directamente a la superficie del viewport, recortados a la celda. Si la lista
cacheada debe reconstruirse, vuelve a dibujar sus entradas en orden antes de
agregar el objeto nuevo. `is_in_view`, `queue_visible_tile` y
`add_tile_render_object` implementan esas ramas; no se encontró una diferencia
estática que justifique otro cambio de código en este bloque. Esta coincidencia
no determina qué mapa ni qué estado de frame produjo la captura negra.

`MapWindowTileCache::invalidate` descarta el conteo de objetos guardados de las
192 celdas sin liberar sus vectores. `MapWindowTileSurfaceRenderer::invalidate`
limpia las superficies de las celdas y descarta las marcas de texturas cargadas
para sprites y cuadros. Las dos operaciones son necesarias juntas después de
perder el contenido GPU: conservar el conteo de objetos podría omitir el dibujo,
y conservar las marcas de upload podría dejar texturas ausentes.

La evidencia nativa de `FUN_004ddb70` (`Tibia.exe`, SHA-256
`fe1172d0ff2e3024b78125184a4ebcdbb0489b9629ff744184ca130033d6321a`, Ghidra
12.1.4; pseudocódigo `ev_c098bacf8fb738dcbfe06a7c4f8e5e77d55d8f88c2f4545778468c05ca32ac87`)
muestra una llamada virtual del backend en `+0x9c` con argumento 2. Si el valor
previo del indicador es distinto de cero, el original pone a cero un campo de
cada celda de una matriz 16×12; luego reinicia otros campos por celda.

El slot OpenGL apunta a `FUN_00524ae0` (`ev_e6f1b08ecf4185b533ed7125c765ce354292c3f350a1c0aa72e72442fa4546ed`).
Su pseudocódigo omite el valor de retorno, pero el ensamblador muestra que carga
`this + 0x494 + Type` en `AL`, lo guarda localmente, borra el byte y retorna sin
modificar `AL`. Por tanto, el método consume y devuelve el indicador anterior;
con argumento 2 lee y borra `this + 0x496`. La inicialización de superficies
OpenGL `FUN_00523f10` pone `this + 0x494`, `+0x495` y `+0x496` a uno
(`ev_4f5821f53808b5835a291548c3e2d1f6971d535d4e7bd935390e63a831a11eff`), y
`FUN_00524240` borra esos indicadores durante la preparación de contexto; la
ruta de reinicio global vuelve a marcar `+0x496`
(`ev_7f766b4e788403e84ea2685ac79d1c3e13e733f65261cb29a15b8e9cc5cd0031`).
En DirectDraw y Direct3D 9, el slot equivalente apunta a `0x00507130`, que lee
el byte `this + 0x70`; no borra el indicador. El slot compartido conserva así
una semántica específica de cada backend.

En C++, las superficies de celdas viven en memoria de CPU y sobreviven a un
cambio de motor, mientras que `GraphicsRenderer` sustituye los recursos del
backend. Para igualar la invalidación del MapWindow al recrear el backend, el
host invalida el estado lógico de las celdas y las superficies compuestas solo
después de que la inicialización del nuevo motor finaliza correctamente. La
siguiente pasada reconstruye las celdas desde el snapshot del mapa y vuelve a
subir sus texturas. No se invalida al seleccionar otra vez el motor ya activo.
La build Release terminó con `scripts/build.ps1 -Action Build -Configuration
Release -BuildDirectory build-msvc`; `build-msvc/Release/tibia_client.exe`
tiene SHA-256 `A99D34080FCD6CCAC8C04E19D9101DD2D322C48D43BF5674F9544391F2A4BBDA`.
El cliente no se ejecutó.
`compute_highest_active_render_floor`
reproduce el cálculo del máximo a partir del piso central y la cruz de celdas
cercana. Las casillas sin objetos se tratan como celdas vacías: `FUN_004c97b0`
devuelve el registro global de tres palabras cero cuando la lista de objetos de
la celda está vacía (`ev_3dbb36a6dad8f87d5c3ee67905b47a2eafc76bb145618db60b8a6a205fff3290`;
los 12 bytes observados en `0x75a8d4` son cero,
`ev_4e414bed3d3d9658889a394cf5dae370f08a3821c69f13cb2f86f1480beffa6c`). Los
duplicados de posición y objetos sin registro DAT consultado siguen devolviendo
`nullopt`, porque sus propiedades no se pueden inferir. `compute_map_render_filter_grid` busca en
`DecodedMap` el objeto de índice cero de cada piso y deriva el valor de filtro a
partir del atributo DAT 31. Las criaturas tipo 99 equivalen a propiedad 31 falsa
y no invalidan el dato. `GameWorldSnapshotMailbox::publish` adjunta esa grilla
al snapshot solo cuando el máximo pudo resolverse. `make_filtered_tile_render_cell_lookup`
combina esa grilla con el proveedor de almacenamiento circular; el router aplica
la comparación observada antes de añadir o someter la celda y devuelve
`unresolved_cell_data` cuando falta el valor del filtro o el proveedor no resuelve
una celda. Las criaturas se consultan con la regla especial nativa de tipo 99,
por lo que no bloquean la resolución de propiedades DAT durante el cálculo del
piso activo. `collect_map_item_render_visits` reproduce la recursión de objetos
colgantes de `FUN_004daef0`: solo entra a los vecinos cuando un objeto no
creatura carece del atributo 3 y posee el 27; las visitas son diagonal arriba a
la izquierda, izquierda y arriba, con los offsets de ancla observados de 8
píxeles. Marca esas visitas como pasada vecina para que el consumidor aplique el
modo recursivo del original. Un tile duplicado o un objeto sin registro DAT
deja el recorrido sin resolver; una coordenada sin tile usa la lista vacía
nativa observada en `FUN_004c97b0` y sigue produciendo la visita de esa celda.
Cada visita separa los objetos del camino
de sprite estándar (atributo 3 ausente) del último objeto con atributo 3, que el
original conserva como candidato diferido. `FUN_004cea50`
(`ev_d8aabcf41b81eae8163b50a5f729f7c27c343d2dbaffa700e62e2160c6ea8174`)
selecciona la celda `(X,Y,Z)`, inicializa el recorrido de efectos y deja en EAX
el resultado de `FUN_004ce7b0(0)`. Esta última devuelve `-1` cuando no hay un
efecto en la celda; en ese caso `FUN_004daef0`
(`ev_d3c015cabc736bbc86160dfe91abd0be4a6d5915536a3ea65b7fda5509291a96`)
procesa el objeto diferido mediante `FUN_004da350` con el ancla base de la
visita. `resolve_deferred_map_item_draw` produce esa acción únicamente cuando el
índice del efecto llega a `-1`; conserva el ancla de la llamada actual, incluidos
los desplazamientos de 8 píxeles de una pasada vecina. El candidato se conserva
también en las pasadas vecinas; estas solo omiten el camino estándar.
El cuerpo de `FUN_004ce7b0`
(`ev_7daf32ea9e5ad06bb85d1510719031db05e1a06822b3b019816c7fff93380c37`)
confirma que avanza por `Effect.Next` al pedir un índice posterior y devuelve
`-1` al final. `FUN_004cf0b0`
(`ev_1fa1e7cfe5bfa042a631068f4dfb8e8718cffb7cb52512f4c40ef5d14a2eb508`)
reserva el primer slot libre entre 200 y enlaza el efecto con el punto de mapa.
Sus callers identifican tipos 1, 2 y 3 para efectos de posición, texto y
proyectil respectivamente (`ev_5e0ce4e3b21c673c4c465e0463396d8195eb1796ff33eaca77f2e7e0b556731d`,
`ev_3953a54c8053eb488990d882915256ccc6aa99656962795e1a2fbc1833cfc9aa`,
`ev_4a5a93f11f33d33dea0908a2f0102dbfd4720e69b5cb1742d4a5ab393f1f58d5`).
El actualizador `FUN_004d03e0`
(`ev_aad266426efb769e7165a8cfc5e3aee2cb49d89a0813ac920f5655be58d386a3`)
incrementa el contador de actualización de los efectos y los libera al superar
la duración que obtiene `FUN_004ee8a0`; también limita los textos a diez
actualizaciones. `FUN_0044cdc0` (`ev_b0145724334d6da7d35e160c22295e340ca828ea994f71b0945420c54eaca883`)
invoca el actualizador una vez por ciclo de ventana; `FUN_004bdee0` configura ese
timer con un intervalo de 100 ms (`ev_197534fee03c65d8e156772b253c1c0b2cf1e4ee3767390cf0e45522e5fda57c`).
`FUN_004ee8a0`
(`ev_69e3177e76faaf528425909b05797e6ce43520288efc018dd76c64d24fdc8dd4`)
lee el campo `+0x1c` del registro DAT; el parser conserva los ocho valores de
dimensiones y patrones en orden, con `animation_frames` como octavo valor. El
host ahora avanza la duración lógica de efectos 0x83/0x84 una vez por cada
intervalo de 100 ms y vuelve a publicar el snapshot cuando cambia el estado;
si un límite de renderizado deja pasar varios intervalos entre actualizaciones,
aplica todos los ciclos vencidos y conserva el resto inferior a 100 ms.
El efecto nativo inicializa su fase en 1 y lo elimina al superar la cantidad de
fases; el snapshot comienza el contador en 0, dibuja `update_ticks + 1` y lo
libera al llegar a ese mismo límite. Esas bases distintas conservan la última
fase del efecto durante el mismo número de ciclos.
La interpolación de proyectiles 0x85 usa el tiempo absoluto y se actualiza con
la cadencia visual configurada; el original recalcula sus desplazamientos durante
`FUN_004ddb70` mediante `FUN_004d0690`
(`ev_ca5e366ada4b2f591c18827f85ae9cb5a71dedeff05c5778bfa2814006cf0c4a`).
El caller de `FUN_004daef0` en `FUN_004ddb70`
(`ev_ace93e91c73674654b86d9b73ef580b2a8be204b7771196dfadea8eda7374ee6`)
itera los pisos desde cero hasta el piso activo más alto. Para cada piso recorre
las coordenadas locales X=0..17 e Y=0..13 y pasa las anclas
`X*32-camera_x` y `Y*32-camera_y`; el piso actual es el quinto argumento y el
sexto argumento es cero. `FUN_004daef0` usa ese quinto argumento al filtrar las
celdas destino. La selección de objetos apilados y sus llamadas recursivas
siguen dentro de `FUN_004daef0`. El módulo
`render/object_sprite_layout` calcula patrones base y fase, aplica las reglas
observadas de objetos acumulables, líquidos/contenedores y objetos colgantes, y
convierte los parámetros de patrón/fase/extensión en ubicaciones de sprite y
marca si el objeto debe evitar el caché por tener varias fases. La sobrecarga
que recibe el objeto y la variante obtiene automáticamente el atributo 25 desde
los datos parseados; si el registro está incompleto devuelve un resultado sin
resolución. En el recorrido nativo, `FUN_004ddb70` pasa los índices locales
X/Y del viewport y el piso a `FUN_004daef0`; el cuerpo de `FUN_004daef0` usa
esos mismos parámetros para consultar la celda y transmitir la posición a
`FUN_004da350`. Las variantes deben recibir, por tanto, `visit.local_x`,
`visit.local_y` y `visit.floor`, no las coordenadas absolutas del mapa. El
router C++ ya coincide en las rutas estándar y diferidas. Evidencia directa de
la llamada del recorrido: `FUN_004ddb70`
`ev_a6283b410ee5cd3a7f6834d9d642e7c2486da0f1b5f0635e2e8bca66cf3096ac`;
uso de parámetros en `FUN_004daef0`
`ev_f547524fc8fc3fa76b1ca49d2624d9ac441987ae84f23d7a3b8e9c4e4c934f45`;
fórmula de patrones en `FUN_004da350`
`ev_0c601deb165addb6ab1887286f714cf467c030af18c2218c1da484c3b195a2e2`.
`route_map_item_visit_sprite_objects` y `route_deferred_map_item_visit` ahora
incluyen esa posición local en `MapItemSpriteVariantInputs`; el resolver genérico
usa las coordenadas del tile solo cuando no recibe una posición de recorrido.
`resolve_map_item_sprite_variant_inputs` deriva el
contexto confirmado desde el registro DAT y `Object.extra`: el atributo 5 alimenta
el selector de cantidad y los atributos 11/12 alimentan el subtipo. Requiere
que el byte adicional exista cuando alguno de esos atributos está presente; el
atributo 8 no se reutiliza para estas variantes. `FUN_00401020`
(`ev_6e5a9dc62ed4eaa1f1aaddc9a2227dd0ef36bd654c5c7be821dc5f7b1f386b7f`) lee
un byte adicional para los atributos DAT 11, 12, 5 u 8, y el decoder nativo
lo almacena en los objetos de mapa (`ev_2c49d9778457e6d395cd313c065734767f6558a09790aea547399edf2d2d75a9`).
Para líquidos, `FUN_004e8550` devuelve explícitamente el patrón 0 para subtipo
0 y el patrón 1 para los valores no enumerados; `FUN_004efca0` convierte luego
ese resultado en coordenadas de patrón X/Y de base uno. El resolver de objetos
del mapa conserva esa distinción (`src/render/object_sprite_layout.cpp`); evidencia
`ev_3bece9e038a401a4b10cad9b9a3a7590f00ba52552f8dca25800a5ebaf709d16`.
El registro nativo de cada objeto de mapa tiene un tercer campo.
`FUN_00415bc0` lo inicializa a cero al decodificar los objetos (`ev_fab941ed02c1e2fb30f6fec6be976e97cbf6aad6f5b5c5ac1cec975bb6132acb`);
`FUN_004c97b0` copia los tres campos del registro
(`ev_e07b3ead3a6b4d579abdff9259e55ea15f0c942e44983505d8ea40f7ad492e99`).
En `FUN_004da350`, la variante de la propiedad 18 consume ese tercer campo:
`FUN_004eff70` compara el valor con `0x13` y `0x14`
(`ev_ff2d3d0076ccb76522ddfc0bdb6e842b6cd30e4b5ea92fb8eb7d949ca63d64cf`,
`ev_3fdccd201e02b97c53028e405113c7abafad63195366206b627f3a5d3174b2fc`).
Por ello, `map::Object.render_context` conserva ese valor y la reconstrucción
usa el cero de `FUN_00415bc0` al decodificar; cuando se completa o modifica la
pila, también aplica la escritura observada de `FUN_004cbb20` descrita abajo.
`render/object_sprite_router` convierte las ubicaciones resueltas
en rutas de sprite; `route_object_sprite` reparte sprites estáticos entre las
celdas tocadas y somete esas celdas antes de producir el draw directo de los animados.
`route_map_item_sprites` conecta la resolución de un objeto del mapa con ese
router y conserva el layout resuelto junto con los lotes producidos. Recibe
explícitamente el ancla, los offsets de cámara y el límite de filtro porque el
recorrido que proporciona esos valores aún no está reconstruido completamente.
Devuelve sin resultado cuando no se puede resolver el layout; el enrutamiento
conserva los lotes previos y termina ante datos de celda no resueltos.
`route_map_item_visit_sprite_objects` consume en orden la lista estándar de cada
visita. Resuelve la variante y enruta cada objeto con su contexto conservado;
usa `visit.floor` como límite de filtro de las celdas destino. La sobrecarga
genérica `route_map_item_sprites` mantiene el límite como argumento explícito
para callers que no usan una visita reconstruida.
El recorrido calcula el ajuste vertical acumulado con el atributo 26 (propiedad
`0x1A`) de cada objeto previo y lo limita a 24 píxeles antes de asociarlo al
siguiente objeto; `FUN_004daef0` realiza ese acumulado mediante
`FUN_004eb240`/`FUN_004e9310`
(`ev_cd58718f9ad6be22337ba2d737443897deb35e2a1f5c48c0921dc06db3cfb9c2`,
`ev_bb5d69d0d424e362650e75a4cbbdf3837ceb2c69311a9cc5b7dc076e11882a60`).
`FUN_004ea730`
confirma que el sentinel de criatura 99 solo tiene propiedad 13
(`ev_f23fe13c3d22e811ea5967a02bd8df8810b8b89f0ba5a947e22776fa263fef32`),
por lo que no requiere altura de pila. `route_deferred_map_item_visit` solo
enruta el candidato guardado cuando el índice actual de efectos es `-1`, y lo
hace con el ancla base, sin aplicar el ajuste de los objetos estándar.
Ambas funciones conservan las rutas parciales y distinguen entrada ausente,
layout no resuelto y datos de celda no resueltos.
`render/creature_render_order` ordena entradas ya seleccionadas por Y y X según
la comparación observada del MapWindow.
`MapWindowCreatureRenderer` prepara la apariencia, compone sprites y los dibuja
en la celda de ancla durante el recorrido del MapWindow. La selección por DAT,
la composición desde SPR y el recorte se conectan al frame en
`src/client/map_window_creature_renderer.cpp`.

`client/map_window_item_frame` implementa la preparación de una pasada del
MapWindow: obtiene el piso activo más alto, visita cada piso en orden ascendente,
recorre las 18x14 coordenadas, enruta los objetos estándar y encola las celdas
visibles. El resultado conserva visitas y solicitudes de draw para un consumidor
posterior. `GameWorldSnapshotMailbox` publica ahora los efectos activos 0x83/0x84
con su índice de slot y la posición local transformada; si alguno coincide con
la celda de un objeto diferido, `route_deferred_map_item_visit` recibe ese índice
y deja el objeto en espera. Los proyectiles compatibles publican su casilla actual,
por lo que también participan en esa comprobación; solo una entrada de misil
ausente o con layout no compatible conserva el frame sin resolver. Sin efecto en
la celda, el candidato se enruta y el consumidor de superficies procesa también
sus draws. La cadena y el orden nativos siguen sin quedar reconstruidos
completamente; se mantienen explícitas las posiciones y los estados que faltan.

`win32_client` ahora conserva referencias a Tibia.dat/spr/pic y crea el caché de
las 192 celdas junto con `MapWindowTileSurfaceRenderer`. Cuando el snapshot tiene
mapa y la cámara del jugador local se resuelve, cambia a `game_world`, oculta los
controles de login/selección, prepara el frame y dibuja las solicitudes del mapa.
El renderer aplica el recorte al rectángulo calculado por
`resolve_map_window_layout`; un timer invalida el frame según el límite de cuadros
configurado. La pantalla de juego ya no depende de que llegue otro paquete para
repintarse.
El armado del frame ya no cancela el mapa completo cuando encuentra una visita
con objeto o datos de celda sin resolver: conserva las rutas resueltas y permite
dibujar los tiles disponibles. Los elementos desconocidos se omiten y quedan
marcados en el estado del frame; una ruta que falla ya no elimina los sprites
resueltos antes del fallo y no se dibuja un sustituto aproximado. Por eso
esta mejora evita el frame completamente vacío en esos casos, pero el mapa no se
considera 1:1 hasta recuperar los datos faltantes y compararlo con una sesión
real. Los estados sin mapa, sin cámara o sin piso activo todavía no producen un
frame del mundo.
`MapWindowCreatureRenderer` prepara los outfits una vez por frame.
`MapWindowTileSurfaceRenderer` consume las visitas en X/Y y agrupa la visita
primaria con las pasadas vecinas de objetos altos. Después de dibujar el fondo y
los objetos de la casilla emite `after_tile`; ahí se dibujan las criaturas cuya
ancla pertenece a esa casilla y, a continuación, el efecto 0x83 local.
`FUN_004daef0` consulta `FUN_004cea50(x,y,floor)` después de su lista de criaturas
(`ev_d8aabcf41b81eae8163b50a5f729f7c27c343d2dbaffa700e62e2160c6ea8174`,
`ev_d3c015cabc736bbc86160dfe91abd0be4a6d5915536a3ea65b7fda5509291a96`).
La misma función recorre los objetos de vecinos cuando se activa la propiedad 27:
llama recursivamente para arriba-izquierda, izquierda y arriba, con el modo de
pasada `1` y el ancla desplazada 8 píxeles por eje correspondiente. En ese modo
omite los objetos ordinarios, pero conserva la selección del objeto con propiedad
3 y consulta sus efectos usando las coordenadas de esa visita. El frame C++ ahora
también compara el efecto con `visit.local_x`, `visit.local_y` y `visit.floor`;
antes usaba las coordenadas del tile raíz para todas las visitas recursivas, lo
que podía dibujar un objeto diferido vecino cuando el efecto pertenecía a otra
casilla. Evidencia del control de modo, selección diferida y llamadas recursivas:
`FUN_004daef0` `ev_096ec7dbd303bb0a2d8d51e44021ee53da8283d019e9ec64ecae5192bb29f267`.
`FUN_004c4a30` devuelve el estado de la entrada de efectos seleccionada
(`ev_131982805c64ff6acc9b151584f5218f72a0a18fa70dce03006a8493a15d3d1b`).
El renderer ya consume opcode 0x83 después de las criaturas por casilla. Los
efectos 1–31 de `Tibia.dat` local son objetos de 1×1, con una sola capa y una
fase de animación; el snapshot conserva el contador de actualización y lo usa
como fase.
Los textos 0x84 se recogen en la pasada posterior del MapWindow. `FUN_004cf610` limita
el texto a menos de 10 bytes y conserva el color recibido; `FUN_004db291` coloca
el ancla en el centro de la casilla y la desplaza 2 píxeles hacia arriba por tick.
`FUN_004ddb70` convierte el índice de color en RGB con seis niveles por canal,
mide y centra el texto, y lo dibuja con la fuente nativa 2. En `Tibia.pic`, esa
fuente está registrada desde la imagen 5 con celdas de 16×16 y espaciado -1;
`win32_client` usa esos glifos, aplica el color observado y recorta el resultado
al rectángulo del MapWindow. El renderizador respeta `TextualEffects`, que el
original consulta mediante `FUN_004f44e0` (`ev_4e463b5b4fbc585393e78b1ed6f644b4235fd5e5ebb7c1bbaf26e93d5da0bb8e`);
`FUN_004ddb70` consume los textos después del recorrido de casillas
(`ev_ace93e91c73674654b86d9b73ef580b2a8be204b7771196dfadea8eda7374ee6`).
El estado conserva el color y los ticks. El ancla se mantiene en su coordenada
nativa aunque el texto toque el borde del MapWindow: el recorte limita los
píxeles visibles, no desplaza el centro ni la altura del mensaje. El parser del
opcode 0x84 pasa X/Y, piso, color y un buffer de 10 bytes a `FUN_004cf610`
(`ev_682a81964257e5725b62e551517064dd320d25095b147010c579e44080f699cc`);
la restricción `< 10` y el almacenamiento del color están en
`FUN_004cf610` (`ev_48095a674d55acf47ccdeba4ad0172b961147d5acf2bc4c97b50914ba4274051`).
`FUN_004db291` calcula el centro horizontal con `+16` y resta dos píxeles por
tick a Y (`ev_8f86314fc150e0090b9e5186aac1b214de49d362b6ea99ffff9bdcdc8d02b93b`).
El gate `FUN_004f44e0` no suprime los efectos de posición 0x83: `FUN_004daef0`
recorre y dibuja su lista por tile sin consultar ese gate. En `FUN_004ddb70`, el
valor se consulta al inicio, pero solo rodea la pasada global posterior de
proyectiles y mensajes. El C++ conserva esa separación: los efectos de posición
se dibujan por tile aunque `show_textual_effects` esté desactivado; los textos
0x84 y proyectiles 0x85 sí respetan la opción. No se requiere cambio de código.
Evidencia: `FUN_004daef0` `ev_d3c015cabc736bbc86160dfe91abd0be4a6d5915536a3ea65b7fda5509291a96`,
`FUN_004ddb70` `ev_ace93e91c73674654b86d9b73ef580b2a8be204b7771196dfadea8eda7374ee6`
y `FUN_004f44e0` `ev_4e463b5b4fbc585393e78b1ed6f644b4235fd5e5ebb7c1bbaf26e93d5da0bb8e`.
Los tipos 1, 2 y 3 comparten la lista por casilla: `FUN_004cf0b0` inserta cada
entrada nueva al frente, y `FUN_004db291` recorre desde esa cabeza con
`FUN_004cea50`/`FUN_004cef00`. El recorrido del MapWindow visita X antes que Y;
por eso el host ordena los textos 0x84 por piso, X, Y y llegada más reciente
primero antes de dibujar la cola de texto del piso.
Los proyectiles `0x85` también pasan ahora por el frame. `FUN_004cfa20` calcula
la dirección con umbrales enteros `0x6A/0x100` y `0x26A/0x100` sobre las
coordenadas normalizadas por `FUN_004c31f0`, redondea la distancia al tile más
cercano y usa 150 ms por tile. `FUN_004d0690` actualiza los
desplazamientos restantes según el tiempo hasta el vencimiento; los getters
`FUN_004c5d10` y `FUN_004c5fa0` convierten esos desplazamientos en píxeles.
`FUN_004ef630` mapea las ocho direcciones a las variantes 3×3 de `Tibia.dat`, y
`FUN_004db291` dibuja la fase 1 con el ajuste de patrón `32 - patrón×32`. El
snapshot conserva por separado la casilla actual del proyectil en la lista nativa,
el ancla en píxeles de esa casilla y el desplazamiento residual;
`MapWindowEffectRenderer` compone el sprite del grupo `missiles` y lo recorta al
mismo MapWindow. Los 16 misiles del `Tibia.dat` local tienen una celda, una capa,
patrones 3×3×1 y una fase. Los efectos con layout incompatible permanecen
registrados en su casilla actual para conservar el orden y la oclusión, pero
no se dibuja un sustituto.

La composición nativa de 0x85 queda localizada en dos etapas. Mientras
`FUN_004ddb70` recorre pisos y casillas, `FUN_004daef0` agrega cada efecto cuyo
estado es 2 a la cola del MapWindow: índice, ancla en píxeles, coordenadas locales
y piso. El contador se reinicia por frame. Después del recorrido de casillas,
la pasada de efectos diferidos comprueba la visibilidad: para coordenadas dentro
de la ventana 18×14 inspecciona cada piso superior hasta el piso activo más alto.
Consulta solo el objeto de índice 0 de cada tile y su propiedad DAT nativa 0
oculta el proyectil: `FUN_004c97b0` obtiene precisamente ese elemento de la pila.
El índice 0 usa el orden de pila nativo, reconstruido por
`resolve_native_tile_object_order`, y no necesariamente coincide con
`tile.objects.front()` del vector de protocolo. El renderer consulta ahora ese
mismo elemento. Para
coordenadas fuera de la ventana solo lo dibuja si su piso coincide con el piso actual de
`FUN_004c3180`. Esa pasada viene después del texto, talkboxes y mensaje de estado,
y está dentro del mismo gate `FUN_004f44e0` que los efectos textuales. Evidencia:
`FUN_004daef0` `ev_096ec7dbd303bb0a2d8d51e44021ee53da8283d019e9ec64ecae5192bb29f267`;
`FUN_004ddb70` y su pasada diferida `ev_c098bacf8fb738dcbfe06a7c4f8e5e77d55d8f88c2f4545778468c05ca32ac87`,
`ev_8780c4770fc32cc84963d3c6b259a62767605e76d9bbce55054e96f32c25c757` y
`ev_d1dcffcfd83aa7723da9e8def0649a9b369b1d28af5ee2f6831fb77473be6da9`;
property 0 `ev_0d9c5d401b4ee15ab6da8d76a155c26bc2f5a638736c544fdf48dcb332f02ae7`,
bit de propiedad `ev_9fc39cfeb03dcb30b392f18ac1a77acd5da4da66da6e606683e4b9592e5c4301`,
y piso actual `ev_71171d2f5079f0ca5e89aa0d7da6257824183860873f8a57376b00403e688c7f`.
`FUN_004cea50` obtiene la lista enlazada de efectos de la casilla consultada;
`FUN_004cfa20` registra inicialmente un proyectil tipo 3 mediante
`FUN_004cf0b0` en la casilla de origen. `FUN_004d0690` actualiza su casilla
enlazada cuando cambia el tile actual, con la misma fórmula de redondeo usada
por el snapshot. La consulta de oclusión y la espera de objetos diferidos usan
esa casilla actual. `FUN_004c5d10` y `FUN_004c5fa0` devuelven el desplazamiento
residual como `(destino - tile_actual) × 32 + remaining`; el renderer ancla el
dibujo en `tile_actual × 32` y suma ese residuo una sola vez. Incluso si el DAT
no permite dibujarlo, el snapshot conserva una entrada no dibujable con su
posición actual. `FUN_004cf0b0` inserta el nuevo slot al frente de la lista de la
casilla y `FUN_004ce7b0` la recorre mediante `Effect.Next`; por eso los misiles
de una misma casilla se dibujan de inserción más reciente a más antigua, después
del recorrido por piso, X e Y. El estado global de recursos sin resolver ya no
detiene todos los objetos diferidos: solo la coincidencia de un efecto en la
misma visita los deja en espera. Evidencia:
`FUN_004cea50`
`ev_fe228a6266891b1b9244251b39e2bf3f3e1e318e541dc6ca00770701f41c5e8a`,
`FUN_004cf0b0` `ev_f466c4a63214d359dda0613394fb662a1f9f6bfef6c1d5cb7581131929cb114e`,
y `FUN_004cfa20` `ev_2631f0b2151566779f4d57424b3745d3f87d1016f2a83b2e722542432ec6ff98`;
actualización/reasociación `FUN_004d0690` `ev_061060407d397a83b7b03d2c96797ce4974b4da8449058e44177ca372dac1d6d`,
recorrido de lista `FUN_004ce7b0` `ev_7daf32ea9e5ad06bb85d1510719031db05e1a06822b3b019816c7fff93380c37`,
getters de desplazamiento `FUN_004c5d10` `ev_3cb7fc91c4d91bfb8cb34597b98e62f5289ed50749ea1328978e6a252f47a475` y
`FUN_004c5fa0` `ev_b695bdb58031fb616b6b2b0e0eeda0b45a2ea0a9f2d5c4138ab16418d6688802`.
El host C++ acumula ahora esos proyectiles, aplica la oclusión por pisos superiores
y los dibuja después de talkboxes y del mensaje de estado, ordenados por piso,
casilla X/Y y, dentro de una casilla, desde la inserción más reciente.

## Interpolación temporal de proyectiles

La actualización `FUN_004d0690` recalcula cada componente restante contra el
tiempo hasta el vencimiento. Para un componente de magnitud `m`, duración `d` y
tiempo transcurrido `e`, los bucles enteros dejan
`signo × floor((d - e) × m / d)`, limitado a cero al vencer. El pool C++ calcula
`m - ceil(e × m / d)`, que es la misma cantidad entera, preserva el signo y deja
el proyectil activo cuando `now == deadline`; ambos lo liberan solo después del
vencimiento y de llegar a cero en los dos ejes. No se encontró una diferencia
que justifique cambiar la interpolación actual. Evidencia nativa:
`FUN_004d0690` `ev_dba18b94ae4094061f7d2ccf90e96867853c34b25f2e62c426f2cbb82c3591e2`;
su llamada por frame está dentro de `FUN_004ddb70`
(`ev_ace93e91c73674654b86d9b73ef580b2a8be204b7771196dfadea8eda7374ee6`).

## Pendiente

La columna lateral ya reserva sus 176 píxeles y el host dibuja inventario,
Skills con sus barras, controles de combate y consola. La composición diferida
y la oclusión de 0x85 ya están implementadas. Quedan casos de layout de objetos
sin resolver que dependen de contexto del caller; el frame conserva ese estado
en vez de inventar los valores que faltan. El layout limita el viewport a
480×352 px, el máximo que valida `FUN_004ddb70` antes de mezclar el MapWindow
(`ev_cdb46fc7ca477efb54cf2665b27b702402eb2c613c2c4fd8bd5dd78677810ba6`). El
frame completo aún requiere comparación visual con una sesión del original.

## Límites independientes del viewport

`FUN_004f4500` selecciona la preferencia `NoStretching` al comprobar
`DAT_0075cfe0 == 1` (`ev_e4c455c58a54e7b771315045d246943531b4505506b663978cacca0b6cfe43bd`).
En esa ruta, `FUN_004ddb70` limita por separado el MapWindow a 480×352 px y
calcula los offsets `(480 - width) / 2` y `(352 - height) / 2` para ventanas
menores. Con NoStretching apagado, envía el buffer fuente 480×352 a las
dimensiones actuales del MapWindow sin imponer esos límites. `resolve_game_window_layout`
ahora conserva el área disponible completa en esa ruta; con NoStretching activo,
limita cada dimensión por separado. Se eliminó la proporción 15:11 que no
aparece en el original. El caso de NoStretching en áreas mayores al máximo y la
comparación visual del resize siguen pendientes. Evidencia:
`FUN_004ddb70`, `ev_ace93e91c73674654b86d9b73ef580b2a8be204b7771196dfadea8eda7374ee6`.

# Correcciones del renderizado del mundo

El renderizado del mapa debe emitir cada celda visible aunque la lista de visitas de objetos esté vacía; los efectos y criaturas se recorren por piso y celda de viewport, igual que en `FUN_004ddb70`. `MapWindowTileSurfaceRenderer` conserva ese recorrido de respaldo para no dejar el viewport en el color de limpieza cuando falte una visita de objeto. Los recursos temporales usan rangos de textura separados: el fondo por imagen, los iconos de inventario, las criaturas de batalla y las máscaras de luz ya no se pisan entre sí.

La recolección de luces ahora continúa cuando el frame tiene layouts de objetos sin resolver. `FUN_004daef0` consulta el atributo DAT `0x16` y envía la luz a `FUN_004d9440` antes de llamar a `FUN_004da350` para resolver y dibujar el sprite del objeto. Por eso, el fallo de layout de otro objeto no cancela luces válidas ya recuperables de la misma visita ni las luces de criaturas del snapshot. `collect_map_window_item_light_sources` y `collect_map_window_creature_light_sources` conservan ahora esas fuentes para los estados parciales del frame. Evidencia: `FUN_004daef0`, `ev_5dd750a5ab14d224c13968dec9ddec2c9280ac0894aeebe97df29cf0ed022927`.

La relectura de `MapWindow::draw` confirma los límites de esta etapa. En
`FUN_004ddb70` (Evidence `ev_a6283b410ee5cd3a7f6834d9d642e7c2486da0f1b5f0635e2e8bca66cf3096ac`),
el original obtiene el jugador activo y afirma que existe; calcula los dos
offsets de cámara y suma 32 píxeles a cada uno; deriva el piso de referencia y
construye los valores de filtro antes de recorrer los pisos de 0 al máximo.
`FUN_004c3180` (Evidence
`ev_13ad5e6740c40083db1a2089df6c7d231edfc86db0f2eb2ca76addf9b0443ca1`)
confirma el piso central: `7 - centerFloor` si `centerFloor < 8`, y `2` en caso
contrario. `FUN_004509e0` y `FUN_00450a00` (Evidence
`ev_1caf086d4293cdf52dbc5c013fff8f78b1a286c372ca4da5ddfa955ee5789a26` y
`ev_845f66db7dcd473ff4374aaee953c74a681c438cba6b71606fce413f520d653b`)
confirman que los offsets nativos del jugador reciben el acumulador de
movimiento multiplicado por 32 cuando el objeto es el jugador activo. La
resolución C++ correspondiente está en `resolve_map_window_camera_offset` y
`compute_highest_active_render_floor`. No se agrega un fallback visual si falta
el jugador, el piso o una propiedad DAT, porque el original presupone esos datos.

`FUN_004d90c0` (Evidence
`ev_3f66d44f6076e8a00713e87069c8750895395767247a2e3add058016fb4366f9`)
confirma que se consideran las 18×14 celdas, pero solo se procesan las que
intersectan el viewport con el margen de un tile; una celda ya sometida no se
dibuja de nuevo en la misma pasada. `queue_visible_map_tiles` y
`queue_visible_tile` reflejan ese culling y estado de envío. `FUN_004d5410`
(Evidence `ev_808de26a9a3053db3655df72eb765ad251d1a23d46d50d37e948d20524171cda`)
confirma la conversión de brillo a textura de 32×32 por grupos de 28, con los
valores 1–27 forzados al primer nivel; `MapWindowLightingRenderer` aplica esa
regla.

El refresco de tiles solicitado por `0x6A`, `0x6B`, `0x6C` y `0x6D` queda
cubierto por la reconstrucción de cada frame. `MapWindowTileCache::begin_frame`
reinicia los contadores actuales; el recorrido de objetos vuelve a llenar la
celda desde el mapa actualizado. `add_tile_render_object` detecta el primer
objeto que difiere de la caché y solicita limpiar y reconstruir la celda; si se
quitaron objetos, `queue_visible_tile` detecta que el conteo nuevo es menor y
reconstruye solo el prefijo que permanece. `MapWindowTileSurfaceRenderer`
limpia la superficie al recibir esa solicitud, compone los objetos actuales y
la vuelve a subir al renderer. Esto implementa el efecto visual de
`FUN_004D36B0` sin conservar una marca dirty; las rutas nativas también emiten
su evento `0xC9` cuando corresponde. El modelo no intenta reproducir la
identidad interna de la caché nativa. Evidencia de llamadas de actualización:
`FUN_0040d790` (`ev_be25a2dc2b9c2501821758eec4de7d9a4038ad974e3fb6beb0093cb7ea3d0fa4`),
`FUN_0040da30` (`ev_1183c192429f469dae0abd903dd62ba338f28af112440d64287fff19e334f8f0`),
`FUN_0040de30` (`ev_eda24f2494b336b5c99f8b81be834b24c008ac1d08fae5d4c770957296bd54ea`),
`FUN_00416d10` (`ev_299fb3f553f442f5207601c3c8c58c2a4893146642cc43124c5a7a283e24020f`)
y `FUN_004d36b0` (`ev_66faf354053dfe00b8705afc7b0865de0f5ea4724db7a0f137f2f1ce12b7172a`).

La revisión del gate del host muestra que el mundo solo se salta si faltan mapa,
recursos o renderer, si no se resuelve el jugador/cámara o layout, o si el frame
no puede resolver la cuadrícula de filtros o el piso activo. Los estados
`unresolved_map_item`, `unresolved_cell_data` y `unresolved_effect_state` sí
entran a la pasada de dibujo. `queue_visible_map_tiles` genera una entrada por
cada una de las 18×14 celdas visibles y `MapWindowTileSurfaceRenderer` las
vacía al viewport aunque no haya una visita de objetos; por tanto, una celda
vacía o un objeto no resoluble no explica por sí solo que todo el mapa quede
negro. El `0x82` sí tenía una ventana de inicialización: el renderer podía
capturar el centinela antes de recibir el primer paquete y conservar valores
actuales en cero. Ahora detecta la transición `-1` → estado válido y copia el
estado inicial completo, conforme a `FUN_004c3270`, que solo interpola los
paquetes posteriores. Esta corrección se compiló en Release, pero su efecto en
el viewport requiere comprobar el cliente en ejecución.

El gate anterior a `render_frame` y las llamadas de presentación del backend ya
se siguen estáticamente. `render_frame` registra cambios de gate, estado del
frame, cantidad de celdas con píxeles y llamadas de dibujo; el registro permite
distinguir falta de mapa/cámara/recursos de una superficie compuesta vacía. Aún
falta observar esos valores en una sesión real y compararlos con el original.
La inspección estática y la compilación no prueban que el cliente reconstruido
muestre correctamente el mundo. El game world sigue sin declararse resuelto ni
1:1 hasta completar la comparación visual y de comportamiento.

## Entrada al mundo y gate de renderizado

`publish_world_snapshot` solo publica la transición a `game_world` cuando hay
mapa y `resolve_map_window_camera_offset` encuentra al jugador local y su
movimiento. Ese gate no exige que el filtro de celdas ni el piso activo hayan
quedado resueltos. En cambio, `prepare_map_window_item_frame` sale antes de
crear celdas visibles cuando falta `render_filter_grid` o
`compute_highest_active_render_floor` devuelve vacío; el host tampoco llama a
las pasadas de terreno, iluminación, criaturas o efectos en esos dos estados.
La interfaz puede por eso mostrar HUD y consola con el viewport negro aun
cuando la conexión y el cambio de pantalla hayan ocurrido. Los estados
`unresolved_map_item`, `unresolved_cell_data` y `unresolved_effect_state` no
activan ese gate y continúan dibujando.

La inspección de la muestra OTBM local había resuelto el piso de referencia,
pero no equivale al mapa recibido del servidor durante la sesión que produjo la
captura. Sin el snapshot o una observación del estado del frame no se puede
atribuir el viewport negro a ese gate ni cambiarlo por un fallback inventado.
La siguiente evidencia necesaria es el estado del mapa recibido, el resultado
de `compute_highest_active_render_floor` y `frame.status`; entonces se puede
comparar el cálculo con `FUN_004ddb70`, que obtiene el jugador activo y deriva
los filtros antes del recorrido nativo.

`WM_PAINT` envuelve `render_frame` y publica `WM_CLOSE` ante una excepción no
controlada. La captura con la ventana aún abierta es consistente con una pasada
que retorna normalmente, así que no hay evidencia para atribuir el negro a una
excepción de sprite o textura. Esto no distingue entre un frame rechazado antes
de la pasada y superficies sin píxeles opacos; ambas rutas siguen abiertas hasta
observar los valores y conteos del frame recibido.

El opcode `0x6D` también mueve criaturas distintas del jugador local. En el
handler nativo `FUN_00416d10` (`ev_97b79d435f24f9a27c044c95363f8e539a5fb5f615436e392052b7544f447790`),
la búsqueda del objeto compara su ID con `FUN_00450770`; `FUN_00456a20` se
llama únicamente si coinciden. El host ahora consulta el objeto en la posición
y stack de origen antes de moverlo, y aplica en ese caso el mismo reset local
que ya se usa al añadir al jugador: limpia los globals de movimiento y copia
el campo `+0x50` de la criatura a `+0x54`, además de limpiar `+0x4C`, `+0x30`
y `+0x34`. No cambia esos globals al caminar otra criatura. El recentering
adicional de cámara sigue pendiente. Si la posición transformada cae dentro de
la ventana, el original emite el evento `0xC9` y marca su celda como sucia en
`FUN_004D36B0`; el host recalcula el frame y vuelve a dibujar las celdas visibles
desde el snapshot en cada pasada, por lo que esa actualización visual deriva
del estado actual sin una bandera de dirty persistente. Evidencia:
`FUN_00409730` (`ev_d0d8be1edd133dd96f07ac7ba3cc157bd4812a03ebeffa9927e6abea75ab198c`),
`FUN_004D36B0` (`ev_6f9564adbf546375c721db992a32faa73adf480e1a6a5465daf876fca9881736`).
La condición nativa de `FUN_00416d10` separa los movimientos con delta de piso
cero y deltas X/Y de magnitud menor que dos: para el jugador local aplica el
reset, y para otra criatura ejecuta `FUN_00450A60` con deltas y duración cero.
Los demás movimientos consultan el atributo DAT `0x00` del tile destino y usan
ese payload como duración. El host reproduce ambas ramas; obtiene además la
velocidad de criatura guardada en `+0x8C`, el quantum de movimiento y dos
lecturas del reloj, y escribe offsets, deadlines, distancias, duración,
dirección y flag de marcha en la proyección. El getter nativo `FUN_004509e0`
devuelve directamente el offset guardado en `+0x30`, y solo suma el delta
pendiente para el jugador local; por eso no añadimos una interpolación por
reloj que el getter no realiza. La comparación visual de esta ruta sigue
pendiente. Evidencia: branch `FUN_00416d10`
(`ev_97b79d435f24f9a27c044c95363f8e539a5fb5f615436e392052b7544f447790`), reset `FUN_00456a20`
(`ev_c4d5bf75ca9d8bf3f722f46083f276c1cfc9eb8b45d03a6ac12899a435e2143e`),
setup `FUN_00450A60` (`ev_38c05a24428432644dd53b8d70124ee11e4142609c48eaed2dce8315102f5344`),
quantum `FUN_004506c0` (`ev_8b1a1d93e70fd5c87401925b32403ca340413fa1656a033fb99e7454dba59a0b`),
reloj `FUN_005251d0` (`ev_55117daa2f0204761450316f8fb6ac4785304ca683d0d18fbf418d369ba9e585`).

La inserción de un objeto añadido por `0x6A` o movido por `0x6D` en la pila del
tile destino no siempre va al final. `FUN_004ccef0` compara en orden las
propiedades DAT 0–3 del objeto entrante con cada objeto que ya está en el tile;
ante una diferencia coloca primero el objeto entrante cuando este tiene la
propiedad. Si las cuatro propiedades coinciden, coloca el objeto antes de una
criatura. `FUN_004cc230` desplaza la pila para insertar en ese índice y mantiene
el límite de diez objetos, descartando el último solo cuando la pila ya está
completa. El host aplicaba un `push_back`, lo que alteraba el orden de capas y
omitía inserciones válidas en tiles llenos. `apply_state6_map_thing_add` y
`apply_state6_thing_move` ahora comparten esa regla y mantienen la pila en diez
entradas. Si falta el registro DAT de algún objeto comparado, conservan el
orden final previo, ya que no hay evidencia que permita inferir sus
propiedades. Evidencia: `FUN_0040d790`
(`ev_be25a2dc2b9c2501821758eec4de7d9a4038ad974e3fb6beb0093cb7ea3d0fa4`), `FUN_004ccef0`
(`ev_6e7a338f0638b3aac075ab9dfce6f61d432326048c8a99702718bf38523b4bc2`),
`FUN_004cc230` (`ev_6858a71b0ed29934a97656f7fbad6b2fba726aa6ddab4214564e3436bb103a63`)
y `FUN_00416d10`
(`ev_299fb3f553f442f5207601c3c8c58c2a4893146642cc43124c5a7a283e24020f`).

En `0x6B` y `0x6C`, las rutas nativas `FUN_0040da30` y `FUN_0040de30`
buscan la criatura anterior en el estado de criaturas y limpian su byte
`+0x90` antes de reemplazarla o quitarla del mapa. No borran el registro de
criatura ni su ranura de batalla. El host antes llamaba a `forget` para una
eliminación, que descartaba más estado que el observado; ahora ambas rutas
ponen `+0x90` a cero y mantienen el resto del registro. Al transformar, el host
vuelve a observar el mapa después de sustituir la pila, para que una criatura
entrante actualice su apariencia y presencia. Evidencia: `FUN_0040da30`
(`ev_1183c192429f469dae0abd903dd62ba338f28af112440d64287fff19e334f8f0`) y
`FUN_0040de30`
(`ev_eda24f2494b336b5c99f8b81be834b24c008ac1d08fae5d4c770957296bd54ea`).

La pila del protocolo no es también el orden de iteración del tile. `FUN_004cbb20`
construye una permutación: conserva desde el índice cero el prefijo contiguo de
objetos que tiene alguna propiedad DAT `0`, `1` o `2`, y agrega el resto de los
índices en orden descendente. `FUN_004c97b0` obtiene el primer objeto de esa
permutación y `FUN_004c9cb0` avanza al siguiente; `FUN_004d6880` consume ambos
lectores al recorrer celdas de `MapWindow`. La misma rutina busca desde el final
de la pila hacia el inicio el primer objeto con propiedad 18 y el primero con
propiedad 19 o 20. Si encuentra ambos, escribe 19 en el tercer campo del objeto
con propiedad 18 cuando el segundo tiene propiedad 19; en caso contrario,
escribe 20. No limpia ese campo cuando ya no existe la pareja, por lo que los
cambios incrementales conservan el último valor, como en el original. El host
ahora aplica esta actualización al decodificar tiles y tras add, transform,
remove, move, strip y floor transition. Conserva intacta la pila
wire para que los selectores de stack sigan siendo válidos, pero ahora usa la
permutación nativa al preparar sprites, objetos diferidos, alturas y fuentes de
luz. Los filtros de piso también consultan el primer objeto iterado donde el
original usa ese lector. Si no se puede resolver el DAT necesario para formar
el prefijo, el cálculo mantiene el estado sin resolver y no inventa el orden.
El getter genérico `FUN_004ea730` trata el tipo interno 99 como una criatura y
solo le asigna la propiedad `0x0d`; por tanto, no pertenece al prefijo de
propiedades `0`–`2`. El modelo host usa `creature_id` para conservar esa misma
separación aunque el paquete haya introducido el objeto con opcode `0x61`,
`0x62` o `0x63`.
Evidencia: `FUN_004cbb20` (`ev_069e6c8319e794eeb3c97e9201e3046c5042efc3d26c2eaca31e84b0e941434c`),
`FUN_004c97b0` (`ev_b5c14a622a2fdeb0f70117748b3abfb7a3506a821670bbff189c0beb101a97c6`),
`FUN_004c9cb0` (`ev_4606e6bd40c76645400c8e7496519358df4b858bdddc289bb5ebefe528ad80c9`),
`FUN_004d6880` (`ev_dc6a4eaf812a6d9cf251ea3ade79c1ccd2b88fabd8cd6360b07f23d50262acba`),
`FUN_004ea730` (`ev_a10fa144c29fea9ae601d93a585459c79082a21ac4c0c2e5bf1ca7508e092160`).

## Compatibilidad del mapa local con los recursos

La inspección offline de `C:\Users\AvalonTM\Desktop\Xeeksot792\data\items\items.otb`
identifica el paquete como `OTB 2.6.5-7.92`. Su entrada de server ID 6589
declara client ID 6589. El `Tibia.dat` del proyecto contiene 6480 objetos de
ítem, IDs 100–6579. El OTBM activo
(`data/world/Evolutions.otbm`) usa el ID 6589 en el tile (999,999,6), junto al
spawn (1000,998,6). Ese client ID no tiene objeto DAT ni sprites resolubles en
los recursos del cliente actual. También hay otros IDs OTBM por encima de
6579.

Los IDs del OTBM son IDs del servidor: hay que aplicar el mapeo server ID →
client ID de `items.otb` antes de sacar conclusiones del mapa; no se deben pasar
directamente al renderer. En el caso observado, 6589 se mapea a sí mismo, así
que ese tile vecino sí es incompatible con este DAT. Esto prueba una diferencia
de recursos en el mapa del servidor, pero no explica por sí sola el viewport
negro: la muestra encuentra ese ID en 14 tiles de 1008 del mapa inicial y los
demás tiles deben seguir llegando por las rutas de sprite resolubles. No se
asigna un sprite sustituto ni se afirma que esto corrija el mundo; la búsqueda
del gate que omite el viewport continúa.

## Verificación del selector de pisos

Se comparó el selector de piso activo del host con la ruta que ejecuta
`FUN_004ddb70`. El original empieza en el índice 7 y examina la cruz de cinco
celdas alrededor del centro. `FUN_004d2e90` permite explorar pisos superiores
cuando ninguna capa de esas celdas tiene la propiedad DAT `0x0f`. Después,
`FUN_004c97b0` obtiene cada objeto de la celda proyectada: el original reduce el
piso activo al piso inferior si encuentra un ground (propiedad `0`) que no sea
especial (`0x17`), o un objeto superior con propiedad `0` o `2` que no sea
especial. `compute_highest_active_render_floor` sigue los mismos índices,
desplazamientos por piso, orden de propiedades y reducción del límite.
Evidencia: `FUN_004d2e90` `ev_7dd32a3969018a5e4f6c166cd74f5fe8107b83247e0aac2e49796e2ec854e674`,
`FUN_004c97b0` `ev_63d13f7a3094aae4c049bfe97d86aa35e33e94e88b4813ae9f121127bd31e7af`
y `FUN_004ddb70` `ev_a6283b410ee5cd3a7f6834d9d642e7c2486da0f1b5f0635e2e8bca66cf3096ac`.
No se encontró una divergencia que justifique cambiar ese cálculo.

La misma lectura confirma que `FUN_004d90c0` valida X `0..17` e Y `0..13`,
descarta celdas fuera del viewport con margen de 32 píxeles, compone el caché
solo cuando cambió y envía una superficie de 32×32 al backend. El recorrido
`MapWindowTileSurfaceRenderer` usa esos límites y separa el llenado de las
superficies de la luz y de las pasadas posteriores. Evidencia:
`FUN_004d90c0` `ev_3f66d44f6076e8a00713e87069c8750895395767247a2e3add058016fb4366f9`.
La comparación estática no explica el viewport vacío: siguen faltando los
valores del frame que recibió el host y una comparación visual de la misma
sesión con el original.

## Variantes de sprites de objetos

Se contrastaron de nuevo los selectores de variante usados antes de componer
cada objeto. `FUN_004da350` obtiene patrones X/Y/Z y fase, aplica primero la
cantidad de la propiedad 5, luego el subtipo de propiedades 11/12 y finalmente
la orientación de la propiedad 18. El C++ mantiene ese mismo orden en
`resolve_object_sprite_variant`; la cantidad usa los mismos umbrales de
`FUN_004ef990`, y el líquido usa la tabla exacta devuelta por `FUN_004e8550`.
El original convierte esa tabla a coordenadas base uno con módulo por las
dimensiones X/Y, igual que `object_sprite_layout.cpp`. Evidencia nueva:
`FUN_004da350` `ev_0c601deb165addb6ab1887286f714cf467c030af18c2218c1da484c3b195a2e2`,
`FUN_004ef990` `ev_6baf91ad2d00891874452e875d47643a0ea952c48f71274f1cc2b256e591ec9`,
`FUN_004efca0` `ev_3d7b742e7ef26614dd432195fd618f4e9ede2362056df7e7f547c56e28a8445a`,
`FUN_004e8550` `ev_b596e89c1657926eec40012e3a38983c9e048a13008e5e9cf53b4a7b11d5a6c9`
y `FUN_004eff70` `ev_921c374b96e5e69d10234d81ebb5168f4978c0dd38bd45bc0cd3e37861fe7bc6`.
Los bucles de celdas X/Y/capa y los offsets por extensión del sprite también
coinciden con el layout C++. No hay un cambio de fuente respaldado por esta
comparación; la selección de los objetos que aún quedan sin resolver depende de
seguir los llamadores que proporcionan su contexto.

## Ruteo de sprites entre celdas

La ruta nativa `FUN_004d9f60` determina las celdas que toca cada sprite usando
su posición de pantalla más el offset de cámara. Para sprites estáticos llama a
`FUN_004d9930` por cada celda intersectada; para sprites animados somete primero
esas superficies mediante `FUN_004d90c0` y luego usa la ruta de dibujo directo.
`FUN_004d5700` resuelve las celdas en el anillo de caché de 16×12 con X/Y local
más el centro del MapWindow. El router C++ aplica el mismo filtro de viewport,
los cuatro posibles vecinos según la fracción del tile, y separa el camino de
caché del sprite animado. Evidencia: `FUN_004d9f60`
`ev_bb62dbcebc725ca6bed5dd3ec6a048f4629fde069a2c4aa1a2885d894540073d`,
`FUN_004d9930` `ev_f23d1fb458ec291b95e76cfdcfc3cb42286f0bcf227c96d7a651ac477f82c401`,
`FUN_004d5700` `ev_9d191ee89e6ea442e67301ef192641a9a7bdbb2e0daaa5f92954417a40606f31`
y `FUN_004d90c0` `ev_3f66d44f6076e8a00713e87069c8750895395767247a2e3add058016fb4366f9`.
`FUN_004d47a0` confirma que una celda acepta el sprite cuando su valor de filtro
es menor o igual al piso solicitado; `route_object_sprite` conserva esa
comparación. Evidencia `ev_d138e77b60d5e49e419e29efaf72b82dbd252fdb0432b2228731524951a35881`.
La comparación respalda el ruteo actual, pero no identifica qué estado recibido
deja el viewport de la captura sin superficies visibles.

## Límites de las barras de criatura

La validación nativa de la posición para calcular oclusión y color de la barra
usa `FUN_00401000(x, 0, 17)` y `FUN_00401000(y, 0, 13)` dentro de
`FUN_004ddb70`. `FUN_00401000` compara ambos extremos de forma inclusiva. El
renderer C++ usaba `<17` y `<13`, por lo que clasificaba como fuera de rango las
criaturas en la columna 17 o la fila 13, aunque pertenecen al viewport de
18×14. Se corrigió a los límites de `map_viewport_tiles_x/y`, que conservan los
índices válidos 0..17 y 0..13. Evidencia: `FUN_00401000`
`ev_d436a9f9ed3c90e5bcf854d122c2dfa112380903ce253071a75a21527f736fe2` y
`FUN_004ddb70` `ev_a6283b410ee5cd3a7f6834d9d642e7c2486da0f1b5f0635e2e8bca66cf3096ac`.
La corrección compiló en Release con `scripts/build.ps1 -Action Build
-Configuration Release -BuildDirectory build-msvc`; se generó
`build-msvc/Release/tibia_client.exe`. El cliente no se ejecutó.

## Oclusión de las barras por pisos superiores

Al revisar la oclusión encontré otra diferencia concreta. Para cada piso sobre
la criatura, `FUN_004ddb70` consulta una sola vez la celda con
`FUN_004c97b0` y evalúa la propiedad 0 del objeto que devuelve. Esa función
resuelve el elemento de índice 0 de la pila (`FUN_004c3eb0(0)`) con el orden
nativo de los objetos. El C++ recorría todos los objetos del tile, así que una
propiedad 0 en un elemento posterior podía volver gris la barra aunque el objeto
de pila 0 no tuviera esa propiedad. Ahora consulta solo el primer objeto de
`resolve_native_tile_object_order`, que reproduce ese índice. Las criaturas y
entradas sin tipo siguen sin tener propiedad DAT 0. Evidencia: `FUN_004ddb70`
`ev_a6283b410ee5cd3a7f6834d9d642e7c2486da0f1b5f0635e2e8bca66cf3096ac` y
`FUN_004c97b0` `ev_63d13f7a3094aae4c049bfe97d86aa35e33e94e88b4813ae9f121127bd31e7af`.
La corrección se compiló en Release con `scripts/build.ps1 -Action Build
-Configuration Release -BuildDirectory build-msvc`; el cliente no se ejecutó.
Tras limitar la consulta al elemento 0 del tile, se repitió la misma build y
terminó correctamente.

## Color y parpadeo de marcas de criatura

La comparación del resto de overlays no encontró otra diferencia de código.
`FUN_00450500` usa los mismos cortes de salud que `prepare_info_overlays`:
`<4`, `<10`, `<30`, `<60`, `<95` y el tramo restante; sus colores son los que
conserva el renderer C++. El original eleva la luz solo para el jugador local,
limitando su nivel a 2..5 y tomando el máximo entre ese valor por `0x33` y la
luz media. `FUN_004d46b0` y `FUN_004d46e0` aplican después la misma escala
entera `color × (luz / 5) / 0x33` del C++.

El blink de los iconos también coincide: `FUN_00502d50` alterna el byte global
cuando el tiempo transcurrido supera el tick guardado más 300 ms y guarda el
tiempo actual; `FUN_00502d80` entrega ese estado al dibujo de `FUN_004ddb70`.
El renderer C++ usa el mismo umbral estricto, alterna una vez y reinicia la
marca temporal al tiempo actual. El host parte de `false` y tick cero. Los
globals nativos solo muestran las lecturas y escrituras de esta rutina; su
valor inicial requiere confirmar los datos de carga del PE, así que no se
afirma aquí que el primer estado visual coincida. Evidencia: `FUN_00450500`
`ev_ab318691b61acb4b7504627c061eb6786894f3d42848cc6d6b6b903461f18569`,
`FUN_00401140` `ev_2193ede9ad8286706d825b5199354767918cfa3655918d1c530423c45c54682b`,
`FUN_004d46b0` `ev_cf6c30b0c12a5756e8dcc5f1e68fce9c262e5d2492fa01db6ddcb5f195e39de4`,
`FUN_004d46e0` `ev_c848c261c24d85d5897dd0748225e3a1b7182d75188271ff44138307f7a42f33`,
`FUN_00502d50` `ev_148c1ae08eab9b31edf192b3643714ebbcc1599eee03619ac952ae643e0dc4a8`,
`FUN_00502d80` `ev_2f6b701245877e0e7d579fc0547caa1fc33b933fbbd9056485a7e2c9f2e90267`
y `FUN_005251d0` `ev_e066eb5df3d8be74d6ad907705e34e0791294a363d7b4ac85a15d06b5b476407`.

## Selección de la ruta de iluminación por backend

Se verificó la condición virtual adicional que rodea la ruta de quads en
`FUN_004ddb70`: `LightEffects` debe estar activo y la ranura `+0xa4` del backend
debe devolver verdadero. En DirectDraw esa ranura apunta a `0x00507140`; la
instrucción lee `this+0x240` y la secuencia devuelve si ese puntero no es cero.
La tabla DirectDraw contiene ese método en `0x005a217c` (`ev_a9a3f4041e57cd33b6e369d2a10f8e689289d18c42e4f68a5758af4d6276e6f3`), y la lectura de
`this+0x240` está observada en `ev_b1414fd57dee7db7d392bd3e43a685dc5e1e4465a30422f53ff1d594a03b97b7`.
En Direct3D 9, la ranura equivalente lee `this+0x1f4`; su constructor deja ese
campo en cero y la inicialización del backend lo establece al crear el objeto
Direct3D (`FUN_0050f9c0` `ev_38511883816c5bc009d4b2786f39ce039f7b853e083243912b02c635d9327`,
`FUN_00515c30` `ev_8f260d43edb3d6db25cd1a12b8eb040a7f10d38652e98d5195831151e42ed93f`,
lectura `ev_900e4a31db589739792607918af36a8a67c619e687227465447b698d27c538b9`).
OpenGL devuelve verdadero sin comprobar un recurso (`0x0052f150`, evidencia
`ev_156b8bc59ad7b4c9a80e850c21cb5aabe0d2f7283f6e55f012697c012c8b2dc4`).

El campo DirectDraw es una capacidad opcional de Direct3D. `FUN_0050f4c0`
inicializa `this+0x240` a cero. `FUN_0050d8d0` consulta en el objeto DirectDraw
la interfaz cuyo GUID en el ejecutable es
`403222bb2be7d011a9b400aa00c0993e` (`ev_668a4dc330644912e713c8a51318ef41bccff04a27a45495affea9647ab3a7f0`), intenta
configurarla y conserva el puntero solo cuando toda esa configuración y
`FUN_0050c500(2)` terminan con éxito. En las rutas fallidas lo libera, lo pone a
cero y crea las superficies de respaldo (`FUN_0050d8d0`
`ev_b1c92962c57ace954b3dff5a47bc490e111fb112cb6666bb0dbca4022082943b`). Así,
el getter `0x00507140` activa quads solo cuando está presente esa capacidad; en
el backend DirectDraw del proyecto, que conserva el renderizado por
`SoftwareSurface` y no crea una interfaz Direct3D, la ruta fiel es la máscara de
brillo de `Tibia.pic`.

Se implementó la misma bifurcación en C++: `GraphicsRenderer::supports_light_quads`
devuelve verdadero para los backends OpenGL y Direct3D 9 inicializados, y falso
para DirectDraw. `MapWindowLightingRenderer::draw_ambient` usa quads únicamente
cuando ambos factores nativos se cumplen (`LightEffects` activo y capacidad del
backend); en los demás casos conserva el recorrido de máscaras. La implementación
por backend se mantiene separada de la elección visual, igual que la llamada
virtual original. Evidencia adicional de las escrituras y la liberación del
recurso: `FUN_0050f4c0` `ev_c5743b93efead0c9306250a5a233f21b5c16a38c024effca410a00d33d511b70`,
`FUN_0050d690` `ev_884c29422325aa891831642279a890d12ec3ce0eff7dae208b1603649c5e902d`.

## Oclusión de proyectiles en la pila del tile

La pasada de misiles de `FUN_004ddb70` consulta cada piso superior con
`FUN_004c97b0` y solo prueba la propiedad DAT 0 del objeto de índice 0 que esa
función obtiene de la pila. Ese índice sigue el orden de objetos reconstruido por
`resolve_native_tile_object_order`; no equivale necesariamente al primer objeto
del vector de protocolo. `projectile_is_occluded` ahora consulta ese mismo
elemento, por lo que un objeto posterior solo puede ocultar el proyectil cuando
el orden nativo lo coloca en el índice 0.
Evidencia: `FUN_004ddb70`
`ev_a6283b410ee5cd3a7f6834d9d642e7c2486da0f1b5f0635e2e8bca66cf3096ac` y
`FUN_004c97b0` `ev_63d13f7a3094aae4c049bfe97d86aa35e33e94e88b4813ae9f121127bd31e7af`;
la permutación del getter y la pila nativa se documentan en la sección
«Orden de iteración de objetos del tile».
La build Release terminó con `scripts/build.ps1 -Action Build -Configuration
Release -BuildDirectory build-msvc`; el ejecutable resultante tiene SHA-256
`F1EC49C22B337F4F5F6C3B28E3113869B4D776284649EE65EFB672BB7479ADF0`. No se ejecutó.
La build Release terminó correctamente con `scripts/build.ps1 -Action Build
-Configuration Release -BuildDirectory build-msvc`; generó
`build-msvc/Release/tibia_client.exe`. No se ejecutó.

## Atenuación de las luces según la celda y el piso

La rutina de luz `FUN_004d9440` recibe las coordenadas de la fuente, el piso y
su radio. Para cada celda calcula la distancia al centro, escala el color por
`(radio × 32 − distancia) / 160` y consulta el objeto de pila 0 en esa misma
celda y piso mediante `FUN_004c9eb0`. Si no hay objeto o el objeto no tiene la
propiedad DAT `0x00`, multiplica el aporte ya escalado por `4 / 5`; si la
propiedad existe conserva el aporte completo. Después limita los canales a
`0..255` y acumula el máximo por canal con `FUN_004d4ac0`. `FUN_004c9eb0`
valida pisos `0..7` y devuelve el objeto solicitado por índice, o el objeto
vacío cuando la pila no llega a ese índice.

La reconstrucción asociaba las fuentes con todo el campo de luz sin conservar
el piso y no aplicaba esa atenuación por celda. `LightSource` ahora guarda el
piso; cada frame prepara una máscara de propiedad `0x00` por piso desde el
objeto de pila 0 de cada visita base, y `LightField::add_source` aplica el
factor entero `4 / 5` al aporte de las celdas sin esa propiedad. Las fuentes
de objetos usan el piso de la visita y las fuentes de criaturas el piso de su
posición de mapa: en `FUN_004db291`, las llamadas a `FUN_004d9440` pasan el
piso del recorrido (`[EBP+0x18]`) como tercer argumento tanto para la luz de
criatura como para sus luces de equipo. Evidencia: `FUN_004d9440`
`ev_cc0757fcbe29f77c8693826ce6a90b1df6c77e2a76df229fdaf6649e9854c66c` y
`FUN_004c9eb0` `ev_4e268764462ad8fee1554456d272835f407c882d522ea7b69fb95b42bd98da89`,
`FUN_004db291` `ev_595b21a07a9e65a196870e48131e66f5c325e2c9d52df98c2927b7524390b4ec`.

La revisión directa de `FUN_004db291` encontró tres detalles que faltaban en la
luz de criaturas. La coordenada parte del ancla de la tile más el movimiento
actual de la criatura, suma el centro de 16 px y resta el desplazamiento DAT
`0x19`; no depende del extent del sprite. Ese desplazamiento se obtiene del
objeto de outfit o del item usado como outfit. Si el tipo y el item son cero,
el original usa `(8,8)` únicamente para IDs de criatura menores que
`0x40000000`. Además de la luz recibida en los campos nativos `0x78/0x7c`, el
original aplica el atributo DAT `0x16` del outfit y da al jugador local una
fuente blanca de nivel 2 cuando su nivel nativo de luz es menor que 2.
`collect_map_window_creature_light_sources` ahora reproduce esos offsets,
ambas fuentes y el mínimo del jugador local. Evidencia: `FUN_004db291`
`ev_8f86314fc150e0090b9e5186aac1b214de49d362b6ea99ffff9bdcdc8d02b93b`,
`FUN_004509e0` `ev_b11541c9c1094a1e0e32a1eb994e948dca8a373679e80620f976c205abb79945`,
`FUN_00450a00` `ev_953429d4266f67ef70139ac5703b4d0294680eafbbb9c9e59e2f8f7ff048e637`,
`FUN_004506d0` `ev_3fe6f987e46b0bba86ae184146ac7bd68496d2b1dcd3ef916ea60531b24c32dd`,
`FUN_004eaee0` `ev_aedb9b7395402ef74535520005dfffc4e0023c0410dc58fdfda2e71ce0e310c0`,
`FUN_004eb090` `ev_ad398f949e8c83e0ad3a9efbd6710e4c41a92f02291919ce6821ef72f016714d`,
`FUN_004e89d0` `ev_4ba6f9618d0e83e1ee3f8baeb775c1e6b12865b6711775fcd31f127496d73b38`,
`FUN_004e8c20` `ev_cbd31618133e6545c79b27a1d07a3f9c873a7f7e235e579fc9888c14fa29413c`,
`FUN_004ec6a0` `ev_0bce20432b943288afb91e026b2eec487c455d8a3edd4070d5d2b464e2faeb07`,
`FUN_004eca00` `ev_ca9f1a44c453f367fe1423d5ce3c99d59bb5ad4a93a4b10fa81bb905fc5d5da7` y
`FUN_004ec850` `ev_bb010e8e8c8cf7ca8533c8597f7c0e62f6df9c6a17ecfb263eb66fbd8feec202`.
La build Release terminó correctamente; no se ejecutó el cliente. Falta
comparar el campo de luz resultante con una sesión del original.

## Mezcla del color ambiente

`FUN_004d4880` mezcla el color global `DAT_00755df4..dfc` (copiado por
`FUN_004c3380`) con el brillo ambiente global `DAT_00610bfc` y el límite de
canal ambiente, que es 200 para pisos de superficie y 255 para pisos bajo
tierra. El original calcula primero el aporte rojo como
`rojo × brillo / 255`; luego obtiene la opacidad inversa como `255 − aporte
rojo` y reutiliza ese mismo valor para combinar los aportes verde y azul con
el ambiente. El código C++ usaba `255 − brillo`, que solo coincide cuando el
rojo de la paleta vale 255. `LightField::blend` ahora conserva el orden de
truncamiento entero nativo y usa el aporte rojo ya calculado como opacidad
inversa común. Evidencia: `FUN_004d4880`
`ev_0cf79619b4132e7a903424fea28738d1ebda3ef997a2577f74a0a1e872a3147b`,
`FUN_004c3380` `ev_25f6354bbe58cd306592a912a6fd39acd852184ff14543ad89dd031fc8d0f4aa`
y `FUN_004c8c40` `ev_a588aa43a88b0a67fc89eff69ff407d78ee0b87455f853049fdd5377d5539798`.

## Ajuste de luz junto a pisos especiales

En la pasada por pisos, `FUN_004ddb70` tiene una segunda mezcla al atravesar
la transición subterránea: cuando el piso actual es `centerFloor − 8 +
FUN_004c3180()` y es menor que 7, consulta el tile diagonal `(x−1,y−1)` del
piso siguiente con `FUN_004d3080`. Esa rutina recorre la pila y devuelve
verdadero si algún objeto tiene la propiedad DAT `0x18`. En ese caso el
original vuelve a mezclar la celda `(x,y)` con el brillo ambiente menos 1.
El renderer no tenía esta consulta. El frame ahora guarda una máscara por
piso para esa propiedad y la pasada de luz aplica la segunda mezcla únicamente
en las coordenadas y pisos observados. Evidencia: `FUN_004ddb70`
`ev_cdb46fc7ca477efb54cf2665b27b702402eb2c613c2c4fd8bd5dd78677810ba6`,
`FUN_004d3080` `ev_5a178af15d380f68d3e5a758f45376285230e96c4cf35abd905c1a1b5ffd93bb`
y `FUN_004c3180` `ev_72a9405e7d9358d3d400d996b0ece548bb5f2503c52cd22d1903c8070c6a9fe3`.

## Posición y color de las fuentes DAT

Se contrastó la recolección de luces de objetos con `FUN_004daef0`. El original
solo consulta fuentes durante la pasada base, lee nivel y color del atributo
DAT `0x16`, aplica los desplazamientos X/Y del atributo `0x19`, resta la altura
acumulada de los objetos anteriores y limita esa altura a 24 píxeles con
`FUN_004193c0`. `collect_map_window_item_light_sources` sigue el mismo orden y
las mismas coordenadas. Los wrappers nativos `FUN_004eab80` y `FUN_004ead30`
leen los dos campos del atributo `0x16`; `FUN_004eaee0` y `FUN_004eb090` leen
los del `0x19`, y `FUN_004eb240` obtiene la altura `0x1a`. La conversión de
color también coincide: `FUN_00427eb0` separa el índice en rojo `index/36`,
verde `(index/6)%6` y azul `index%6`, multiplicando cada canal por `0x33`.
La comparación no encontró una diferencia de implementación en este bloque.
Evidencia: `FUN_004daef0`
`ev_5dd750a5ab14d224c13968dec9ddec2c9280ac0894aeebe97df29cf0ed022927`,
`FUN_004193c0` `ev_95a5a7e0f8e1b6f864f37f2e42fb07ddc359c62745c6cd6bebf1849d2b479e10`,
`FUN_004e89d0` `ev_4ba6f9618d0e83e1ee3f8baeb775c1e6b12865b6711775fcd31f127496d73b38`,
`FUN_004e8c20` `ev_cbd31618133e6545c79b27a1d07a3f9c873a7f7e235e579fc9888c14fa29413c`,
`FUN_004e8e70` `ev_3aab3d951e7fe28814882946bb835873654f8bdd5594037972b2cc38f2fbf9ef`,
`FUN_004e90c0` `ev_24530d08ab589121d8b9a46f3c7003babcc69b6e5d5f7aed5e63685e27fb3669`,
`FUN_004e9310` `ev_4b7de1d906cf1bdb64f7b83f349519cc671513bc14c426d12900a862848f3be0`
y `FUN_00427eb0` `ev_5cc8c05bf34c447b6421a1f3f647c22e9ffd75a2694acd8dd9cae67ad407f04d`.

La propagación de cada fuente sigue `FUN_004d9440`: el radio DAT se convierte a
píxeles multiplicándolo por 32; la distancia se mide desde el centro de cada
tile y la contribución es `color × (radio_px − distancia) / 160`. La rutina
redondea la raíz cuadrada a entero antes de restarla. Si el primer objeto
consultado para la celda no tiene la propiedad DAT `0`, usa el color base
reducido a `4/5`; de lo contrario conserva la contribución completa.
`LightField::add_source` usa el mismo centro, radio, división entera y máscara
de propiedad, así que no requirió cambio de código en esta comparación.
Evidencia: `FUN_004d9440`
`ev_bf5836a5c987f9224cc983883dfa56acedc0679882903ad83cf4442c1871839e`,
redondeo `FUN_0053b554`
`ev_b14eba5416d5e2c6d7d3881d3230929768e21d1144987fc0e46952289d4f45d3`,
escalado `FUN_004d46b0`
`ev_eac99a4257b37f444b16156890bb46db1c2f44c96b05a77bb4fc0b2ca165fd50`
y división `FUN_004d46e0`
`ev_88f96af800ea665f41d79742981151089db9309dfa734b78088fa814be4a0d95`.

## Muestreo del campo de luz en los bordes

La ruta de quads de `FUN_004ddb70` emite una cuadrícula de 19×15 con cuatro
muestras de color por quad. `FUN_004d4820` limita cada muestra a X `0..17` y
Y `0..13`, y usa el índice `x×14+y`. `MapWindowLightingRenderer` genera la
misma cuadrícula y `LightField::clamped_color` aplica los mismos límites e
índice; el orden de sus cuatro esquinas también corresponde a las cuatro
consultas nativas. Esta comparación no encontró otra divergencia en el
muestreo del campo. Evidencia: `FUN_004ddb70`
`ev_cdb46fc7ca477efb54cf2665b27b702402eb2c613c2c4fd8bd5dd78677810ba6` y
`FUN_004d4820` `ev_ecddc9133a48094b04d2ecb55774afdf515a7ce6961b93b394be95e4f793e641`.

La ruta alternativa por máscaras valida un brillo de `0..255`. Los valores
`1..27` seleccionan el tile 1; los demás se dividen entre 28 y se omiten si el
índice supera 7. El dibujo usa la imagen 7 de `Tibia.pic`, con origen de
textura `(índice×32,0)` y tamaño `32×32`. `MapWindowLightingRenderer` aplica
los mismos rangos, omite el mismo excedente y decodifica el tile `(índice,0)`;
por ello no se cambió esa ruta. Evidencia: `FUN_004d5410`
`ev_0c6458efdaabf01ebc48f064b2cbd83f3e87bc264e8f6aa1a2af71fbd7826cf0` y
`FUN_00401000` `ev_d436a9f9ed3c90e5bcf854d122c2dfa112380903ce253071a75a21527f736fe2`.

## Diagnóstico del gate del game world

La revisión de `FUN_004ddb70` confirma que el original reinicia los contadores
de las 192 celdas una vez al iniciar el frame, recorre las capas del mapa y
después somete las 18×14 celdas mediante `FUN_004d90c0`. La preparación del
frame C++ usa también una caché circular compartida y conserva las rutas de
objetos de las capas antes de someter las celdas visibles; no se cambió ese
orden porque la evidencia lo respalda.

Para identificar por qué puede quedar negro el viewport sin alterar el dibujo,
`render_frame` emite a `OutputDebugStringW` y escribe junto al ejecutable en
`tibia-world-render.log` una línea solo cuando cambia el gate o el estado del
frame. El archivo se reinicia en el primer registro de cada ejecución. Registra
recursos ausentes, cámara/layout no
resueltos o, si el frame se prepara, estado legible, cantidad de tiles y
criaturas del snapshot, centro del mapa, ID del jugador local, objetos
decodificados en todas las tiles, bytes sobrantes del stream, visitas, objetos,
rutas de sprite resueltas y fallidas, rutas diferidas, tiles sometidos, piso
activo y offset de cámara. La línea del frame también cuenta las tiles con un
objeto que tiene la propiedad DAT `0`, los IDs de objetos ausentes de DAT y la
posición de mundo de la criatura que coincide con el jugador local. Esto
distingue un mapa recibido sin terreno de un fallo de proyección del jugador o
de recursos DAT ausentes, sin alterar el dibujo. Después de la pasada informa
llamadas de dibujo de tiles, efectos, luces, celdas
visibles con píxeles no transparentes y el total de píxeles con alfa distinto
de cero en las superficies compuestas. Así se distinguen un stream incompleto o
un snapshot sin objetos de una superficie poblada que el backend no presenta.
La ruta de entrada de
alfa se observa en `decode_tibia_spr_image`: los tramos transparentes producen
alfa 0 y los tramos de color alfa 255; D3D9 y OpenGL configuran mezcla
`SRC_ALPHA`/`ONE_MINUS_SRC_ALPHA`. El registro no añade contenido a la UI ni
elige sustitutos visuales. La comparación estática usa `FUN_004ddb70`,
Evidence `ev_cdb46fc7ca477efb54cf2665b27b702402eb2c613c2c4fd8bd5dd78677810ba6`;
el registro extendido compiló con `scripts/build.ps1 -Action Build
-Configuration Release -BuildDirectory build-msvc`. El ejecutable
`build-msvc/Release/tibia_client.exe` tiene SHA-256
`DAC211B7BDE44839257495DDE32EC43AC77D132AC39365215F3F47EFB030B43C`. El
cliente no se ejecutó, así que el estado observado en una sesión sigue
pendiente.

## Orden de terreno, iluminación y criaturas

La pseudocodificación completa de `FUN_004ddb70` muestra el orden del frame:
recorre los pisos y llama `FUN_004daef0` por celda; después recorre todas las
celdas con `FUN_004d90c0`; aplica la iluminación ambiental con `FUN_004d5410`
o la malla virtual de luz; copia el MapWindow al destino mediante el slot
virtual `+0x30`; y entonces recorre la lista de criaturas y dibuja sus
indicadores. La evidencia es el cuerpo de `FUN_004ddb70`,
`ev_cdb46fc7ca477efb54cf2665b27b702402eb2c613c2c4fd8bd5dd78677810ba6`.

El host antes invocaba `draw_tile` para criaturas dentro del recorrido de
composición y aplicaba la luz al terminar toda esa pasada. Eso hacía que la luz
oscureciera también a las criaturas, en un orden distinto al original. Ahora
`MapWindowTileSurfaceRenderer` aplaza las llamadas por celda hasta que termina
el terreno, ejecuta `before_overlays` y luego procesa los overlays. El host
aplica la luz en `before_overlays`, así que las criaturas se dibujan después,
como en la rutina nativa.

La ruta de efectos no pertenece a esa pasada posterior: `FUN_004daef0` recorre
los efectos de la celda durante el recorrido por piso y emite sus sprites por
`FUN_004d9f60`, antes de que `FUN_004ddb70` someta todas las celdas y aplique la
luz ambiental. `FUN_004d90c0` confirma que la caché del tile se presenta después
del recorrido de composición y con el recorte nativo de 18×14 celdas. El callback
de efectos C++ ahora se ejecuta durante cada recorrido de piso. El renderer
acumula los pisos y presenta cada superficie al terminar esos recorridos antes
de aplicar la iluminación una sola vez, igual que el original. Si el enrutador
observado necesita un sprite directo porque esa celda ya se presentó, primero
envía la celda y dibuja luego el sprite encima; esa excepción conserva el orden
de `FUN_004d9930`. El terreno puede ocluir efectos en el mismo orden que el
original y las criaturas quedan después de la iluminación.
Evidencia: `FUN_004daef0`
`ev_5dd750a5ab14d224c13968dec9ddec2c9280ac0894aeebe97df29cf0ed022927` y
`FUN_004d90c0` `ev_7991357d1fcb4dea098c09fdfc772be918d067b4dd45dada413e0a31b4ca8c54`,
`FUN_004d9930` `ev_c2a3cb63cd9db49d0203ceb60fd4f4460d005843fa3642b9731d8645d3f03ddb`
y `FUN_004ddb70` `ev_cdb46fc7ca477efb54cf2665b27b702402eb2c613c2c4fd8bd5dd78677810ba6`.

