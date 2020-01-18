Automatic build pipeline for Libdragon
[![Build Status](https://dev.azure.com/n64-tools/N64-Tools/_apis/build/status/N64-tools.libdragon-sdk)](https://dev.azure.com/n64-tools/N64-Tools/_build/latest?definitionId=2)

# Download
Architecture | Download Links
--- | --- 
Windows x64 | [Latest](https://n64tools.blob.core.windows.net/binaries/N64-tools/libdragon/develop/latest/libdragon-win64.zip)
Linux x64 | [Latest](https://n64tools.blob.core.windows.net/binaries/N64-tools/libdragon/develop/latest/libdragon-linux64.tar.gz)


# libdragon
The Conker64 version of Libdragon includes extended RDP features:
- Sprite flip (Vertically, horizontally or both)
- Sprite zooming (Scale X, Scale Y or both)
- Sprite center align
- Alpha blending (32 levels on 16bit mode)
- Additive blending (somewhat)
- Intensify / Color Mask and other kind of effects using different color combiners
- 4 and 8bit texture support (Color Index)
- Set other modes options: enable copy / 1cycle, enable tlut, enable bilinear filter, enable atomic prim, etc
- Frame buffer effects
- Bug fixes on 32bit mode / textures
- Some other fixes

This version also includes:
- automated build pipeline
- 240p support

Conker64 repo branch:
https://github.com/Conker64/libdragon | develop-conker64
Somes changes applied from Chillywilly's libdragon (2014-11-12) 

Original repo branch:
https://github.com/DragonMinded/libdragon | develop-dragonminded
