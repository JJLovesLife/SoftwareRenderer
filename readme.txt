Software Renderer

This is a software renderer dedicate on technology practice.
Therefore I do not care about performance or anything else.
A stupid but making sense implementation is what I want to build.

The API mainly borrows from Direct3D 12.
The implementation is mainly according to the discussion in <Real-Time Rendering 4th>.
The d3dApp.h/.cpp, GameTimer.h/.cpp, TF.h, MathHelper.h, d3dUtil.h are modified from the version discussed in <Introduction to 3D Game Programming with DirectX 12>.



----------------
Following is a list of technologies/features I implemented :
* 8 * 8 tile and 2 * 2 quad hierarchical with zigzag order
* Hierarchical z-buffering algorithm(Hi-Z)
* Tile size pre-edge test
* Binning with AABB
* Near-Z clip and assumed infinity guard-bands
* SIMD(SSE2) and OPENMP
* Optimization for UMA
* 4D-space linear interpolation
* Edge equation linear property
* Top-left rule
* Z-prepass
* Programmable shader
* Quad level pixel shader


Sample Usage:
Press F3 to allow / not allow tearing(unlock 60fps limitation).
Press F4 to on / off debug mode(magnify pixels and showing tile outline).
When in debug mode, press key 1-8 to set magnification level.


Annotate:
1. Only a few error checking, since building a robust renderer has too much works to do, and I just want to build a software renderer to check and enhance my understanding of hardware renderer.
2. No positive w clip, since that is mathematically imperfect and no necessary.
3. No texture and mipmap support, since that can be easily implement with constant buffer and quad level pixel shader. (though it is slow, but I don't care performance in this project.)
