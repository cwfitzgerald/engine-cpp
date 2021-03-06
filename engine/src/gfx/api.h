#ifndef API_H
#define API_H

#include "../core/builder.h"
#include "../core/types.h"

extern "C" {
	#include "glad/glad.h"
}

NS_BEGIN

namespace api {

#define DEF_GL_TYPE_TRAIT_R(name, gen, del) \
struct GL##name { \
	static GLuint create(GLenum param = GL_NONE) gen \
	static void destroy(GLuint v) del \
}

#define DEF_GL_TYPE_TRAIT(name, gen, del) \
DEF_GL_TYPE_TRAIT_R(name, { GLuint v; gen(1, &v); return v; }, { del(1, &v); })

	DEF_GL_TYPE_TRAIT(Texture, glGenTextures, glDeleteTextures);
	DEF_GL_TYPE_TRAIT(Sampler, glGenSamplers, glDeleteSamplers);
	DEF_GL_TYPE_TRAIT(Buffer, glGenBuffers, glDeleteBuffers);
	DEF_GL_TYPE_TRAIT(Framebuffer, glGenFramebuffers, glDeleteFramebuffers);
	DEF_GL_TYPE_TRAIT(Renderbuffer, glGenRenderbuffers, glDeleteRenderbuffers);
	DEF_GL_TYPE_TRAIT(VertexArray, glGenVertexArrays, glDeleteVertexArrays);
	
	DEF_GL_TYPE_TRAIT_R(Shader, { return glCreateShader(param); }, { glDeleteShader(v); });
	DEF_GL_TYPE_TRAIT_R(Program, { return glCreateProgram(); }, { glDeleteProgram(v); });

	enum ClearBufferMask {
		ColorBuffer = GL_COLOR_BUFFER_BIT,
		DepthBuffer = GL_DEPTH_BUFFER_BIT,
		StencilBuffer = GL_STENCIL_BUFFER_BIT
	};
	
	enum PrimitiveType {
		Points = GL_POINTS,
		Lines = GL_LINES,
		LineLoop = GL_LINE_LOOP,
		LineStrip = GL_LINE_STRIP,
		Triangles = GL_TRIANGLES,
		TriangleFan = GL_TRIANGLE_FAN,
		TriangleStrip = GL_TRIANGLE_STRIP
	};

	enum DataType {
		Int = GL_INT,
		Short = GL_SHORT,
		Float = GL_FLOAT,
		Byte = GL_BYTE,
		UInt = GL_UNSIGNED_INT,
		UShort = GL_UNSIGNED_SHORT,
		UByte = GL_UNSIGNED_BYTE
	};

	enum ShaderType {
		VertexShader = GL_VERTEX_SHADER,
		FragmentShader = GL_FRAGMENT_SHADER,
		GeometryShader = GL_GEOMETRY_SHADER
	};

	enum BufferType {
		ArrayBuffer = GL_ARRAY_BUFFER,
		IndexBuffer = GL_ELEMENT_ARRAY_BUFFER,
		UniformBuffer = GL_UNIFORM_BUFFER
	};

	enum BufferUsage {
		Static = GL_STATIC_DRAW,
		Dynamic = GL_DYNAMIC_DRAW,
		Stream = GL_STREAM_DRAW
	};

	enum TextureTarget {
		Texture1D = GL_TEXTURE_1D,
		Texture1DArray = GL_TEXTURE_1D_ARRAY,
		Texture2D = GL_TEXTURE_2D,
		Texture2DArray = GL_TEXTURE_2D_ARRAY,
		Texture3D = GL_TEXTURE_3D,
		CubeMap = GL_TEXTURE_CUBE_MAP,
		CubeMapPX = GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		CubeMapNX = GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		CubeMapPY = GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		CubeMapNY = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
		CubeMapPZ = GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		CubeMapNZ = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
	};
	
	enum TextureWrap {
		None = 0,
		Repeat = GL_REPEAT,
		ClampToEdge = GL_CLAMP_TO_EDGE,
		ClampToBorder = GL_CLAMP_TO_BORDER
	};
	
	enum TextureFilter {
		Nearest = GL_NEAREST,
		Linear = GL_LINEAR,
		NearestMipNearest = GL_NEAREST_MIPMAP_NEAREST,
		NearestMipLinear = GL_NEAREST_MIPMAP_LINEAR,
		LinearMipLinear = GL_LINEAR_MIPMAP_LINEAR,
		LinearMipNearest = GL_LINEAR_MIPMAP_NEAREST
	};
	
	enum TextureFormat {
		R = 0,
		RG,
		RGB,
		RGBA,
		Rf,
		RGf,
		RGBf,
		RGBAf,
		Depth,
		DepthStencil
	};

	enum FrameBufferTarget {
		Framebuffer = GL_FRAMEBUFFER,
		DrawFramebuffer = GL_DRAW_FRAMEBUFFER,
		ReadFramebuffer = GL_READ_FRAMEBUFFER
	};

	enum Attachment {
		ColorAttachment = GL_COLOR_ATTACHMENT0,
		DepthAttachment = GL_DEPTH_ATTACHMENT,
		StencilAttachment = GL_STENCIL_ATTACHMENT,
		DepthStencilAttachment = GL_DEPTH_STENCIL_ATTACHMENT
	};
	
	enum DataAccess {
		ReadOnly = GL_READ_ONLY,
		WriteOnly = GL_WRITE_ONLY,
		ReadWrite = GL_READ_WRITE
	};
	
	static i32 getDataTypeSize(DataType dt) {
		switch (dt) {
			case DataType::Byte:
			case DataType::UByte:
				return 1;
			case DataType::Float:
			case DataType::Int:
			case DataType::UInt:
				return 4;
			case DataType::Short:
			case DataType::UShort:
				return 2;
		}
	}
	
	static std::tuple<GLint, GLenum, DataType> getTextureFormat(TextureFormat format) {
		GLenum ifmt;
		GLint fmt;
		DataType type;
		switch (format) {
			case TextureFormat::R: ifmt = GL_R8; fmt = GL_RED; type = DataType::UByte; break;
			case TextureFormat::RG: ifmt = GL_RG8; fmt = GL_RG; type = DataType::UByte; break;
			case TextureFormat::RGB: ifmt = GL_RGB8; fmt = GL_RGB; type = DataType::UByte; break;
			case TextureFormat::RGBA: ifmt = GL_RGBA8; fmt = GL_RGBA; type = DataType::UByte; break;
			case TextureFormat::Rf: ifmt = GL_R32F; fmt = GL_RED; type = DataType::Float; break;
			case TextureFormat::RGf: ifmt = GL_RG32F; fmt = GL_RG; type = DataType::Float; break;
			case TextureFormat::RGBf: ifmt = GL_RGB32F; fmt = GL_RGB; type = DataType::Float; break;
			case TextureFormat::RGBAf: ifmt = GL_RGBA32F; fmt = GL_RGBA; type = DataType::Float; break;
			case TextureFormat::Depth: ifmt = GL_DEPTH_COMPONENT24; fmt = GL_DEPTH_COMPONENT; type = DataType::Float; break;
			case TextureFormat::DepthStencil: ifmt = GL_DEPTH24_STENCIL8; fmt = GL_DEPTH_STENCIL; type = DataType::Float; break;
		}
		return tup(ifmt, fmt, type);
	}
	
}

NS_END

#endif // API_H