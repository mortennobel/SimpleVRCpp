# SimpleVRCpp

Simple example of how to use OpenGL, SDL2 (2.0.8) with Oculus VR SDK 1.3.0 (OVR).
This should be considered a hello world program with as few dependencies as possible.

#Dependencies
 - SDL2 Development Libraries (2.0.8) https://www.libsdl.org/download-2.0.php
 - GLEW (2.1.0) http://glew.sourceforge.net/
 - Oculus SDK for Windows https://developer.oculus.com/downloads/native-windows/
 - GLM (bundled)

#Getting started

- Create Visual Studio 2017 project using CMake GUI  (https://cmake.org/download/). 
- Open project and run

Notes:
 - ovr_CreateTextureSwapChainGL crashes in 32bit debug build. This is an issue with the SDK. https://forums.oculusvr.com/developer/discussion/39734/ovr-createtextureswapchaingl-crashes-everytime
 - Compile LibOVR with runtime library set to Multi-threaded DLL / Multi-threaded Debug DLL
 - Use GLEW static library (to avoid depending on the DLL)
