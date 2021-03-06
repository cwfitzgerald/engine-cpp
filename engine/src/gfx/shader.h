#ifndef SHADER_H
#define SHADER_H

#include "api.h"
using namespace api;

#include "../core/builder.h"
#include "../core/types.h"

#include "../math/vec.h"
#include "../math/mat.h"

NS_BEGIN

#define glUniform(T, ...) glUniform##T(location, __VA_ARGS__)

class Uniform {
	friend class ShaderProgram;
public:
	void set(u64 v) { glUniform(1ui, v); }
	void set(i32 v) { glUniform(1i, v); }
	void set(float v) { glUniform(1f, v); }
	void set(Vec2 v) { glUniform(2f, v.x, v.y); }
	void set(Vec3 v) { glUniform(3f, v.x, v.y, v.z); }
	void set(Vec4 v) { glUniform(4f, v.x, v.y, v.z, v.w); }
	void set(Mat4 v) { glUniform(Matrix4fv, 1, false, v.val); }
	
	void set(const Mat4* v, u32 size) {
		Vector<float> mv; mv.reserve(size * 16);
		for (u32 i = 0; i < size; i++) {
			mv.insert(mv.begin(), v[i].val, v[i].val + 16);
		}
		glUniform(Matrix4fv, size, false, mv.data());
	}
	
	void set(const Vector<Mat4>& v) {
		Vector<float> mv; mv.reserve(v.size() * 16);
		for (Mat4 m : v) {
			mv.insert(mv.begin(), m.val, m.val + 16);
		}
		glUniform(Matrix4fv, v.size(), false, mv.data());
	}

protected:
	i32 location;
	Uniform(i32 loc) : location(loc) {}
};

class ShaderProgram {
public:
	ShaderProgram() = default;
	ShaderProgram(GLuint prog) : m_program(prog) {}
	
	ShaderProgram& add(const String& source, ShaderType type);
	void link();
	
	void bind();
	void unbind();

	i32 getAttributeLocation(const String& name);
	i32 getUniformLocation(const String& name);

	Uniform get(const String& name);

	bool valid() const { return m_valid; }
	
protected:
	GLuint m_program;
	bool m_valid;
	Map<String, i32> m_uniforms, m_attributes;
};

template <>
class Builder<ShaderProgram> {
public:
	static ShaderProgram build() {
		g_programs.push_back(GLProgram::create());
		return ShaderProgram(g_programs.back());
	}
	
	static void clean() {
		for (GLuint prog : g_programs) {
			GLProgram::destroy(prog);
		}
	}
private:
	static Vector<GLuint> g_programs;
};

NS_END

#endif // SHADER_H