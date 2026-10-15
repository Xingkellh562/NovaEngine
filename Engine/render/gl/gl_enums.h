#pragma once

#include <render/core/enums.h>
#include <render/core/sampler_state.h>
#include <render/core/texture_desc.h>
#include <render/core/shader_desc.h>

#include <glad/glad.h>

namespace nova_engine::render::gl {

	inline GLenum ToGLDepthFunc(core::DepthTest v) {
		switch (v) {
		case core::DepthTest::Less:      return GL_LESS;
		case core::DepthTest::LessEqual: return GL_LEQUAL;
		case core::DepthTest::Greater:   return GL_GREATER;
		case core::DepthTest::Always:
		case core::DepthTest::None:
		default:                   return GL_ALWAYS;
		}
	}

	inline GLenum ToGLCullFace(core::CullMode v) {
		switch (v) {
		case core::CullMode::Front: return GL_FRONT;
		case core::CullMode::Back:
		default:              return GL_BACK;
		}
	}

	inline GLenum ToGLTopology(core::Topology v) {
		switch (v) {
		case core::Topology::LineList:  return GL_LINES;
		case core::Topology::PointList: return GL_POINTS;
		case core::Topology::TriangleList:
		default:                  return GL_TRIANGLES;
		}
	}

	inline GLint ToGLMinFilter(core::TextureFilter v) {
		switch (v) {
		case core::TextureFilter::Nearest:            return GL_NEAREST;
		case core::TextureFilter::NearestMipmapLinear:return GL_NEAREST_MIPMAP_LINEAR;
		case core::TextureFilter::LinearMipmapLinear: return GL_LINEAR_MIPMAP_LINEAR;
		case core::TextureFilter::Linear:
		default:                                return GL_LINEAR;
		}
	}

	inline GLint ToGLMagFilter(core::TextureFilter v) {
		switch (v) {
		case core::TextureFilter::Nearest:
		case core::TextureFilter::NearestMipmapLinear:
			return GL_NEAREST;
		case core::TextureFilter::Linear:
		case core::TextureFilter::LinearMipmapLinear:
		default:
			return GL_LINEAR;
		}
	}

	inline GLint ToGLWrap(core::TextureWrap v) {
		switch (v) {
		case core::TextureWrap::Repeat:        return GL_REPEAT;
		case core::TextureWrap::ClampToBorder: return GL_CLAMP_TO_BORDER;
		case core::TextureWrap::ClampToEdge:
		default:                         return GL_CLAMP_TO_EDGE;
		}
	}

	// (internalFormat, format, type) 三元组。
	// 取值逐条对应改动前 frame_buffer_library.cpp:9-40 里硬编码的参数。
	struct GLTexFormat { GLenum internalFormat; GLenum format; GLenum type; };

	inline GLTexFormat ToGLTexFormat(core::TextureFormat v) {
		switch (v) {
		case core::TextureFormat::RGB16F:   return { GL_RGB16F,             GL_RGB,             GL_FLOAT };
		case core::TextureFormat::RGBA16F:  return { GL_RGBA16F,            GL_RGBA,            GL_FLOAT };
		case core::TextureFormat::R16F:     return { GL_R16F,               GL_RED,             GL_FLOAT };
		case core::TextureFormat::Depth32F: return { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT };
		case core::TextureFormat::RGBA8:
		default:                      return { GL_RGBA,               GL_RGBA,            GL_UNSIGNED_BYTE };
		}
	}

	inline GLenum ToGLShaderStage(core::ShaderStage v) {
		switch (v) {
		case core::ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
		case core::ShaderStage::Geometry: return GL_GEOMETRY_SHADER;
		case core::ShaderStage::Compute:  return GL_COMPUTE_SHADER;
		case core::ShaderStage::Vertex:
		default:                    return GL_VERTEX_SHADER;
		}
	}

}