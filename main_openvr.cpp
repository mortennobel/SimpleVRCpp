
#define SDL_MAIN_HANDLED
#include "SDL.h"
#include <iostream>
#include <ostream>
#include "SDL_opengl.h"


int main(int argc, char* argv[]) {
	// todo add Ope
	SDL_Window *window;                    // Declare a pointer

	SDL_Init(SDL_INIT_VIDEO);              // Initialize SDL2

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	// Create an application window with the following settings:
	window = SDL_CreateWindow(
		"An SDL2 window",                  // window title
		SDL_WINDOWPOS_UNDEFINED,           // initial x position
		SDL_WINDOWPOS_UNDEFINED,           // initial y position
		640,                               // width, in pixels
		480,                               // height, in pixels
		SDL_WINDOW_OPENGL                  // flags - see below
	);

	// Check that the window was successfully made
	if (window == NULL) {
		// In the event that the window could not be made...
		printf("Could not create window: %s\n", SDL_GetError());
		return 1;
	}

	auto glContext = SDL_GL_CreateContext(window);

	std::cout << glGetString(GL_VERSION) << std::endl;
	std::cout << "OpenGL version " << glGetString(GL_VERSION) << "." << std::endl;
	// The window is open: enter program loop (see SDL_PollEvent)

	SDL_Delay(3000);  // Pause execution for 3000 milliseconds, for example

	SDL_GL_DeleteContext(glContext);

	// Close and destroy the window
	SDL_DestroyWindow(window);

	// Clean up
	SDL_Quit();
	return 0;
}