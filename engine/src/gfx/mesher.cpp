#include <cfloat>

#include "mesher.h"

#include "../core/logging/log.h"

NS_BEGIN

Vector<GLuint> Builder<Mesh>::g_vbos;
Vector<GLuint> Builder<Mesh>::g_ibos;
Vector<GLuint> Builder<Mesh>::g_vaos;

void VertexFormat::put(const String& name, AttributeType type, bool normalized, i32 location) {
	VertexAttribute attr;
	attr.size = type;
	attr.normalized = normalized;
	attr.location = location;
	attr.name = name;
	m_attributes.push_back(attr);

	m_stride += u32(type) * 4;
}

void VertexFormat::bind(ShaderProgram* shader) {
	u32 off = 0;
	bool shaderValid = shader != nullptr;
	for (auto attr : m_attributes) {
		i32 loc = attr.location;
		if (shaderValid && attr.location == -1) {
			loc = shader->getAttributeLocation(attr.name);
		}
		if (loc != -1) {
			glEnableVertexAttribArray(loc);
			glVertexAttribPointer(loc, attr.size, GL_FLOAT, attr.normalized, m_stride, ((void*) off));
		}
		off += attr.size * 4;
	}
}

void VertexFormat::unbind(ShaderProgram* shader) {
	if (shader == nullptr) return;
	for (auto attr : m_attributes) {
		i32 loc = attr.location;
		if (attr.location == -1) {
			loc = shader->getAttributeLocation(attr.name);
		}
		if (loc != -1) {
			glDisableVertexAttribArray(loc);
		}
	}
}

void Mesh::bind() {
	glBindVertexArray(m_vao);
}

void Mesh::unbind() {
	glBindVertexArray(0);
}

u8* Mesh::map() {
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	return (u8*) glMapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE);
}

void Mesh::unmap() {
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glUnmapBuffer(GL_ARRAY_BUFFER);
}

void Mesh::flush() {
	if (m_vertexData.empty()) return;

	glBindVertexArray(m_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

	m_format.bind();

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
	glBindVertexArray(0);

	i32 vsize = m_format.stride() * m_vertexData.size();

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, vsize, m_vertexData.data(), GL_STATIC_DRAW);

	i32 isize = 4 * m_indexData.size();
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, isize, m_indexData.data(), GL_STATIC_DRAW);
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	m_vertexCount = m_vertexData.size();
	m_indexCount = m_indexData.size();

	// Compute bounding box
	Vec3 aabbMin = Vec3(FLT_MAX);
	Vec3 aabbMax = Vec3(FLT_MIN);
	for (Vertex v : m_vertexData) {
		aabbMin.x = std::min(aabbMin.x, v.position.x);
		aabbMin.y = std::min(aabbMin.y, v.position.y);
		aabbMin.z = std::min(aabbMin.z, v.position.z);
		aabbMax.x = std::max(aabbMax.x, v.position.x);
		aabbMax.y = std::max(aabbMax.y, v.position.y);
		aabbMax.z = std::max(aabbMax.z, v.position.z);
	}
	m_aabb = AABB(aabbMin, aabbMax);
	
	m_vertexData.clear();
	m_indexData.clear();
}

Mesh& Mesh::addVertex(const Vertex& vert) {
	m_vertexData.push_back(vert);
	return *this;
}

Mesh& Mesh::addIndex(i32 index) {
	m_indexData.push_back(index);
	return *this;
}

Mesh& Mesh::addTriangle(i32 i0, i32 i1, i32 i2) {
	m_indexData.push_back(i0);
	m_indexData.push_back(i1);
	m_indexData.push_back(i2);
	return *this;
}

Mesh& Mesh::addData(const Vector<Vertex>& vertices, const Vector<i32>& indices) {
	i32 off = m_vertexData.size();
	m_vertexData.insert(m_vertexData.end(), vertices.begin(), vertices.end());
	for (auto i : indices) {
		m_indexData.push_back(off + i);
	}
	return *this;
}

Mesh& Mesh::addFromFile(const String& file) {
	Assimp::Importer imp;
	
	String ext = file.substr(file.find_last_of('.')+1);
	
	VFS::get().openRead(file);
	fsize len;
	u8* data = VFS::get().read(&len);
	VFS::get().close();
	
	const aiScene* scene = imp.ReadFileFromMemory(data, len,
			aiPostProcessSteps::aiProcess_Triangulate |
			aiPostProcessSteps::aiProcess_FlipUVs |
			aiPostProcessSteps::aiProcess_CalcTangentSpace,
			ext.c_str()
	);
	
	delete[] data;
	
	if (scene == nullptr || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
		LogError(imp.GetErrorString());
		return *this;
	}
	addAIScene(scene);

	return *this;
}

Mesh& Mesh::addPlane(Axis axis, float size, const Vec3& off, bool flip) {
	i32 ioff = m_vertexData.size();
	float s = size;
	switch (axis) {
		case Axis::X: {
			addVertex(Vertex(off + Vec3(0, -s, -s), Vec2(0, 0)));
			addVertex(Vertex(off + Vec3(0,  s, -s), Vec2(1, 0)));
			addVertex(Vertex(off + Vec3(0,  s,  s), Vec2(1, 1)));
			addVertex(Vertex(off + Vec3(0, -s,  s), Vec2(0, 1)));
		} break;
		case Axis::Y: {
			addVertex(Vertex(off + Vec3(-s, 0, -s), Vec2(0, 0)));
			addVertex(Vertex(off + Vec3( s, 0, -s), Vec2(1, 0)));
			addVertex(Vertex(off + Vec3( s, 0,  s), Vec2(1, 1)));
			addVertex(Vertex(off + Vec3(-s, 0,  s), Vec2(0, 1)));
		} break;
		case Axis::Z: {
			addVertex(Vertex(off + Vec3(-s, -s, 0), Vec2(0, 0)));
			addVertex(Vertex(off + Vec3( s, -s, 0), Vec2(1, 0)));
			addVertex(Vertex(off + Vec3( s,  s, 0), Vec2(1, 1)));
			addVertex(Vertex(off + Vec3(-s,  s, 0), Vec2(0, 1)));
		} break;
	}
	
	if (!flip) {
		addTriangle(ioff+0, ioff+1, ioff+2);
		addTriangle(ioff+2, ioff+3, ioff+0);
	} else {
		addTriangle(ioff+0, ioff+3, ioff+2);
		addTriangle(ioff+2, ioff+1, ioff+0);
	}
	
	return *this;
}

Mesh& Mesh::calculateNormals(PrimitiveType primitive) {
	switch (primitive) {
		case PrimitiveType::Points:
		case PrimitiveType::Lines:
		case PrimitiveType::LineLoop:
		case PrimitiveType::LineStrip:
			break;
		case PrimitiveType::Triangles:
		{
			for (u32 i = 0; i < m_indexData.size(); i += 3) {
				i32 i0 = index(i + 0);
				i32 i1 = index(i + 1);
				i32 i2 = index(i + 2);
				triNormal(i0, i1, i2);
			}
		} break;
		case PrimitiveType::TriangleFan:
		{
			for (u32 i = 0; i < m_indexData.size(); i += 2) {
				i32 i0 = index(0);
				i32 i1 = index(i);
				i32 i2 = index(i + 1);
				triNormal(i0, i1, i2);
			}
		} break;
		case PrimitiveType::TriangleStrip:
		{
			for (u32 i = 0; i < m_indexData.size(); i += 2) {
				i32 i0, i1, i2;
				if (i % 2 == 0) {
					i0 = index(i + 0);
					i1 = index(i + 1);
					i2 = index(i + 2);
				} else {
					i0 = index(i + 2);
					i1 = index(i + 1);
					i2 = index(i + 0);
				}
				triNormal(i0, i1, i2);
			}
		} break;
	}

	for (auto& v : m_vertexData) {
		v.normal = v.normal.normalized();
	}

	return *this;
}

Mesh& Mesh::calculateTangents(PrimitiveType primitive) {
	switch (primitive) {
		case PrimitiveType::Points:
		case PrimitiveType::Lines:
		case PrimitiveType::LineLoop:
		case PrimitiveType::LineStrip:
			break;
		case PrimitiveType::Triangles:
		{
			for (u32 i = 0; i < m_indexData.size(); i += 3) {
				i32 i0 = index(i + 0);
				i32 i1 = index(i + 1);
				i32 i2 = index(i + 2);
				triTangent(i0, i1, i2);
			}
		} break;
		case PrimitiveType::TriangleFan:
		{
			for (u32 i = 0; i < m_indexData.size(); i += 2) {
				i32 i0 = index(0);
				i32 i1 = index(i);
				i32 i2 = index(i + 1);
				triTangent(i0, i1, i2);
			}
		} break;
		case PrimitiveType::TriangleStrip:
		{
			for (u32 i = 0; i < m_indexData.size(); i += 2) {
				i32 i0, i1, i2;
				if (i % 2 == 0) {
					i0 = index(i + 0);
					i1 = index(i + 1);
					i2 = index(i + 2);
				} else {
					i0 = index(i + 2);
					i1 = index(i + 1);
					i2 = index(i + 0);
				}
				triTangent(i0, i1, i2);
			}
		} break;
	}

	for (auto& v : m_vertexData) {
		v.tangent = v.tangent.normalized();
	}

	return *this;
}

Mesh& Mesh::transformTexCoords(const Mat4& t) {
	for (auto& v : m_vertexData) {
		v.texCoord = (t * Vec4(v.texCoord.x, v.texCoord.y, 0.0f, 1.0f)).xy;
	}
	return *this;
}

void Mesh::triNormal(i32 i0, i32 i1, i32 i2) {
	Vec3 v0 = m_vertexData[i0].position;
	Vec3 v1 = m_vertexData[i1].position;
	Vec3 v2 = m_vertexData[i2].position;

	Vec3 e0 = v1 - v0;
	Vec3 e1 = v2 - v0;
	Vec3 n = e0.cross(e1);

	m_vertexData[i0].normal += n;
	m_vertexData[i1].normal += n;
	m_vertexData[i2].normal += n;
}

void Mesh::triTangent(i32 i0, i32 i1, i32 i2) {
	Vec3 v0 = m_vertexData[i0].position;
	Vec3 v1 = m_vertexData[i1].position;
	Vec3 v2 = m_vertexData[i2].position;

	Vec2 t0 = m_vertexData[i0].texCoord;
	Vec2 t1 = m_vertexData[i1].texCoord;
	Vec2 t2 = m_vertexData[i2].texCoord;

	Vec3 e0 = v1 - v0;
	Vec3 e1 = v2 - v0;
	
	Vec2 dt1 = t1 - t0;
	Vec2 dt2 = t2 - t0;

	float dividend = dt1.perpDot(dt2);
	float f = dividend == 0.0f ? 0.0f : 1.0f / dividend;

	Vec3 t = Vec3();

	t.x = (f * (dt2.y * e0.x - dt1.y * e1.x));
	t.y = (f * (dt2.y * e0.y - dt1.y * e1.y));
	t.z = (f * (dt2.y * e0.z - dt1.y * e1.z));

	m_vertexData[i0].tangent += t;
	m_vertexData[i1].tangent += t;
	m_vertexData[i2].tangent += t;
}

void Mesh::addAIScene(const aiScene* scene) {
	i32 off = m_vertexData.size();

	const aiVector3D aiZeroVector(0.0f, 0.0f, 0.0f);
	const aiColor4D aiOneVector4(1.0f, 1.0f, 1.0f, 1.0f);

	for (u32 m = 0; m < scene->mNumMeshes; m++) {
		aiMesh* mesh = scene->mMeshes[m];
		bool hasPositions = mesh->HasPositions();
		bool hasNormals = mesh->HasNormals();
		bool hasUVs = mesh->HasTextureCoords(0);
		bool hasTangents = mesh->HasTangentsAndBitangents();
		bool hasColors = mesh->HasVertexColors(0);

		for (u32 i = 0; i < mesh->mNumVertices; i++) {
			Vertex v;
			const aiVector3D pos = hasPositions ? mesh->mVertices[i] : aiZeroVector;
			const aiVector3D normal = hasNormals ? mesh->mNormals[i] : aiZeroVector;
			const aiVector3D texCoord = hasUVs ? mesh->mTextureCoords[0][i] : aiZeroVector;
			const aiVector3D tangent = hasTangents ? mesh->mTangents[i] : aiZeroVector;
			const aiColor4D color = hasColors ? mesh->mColors[0][i] : aiOneVector4;

			v.position = Vec3(pos.x, pos.y, pos.z);
			v.normal = Vec3(normal.x, normal.y, normal.z);
			v.texCoord = Vec2(texCoord.x, texCoord.y);
			v.tangent = Vec3(tangent.x, tangent.y, tangent.z);
			v.color = Vec4(color.r, color.g, color.b, color.a);

			addVertex(v);
		}

		for (u32 i = 0; i < mesh->mNumFaces; i++) {
			aiFace face = mesh->mFaces[i];
			for (u32 j = 0; j < face.mNumIndices; j++) {
				addIndex(off + face.mIndices[j]);
			}
		}

		off += m_vertexData.size();
	}
}

NS_END
