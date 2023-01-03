# gldirect
Updating the open source GLDirect v5, a Direct3D Device Driver for Mesa

This code was released by SciTech Software in 2007 and has enabled modern DirectX systems to play older OpenGL games.
Usually just the compiled opengl32.dll has been available for download somewhere, where the user would just drop it into the game's main directory.
This functions fine for most games except (as in the case of Medal of Honor: Allied Assault) there is no fog.

I updated the code to enable the fog (a simple change, but difficult to find) and compiled it with currently available libraries.

I only tested this on my GoG MOHAA installation but it did the trick.  I hope it works for you, too.
