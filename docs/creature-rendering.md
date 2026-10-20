# Apariencia y dibujo de criaturas

## Evidencia

- `Tibia.exe`, SHA-256
  `fe1172d0ff2e3024b78125184a4ebcdbb0489b9629ff744184ca130033d6321a`.
- `FUN_004da800` (`ev_c4648ffa2169b980df4126ab9bd1ab22d86909d2f7bd27eb5c6cf9d8778ee57e`)
  requiere una criatura y un tipo de outfit no nulo. Comprueba que el ancho del
  patrón del outfit sea 4, obtiene los cuatro selectores de color desde los
  campos `+0x64`, `+0x68`, `+0x6C` y `+0x70`, y lee la máscara de addons en
  `+0x74`. Esa máscara se limita con el resultado de `FUN_004edae0`. La rutina
  recorre los patrones Y, X y las capas; al resolver un sprite primario también
  resuelve la segunda capa cuando el outfit tiene dos.
- `FUN_004da800` (`ev_b2dff2917f4001a788dd7c26168359cb62e520ba85e7ac30d907ef7f1ff830bc`)
  dibuja cada celda con
  `x = ancla_x + 32 - indice_x * 32 - extent_x` y
  `y = ancla_y + 32 - indice_y * 32 - extent_y` antes de llamar a
  `FUN_004d9f60`. `FUN_004e9a00`
  (`ev_fa381c10512464c984b9d6746dfee98aa47cbec24aae4e7000bc058c07d24e47`)
  confirma que el primer índice es X y el segundo Y. Los getters
  `FUN_004ecf10` (`ev_2ce1917460706398f0ee1748079d0e13a96d40768a761f1b7734df30eea831b2`)
  y `FUN_004ed270` (`ev_f6ccc9337306b0268b337d67e5eeae1080522b2a81b706055e84f2cff16412c8`)
  leen Width y Height, respectivamente.
- En la misma rutina, si el outfit contiene atributo 28, la fase es
  `(FUN_00502800() / 3) % fases + 1`. `FUN_00502800`
  (`ev_9a27a4ae1ffaeed51bf74a95d9ef2bcbf7e9d08e66483b783f199a8e03ad809f`)
  devuelve el valor global `DAT_0075D444`.
- Si el outfit no tiene atributo 28, `FUN_004da800` llama a `FUN_00450ef0`
  (`ev_8da0e67a49cc1950a8b52717698d87be1b548954312c13c541f3f4d59d2d4eb9`).
  Para una fase, devuelve 1. Si las dos coordenadas de movimiento en `+0x30` y
  `+0x34` son cero y el tick en `+0x38` ya expiró, también devuelve 1. En los
  demás casos usa `max(abs(x),abs(y)) / 8 % (fases - 1) + 2`.
- `FUN_005251d0` (`ev_f56f8705f7fae69e5a544d705fc7724aa40cb71ea055712198d2e9dd9589da84`)
  proporciona el tick de expiración como `timeGetTime() - DAT_0075EA28`.
  `FUN_00401140` (`ev_ce4cbd6065056c779b5bf4f1436957387e28d2ad57036612cd77ba5ca2d1075c`)
  devuelve el mayor de los valores absolutos entregados.
- `FUN_00502810` (`ev_18a4bd45e93f3722d911b5200b8b83a1c6cdf5d60500345d6ab4cf23f9a42fc2`)
  incrementa `DAT_0075D444` en uno. `FUN_0044CDC0`
  (`ev_12533f160cc5b9a1aa195cf3c01209581058aea27f1002d0c3960d1725366c18`)
  llama a ese incremento al entrar y luego realiza las actualizaciones del ciclo.
- `FUN_004edae0` (`ev_727daf8896ff92f35f1551d26a43ae82025e7023e9c01ef1c3d4832974ec45f7`)
  devuelve la máscara permitida `1` para `pattern_y == 2`, `3` para `3`, `7`
  para `4` y `0` en los demás casos. `FUN_004da800` combina esa máscara con el
  valor de la criatura y omite los patrones addon que no estén habilitados.
- `FUN_00450a20` (`ev_3996d19858031ec5c32065b2715d9aa52d595fc201c2e4998760a91c09a8524b`)
  devuelve `DAT_00605A18` para la criatura cuyo primer campo coincide con
  `DAT_006059D0` cuando alguno de `DAT_00605A20` o `DAT_00605A1C` es distinto de
  cero. En los demás casos devuelve el campo `+0x54` si el byte `+0x4C` no es
  cero; si es cero devuelve `+0x50`.
- `FUN_00456010` (`ev_2331130988efb1b2787e9a9e9cfd65c101a66fb6e91cf947e6cfa414260b60a6`)
  selecciona `DAT_00605A18` a partir del delta de movimiento: X positivo da `1`,
  X negativo `3`; con X cero, Y negativo da `0` y Y positivo `2`. Para diagonales
  prevalece el signo de X.
- `FUN_004edcb0` (`ev_434c0bf5bd6c47e9344d7abd4b1e9d4a3807828ba8466756fe0b3f418ef9f8c6`)
  incrementa en uno el valor de dirección antes de pasarlo al resolvedor de
  sprites y pasa el patrón Z con el valor constante `1`. El valor que
  `resolve_creature_outfit_direction_for_state` entrega al compositor es por
  tanto el índice de patrón X basado en uno.
- `FUN_004509e0` (`ev_dc8c0df882b3f1c3bef4ba1d7fbe2d5db797f2adc7b8f1bceb43c6defa73493e`)
  devuelve el offset nativo de movimiento X y, para el jugador local, le suma
  `DAT_00605A20 * 32`. `FUN_00450a00`
  (`ev_25628ed615c5cc4f9ea958ee3fd5139c01f165c050f71f972197486479836d8c`)
  aplica lo mismo a Y con `DAT_00605A1C` y el campo `+0x34`.
- `FUN_004ddb70` (`ev_7a7e7b6faffcf01d802af4430ba70f0fae82d4ddf334812d0c9329119d06be04`)
  inicializa los offsets de cámara en `this+0x29cc` y `this+0x29d0` como el
  resultado de los getters anteriores para el jugador local más `32`. En el
  barrido de tiles calcula cada destino como `tile_x*32-camera_x` y
  `tile_y*32-camera_y`. `resolve_map_window_camera_offset` y
  `project_map_window_tile_position` reproducen esa operación desde el snapshot;
  devuelven sin resolución cuando falta la criatura del jugador local o hay
  desbordamiento fuera del rango nativo.
- `FUN_00473650` delega la conversión de selector en `FUN_005328f0`
  (`ev_05ea1c55aa002d83a6b39f90c6b46bf5b02d3f701688c4eb256509b6b02f2852`).
  Esta última limita selectores mayores que 132 a cero y calcula el color
  resultante. Los cuatro valores convertidos se almacenan consecutivamente y se
  entregan como paleta a `FUN_004d9f60` para el dibujo enmascarado.
- `FUN_004d9f60` (`ev_f5d191acd9c06556f5355f4097f71581b08d1adc87ba3f65d51d8b4c371b137e`)
  somete las celdas afectadas antes de dibujar el outfit en el viewport. Cuando
  hay sprite de máscara, llama a `FUN_0049e8c0` con el sprite base, la máscara y
  la paleta.
- El convertidor DirectX9 `FUN_00512fe0`
  (`ev_a70b0046a5521fe7942de71ca842004e116dabdfb088c949c70419c3f2f21fd4`)
  recorre el sprite base y la máscara como datos RGB de tres bytes por píxel.
  Reconoce los marcadores amarillo, rojo, verde y azul de la máscara solo por
  sus canales RGB; ese camino no consulta alfa. Para esos marcadores calcula la
  intensidad como `(G*59 + B*11 + R*30) / 100`, multiplica cada canal de la
  entrada de paleta por esa intensidad y desplaza ocho bits a la derecha; el
  píxel resultante queda opaco. Por ello `colorize_tibia_outfit_tile` también
  clasifica los marcadores RGB cuando el alfa ya decodificado de la máscara es
  cero. Para píxeles sin marcador, el convertidor conserva el RGB base y deriva
  su alfa comparándolo con la clave de color configurada para la superficie.
- `FUN_004db291` (`ev_595b21a07a9e65a196870e48131e66f5c325e2c9d52df98c2927b7524390b4ec`)
  reúne candidatos de las nueve celdas alrededor de cada ancla local, limita la
  lista a diez y los ordena por Y y después X antes de procesarlos. El barrido
  valida coordenadas locales `x` en `0..17` e `y` en `0..13`. Para obtener la
  coordenada usada al ordenar calcula
  `x = movimiento_x + (tile_x - referencia_x) * 32 - extent_x` y el mismo
  cálculo en Y. Acepta un candidato para el ancla cuando ambos offsets están en
  `[-31,0]`, mediante dos llamadas a `FUN_00401000`; así cada sprite queda
  asignado a la celda cuya región cubre su esquina de anclaje. `render/creature_render_order`
  reproduce el intercambio por Y y X. Evidence del filtro, límite y registro de
  candidatos: `ev_595b21a07a9e65a196870e48131e66f5c325e2c9d52df98c2927b7524390b4ec`
  y `ev_7a0fb81b027c6f02621cb5e97cc1992bbdfc209bdbff3f7d2f3b46bc0c3919f9`;
  el predicado inclusivo es `FUN_00401000`, `ev_0c0cd0e038b7f23593d20acf125c182aa09e310eee6da3b7a765bf8d8b5164e1`.

## Implementación

`State6CreatureStateStore::apply` conserva los campos recibidos por el opcode
`0x8E`. `resolve_creature_outfit_appearance` lee el tipo y los cuatro selectores
de esos campos, resuelve el registro en la categoría de outfits de `Tibia.dat`,
convierte los colores mediante `tibia_outfit_color` y aplica la máscara de addons
derivada del número de patrones Y. Si falta el registro o un campo, devuelve
`nullopt`. `resolve_creature_outfit_phase` implementa ambas reglas de fase usando
`CreatureOutfitPhaseContext`; el reloj, el tiempo desde el epoch y los valores de
movimiento son entradas explícitas. `resolve_creature_outfit_phase_for_state` extrae los
valores de movimiento y el tick previo de `+0x30`, `+0x34` y `+0x38` del estado
nativo; deja el reloj de animación y el tiempo desde el epoch como entradas
explícitas. `resolve_creature_outfit_direction_for_state` aplica la selección de
`FUN_00450A20` usando el id local, los dos indicadores de movimiento y los campos
de dirección de la criatura. `LocalPlayerMovementState` conserva ahora
`DAT_00605A18`; `apply_observed_state6_player_move_direction` actualiza ese valor
desde los deltas del movimiento local antes de mover la entrada en el mapa. El
resultado incorpora el ajuste `+1` observado antes de resolver el patrón X.
`compose_creature_outfit_appearance` adapta la apariencia al compositor existente.
`compose_creature_outfit_from_state` reúne la resolución de apariencia, dirección
y fase, y usa el patrón Z `1` observado en la llamada original para producir la
superficie del outfit.

`ClientHost::animation_clock` avanza una vez por cada tick del timer original de
ventana. `FUN_004bbb50` maneja `WM_TIMER` y llama a `FUN_0044cdc0`;
`FUN_004bdee0` instala el timer ID 1 con intervalo de 100 ms. Los repintados no
avanzan ese contador. Evidencia REA: `ev_f012101cef7d91b8a8fc6c35cab99d0d749951e8951c455a345b28bbfd6cdf47`
y `ev_197534fee03c65d8e156772b253c1c0b2cf1e4ee3767390cf0e45522e5fda57c`.

`GameWorldSnapshotMailbox::publish`

copia al snapshot los estados de criatura que corresponden a ids presentes en el
mapa, manteniendo el primer orden observado de cada id; el frame ya puede consumir
esas copias sin acceder al almacén propiedad del hilo de red. Cada entrada conserva
además la coordenada de mundo y el piso de la casilla del mapa donde se encontró.
Los registros `0x61`, `0x62` y `0x63` pueden referirse a una criatura viva: el
tercero actualiza su dirección y no elimina su ID.
El parser conserva también el nombre de criatura leído en la aparición `0x61` y lo
propaga a `GameWorldCreatureSnapshot::name`. Las apariciones `0x62` reutilizan el
nombre asociado al mismo ID conocido al recorrer el mapa o su actualización;
`0x63` conserva el nombre previo y cambia únicamente la dirección. El límite de
nombre continúa siendo el del paquete observado: hasta 29 bytes. Esto aporta los
nombres reales para el panel Battle sin inventar etiquetas ni volver a consultar
el almacén del hilo de red.
Cuando una actualización `0x62` llega para un ID que todavía no existe en el
registro del cliente, el original consume salud, dirección y el selector de
apariencia; con selector distinto de cero consume cinco bytes de outfit y luego
velocidad. El lector C++ consumía seis bytes de outfit en esa rama, desplazando
un byte la lectura de las casillas siguientes. `skip_unknown_creature_update`
reproduce ahora las dos ramas de descarte observadas y se comparte entre el mapa
inicial, las tiras y los cambios de piso. Evidencia REA: `FUN_0040d1a0`
(`ev_488fb360455412b46a86ce3a697143730af5df78aac78846a3e4cec5fe23c7c8`),
que delega el registro `0x62` a la lectura de una criatura existente o consume
solo esos campos cuando no encuentra el ID.
El snapshot también copia `LocalPlayerMovementState`, incluida la dirección local,
para que el plan de render pueda consumirla desde el hilo gráfico.
`map::viewport_map_position` proyecta cada criatura al espacio que consume el
renderer: además de X/Y local, calcula el piso relativo usando `center_floor`,
el piso del tile y la regla superficie/underground ya documentada en
`map-render-queue.md`. `GameWorldCreatureSnapshot` conserva ahora esa posición
completa; el plan de dibujo filtra por el piso proyectado y las fuentes de luz
de criaturas usan el mismo piso relativo. Antes ambos caminos comparaban el piso
mundial directamente con el índice de piso del viewport, lo que podía colocar
las criaturas en otra capa. Las coordenadas saturadas en los bordes globales
siguen sin resolución, donde la transformación inversa no es única.
`render/creature_projection` implementa los offsets de movimiento, incluida la
contribución global del jugador local con aritmética modular de 32 bits, y la
fórmula de posición usada para clasificar el candidato. Recibe explícitamente las
coordenadas local y de referencia y los extents del outfit; ahora la coordenada
local queda publicada. `prepare_creature_render_plan` reúne la apariencia DAT, los
extents del atributo 25, el movimiento y la proyección para el piso solicitado.
Asigna cada sprite a la celda que contiene su ancla, conserva hasta diez
candidatos por celda y aplica la comparación Y/X dentro de cada celda en el orden
X-luego-Y del viewport. Conserva los ids sin resolver en una lista separada.
`compose_creature_render_batch` consume ese orden y prepara
las superficies de outfit desde los registros DAT/Spr, conservando la clave de
orden de cada criatura y los ids cuya apariencia o fase no se pudo resolver. El
reloj y el tiempo desde el epoch siguen siendo entradas explícitas.
`draw_creature_render_batch` dibuja las superficies en orden sobre un canvas y recibe el origen como parámetro. El layout observado del MapWindow usa origen `(0,0)`. `MapWindowCreatureRenderer` dibuja cada outfit por la casilla de su ancla, con origen `(32-camera_x, 32-camera_y)`, calculado desde `FUN_004db291` y el ajuste de 32 px de la rutina del outfit.
`render_creature_frame` encadena el plan, la composición de sprites y el dibujo, y devuelve cantidades compuestas, dibujadas y no resueltas. Su piso, referencia de cámara, origen, reloj y tiempo desde el epoch son entradas explícitas. La ruta de `win32_client` invoca el renderer al recorrer el frame del MapWindow. `MapWindowCreatureRenderer` prepara las criaturas por piso y las dibuja al visitar la celda del ancla, en fragmentos 32×32 recortados al viewport. La fase de outfit recibe el contador global de 100 ms. El tick de movimiento y el epoch usan `timeGetTime`; el inicializador nativo guarda la hora de arranque en `DAT_0075EA28`, y `win32_client` conserva esa referencia en `animation_epoch_tick`.


`FUN_004db291` calcula el ancla de criatura como movimiento más
`(tile_local-referencia_local)*32` menos el extent del outfit. La rutina de
outfit añade 32 píxeles al ancla antes de dibujar; por eso las superficies usan
referencia local `(0,0)` y origen `(32-camera_x, 32-camera_y)`. Esta combinación
reproduce el desplazamiento de pantalla sin escoger una referencia arbitraria.

`FUN_004ddb70` (`ev_ace93e91c73674654b86d9b73ef580b2a8be204b7771196dfadea8eda7374ee6`)
recorre los pisos activos en orden ascendente y, dentro de cada piso, X `0..17`
como bucle exterior y Y `0..13` como bucle interior; llama a `FUN_004daef0` una
vez por casilla con su origen `tile*32-camera`. La rutina `FUN_004daef0`
(`ev_d3c015cabc736bbc86160dfe91abd0be4a6d5915536a3ea65b7fda5509291a96`)
procesa primero los objetos de la casilla y las visitas recursivas requeridas
por objetos altos. Después reúne hasta diez criaturas de las nueve casillas
vecinas, ordena por Y/X y dibuja sus outfits. `MapWindowTileSurfaceRenderer`
agrupa ahora cada visita primaria con sus pasadas vecinas, termina el fondo y
los objetos de esa casilla y emite `after_tile`; `MapWindowCreatureRenderer`
prepara los outfits una vez por frame y dibuja ahí las criaturas cuya ancla
pertenece a esa casilla. Sus fragmentos de 32×32 conservan el orden compuesto
de cada outfit. Esto reemplaza la pasada de criaturas al final del piso y
recupera la intercalación observada con objetos altos. La pasada de efectos
posicionales sigue su propio orden, descrito a continuación.

El callback `after_tile` dibuja el outfit y luego el efecto 0x83 de esa casilla,
en ese orden. Después de recorrer todos los tiles, `FUN_004ddb70` hace la pasada
global de iluminación; el host llama a `MapWindowLightingRenderer::draw_ambient`
en el mismo punto. Antes, el host aplazaba las criaturas hasta después de la luz,
lo que dejaba los outfits fuera de la composición de iluminación. Evidencia:
`FUN_004daef0` (`ev_d3c015cabc736bbc86160dfe91abd0be4a6d5915536a3ea65b7fda5509291a96`)
recorre criaturas y efectos posicionales dentro de cada visita; `FUN_004ddb70`
(`ev_ace93e91c73674654b86d9b73ef580b2a8be204b7771196dfadea8eda7374ee6`) recorre
las visitas antes de la pasada final `FUN_004d5410` o la ruta de quads.
La build Release terminó con `scripts/build.ps1 -Action Build -Configuration
Release -BuildDirectory build-msvc`; el ejecutable resultante tiene SHA-256
`5CF259C551D8106470BB38CD67918CB9CC931AEB5DDBAE8DFD03E4B1B3D326C4`. No se ejecutó.

`FUN_004daef0` consulta `FUN_004cea50(x,y,floor)` después de dibujar las
criaturas de la casilla (`ev_d8aabcf41b81eae8163b50a5f729f7c27c343d2dbaffa700e62e2160c6ea8174`;
llamada y consumo en `ev_d3c015cabc736bbc86160dfe91abd0be4a6d5915536a3ea65b7fda5509291a96`).
La rutina valida el slot con `FUN_004c4a30`, que devuelve el estado de
`Effects[EffectNumber]` (`ev_131982805c64ff6acc9b151584f5218f72a0a18fa70dce03006a8493a15d3d1b`).
`MapWindowEffectRenderer::draw_tile` dibuja ahora el efecto 0x83 de esa posición
después de la criatura, manteniendo su tick y fase del DAT. Los textos 0x84 se
recogen en la pasada posterior del MapWindow y solo se dibujan si
`TextualEffects` está activo (`FUN_004f44e0`, `ev_4e463b5b4fbc585393e78b1ed6f644b4235fd5e5ebb7c1bbaf26e93d5da0bb8e`;
pasada posterior en `FUN_004ddb70`, `ev_ace93e91c73674654b86d9b73ef580b2a8be204b7771196dfadea8eda7374ee6`).
Los 0x85 con estado 2 se guardan durante el recorrido de cada casilla y se dibujan
en una pasada posterior a talkboxes y mensaje de estado. Antes de dibujarlos,
el original revisa los pisos superiores de la misma coordenada local; cualquier
objeto con propiedad DAT 0 los oculta. Fuera del viewport, el piso del proyectil
debe coincidir con el piso actual del mapa. La build C++ ahora conserva esa pasada,
la oclusión y el orden por piso/casilla/slot (`FUN_004daef0` `ev_096ec7dbd303bb0a2d8d51e44021ee53da8283d019e9ec64ecae5192bb29f267`;
`FUN_004ddb70` `ev_c098bacf8fb738dcbfe06a7c4f8e5e77d55d8f88c2f4545778468c05ca32ac87`;
pasada diferida `ev_d1dcffcfd83aa7723da9e8def0649a9b369b1d28af5ee2f6831fb77473be6da9`).

El contador de ticks de cada ranura alimenta la fase de animación obtenida desde
`Tibia.dat`.
`compose_tibia_outfit` ordena las celdas de la superficie de derecha a izquierda
y de abajo hacia arriba, de modo que cada índice creciente queda 32 píxeles más
arriba y a la izquierda, como en la fórmula original. El canvas compensa la
extensión de `(width - 1, height - 1)` tiles respecto de la clave usada para
ordenar.

El orden de entrada para criaturas con la misma posición también conserva el
recorrido nativo. `FUN_004daef0` inspecciona las celdas vecinas con X como bucle
exterior y Y como bucle interior, y recorre cada lista de objetos antes de
ordenar las criaturas por Y/X. El snapshot puede incorporar celdas recibidas en
actualizaciones posteriores, así que su orden de almacenamiento no basta para
reproducir ese desempate. `prepare_creature_render_plan` ordena las fuentes por
posición local X/Y con orden estable. Al publicar el snapshot, cada tile se
recorre mediante `resolve_native_tile_object_order`, que proporciona el orden
de pila consumido por `FUN_004c97b0` y `FUN_004c9cb0`; así se preserva el orden
de criaturas dentro de una misma celda. La ordenación final usa la misma
comparación Y/X nativa. Evidencia: `FUN_004daef0`
`ev_d3c015cabc736bbc86160dfe91abd0be4a6d5915536a3ea65b7fda5509291a96` y
`FUN_004cbb20`
`ev_069e6c8319e794eeb3c97e9201e3046c5042efc3d26c2eaca31e84b0e941434c`.
La build Release terminó con `scripts/build.ps1 -Action Build -Configuration
Release -BuildDirectory build-msvc`; el ejecutable resultante tiene SHA-256
`260BC61F003B276E3A0F4D5ED8B48422595BD044F167BF9B73A5C7896E9A1EFD`. No se ejecutó.

La posición de la superficie compuesta ya no suma un tile extra. `FUN_004daef0`
entrega a `FUN_004da800` el origen de la celda y el desplazamiento de movimiento;
`FUN_004da800` recorre las celdas DAT desde 1 y dibuja cada una con
`origen + índice * -32 + 32 - extensión`. En la celda superior izquierda, el
`+32` se cancela con el índice 1-based; al componer la superficie completa, su
origen queda en `posición ordenada - (dimensión - 32) - cámara`. Se eliminó el
`+32` adicional que desplazaba todas las criaturas a la derecha y abajo.
Evidencia: `FUN_004da800`, `ev_b2dff2917f4001a788dd7c26168359cb62e520ba85e7ac30d907ef7f1ff830bc`;
llamada y coordenadas en `FUN_004daef0`, `ev_5dd750a5ab14d224c13968dec9ddec2c9280ac0894aeebe97df29cf0ed022927`.

Verificación del bloque: `scripts/build.ps1 -Action Build -Configuration
Release -BuildDirectory build-creature-order` terminó correctamente el
2026-10-16 después de intercalar criaturas, efectos 0x83 y la pasada diferida
de proyectiles 0x85. El ejecutable de esta revisión se compiló, no se inició.

`resolve_map_window_layout` reproduce el layout observado en
`FUN_004c1d50` (`ev_a19b019e154d486245d6261eeda6bf2a809bf87058dfd6e6a7d0c9d2d2e011de`):
el MapWindow se coloca en `(0,0)` y se dimensiona a `width = client_width - 176`,
`height = client_height - max(panel_height, console_height) - 5`. Si la altura
calculada queda por debajo de la altura mínima del MapWindow, el original
recalcula con `panel_height`. `FUN_004a7650`
(`ev_97f1c3a8441049d4ac89f925463123071691ddc7df13f21ac42104b94d201390`)
inicializa los márgenes horizontal y vertical a 176 y 5. `consoleheight` se lee
de la configuración en `FUN_004f93e0` y su valor predeterminado es cero en
`FUN_004f5820`. El canvas acepta directamente el resultado de ese layout; las
alturas del panel y de MapWindow deben proceder de sus controles originales.
La revisión de `FUN_004c1d50` en `Tibia.exe` (SHA-256
`fe1172d0ff2e3024b78125184a4ebcdbb0489b9629ff744184ca130033d6321a`, Ghidra
12.1.4; Evidence `ev_d733fcb3b79d66cc3fc21257e11cc8609e2d4c89f5c703b0f7d42e4fb1ea7b6c`)
confirma la resta horizontal de 176, la reserva vertical de 5 y la comparación
con la altura mínima del MapWindow; si no cabe, recalcula usando la altura del
panel. `FUN_0044a8a0` crea esta configuración al entrar al juego
(`ev_b82333a08e79a6fd7604cbcc182f32e4848361003cbdbd4731ad2604c2efabca`), y
`FUN_004c2c50` también la invoca al construir la ventana
(`ev_411ba7d5109f7b13b30d5612a658b0cb03da891c13c4afc470a1e042756fde7c`).
El getter virtual del panel, usado por `FUN_004c1d50`, lee `DAT_0060cdb0`;
`FUN_004a7650` establece esa variable en `0x6a` (106). La tabla virtual del
panel y la escritura se confirmaron con Evidence
`ev_0c70875204efa32e4a0ee7c715c8eb64c3f61326dd55122676180d8c0876f2f9` y
`ev_5a622ade92ad9d03c7b4cb67325baddff715f1e3dbddf4420feecfd83614b3bf`.
`MapWindowLayoutInput` usa 106 como altura de panel predeterminada. El mínimo
del MapWindow queda identificado por su vtable: el slot `+0x40` en
`PTR_FUN_0059fba0` apunta a `0x004d4790`, cuyo `MOV EAX,0xb0` devuelve 176
(tabla `ev_2398d179eb38096bcdb36bbba04f1d2ff8b33689a4d326bba0c236fbfbee65e0`,
instrucción `ev_3af9077c81e887d675567f30258ad5cae11d2c9da7f982b8f2e0bf2e9e757a11`).
`FUN_004c1d50` compara la altura disponible con ese valor; si no cabe, vuelve a
calcular usando los 106 px del panel. `map_window_minimum_height` contiene ese
176 y todas las llamadas del cliente lo pasan al layout. Los márgenes observados
de 176 y 5 son constantes con nombre, no literales repetidos.
El cliente lee `consoleheight` de `Tibia.cfg` y lo pasa al cálculo del layout;
el parser acepta el entero con signo que consume `FUN_004ff200`, cuyo valor
predeterminado observado es cero. La misma lectura normalizada a minúsculas
también permite reconocer las claves de mensajes y listas ya manejadas por el
lector.
`resolve_game_window_layout` desglosa además el área disponible para el mapa,
la consola, el espacio de 5 px y la columna lateral de 176 px. `FUN_004f4500`
devuelve `DAT_0075cfe0 == 1`, la preferencia `NoStretching`. Cuando es falsa,
`FUN_004ddb70` dibuja la imagen de 480×352 en las dimensiones actuales del
MapWindow. Cuando es verdadera, valida ancho y alto contra esos máximos y centra
los offsets X e Y por separado si el MapWindow es menor
(`ev_ace93e91c73674654b86d9b73ef580b2a8be204b7771196dfadea8eda7374ee6`,
`FUN_004f4500` `ev_e4c455c58a54e7b771315045d246943531b4505506b663978cacca0b6cfe43bd`).
El layout C++ ahora conserva el área completa al estirar y limita cada dimensión
independientemente al activar NoStretching; quitó la proporción 15:11 que no
aparece en el original. Cuando cualquiera de las dimensiones queda por debajo
de 480×352, centra el contenido en ambos ejes; cuando ambas alcanzan el tamaño
de referencia, lo coloca en el origen. Esa rama reproduce los offsets y tamaños
que `FUN_004ddb70` pasa a la presentación del MapWindow. Evidencia:
`FUN_004ddb70` `ev_ace93e91c73674654b86d9b73ef580b2a8be204b7771196dfadea8eda7374ee6`.
La comparación visual del resize sigue pendiente.
El método virtual `+0x40` de
la consola usa `0x0041ff50`, que devuelve `DAT_0060cdb0`; `FUN_004a7650`
establece esa variable en 106. El layout reserva `max(106, consoleheight)` para
la consola, y al activar el fallback por altura insuficiente la reserva vuelve
a 106 px, igual que en `FUN_004c1d50`. La columna derecha ocupa la franja de 176
px que resta ese mismo layout. El host dibuja allí los slots de inventario, los
controles de combate y navegación, y las ventanas de Skills y Battle; el
contenido de Skills procede del snapshot de estado. La integración funcional
está conectada, pero su comparación visual completa con una sesión del original
sigue pendiente.

## Pendiente

La fuente de `DAT_0075D444` es el incremento de `FUN_00502810`, llamado al inicio
de `FUN_0044CDC0` (`ev_b0145724334d6da7d35e160c22295e340ca828ea994f71b0945420c54eaca883`;
incremento `ev_7a1901148fe8fd235dc98dafd83ecb4373374cfbd6f777a1ebc50aabfb944e8f`).
`FUN_005251d0` obtiene el tiempo transcurrido mediante la resta modular de
`timeGetTime()` y `DAT_0075EA28` (`ev_f56f8705f7fae69e5a544d705fc7724aa40cb71ea055712198d2e9dd9589da84`).
El inicializador en `0x58c9c0`, referenciado desde `0x5ff0bc`
(`ev_e378f390a047fb77de8d5b258776c7cb25f08eb37751e69587a3c5e9c70fa662`),
llama por la importación `0x58d5a4` (`ev_7ef8326733264449165e4e359a59486f0eec8ae659c585419028e72aaba75c29`).
REA resuelve esa llamada a `WINMM.DLL::timeGetTime` (`ev_fefcece03546d6ece4ce10465b08d1e09515ff2f4d69f9c24adabb6f4d1e510a`,
`ev_a3e0a37c65198fdba54600097c44975095172de5c6f37567b5fa4acd6e524a05`); su
retorno se almacena en `DAT_0075EA28` por la instrucción `0x58c9c6`
(`ev_556bd8e342391287f1dace0b19d8d12bb4bcf26a24aa93d2fe905531d324cce4`).
El registro de `0x58cc30` desde `0x58c9b0` es una rutina separada y no inicializa
esa variable. `win32_client` captura ahora `timeGetTime()` durante su inicialización
estática y pasa `timeGetTime() - animation_epoch_tick` a la composición de outfits;
así deja de usar cero como epoch provisional.
La intercalación de criaturas y efectos 0x83 por casilla ya sigue el orden
observado de `FUN_004ddb70`/`FUN_004daef0`. El epoch de animación usa el
inicializador original. La pasada diferida de proyectiles 0x85 y su oclusión por
propiedad 0 de pisos superiores ya están implementadas. Skills y sus barras
tienen renderer y handlers en `win32_client`; falta comprobarlos visualmente
contra el original. La composición del mundo sigue sin ser 1:1 por los layouts
de objetos pendientes y por la comparación incompleta del frame completo.

El límite de diez criaturas por celda, la selección por offset `[-31,0]`, el
orden por celda, epoch, efectos 0x83 intercalados y proyectiles 0x85 diferidos compilaron en Release con
`scripts/build.ps1` en `build-creature-order`. El ejecutable generado no se
inició después de estos cambios.

La pasada `CreatureInfo` ya se ejecuta después de outfits e iluminación. El modo
0 la oculta; el modo 2 añade el nombre y la barra, y los demás valores no nulos
dibujan solo la barra, siguiendo la rama de `FUN_004ddb70`. La barra tiene fondo
negro de 27×4 px; el relleno ocupa `health / 4` por 2 px. Su origen es el ancla de
la criatura menos `(14, 4)`, limitado a los bordes del MapWindow. En modo 2, el
nombre usa la fuente 2, se centra sobre la barra y se coloca una altura de fuente
arriba; también se limita horizontal y verticalmente. Evidencia: pseudocódigo de
`FUN_004ddb70`, `ev_c098bacf8fb738dcbfe06a7c4f8e5e77d55d8f88c2f4545778468c05ca32ac87`,
página del pase de criatura `ev_d2638b744845347948d3b666ee5b8b95d1a8657390bb116cbcbcc0b970a636e4`.

El color base de la barra y el nombre sigue los intervalos de salud de
`FUN_00450500`: `<4` rojo oscuro, `<10` rojo, `<30` rojo claro, `<60` amarillo,
`<95` verde claro y el resto verde. Si la casilla está fuera del área de
visibilidad o un objeto de un piso superior con propiedad nativa 0 la oculta, la
base pasa a gris (192,192,192). El color se escala con la luz promedio de la
casilla; para el jugador local se usa como mínimo `clamp(light_level,2,5) × 51`.
`FUN_004d46b0` multiplica los componentes por intensidad/5 y `FUN_004d46e0` los
divide entre 51. `FUN_004d47e0` confirma que la intensidad procede del promedio
de los tres canales del campo de iluminación. Evidencia:
`ev_1eea36f8645bb0a98bc07c8a25b1812d89359f884808bb66c0bb94331d04d537`,
`ev_eac99a4257b37f444b16156890bb46db1c2f44c96b05a77bb4fc0b2ca165fd50`,
`ev_88f96af800ea665f41d79742981151089db9309dfa734b78088fa814be4a0d95`,
`ev_e9f3c810b4bdb1861e9b5f6a75cd5d5fc85a1bc82d49550d007fb5eca3d36a5d`.
La selección se rige por `CreatureInfo` (`DAT_0075cfe4`), getter
`FUN_004f44f0`, evidencia `ev_f6a4acfd0aa210e26d45c0c52caa990de9d555246d0587b37efafc800cf3dcff`;
la opción se actualiza desde la página General y se persiste en `Tibia.cfg`.
El bloque compiló en Release; no se inició el cliente. Falta la comparación de
estos indicadores con una captura del original y la verificación visual de la
posición relativa en movimiento.

El pase continúa con las insignias de party y PK a la derecha del ancla. Las
banderas 1–4 de party seleccionan skins `0x101`–`0x104`; las banderas PK 1–4
seleccionan `0xF9`–`0xFC`. Cuando aparecen juntas, el icono PK se coloca 13 px a
la derecha del icono party. La primera insignia usa las dimensiones registradas
en la tabla de skins y se limita al MapWindow; la coordenada vertical se ancla un
píxel debajo de la criatura. La bandera party 1 solo aparece cuando la bandera
del jugador local es 1, 3 o 4, condición de `FUN_00452980`. Cuando el campo nativo
de relación en `+0x9C` es distinto de cero, la insignia PK parpadea; el original
invierte su estado al superar 300 ms en `FUN_00502d50`, y `FUN_00502d80` lo lee.
Evidencia: el pase de insignias en `FUN_004ddb70`, página
`ev_e8ead1c36295b0f1e51abf3322c03b6cad19208d3f1e0b97c669a5e7a8f5b4d3`,
`FUN_00452980` `ev_755461870415fbec3b42e0a934dcf579605892ee8484f11f3552c36f84b9a312`,
`FUN_00502d50` `ev_ccd1a85c36a7c0a98dd70d1813013abf48a41f7be06c4348c649f5a9184a2ddb`,
`FUN_00502d80` `ev_ef0255dbf4ff9ce7fe2e2f20b82e9531dcd54357f85d1d3f4534338fd38a9b1f`,
y dibujo de skins `FUN_004a6e40`
`ev_2c464c6d0c529442390f151ae3f5b78bf9ec760253fc937eea72660f47dac3d3`.
El byte de estado en `+0x9C` aún no está asociado a un campo del protocolo; el
snapshot lo conserva en cero cuando no hay evidencia que lo alimente. Por eso,
el reloj de parpadeo está reconstruido, pero esa condición todavía no se puede
confirmar en una sesión de juego y queda pendiente de resolver.
