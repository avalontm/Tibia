# Reconstrucción de talkboxes y texto de estado

## Estado

La semántica del contador, la cadencia y el cálculo de distribución de
talkboxes están implementados. Falta conectar sus ubicaciones al renderizado
del mundo, que aún no existe en el cliente reconstruido.

## Evidencia

- Artefacto: `Tibia.exe`, SHA-256
  `fe1172d0ff2e3024b78125184a4ebcdbb0489b9629ff744184ca130033d6321a`.
- `FUN_004bdee0` (`ev_7ac7909ba8d2f2ddad944a4def7d9ffcdb5b562701457cadd50578451b2c8112`)
  agenda `SetTimer(hwnd, 1, 100, nullptr)` durante `OnCreate`.
- `FUN_004bbb50` (`ev_e0ac86a04c87d550bf5dc7da866ce57ca705b1d5d69498172bc9c3dec660f557`)
  procesa el mensaje de ventana y llama a `FUN_0044cdc0`.
- `FUN_0044cdc0` (`ev_b0145724334d6da7d35e160c22295e340ca828ea994f71b0945420c54eaca883`)
  llama una vez a `FUN_00527ef0` en ese ciclo.
- `FUN_00527ef0` (`ev_b1b116bcbc87d0bcfa85f08f62a7b6c28a9cbedcc449138ed4f423cbaf49feca`)
  decrementa el contador solo si es positivo y activa la marca de actualización
  al llegar a cero. Los valores `0` y `-1` no cambian durante los ticks.
- `FUN_00527c40` (`ev_634416146237a8208c3fc40b3d53145671da2d3987708c4f27f9e278e9180195`)
  asigna `50` cuando el segundo argumento es falso y `-1` cuando es verdadero.
  Con texto nulo, pone el contador en `0`, activa la marca de actualización y
  conserva el buffer de texto existente.
- `FUN_00412a80` (`ev_eb07f0a7abc8db050e50b7b2d99b48927682c4484335fa0b1a6968d660e59319`)
  envía los modos `0x14`, `0x15` y `0x17` a `FUN_00527c40` con segundo
  argumento falso.
- `FUN_00525b50` (`ev_08257ce7f24c8bbc609b17b69d396e7eecb4a0ec8d68ee1270e1bdeda9a635bb`)
  devuelve el buffer del estado solo cuando el contador no es cero.
- `FUN_004ddb70` y sus referencias de salida (`ev_4451b7e84df05681c2503bcbc4dc5be7719524dedbd75ace224980da0e054268`)
  consultan ese buffer y llaman a `FUN_004a1b80` para dibujarlo. El punto de
  dibujo usa el centro horizontal del canvas y la coordenada inferior menos
  la altura medida de la fuente; los tres componentes RGB suministrados son
  `0xFF`.
- El dossier de `FUN_004ddb70` también muestra la iteración por líneas de cada
  talkbox: la Y de cada línea suma `índice * altura_fuente_2`, se carga el RGB
  del modo mediante `FUN_00526b60` (`ev_2a8d59a2c7a6773bd4d8b22e466ae4777b33a5d004b94fc557d763f9d53cb4bf`)
  y se llama a `FUN_004a2350`.
- `FUN_004a2350` (`ev_aa6d2952c24a5e6b4d9ca6c0bd7561bc9b199d720bdac9702bc1ac5011d17434`)
  valida los alineados `0`, `1` y `2` como izquierda, centro y derecha. Para
  centro resta media anchura medida; para derecha resta la anchura completa.
- El dossier de `FUN_004ddb70` (`ev_cdb46fc7ca477efb54cf2665b27b702402eb2c613c2c4fd8bd5dd78677810ba6`)
  conserva la llamada del estado con `x = ancho/2 + offset_x` y
  `y = alto - altura_fuente_2 + offset_y`.
- `FUN_004d59b0` (`ev_d6d38c39c775f4f5354f9c10a227a623f87760424aae7b943a89c42ca6d342b3`)
  mide cada talkbox con la fuente 2, calcula anclas desde la posición del mapa
  o el modo de pantalla, limita el rectángulo al viewport y resuelve
  intersecciones moviendo los centros de un píxel por iteración.
- `FUN_004c31f0` (`ev_c5113480e581fdeb8d9b7107d00aa12a637a6c598d37da32d036656f0216f7fa`)
  proporciona la transformación de posición del mapa usada por el cálculo de
  las anclas.
- `FUN_004f4500` (`ev_3a89c3d820d263025a7867a6c309d80f04bff9d2dedd2ed6d6aec32421316741`)
  selecciona la rama de coordenadas en píxeles cuando
  `DAT_0075cfe0 == 1`.
- `FUN_004509e0` (`ev_b11541c9c1094a1e0e32a1eb994e948dca8a373679e80620f976c205abb79945`)
  y `FUN_00450a00` (`ev_953429d4266f67ef70139ac5703b4d0294680eafbbb9c9e59e2f8f7ff048e637`)
  devuelven los desplazamientos horizontales y verticales del canvas durante
  el scroll; `FUN_004ddb70` los resta de talkboxes no fijadas al borde.

## Implementación

`TalkSystem` representa el contador con signo de 32 bits, distingue texto nulo de texto
vacío, conserva el centinela persistente `-1` y descuenta un tick por cada
intervalo observado de 100 ms. El bucle de red limita su espera a ese intervalo
y recupera ticks vencidos si el ciclo se retrasa. `has_active_status_message()`
reproduce la condición de visibilidad del getter original: cualquier contador
distinto de cero mantiene el texto activo.

La selección de talkboxes también sigue la evidencia de `FUN_0052d890`
(`ev_7ffd51b13216ce4ccc52f3790af68345ad0410d98a31b8da97560d31418d4e0b`),
`FUN_00527a60` (`ev_36a2052593aacb9d0bf60842a897d076a0754d0cf47a2751320959f5505f7aa9`)
y `FUN_00527070` (`ev_4d86ebe7e000c6e3c6587625e9ce3ab70bbdfba37e131874106de9e4d37cd831`).
Cuando los diez espacios están ocupados, el original libera el espacio ocupado
con menor orden de llegada, incluso si estaba pendiente. Al terminar un grupo,
reactiva el pendiente más antiguo de ese grupo. El modelo ya usa esos criterios,
sin ordenar por el tiempo restante.

El corte de texto coincide con `FUN_005273d0`
(`ev_f8fae89e6c62025b33ee24ec6e721b46e60b20c7154a006fe9a456f47ac2fffa`) y el
predicado de separación `FUN_00532610`
(`ev_79da1d237b8871d0da25a9f8de54ed76b4961a84ac644b9370471f2b47a306c7`). El
original admite diez líneas de hasta 39 bytes más terminador, respeta `\n`,
busca el último espacio/tabulador/control whitespace de los primeros 40 bytes
para cortar, y si no encuentra uno copia 38 bytes y agrega `-`. El código C++
ya reproduce esos límites y mantiene el texto como bytes, igual que la ruta
original basada en `char*`.

El color se recupera de `FUN_005260b0`
(`ev_0ecf48d0f416d0ccd2c30b7548eccc1c5da07afc5c72024fa62de37a66618792`) y el
getter de modo usado antes del dibujo es `FUN_00526b60`
(`ev_058dcde1683da7e3e3cefe86646bd4fe60a290e9eea9c54cfea5fc40a6469d56`). Para
los modos implementados, los triples de componentes son: `1–3 = F0 F0 00`,
`4 = 60 F8 F8`, `9/B/12 = F8 60 60`, `10/11 = FF 66 00`, `13 = F0 F0 F0` y
`16 = 00 F0 00`. `Talkbox` ahora conserva estos componentes junto al modo para
que el renderizador no tenga que inventarlos.

`include/client/talkbox_layout.hpp` y `src/client/talkbox_layout.cpp` calculan
la posición visual por separado del estado de red y del dibujo. El cálculo
considera únicamente slots con estado `1`, reutiliza
`transform_state6_wire_position`, reserva el espacio inferior del mensaje de
estado y usa la medida de texto entregada por el futuro renderizador de fuente
2. Las anclas especiales observadas son los modos `4`, `9`, `0x0B`, `0x12`,
`0x13` y `0x16`; los demás modos usan posición del mapa. La geometría del mapa
usa la referencia de viewport `480 x 352`, con ramas de píxel o proporcionales.
La rama proporcional ubica el ancla vertical en `((local_y - 1) * alto) / 11`
y el centro de la caja empieza una mitad de alto tipográfico por encima; la
rama de píxeles usa `local_y * 32 - offset_y - 32` antes del mismo ajuste. El
cálculo no agrega un centrado `alto/22` en Y.

Cada centro se limita primero a una ventana de movimiento alrededor de su
ancla y después al rectángulo útil del viewport; para los anclajes de mapa, el
límite vertical inicial empieza en el centro proyectado y se extiende un alto
de texto. Para los anclajes de pantalla, la ventana vertical se extiende un
alto a cada lado. El espacio reservado por estado afecta el borde inferior.
Las talkboxes se separan cuando las distancias entre centros son menores que
la suma de semidimensiones más diez píxeles. Cada iteración acumula fuerzas por
eje y mueve como máximo un píxel, respetando la primera ventana de movimiento.
El ciclo continúa mientras la suma del producto de separaciones disminuye;
la última pasada que ya no reduce esa suma también aplica sus desplazamientos,
igual que la rutina original.
La alineación final cambia a derecha, centro o izquierda según el
desplazamiento horizontal respecto al ancla; los bits de borde registran
ancla fija y contacto con el rectángulo útil. El módulo devuelve posiciones y
alineación, no dibuja ni inventa métricas tipográficas.

El contexto del layout recibe los desplazamientos de scroll en píxeles. Si el
bit de borde correspondiente está apagado, resta el desplazamiento directamente
en modo píxel; en modo proporcional escala X por `ancho/480` e Y por `alto/352`,
con división entera como el cliente original. Los modos fijados a un borde no
reciben ese desplazamiento.

`layout_status_message` produce el ancla central y la baseline inferior del
texto de estado usando la altura que mida la fuente 2 y los offsets entregados
por el canvas. Devuelve RGB `(255,255,255)` y no produce ubicación cuando el
contador original indica que el estado no está visible.

`build_talkbox_text_commands` convierte placements y líneas activas en
comandos ordenados por talkbox y línea. Cada comando conserva el texto original
en bytes, el color RGB del modo, el número de fuente `2`, el ancla horizontal
y el alineado; su Y suma el índice de línea por la altura de fuente 2 y los
offsets del canvas. El renderer consumidor debe respetar el alineado de la
fuente, que ajusta el ancla usando la anchura medida de cada línea.

`GameWorldSnapshotMailbox` publica copias inmutables del mapa, talkboxes y
estado del mensaje entre el hilo de red y el de ventana. El productor publica
después de despachar mensajes y de actualizar los temporizadores; el consumidor
obtiene una referencia compartida bajo mutex. Así, la futura ruta de dibujo no
lee los contenedores mutables del hilo de red.

## Pendiente

El módulo de layout todavía no tiene consumidor. Falta implementar la ruta de
renderizado del mundo, medir texto con la fuente 2 original y pasar esas
dimensiones y los offsets reales del canvas. El cliente reconstruido aún no
cuenta con esa ruta de renderizado de mundo, así que la distribución no está
visible ni integrada.
