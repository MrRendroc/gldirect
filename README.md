# GLDirect 5.0 (Mesa) - Modernized Build for Legacy OpenGL Games
Updating the open source GLDirect v5, a Direct3D Device Driver for Mesa

This code was released by SciTech Software in 2007 and has enabled modern DirectX systems to play older OpenGL games.
Usually just the compiled opengl32.dll has been available for download somewhere, where the user would just drop it into the game's main directory.
This functions fine for most games except (as in the case of Medal of Honor: Allied Assault) there is no fog.

I updated the code to enable the fog (a simple change, but difficult to find) and compiled it with currently available libraries.

I only tested this on my GoG MOHAA installation but it did the trick.  I hope it works for you, too.

# Detailed Description
This is a modernized build of the GLDirect 5.0 (Mesa) wrapper, specifically updated to run on Windows 10/11 using Visual Studio 2022. It acts as a translator between legacy OpenGL calls and DirectX 9.

## Key Features & Fixes:

Fog Restoration: Fixes the missing/broken fog in games like Medal of Honor: Allied Assault (MOHAA).

Display List Support: Restored display list functionality for improved compatibility with id Tech 3 engine titles.

Multithreading: Includes gldirect.ini with optional multithreading support (experimental).

DirectX 9 Backend: Leverages modern GPU hardware via a stable DX9 translation layer.

Statically Linked dependencies.  No need to download other DirectX Redistributables or Mesa files.

## Installation:

Copy opengl32.dll and gldirect.ini into the main folder where the game executable lives (e.g., MOHAA.exe).

Ensure the game is set to use the "Default" or "OpenGL" driver in settings.

## Compatibility:
Primarily tested with MOHAA, but should improve stability and visual fidelity for other late-90s/early-2000s OpenGL titles.
