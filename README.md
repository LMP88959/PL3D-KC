# PL3D-KC
PiSHi LE + FW LE Public Domain Source Release by EMMIR 2018-2022
================================================================

![alt text](/screenshot.png?raw=true)

PiSHi LE (PL) is a subset of the 3D graphics library used in King's Crook.
FW LE is a subset of the windowing library used in King's Crook.

Just like King's Crook, this code follows the same restrictions:

1. Everything must be done in software, no explicit usage of hardware acceleration.
2. No floating point types or literals, everything must be integer only.
3. No 3rd party libraries, only C standard library and OS libraries for window, input, etc.
4. No languages used besides C.
5. No compiler specific features and no SIMD.
6. Single threaded.

================================================================
Feature List:

- N-gon rendering (not limited to triangles)
- Depth (Z) buffering
- Flat polygon filling
- Affine texture mapped polygon filling
- Near plane clipping
- Viewport clipping
- Back face culling
- Immediate mode interface
- Matrix stack for transformations
- Code to generate a box
- King's Crook DMDL format importer

================================================================

Compiling for macOS/Linux:
```
  cd PL3D-KC
  
  cc -O3 -o pl *.c fw/*.c -lX11 -lXext
  
  ./pl  
```
  
Compiling for Win32 is a bit more involved, I've only used Visual Studio
to develop for Windows but this program doesn't use anything other than
the Win32 library and winmm.lib (don't forget to link against winmm.lib!).
Of course, you're not limited to using Visual Studio, feel free to use
any build system you're comfortable with under Windows.
Don't forget to compile with max optimization!

If you have any questions feel free to leave a comment on YouTube OR
join the King's Crook Discord server :)

YouTube: https://www.youtube.com/c/LMP88

Discord: https://discord.gg/hdYctSmyQJ

itch.io: https://kingscrook.itch.io/kings-crook
