#ifndef TEXTURE_H
#define TEXTURE_H

#include "../core/builder.h"
#include "../core/types.h"

#include "api.h"
using namespace api;

NS_BEGIN

struct ImageData {
	u8* data;
	float* fdata;
	int w, h, comp;
	bool hdr;
};

class Sampler {
	friend class Texture;
public:
	Sampler() = default;
	Sampler(GLuint id) : m_id(id) {}
	
	Sampler& setWrap(
			TextureWrap s = TextureWrap::Repeat,
			TextureWrap t = TextureWrap::Repeat,
			TextureWrap r = TextureWrap::None
	);
	Sampler& setFilter(TextureFilter min, TextureFilter mag);
	
	Sampler& setSeamlessCubemap(bool enable);
	
	void bind(u32 slot) const { glBindSampler(slot, m_id); }
	void unbind(u32 slot) const { glBindSampler(slot, 0); }
	
protected:
	GLuint m_id;
};

class Texture {
public:
	Texture() = default;
	Texture(GLuint id) : m_id(id) {}

	Texture& setFromFile(const String& file, TextureTarget tgt);
	Texture& setFromFile(const String& file);
	Texture& setNull(int w, int h, GLint ifmt, GLenum fmt, DataType data);
	
	Texture& generateMipmaps();
	
	Texture& bind(TextureTarget target);
	void bind(const Sampler& sampler, u32 slot = 0);
	void unbind();
	
	TextureTarget target() const { return m_target; }
	
private:
	GLuint m_id;
	TextureTarget m_target;
};

template <>
class Builder<Texture> {
public:
	static Texture build() {
		g_textures.push_back(GLTexture::create());
		return Texture(g_textures.back());
	}
	
	static void clean() {
		for (GLuint b : g_textures) {
			GLTexture::destroy(b);
		}
	}
private:
	static Vector<GLuint> g_textures;
};

template <>
class Builder<Sampler> {
public:
	static Sampler build() {
		g_samplers.push_back(GLSampler::create());
		return Sampler(g_samplers.back());
	}
	
	static void clean() {
		for (GLuint b : g_samplers) {
			GLSampler::destroy(b);
		}
	}
private:
	static Vector<GLuint> g_samplers;
};

NS_END

#endif /* TEXTURE_H */

