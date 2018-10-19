#define SDL_MAIN_HANDLED
#include "SDL.h"
#include <iostream>
#include <ostream>
#define GLEW_STATIC
#include <GL/glew.h>
#include "SDL_opengl.h"

#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"

#pragma region VR_INCLUDES
#include "OVR_CAPI_GL.h"
#include "Extras/OVR_Math.h"
#pragma endregion VR_INCLUDES  

#pragma region OPENGL_STATE
bool quitting = false;
SDL_Window *window = NULL;
SDL_GLContext gl_context;


GLuint vertexBuffer, vertexArrayObject, shaderProgram;
GLint positionAttribute, colorAttribute;

GLint modelUniform, viewUniform, projectionUniform;

glm::vec3 pos = { 0.0f, 0.0f, 0.0f };
float nearPlane = 0.1;
float farPlane = 100.0;
#pragma endregion OPENGL_STATE

#pragma region VR_HELPER_FN
// OculusTextureBuffer is from the OculusSDK\Samples\OculusRoomTiny\OculusRoomTiny (GL)\main.cpp
struct OculusTextureBuffer
{
	ovrSession          Session;
	ovrTextureSwapChain ColorTextureChain;
	ovrTextureSwapChain DepthTextureChain;
	GLuint              fboId;
	OVR::Sizei               texSize;

	OculusTextureBuffer(ovrSession session, OVR::Sizei size, int sampleCount) :
		Session(session),
		ColorTextureChain(nullptr),
		DepthTextureChain(nullptr),
		fboId(0),
		texSize(0, 0)
	{
		assert(sampleCount <= 1); // The code doesn't currently handle MSAA textures.

		texSize = size;

		// This texture isn't necessarily going to be a rendertarget, but it usually is.
		assert(session); // No HMD? A little odd.

		ovrTextureSwapChainDesc desc = {};
		desc.Type = ovrTexture_2D;
		desc.ArraySize = 1;
		desc.Width = size.w;
		desc.Height = size.h;
		desc.MipLevels = 1;
		desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.SampleCount = sampleCount;
		desc.StaticImage = ovrFalse;

		{
			ovrResult result = ovr_CreateTextureSwapChainGL(Session, &desc, &ColorTextureChain);

			int length = 0;
			ovr_GetTextureSwapChainLength(session, ColorTextureChain, &length);

			if (OVR_SUCCESS(result))
			{
				for (int i = 0; i < length; ++i)
				{
					GLuint chainTexId;
					ovr_GetTextureSwapChainBufferGL(Session, ColorTextureChain, i, &chainTexId);
					glBindTexture(GL_TEXTURE_2D, chainTexId);

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}
			}
		}

		desc.Format = OVR_FORMAT_D32_FLOAT;

		{
			ovrResult result = ovr_CreateTextureSwapChainGL(Session, &desc, &DepthTextureChain);

			int length = 0;
			ovr_GetTextureSwapChainLength(session, DepthTextureChain, &length);

			if (OVR_SUCCESS(result))
			{
				for (int i = 0; i < length; ++i)
				{
					GLuint chainTexId;
					ovr_GetTextureSwapChainBufferGL(Session, DepthTextureChain, i, &chainTexId);
					glBindTexture(GL_TEXTURE_2D, chainTexId);

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}
			}
		}

		glGenFramebuffers(1, &fboId);
	}

	~OculusTextureBuffer()
	{
		if (ColorTextureChain)
		{
			ovr_DestroyTextureSwapChain(Session, ColorTextureChain);
			ColorTextureChain = nullptr;
		}
		if (DepthTextureChain)
		{
			ovr_DestroyTextureSwapChain(Session, DepthTextureChain);
			DepthTextureChain = nullptr;
		}
		if (fboId)
		{
			glDeleteFramebuffers(1, &fboId);
			fboId = 0;
		}
	}

	OVR::Sizei GetSize() const
	{
		return texSize;
	}

	void SetAndClearRenderSurface()
	{
		GLuint curColorTexId;
		GLuint curDepthTexId;
		{
			int curIndex;
			ovr_GetTextureSwapChainCurrentIndex(Session, ColorTextureChain, &curIndex);
			ovr_GetTextureSwapChainBufferGL(Session, ColorTextureChain, curIndex, &curColorTexId);
		}
		{
			int curIndex;
			ovr_GetTextureSwapChainCurrentIndex(Session, DepthTextureChain, &curIndex);
			ovr_GetTextureSwapChainBufferGL(Session, DepthTextureChain, curIndex, &curDepthTexId);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, fboId);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curColorTexId, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, curDepthTexId, 0);

		glViewport(0, 0, texSize.w, texSize.h);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_FRAMEBUFFER_SRGB);
	}

	void UnsetRenderSurface()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fboId);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
	}

	void Commit()
	{
		ovr_CommitTextureSwapChain(Session, ColorTextureChain);
		ovr_CommitTextureSwapChain(Session, DepthTextureChain);
	}
};

#pragma endregion VR_HELPER_FN

#pragma region VR_STATE
OculusTextureBuffer * eyeRenderTexture[2] = { nullptr, nullptr };
ovrSession session;
ovrGraphicsLuid luid;
ovrHmdDesc hmdDesc;
double sensorSampleTime;
ovrEyeRenderDesc eyeRenderDesc[2];
ovrPosef EyeRenderPose[2];
ovrPosef HmdToEyePose[2];
#pragma endregion VR_STATE



#pragma region OPENGL_RENDERING
void loadBufferData() {
	// vertex position, color
	float vertexData[32] = {
		 -0.5, -0.5, -2.0, 1.0 ,  1.0, 0.0, 0.0, 1.0  ,
		 -0.5,  0.5, -2.0, 1.0 ,  0.0, 1.0, 0.0, 1.0  ,
		  0.5,  0.5, -2.0, 1.0 ,  0.0, 0.0, 1.0, 1.0  ,
		  0.5, -0.5, -2.0, 1.0 ,  1.0, 1.0, 1.0, 1.0
	};

	glGenVertexArrays(1, &vertexArrayObject);
	glBindVertexArray(vertexArrayObject);

	glGenBuffers(1, &vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, 32 * sizeof(float), vertexData, GL_STATIC_DRAW);

	glEnableVertexAttribArray(positionAttribute);
	glEnableVertexAttribArray(colorAttribute);
	int vertexSize = sizeof(float) * 8;
	glVertexAttribPointer(positionAttribute, 4, GL_FLOAT, GL_FALSE, vertexSize, (const GLvoid *)0);
	glVertexAttribPointer(colorAttribute, 4, GL_FLOAT, GL_FALSE, vertexSize, (const GLvoid *)(sizeof(float) * 4));
}

GLuint initShader(const char* vShader, const char* fShader, const char* outputAttributeName) {
	struct Shader {
		GLenum       type;
		const char*      source;
	}  shaders[2] = {
		{ GL_VERTEX_SHADER, vShader },
		{ GL_FRAGMENT_SHADER, fShader }
	};

	GLuint program = glCreateProgram();

	for (int i = 0; i < 2; ++i) {
		Shader& s = shaders[i];
		GLuint shader = glCreateShader(s.type);
		glShaderSource(shader, 1, (const GLchar**)&s.source, NULL);
		glCompileShader(shader);

		GLint  compiled;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (!compiled) {
			std::cerr << " failed to compile:" << std::endl;
			GLint  logSize;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
			char* logMsg = new char[logSize];
			glGetShaderInfoLog(shader, logSize, NULL, logMsg);
			std::cerr << logMsg << std::endl;
			delete[] logMsg;

			exit(EXIT_FAILURE);
		}

		glAttachShader(program, shader);
	}

	/* Link output */
	glBindFragDataLocation(program, 0, outputAttributeName);

	/* link  and error check */
	glLinkProgram(program);

	GLint  linked;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked) {
		std::cerr << "Shader program failed to link" << std::endl;
		GLint  logSize;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logSize);
		char* logMsg = new char[logSize];
		glGetProgramInfoLog(program, logSize, NULL, logMsg);
		std::cerr << logMsg << std::endl;
		delete[] logMsg;

		exit(EXIT_FAILURE);
	}

	/* use program object */
	glUseProgram(program);

	return program;
}


void loadShader() {
	const char *  vert = R"(#version 150
    
    in vec4 position;
    in vec4 color;
	uniform mat4 model;
	uniform mat4 view;
	uniform mat4 projection;
    
    out vec4 colorV;
    
    void main (void)
    {
        colorV = color;
        gl_Position = projection * view * model * position;
    })";
	const char *  frag = R"(#version 150
    
    in vec4 colorV;
    out vec4 fragColor;
    
    void main(void)
    {
        fragColor = colorV;
    })";
	shaderProgram = initShader(vert, frag, "fragColor");

	colorAttribute = glGetAttribLocation(shaderProgram, "color");
	positionAttribute = glGetAttribLocation(shaderProgram, "position");
	modelUniform = glGetUniformLocation(shaderProgram, "model");
	viewUniform = glGetUniformLocation(shaderProgram, "view");
	projectionUniform = glGetUniformLocation(shaderProgram, "projection");
}

void setupOpenGL() {
	std::cout << "OpenGL version " << glGetString(GL_VERSION) << std::endl;
	loadShader();
	loadBufferData();

	glEnable(GL_DEPTH_TEST);
}

void renderWorld(glm::mat4 view, glm::mat4 projection) {
	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(shaderProgram);

	glUniformMatrix4fv(viewUniform, 1, false, glm::value_ptr(view));
	glUniformMatrix4fv(projectionUniform, 1, false, glm::value_ptr(projection));
	for (float x = -10; x <= 10; x += 2.5f) {
		for (float z = -10; z <= 10; z += 2.5f) {
			glm::mat4 modelMat = glm::translate(glm::mat4(1), glm::vec3(x, 0.0f, z));
			glUniformMatrix4fv(modelUniform, 1, false, glm::value_ptr(modelMat));
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}
	}

}

void renderScreen(int width, int height, glm::mat4& viewMatCam) {
	SDL_GL_MakeCurrent(window, gl_context);

	// restore OpenGL state
	glViewport(0, 0, width, height);
	glScissor(0, 0, width, height);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glm::mat4 projection = glm::perspective(glm::radians(60.0f), width / (float)height, 0.1f, 100.0f);
	// render to screen - same view as last eye, but different projection
	renderWorld(viewMatCam, projection);

	SDL_GL_SwapWindow(window);
}
#pragma endregion OPENGL_RENDERING

#pragma region VR_RENDERING
void setupOculus() {
	ovrInitParams initParams = { ovrInit_RequestVersion | ovrInit_FocusAware, OVR_MINOR_VERSION, NULL, 0, 0 };
	ovrResult result = ovr_Initialize(&initParams);
	assert(OVR_SUCCESS(result) && "Failed to initialize libOVR.");
	result = ovr_Create(&session, &luid);
	assert(OVR_SUCCESS(result) && "Failed to create libOVR session.");

	hmdDesc = ovr_GetHmdDesc(session);

	// Setup Window and Graphics
	// Note: the mirror window can be any size, for this sample we use 1/2 the HMD resolution
	ovrSizei windowSize = { hmdDesc.Resolution.w / 2, hmdDesc.Resolution.h / 2 };

	// Make eye render buffers
	for (int eye = 0; eye < 2; eye++) {
		int pixelsPerDisplayUnits = 1;
		ovrSizei idealTextureSize = ovr_GetFovTextureSize(session, ovrEyeType(eye), hmdDesc.DefaultEyeFov[eye], pixelsPerDisplayUnits);
		eyeRenderTexture[eye] = new OculusTextureBuffer(session, idealTextureSize, 1);
	}
}


void renderVR(glm::mat4 & viewMatWorld, glm::mat4 & viewMatCam, int frameIndex) {
	for (int eye = 0; eye < 2; eye++) {
		eyeRenderTexture[eye]->SetAndClearRenderSurface();
		// Get view and projection matrices

		float Yaw = 0;
		static OVR::Vector3f Pos2(0.0f, 0.0f, -5.0f);
		OVR::Matrix4f rollPitchYaw = OVR::Matrix4f::RotationY(Yaw);
		OVR::Matrix4f finalRollPitchYaw = OVR::Matrix4f(EyeRenderPose[eye].Orientation);
		OVR::Vector3f finalUp = finalRollPitchYaw.Transform(OVR::Vector3f(0, 1, 0));
		OVR::Vector3f finalForward = finalRollPitchYaw.Transform(OVR::Vector3f(0, 0, -1));
		OVR::Vector3f shiftedEyePos = Pos2 + rollPitchYaw.Transform(EyeRenderPose[eye].Position);

		OVR::Matrix4f view = OVR::Matrix4f::LookAtRH(shiftedEyePos, shiftedEyePos + finalForward, finalUp);
		OVR::Matrix4f proj = ovrMatrix4f_Projection(hmdDesc.DefaultEyeFov[eye], nearPlane, farPlane, ovrProjection_None);
		viewMatCam = glm::transpose(glm::make_mat4(view.M[0])) * viewMatWorld;
		glm::mat4 projG = glm::transpose(glm::make_mat4(proj.M[0]));

		renderWorld(viewMatCam, projG);

		// Avoids an error when calling SetAndClearRenderSurface during next iteration.
		// Without this, during the next while loop iteration SetAndClearRenderSurface
		// would bind a framebuffer with an invalid COLOR_ATTACHMENT0 because the texture ID
		// associated with COLOR_ATTACHMENT0 had been unlocked by calling wglDXUnlockObjectsNV.
		eyeRenderTexture[eye]->UnsetRenderSurface();

		// Commit changes to the textures so they get picked up frame
		eyeRenderTexture[eye]->Commit();
	}

	// Do distortion rendering, Present and flush/sync

	ovrLayerEyeFovDepth ld = {};
	ld.Header.Type = ovrLayerType_EyeFovDepth;
	ld.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;   // Because OpenGL.
	ovrTimewarpProjectionDesc posTimewarpProjectionDesc = {};
	ld.ProjectionDesc = posTimewarpProjectionDesc;

	for (int eye = 0; eye < 2; ++eye)
	{
		ld.ColorTexture[eye] = eyeRenderTexture[eye]->ColorTextureChain;
		ld.DepthTexture[eye] = eyeRenderTexture[eye]->DepthTextureChain;
		ld.Viewport[eye] = OVR::Recti(eyeRenderTexture[eye]->GetSize());
		ld.Fov[eye] = hmdDesc.DefaultEyeFov[eye];
		ld.RenderPose[eye] = EyeRenderPose[eye];
		ld.SensorSampleTime = sensorSampleTime;
	}

	ovrLayerHeader* layers = &ld.Header;
	ovr_SubmitFrame(session, frameIndex, nullptr, &layers, 1);
}

void updateHMDMatrixPose(int frameIndex) {
	// Call ovr_GetRenderDesc each frame to get the ovrEyeRenderDesc, as the returned values (e.g. HmdToEyePose) may change at runtime.
	eyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
	eyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);

	// Get eye poses, feeding in correct IPD offset
	HmdToEyePose[0] = eyeRenderDesc[0].HmdToEyePose;
	HmdToEyePose[1] = eyeRenderDesc[1].HmdToEyePose;

	// this data is fetched only for the debug display, no need to do this to just get the rendering work
	auto m_frameTiming = ovr_GetPredictedDisplayTime(session, frameIndex);
	auto m_trackingState = ovr_GetTrackingState(session, m_frameTiming, ovrTrue);

	// sensorSampleTime is fed into the layer later
	ovr_GetEyePoses(session, frameIndex, ovrTrue, HmdToEyePose, EyeRenderPose, &sensorSampleTime);


}
#pragma endregion VR_RENDERING


int main(int argc, char* argv[]) {

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);    // Initialize SDL2

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	// Create an application window with the following settings:
	int width = 640;
	int height = 480;
	window = SDL_CreateWindow(
		"An SDL2 window",                  // window title
		SDL_WINDOWPOS_UNDEFINED,           // initial x position
		SDL_WINDOWPOS_UNDEFINED,           // initial y position
		width,                               // width, in pixels
		height,                               // height, in pixels
		SDL_WINDOW_OPENGL
	);

	// Check that the window was successfully made
	if (window == NULL) {
		// In the event that the window could not be made...
		printf("Could not create window: %s\n", SDL_GetError());
		return 1;
	}

	gl_context = SDL_GL_CreateContext(window);

	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		/* Problem: glewInit failed, something is seriously wrong. */
		std::cout << "Error initializing OpenGL using GLEW: " << glewGetErrorString(err) << std::endl;
	}


	std::cout << glGetString(GL_VERSION) << std::endl;
	std::cout << "OpenGL version " << glGetString(GL_VERSION) << "." << std::endl;

	setupOpenGL();
	setupOculus();

	while (!quitting) {

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				quitting = true;
			}
			if (event.type == SDL_KEYDOWN) {
				switch (event.key.keysym.sym) {
				case SDLK_w:
					pos.z += 0.1;
					break;
				case SDLK_s:
					pos.z -= 0.1;
					break;
				case SDLK_a:
					pos.x -= 0.1;
					break;
				case SDLK_d:
					pos.x += 0.1;
					break;
				}
			}
		}

		glm::mat4 viewMatWorld = glm::translate(glm::mat4(1), pos);
		glm::mat4 viewMatCam;
		
		static int frameIndex = 0;
		frameIndex++;

		updateHMDMatrixPose(frameIndex);
		
		renderVR(viewMatWorld, viewMatCam, frameIndex);
		renderScreen(width, height, viewMatCam);
	}

	SDL_GL_DeleteContext(gl_context);

	// Close and destroy the window
	SDL_DestroyWindow(window);

	// Clean up
	SDL_Quit();
	return 0;
}