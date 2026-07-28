# openvice
Open source engine for [Grand Theft Auto: Vice City](https://www.rockstargames.com/games/vicecity).

Features:
* Read GTA format files: IMG, DIR, DFF, TXD
* Render: Frustum culling

## Build 
Open `openvice.sln` solution in **Microsoft Visual Studio 2019** and click **Build** -> **Build Solution**.

## Controls

* `NUMPAD 0` - Change to control character (Tommy)
* `NUMPAD 1` - Free camera
* `NUMAPD 2` - Change to control Cheetah
* `F3` - Enable/disable debug line physics
* `F7` - Enable/disable shadows
* `F8` - Enable/disable SSAO
* `F9` - Change post effects mode to: Off, Post FX, Post FX with Motion blur
* `WASD` - Controls
* `Mouse`
* `Mouse wheel` - Camera speed or Camera+-

## System requirements
* Windows Vista or higher
* DirectX 11

## Dependencies
* [Bullet Physics 2.89](https://github.com/bulletphysics/bullet3/releases/tag/2.89)
* [DirectXTex](https://github.com/microsoft/DirectXTex). Build DirectXTex_Desktop_2019.sln
* [DirectXMath](https://github.com/microsoft/DirectXMath)

## Tested
* Windows 10 Pro (version 22H2)
