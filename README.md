[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/mortennobel/SimpleVRCpp/master/LICENSE)

# SimpleVRCpp

Simple C++14 example of how to use OpenGL, SDL2 (2.0.8) with Oculus VR SDK 1.3.0 (OVR) and OpenVR SDK (1.0.17).
This should be considered a hello world program with as few dependencies as possible.
Both project uses a single source file with a similar structure.

## Dependencies
 - SDL2 Development Libraries (2.0.8) https://www.libsdl.org/download-2.0.php
 - GLEW (2.1.0) http://glew.sourceforge.net/
 - Oculus SDK (1.30) for Windows https://developer.oculus.com/downloads/native-windows/
 - OpenVR SDK (1.0.17) https://github.com/ValveSoftware/openvr/releases
 - GLM (bundled)

## Getting started

- Create Visual Studio 2017 project using CMake GUI  (https://cmake.org/download/). 
- Open project and run

## Notes:
 - ovr_CreateTextureSwapChainGL crashes in 32bit debug build. This is an issue with the SDK. https://forums.oculusvr.com/developer/discussion/39734/ovr-createtextureswapchaingl-crashes-everytime
 - Compile LibOVR with runtime library set to Multi-threaded DLL / Multi-threaded Debug DLL
 - Use GLEW static library (to avoid depending on the DLL)
