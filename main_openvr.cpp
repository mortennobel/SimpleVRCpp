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
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/transform.hpp"
#include "glm/gtx/string_cast.hpp"

#pragma region VR_INCLUDES
#include <openvr.h>
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
std::string GetTrackedDeviceString(vr::IVRSystem *pHmd, vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError *peError = NULL)
{
	uint32_t unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, NULL, 0, peError);
	if (unRequiredBufferLen == 0)
		return "";

	char *pchBuffer = new char[unRequiredBufferLen];
	unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer, unRequiredBufferLen, peError);
	std::string sResult = pchBuffer;
	delete[] pchBuffer;
	return sResult;
}

void createFBO(GLuint*  FB, GLuint*  tex, GLuint*  depthTex, int targetSizeW, int targetSizeH) {
	glCreateTextures(GL_TEXTURE_2D, 1, tex);
	glCreateTextures(GL_TEXTURE_2D, 1, depthTex);
	glCreateFramebuffers(1, FB);

	glBindFramebuffer(GL_FRAMEBUFFER, *FB);
	unsigned int texture;
	glBindTexture(GL_TEXTURE_2D, *tex);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, targetSizeW, targetSizeH, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);

	glBindTexture(GL_TEXTURE_2D, *depthTex);
	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, targetSizeW, targetSizeH, 0,
		GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL
	);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, *depthTex, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

#pragma endregion VR_HELPER_FN

#pragma region VR_STATE
uint32_t targetSizeW;
uint32_t targetSizeH;
glm::mat4 mat4ProjectionLeft;
glm::mat4 mat4ProjectionRight;
glm::mat4 mat4ViewLeft;
glm::mat4 mat4ViewRight;
glm::mat4 mat4eyePosLeft;
glm::mat4 mat4eyePosRight;
glm::mat4 getHMDMatrixPoseEye(vr::Hmd_Eye nEye);
glm::mat4 getHMDMatrixProjectionEye(vr::Hmd_Eye nEye);
vr::IVRSystem* vrSystem;
vr::TrackedDevicePose_t m_rTrackedDevicePose[vr::k_unMaxTrackedDeviceCount];
glm::mat4 m_rmat4DevicePose[vr::k_unMaxTrackedDeviceCount];
bool m_rbShowTrackedDevice[vr::k_unMaxTrackedDeviceCount];
GLuint  leftFB;
GLuint  leftTex;
GLuint  leftDepthTex;
GLuint  rightFB;
GLuint  rightTex;
GLuint  rightDepthTex;
int m_iValidPoseCount;
int m_iValidPoseCount_Last;
glm::mat4 m_mat4HMDPose;
std::string m_strPoseClasses;                          // what classes we saw poses for this frame
char m_rDevClassChar[vr::k_unMaxTrackedDeviceCount];   // for each device, a character representing its class

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
	glEnable(GL_FRAMEBUFFER_SRGB);
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
void setupCameras()
{
	mat4ProjectionLeft = getHMDMatrixProjectionEye(vr::Eye_Left);
	mat4ProjectionRight = getHMDMatrixProjectionEye(vr::Eye_Right);
}

void setupOpenVR() {
	vr::HmdError peError = vr::VRInitError_None;
	vrSystem = vr::VR_Init(&peError, vr::VRApplication_Scene);
	if (peError != vr::VRInitError_None)
	{
		vrSystem = nullptr;
		std::cout << vr::VR_GetVRInitErrorAsEnglishDescription(peError) << std::endl;
		return;
	}
	else
	{
		auto m_strDriver = GetTrackedDeviceString(vrSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
		auto m_strDisplay = GetTrackedDeviceString(vrSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);
		std::cout << "HMD Driver: " << m_strDriver.c_str() << std::endl;
		std::cout << "HMD Display: " << m_strDisplay.c_str() << std::endl;
	}

	if (!vr::VRCompositor())
	{
		std::cout << "Compositor initialization failed. See log file for details\n";
		return;
	}
	memset(m_rDevClassChar, 0, sizeof(m_rDevClassChar));

	vrSystem->GetRecommendedRenderTargetSize(&targetSizeW, &targetSizeH);
	
	createFBO(&leftFB, &leftTex, &leftDepthTex, targetSizeW, targetSizeH);
	createFBO(&rightFB, &rightTex, &rightDepthTex, targetSizeW, targetSizeH);
	
	setupCameras();
	mat4eyePosLeft = getHMDMatrixPoseEye(vr::Eye_Left);
	mat4eyePosRight = getHMDMatrixPoseEye(vr::Eye_Right);
}


glm::mat4 getHMDMatrixProjectionEye(vr::Hmd_Eye nEye)
{
	vr::HmdMatrix44_t mat = vrSystem->GetProjectionMatrix(nEye, nearPlane, farPlane);

	return glm::mat4(
		mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
		mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1],
		mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2],
		mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]
	);
}

glm::mat4 getHMDMatrixPoseEye(vr::Hmd_Eye nEye)
{
	vr::HmdMatrix34_t matEyeRight = vrSystem->GetEyeToHeadTransform(nEye);
	glm::mat4 matrixObj(
		matEyeRight.m[0][0], matEyeRight.m[1][0], matEyeRight.m[2][0], 0.0,
		matEyeRight.m[0][1], matEyeRight.m[1][1], matEyeRight.m[2][1], 0.0,
		matEyeRight.m[0][2], matEyeRight.m[1][2], matEyeRight.m[2][2], 0.0,
		matEyeRight.m[0][3], matEyeRight.m[1][3], matEyeRight.m[2][3], 1.0f
	);

	return  matrixObj;
}

void updateHMDMatrixPose()
{
	
	vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, nullptr, 0);

	m_iValidPoseCount = 0;
	m_strPoseClasses = "";
	for (int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice)
	{
		if (m_rTrackedDevicePose[nDevice].bPoseIsValid)
		{
			m_iValidPoseCount++;
			float *f = &(m_rTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking.m[0][0]);
			glm::mat3x4 m = glm::make_mat3x4(f);
			m_rmat4DevicePose[nDevice] = glm::transpose((glm::mat4)m);
			if (m_rDevClassChar[nDevice] == 0)
			{
				switch (vrSystem->GetTrackedDeviceClass(nDevice))
				{
				case vr::TrackedDeviceClass_Controller:        m_rDevClassChar[nDevice] = 'C'; break;
				case vr::TrackedDeviceClass_HMD:               m_rDevClassChar[nDevice] = 'H'; break;
				case vr::TrackedDeviceClass_Invalid:           m_rDevClassChar[nDevice] = 'I'; break;
				case vr::TrackedDeviceClass_GenericTracker:    m_rDevClassChar[nDevice] = 'G'; break;
				case vr::TrackedDeviceClass_TrackingReference: m_rDevClassChar[nDevice] = 'T'; break;
				default:                                       m_rDevClassChar[nDevice] = '?'; break;
				}
			}
			m_strPoseClasses += m_rDevClassChar[nDevice];
		}
	}

	if (m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
	{
		m_mat4HMDPose = m_rmat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd];
		mat4ViewLeft = glm::inverse(m_mat4HMDPose*mat4eyePosLeft);
		mat4ViewRight = glm::inverse(m_mat4HMDPose*mat4eyePosRight);
	}
	
}

void renderVR(glm::mat4 & viewMatWorld) {
	updateHMDMatrixPose();
	
	glViewport(0, 0, targetSizeW, targetSizeH);
	glScissor(0, 0, targetSizeW, targetSizeH);

	glBindFramebuffer(GL_FRAMEBUFFER, leftFB);
	renderWorld(mat4ViewLeft*viewMatWorld, mat4ProjectionLeft);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	vr::Texture_t leftEyeTexture = { (void*)(uintptr_t)leftTex, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
	vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture);
	
	glBindFramebuffer(GL_FRAMEBUFFER, rightFB);
	
	renderWorld(mat4ViewRight*viewMatWorld, mat4ProjectionRight);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	vr::Texture_t rightEyeTexture = { (void*)(uintptr_t)rightTex, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
	vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture);
	

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
	setupOpenVR();

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
		
		renderVR(viewMatWorld);
		renderScreen(width, height, viewMatWorld);
		SDL_Delay(1);
	}

	vr::VR_Shutdown();

	SDL_GL_DeleteContext(gl_context);

	// Close and destroy the window
	SDL_DestroyWindow(window);

	// Clean up
	SDL_Quit();
	return 0;
}