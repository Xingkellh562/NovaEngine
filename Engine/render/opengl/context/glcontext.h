#pragma once
#include <glad/glad.h>

namespace nova_engine::render::opengl::context {
	struct GLContext {
		GLuint mainFBO = 0;
		GLuint shadowFBO = 0;
		GLuint oitFbo = 0;
		GLuint colorTex = 0, depthTex = 0;
		GLuint shadowMap = 0;
		GLuint accumTex = 0, revealTex = 0;
	};
}