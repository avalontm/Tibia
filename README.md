# Tibia 7.92 — reconstrucción en C++

Reconstrucción de código abierto de la funcionalidad del cliente Windows de Tibia 7.92, desarrollada mediante ingeniería inversa del ejecutable original. El objetivo es reproducir su comportamiento real a partir de evidencia del cliente; no es una conversión automática del binario ni contiene el código fuente original.

## Ficha técnica

- **Cliente de referencia:** Tibia Windows Client 7.92, PE32 x86.
- **Lenguaje:** C++20.
- **Plataforma:** Windows.
- **Build:** CMake y MSVC.
- **Autor:** AvalonTM.
- **Estado:** reconstrucción activa; algunas funciones y diferencias respecto al original siguen pendientes de análisis e implementación.

## Funcionalidad implementada

- Lectura de recursos `.dat`, `.spr` y `.pic`, decodificación de sprites y composición de objetos.
- Renderizadores para DirectDraw, OpenGL y Direct3D 9.
- Interfaz del cliente: login, diálogos y opciones, y pantallas del juego con mapa, panel lateral, minimapa, contenedores y consola.
- Componentes de protocolo de red, login, cifrado XTEA/RSA, lectura de paquetes y comunicación con el servidor.
- Reconstrucción modular en `src/` e interfaces públicas en `include/`.

La cobertura y el grado de equivalencia varían por módulo. La meta es una reconstrucción 1:1 verificada; este repositorio no afirma que el cliente completo ya esté terminado.

## Compilación

Se requiere Visual Studio o Build Tools con MSVC x86/x64, CMake y PowerShell. Desde la raíz del repositorio:

```powershell
.\scripts\build.ps1 -Action Build -Configuration Release
```

El build espera los recursos originales `Tibia.dat`, `Tibia.pic`, `Tibia.spr` y `Tibia.cfg` en la raíz. Por derechos de autor, esos archivos y `Tibia.exe` no se distribuyen en este repositorio.

## Ingeniería inversa y contribuciones

Las reglas de reconstrucción están en [`docs/decompilation-rules.md`](docs/decompilation-rules.md). La documentación técnica de cada subsistema está en [`docs/`](docs/). Los cambios deben basarse en el comportamiento observado del cliente original y dejar claro qué está confirmado y qué sigue sin resolver.
