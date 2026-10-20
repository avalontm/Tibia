# Tibia 7.92 Client Reconstruction

An open C++ reconstruction of the Windows Tibia 7.92 client, developed through reverse engineering of the original executable. The project rebuilds client behavior from binary analysis and verified observations. It is not an automatic binary-to-source conversion and does not contain the original source code.

The goal is a modular, evidence-based 1:1 reconstruction. The project is still in active development, and its current implementation is incomplete.

## Project at a Glance

| | Details |
| --- | --- |
| Reference client | Tibia Windows Client 7.92 |
| Target format | PE32, x86 |
| Implementation language | C++20 |
| Platform | Windows |
| Build system | CMake with MSVC |
| Author | AvalonTM |
| Development status | Active; reconstruction and verification proceed module by module |

## Features

| Area | Current implementation | Status |
| --- | --- | --- |
| Resource formats | Parsers for `Tibia.dat`, `Tibia.spr`, and `Tibia.pic`; indexed access to item, outfit, effect, missile, sprite, and picture data. | Implemented; format coverage continues to grow |
| Asset decoding | 32×32 sprite decoding, picture tile decoding, alpha-aware software surfaces, and object/outfit composition. | Implemented |
| Asset inspection | `tibia_asset_tool` can inspect resource files, list IDs, and export sprites, pictures, objects, and outfits to BMP. | Implemented |
| Graphics backends | DirectDraw, OpenGL, and Direct3D 9 backends behind a shared renderer interface. Engine values match the reference client's selection order. | Implemented; runtime parity is still being verified |
| Login and dialogs | Reconstructed login flow, account fields, status/message windows, options, and supporting dialogs. | Partial; fidelity work continues |
| Game interface | Game-screen map frame, sidebar, status bars, equipment area, automap controls, content panels, and chat console. | Partial; interaction and layout details remain under reconstruction |
| Map pipeline | Map model, viewport/floor handling, tile ordering, render queues, lighting, effects, creatures, and automap storage. | Partial; server-fed world behavior is still being completed |
| Network protocol | Packet readers/writers, framed TCP streams, login request construction, response handling, and state/opcode dispatch. | Partial; some fields and routes remain unresolved |
| Cryptography | XTEA frame encryption and RSA login-block/public-key operations reconstructed as separate modules. | Implemented; full live-client equivalence is not claimed |
| Windows client host | Native Win32 application with renderer selection, window resizing, input handling, and game-client integration. | Implemented; end-to-end behavior is still under development |

“Implemented” means a corresponding C++ module exists; it does not imply the complete client is finished or that every path has been validated against the original at runtime.

## Repository Layout

| Path | Contents |
| --- | --- |
| `src/client/` | Client interface, game-screen, input, and UI behavior |
| `src/network/` | Protocol messages, packet streams, login, and TCP transport |
| `src/render/` | Software composition and graphics backends |
| `src/resources/` | `.dat`, `.spr`, and `.pic` readers |
| `src/map/` | Map model, automap storage, rendering data, and caches |
| `src/crypto/` | XTEA and MSVC-compatible random sequence helpers |
| `include/` | Public C++ interfaces grouped by subsystem |
| `scripts/` | Build and protocol-data scripts |
| `tools/` | Asset inspection and export utility |

## Build

### Requirements

- Visual Studio 2022 or Visual Studio Build Tools with the MSVC x86/x64 C++ workload
- CMake available on `PATH`
- PowerShell

### Build a Release version

Run this from the repository root:

```powershell
.\scripts\build.ps1 -Action Build -Configuration Release
```

The script locates the MSVC installation and configures the `build-msvc/` directory. Build outputs are ignored by Git.

## Local Game Resources

The client build expects these original files in the repository root. The asset tool uses the resource files relevant to each command:

| File | Purpose |
| --- | --- |
| `Tibia.dat` | Object attributes and sprite references |
| `Tibia.spr` | Sprite image data |
| `Tibia.pic` | UI picture and skin data |
| `Tibia.cfg` | Original client configuration |

These files and `Tibia.exe` are excluded from this repository and must be obtained separately from a legally held Tibia 7.92 installation. Local copies are ignored by Git.

## Scope and Verification

This repository contains a new C++ implementation based on reverse-engineering findings, not recovered CipSoft source. Binary-derived behavior is reconstructed in small modules so that each subsystem can be compared with the reference client. Unresolved details remain explicitly incomplete instead of being presented as original behavior.

The objective is faithful compatibility with the 7.92 client. The current project should be treated as a work in progress rather than a drop-in replacement.

## Contributing

Keep changes focused by subsystem, use clear names, and avoid undocumented assumptions. When behavior depends on the original client, verify it against the reference before describing it as equivalent. Keep local binaries, game resources, build output, and reverse-engineering tools out of commits.
