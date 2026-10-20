# Tibia 7.92 Client Reconstruction

An open C++ reconstruction of the Windows Tibia 7.92 client, built through reverse engineering of the original executable. The goal is to reproduce the original client's real behavior from observed evidence. This is not an automatic binary-to-source conversion and does not contain the original source code.

## Technical Overview

- **Reference client:** Tibia Windows Client 7.92, PE32 x86.
- **Language:** C++20.
- **Platform:** Windows.
- **Build system:** CMake and MSVC.
- **Author:** AvalonTM.
- **Status:** In active development. Some behavior and subsystems remain under investigation and implementation.

## Implemented Features

- Readers for `.dat`, `.spr`, and `.pic` resources, sprite decoding, and object composition.
- Rendering backends for DirectDraw, OpenGL, and Direct3D 9.
- Client interface reconstruction, including login, dialogs, options, and game-screen UI such as the map view, sidebar, automap, containers, and chat console.
- Network and login components, including XTEA/RSA routines, packet readers, and server communication.
- Modular C++ code organized under `src/` and public interfaces under `include/`.

Coverage varies by subsystem. The target is a verified 1:1 reconstruction; the complete client is not yet finished.

## Build

Requirements: Visual Studio or Build Tools with MSVC, CMake, and PowerShell.

From the repository root, run:

```powershell
.\scripts\build.ps1 -Action Build -Configuration Release
```

The build expects the original `Tibia.dat`, `Tibia.pic`, `Tibia.spr`, and `Tibia.cfg` files in the repository root. These files and `Tibia.exe` are not distributed here.

## Contributing

Contributions should be based on behavior verified against the original client. Keep confirmed observations separate from unresolved behavior, and organize implementation by module.
