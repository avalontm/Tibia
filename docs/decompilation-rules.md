# Reglas de decompilación y reconstrucción 1:1

El objetivo de este proyecto es reconstruir el comportamiento y la interfaz del
cliente original con fidelidad 1:1. La evidencia debe venir del artefacto
original (`Tibia.exe`, sus recursos) o de una observación autorizada del
original en ejecución. Una reconstrucción de C++ no es el lugar para completar
vacíos con decisiones de diseño propias.

## Reglas obligatorias

1. **No inventar datos.** No adivinar dimensiones, coordenadas, anclajes,
   colores, fuentes, skins, estados, nombres, algoritmos ni comportamiento. No
   usar un valor “razonable”, aproximado o típico como si fuera del original.
2. **Seguir cada detalle hasta su fuente.** Para una afirmación estática,
   identificar la función, los datos/globals y las referencias que la sustentan.
   Para una afirmación visual, identificar además el recorte, la fuente o la
   secuencia de dibujo en los recursos originales. Para una afirmación dinámica,
   conservar la observación y sus condiciones (resolución, estado, entrada).
3. **Separar hechos, derivaciones e incógnitas.** Marcar un dato como observado
   cuando aparece directamente en el binario/recurso/ejecución; como derivado
   cuando el cálculo se puede reproducir desde hechos citados; y como
   desconocido cuando falta evidencia. No presentar una inferencia como un
   hecho.
4. **No convertir una incógnita en implementación final.** Si falta evidencia,
   continuar el análisis con xrefs, inicializadores, constructores, rutas de
   dibujo, datos de recursos u observación autorizada. Si sigue sin resolverse,
   documentar el bloqueo y mantener ese detalle fuera de la ruta que se declare
   1:1. Un prototipo aproximado debe identificarse de forma visible como
   provisional y no como reconstrucción fiel.
5. **Mantener trazabilidad.** Registrar el hash y la versión del artefacto, la
   función/dirección o recurso examinado y el identificador de evidencia REA
   cuando exista. Citar la evidencia cerca de la conclusión en la documentación.
6. **Comparar el resultado con el original.** Para UI, contrastar capturas con
   el mismo estado, resolución y escala; revisar geometría, recortes, fuente,
   alfa y orden de dibujo. Para comportamiento, comparar las entradas y salidas
   observables equivalentes. Un build correcto solo demuestra que compila; no
   demuestra equivalencia 1:1.
7. **Respetar límites de ejecución acordados.** Si el usuario pide análisis
   estático o solo build, no ejecutar el cliente original ni la reconstrucción
   para conseguir evidencia. Usar artefactos estáticos o pedir una captura/
   observación autorizada cuando sea necesaria.
8. **Mantener módulos pequeños, cohesionados y con nombres funcionales.**
   Separar despacho de paquetes, mapa, movimiento, búsqueda de rutas, recursos
   y UI en archivos propios. Nombrar los archivos por su responsabilidad en el
   proyecto (`navigation`, `map/cache`), no por estados internos del protocolo,
   direcciones ni etiquetas temporales de análisis. No seguir acumulando
   funciones de subsistemas distintos en un archivo central. El archivo que
   declara el dispatcher debe limitarse a su API y al registro/despacho
   correspondiente.
9. **Mantener el código fuente limpio.** No agregar comentarios explicativos,
   bloques de código comentados ni duplicar en el fuente la documentación de
   evidencia. Registrar la explicación y los IDs REA en `docs/`; en el código,
   usar nombres claros y funciones acotadas. Al reorganizar un módulo, quitar
   los comentarios explicativos heredados del código que se está moviendo.
10. **No añadir funciones ajenas al original.** No incorporar opciones,
    controles, archivos de configuración, flujos de conexión ni otros
    comportamientos que el cliente original no tenga. Antes de implementarlos,
    comprobar su presencia en el ejecutable, sus recursos o una observación
    autorizada del cliente original y registrar la evidencia. Si se necesita
    una adaptación temporal para pruebas, mantenerla fuera del cliente
    reconstruido y documentarla claramente como herramienta de prueba.

## Registro mínimo por sección

- **Estado:** observado, derivado, desconocido o provisional.
- **Afirmación:** qué hace o cómo se ve el original.
- **Evidencia:** función/dirección, datos, recurso y/o evidencia REA.
- **Derivación:** cálculo explícito, si aplica.
- **Pendiente:** la prueba concreta que falta para cerrar la sección.

Una sección no está terminada mientras un detalle necesario para la fidelidad
1:1 siga siendo una suposición sin marcar. La prioridad es investigar el original,
no hacer que la aproximación parezca convincente.
