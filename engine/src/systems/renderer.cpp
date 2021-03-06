#include "renderer.h"

NS_BEGIN

Mat4 Camera::getProjection() {
	if (type == CameraType::Orthographic) {
		return Mat4::ortho(-orthoScale, orthoScale, -orthoScale, orthoScale, zNear, zFar);
	}
	
	int vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);
	
	return Mat4::perspective(FOV, float(vp[2]) / float(vp[3]), zNear, zFar);
}

const String RendererSystem::POST_FX_VS = 
#include "../shaders/lightingV.glsl"
;

RendererSystem::RendererSystem() {
	m_activeCamera = nullptr;
	m_activeCameraTransform = nullptr;
	m_IBLGenerated = false;
	
	int vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);
	
	m_gbuffer = Builder<FrameBuffer>::build()
			.setSize(vp[2], vp[3])
			.addRenderBuffer(TextureFormat::Depth, Attachment::DepthAttachment)
			.addColorAttachment(TextureFormat::RGBf) // Normals
			.addColorAttachment(TextureFormat::RGB) // Albedo
			.addColorAttachment(TextureFormat::RGBf) // RME
			.addDepthAttachment();
	
	m_finalBuffer = Builder<FrameBuffer>::build()
			.setSize(vp[2], vp[3])
			.addColorAttachment(TextureFormat::RGBf)
			.addDepthAttachment();
	
	m_pingPongBuffer = Builder<FrameBuffer>::build()
			.setSize(vp[2], vp[3])
			.addColorAttachment(TextureFormat::RGBf)
			.addColorAttachment(TextureFormat::RGBf);

	m_captureBuffer = Builder<FrameBuffer>::build()
			.setSize(128, 128)
			.addRenderBuffer(TextureFormat::Depth, Attachment::DepthAttachment);

	m_pickingBuffer = Builder<FrameBuffer>::build()
			.setSize(vp[2], vp[3])
			.addColorAttachment(TextureFormat::RGB);
	
	String common =
#include "../shaders/common.glsl"
			;
	String brdf =
#include "../shaders/brdf.glsl"
			;
	
	String gVS =
#include "../shaders/gbufferV.glsl"
			;
	String gFS =
#include "../shaders/gbufferF.glsl"
			;
	
	gVS = Util::replace(gVS, "#include common", common);
	gFS = Util::replace(gFS, "#include common", common);
	gFS = Util::replace(gFS, "#include brdf", brdf);
	
	m_gbufferShader = Builder<ShaderProgram>::build()
			.add(gVS, ShaderType::VertexShader)
			.add(gFS, ShaderType::FragmentShader);
	m_gbufferShader.link();
	
	String lVS =
#include "../shaders/lightingV.glsl"
			;
	String lFS =
#include "../shaders/lightingF.glsl"
			;
	
	lVS = Util::replace(lVS, "#include common", common);
	lFS = Util::replace(lFS, "#include common", common);
	lFS = Util::replace(lFS, "#include brdf", brdf);
	
	m_lightingShader = Builder<ShaderProgram>::build()
			.add(lVS, ShaderType::VertexShader)
			.add(lFS, ShaderType::FragmentShader);
	m_lightingShader.link();
	
	String fVS =
#include "../shaders/lightingV.glsl"
			;
	String fFS =
#include "../shaders/screenF.glsl"
			;
	
	fVS = Util::replace(fVS, "#include common", common);
	fFS = Util::replace(fFS, "#include common", common);
	fFS = Util::replace(fFS, "#include brdf", brdf);
	
	m_finalShader = Builder<ShaderProgram>::build()
			.add(fVS, ShaderType::VertexShader)
			.add(fFS, ShaderType::FragmentShader);
	m_finalShader.link();
	
	String cmVS = 
#include "../shaders/cmV.glsl"
			;
	String cmFS = 
#include "../shaders/cmF.glsl"
			;
	m_cubeMapShader = Builder<ShaderProgram>::build()
			.add(cmVS, ShaderType::VertexShader)
			.add(cmFS, ShaderType::FragmentShader);
	m_cubeMapShader.link();
	
	String cmiVS = 
#include "../shaders/cmIrradianceV.glsl"
			;
	String cmiFS = 
#include "../shaders/cmIrradianceF.glsl"
			;
	m_irradianceShader = Builder<ShaderProgram>::build()
			.add(cmiVS, ShaderType::VertexShader)
			.add(cmiFS, ShaderType::FragmentShader);
	m_irradianceShader.link();
	
	String cmfVS = 
#include "../shaders/cmIrradianceV.glsl"
			;
	String cmfFS = 
#include "../shaders/cmPreFilterF.glsl"
			;
	m_preFilterShader = Builder<ShaderProgram>::build()
			.add(cmfVS, ShaderType::VertexShader)
			.add(cmfFS, ShaderType::FragmentShader);
	m_preFilterShader.link();
	
	String brdfFS = 
#include "../shaders/brdfLUTF.glsl"
			;
	
	m_brdfLUTShader = Builder<ShaderProgram>::build()
			.add(fVS, ShaderType::VertexShader)
			.add(brdfFS, ShaderType::FragmentShader);
	m_brdfLUTShader.link();
	
	String pickFS = 
#include "../shaders/pickingF.glsl"
			;
	m_pickingShader = Builder<ShaderProgram>::build()
			.add(gVS, ShaderType::VertexShader)
			.add(pickFS, ShaderType::FragmentShader);
	m_pickingShader.link();
	
	m_plane = Builder<Mesh>::build();
	m_plane.addVertex(Vertex(Vec3(0, 0, 0)))
		.addVertex(Vertex(Vec3(1, 0, 0)))
		.addVertex(Vertex(Vec3(1, 1, 0)))
		.addVertex(Vertex(Vec3(0, 1, 0)))
		.addTriangle(0, 1, 2)
		.addTriangle(2, 3, 0)
		.flush();
	
	m_cube = Builder<Mesh>::build();
	m_cube.addPlane(Axis::X, 1.0f, Vec3(-1, 0, 0), true);
	m_cube.addPlane(Axis::X, 1.0f, Vec3(1, 0, 0));
	m_cube.addPlane(Axis::Y, 1.0f, Vec3(0, -1, 0), true);
	m_cube.addPlane(Axis::Y, 1.0f, Vec3(0, 1, 0));
	m_cube.addPlane(Axis::Z, 1.0f, Vec3(0, 0, -1), true);
	m_cube.addPlane(Axis::Z, 1.0f, Vec3(0, 0, 1));
	m_cube.flush();
	
	m_screenMipSampler = Builder<Sampler>::build()
			.setFilter(TextureFilter::LinearMipLinear, TextureFilter::Linear)
			.setWrap(TextureWrap::ClampToEdge, TextureWrap::ClampToEdge);
	
	m_screenTextureSampler = Builder<Sampler>::build()
			.setFilter(TextureFilter::Linear, TextureFilter::Linear)
			.setWrap(TextureWrap::ClampToEdge, TextureWrap::ClampToEdge);
	
	m_screenDepthSampler = Builder<Sampler>::build()
			.setFilter(TextureFilter::Nearest, TextureFilter::Nearest)
			.setWrap(TextureWrap::ClampToEdge, TextureWrap::ClampToEdge);
	
	m_cubeMapSampler = Builder<Sampler>::build()
			.setFilter(TextureFilter::LinearMipLinear, TextureFilter::Linear)
			.setWrap(TextureWrap::ClampToEdge, TextureWrap::ClampToEdge, TextureWrap::ClampToEdge)
			.setSeamlessCubemap(true);
	
	m_cubeMapSamplerNoMip = Builder<Sampler>::build()
			.setFilter(TextureFilter::Linear, TextureFilter::Linear)
			.setWrap(TextureWrap::ClampToEdge, TextureWrap::ClampToEdge, TextureWrap::ClampToEdge)
			.setSeamlessCubemap(true);
	
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
}

void RendererSystem::update(float dt) {
	m_time += dt;
}

void RendererSystem::render(EntityWorld& world) {
	if (m_activeCamera == nullptr || m_activeCameraTransform == nullptr) {
		Entity* cameraEnt = world.find<Camera>();
		if (cameraEnt) {
			m_activeCamera = cameraEnt->get<Camera>();
			m_activeCameraTransform = cameraEnt->get<Transform>();
		}
	}

	Mat4 projMat = Mat4::ident();
	if (m_activeCamera) {
		projMat = m_activeCamera->getProjection();
	}
	
	Mat4 viewMat = Mat4::ident();
	if (m_activeCameraTransform) {
		Mat4 rot = m_activeCameraTransform->worldRotation().conjugated().toMat4();
		Mat4 loc = Mat4::translation(m_activeCameraTransform->worldPosition() * -1.0f);
		viewMat = loc * rot;
	}
	
	if (!m_IBLGenerated && m_envMap.id() != 0) {
		computeIBL();
		m_IBLGenerated = true;
	}
	
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	
	/// Fill picking buffer
	m_pickingBuffer.bind();
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	m_pickingShader.bind();
	m_pickingShader.get("mProjection").set(projMat);
	m_pickingShader.get("mView").set(viewMat);
	
	// Render everything that has a transform
	m_cube.bind();
	world.each([&](Entity& ent, Transform& T) {
		Mat4 modelMat = T.localToWorldMatrix();
		
		// Scale the cube to the same size of the object
		Vec3 scale = Vec3(1.0f);
		if (ent.has<Drawable3D>()) {
			Drawable3D* drw = ent.get<Drawable3D>();
			scale = (drw->mesh.aabb().max() - drw->mesh.aabb().min()) * 0.5f;
		} else {
			scale = Vec3(0.1f);
		}
		modelMat = Mat4::scaling(scale) * modelMat;
		
		m_pickingShader.get("mModel").set(modelMat);
		m_pickingShader.get("uEID").set(ent.id());
		glDrawElements(GL_TRIANGLES, m_cube.indexCount(), GL_UNSIGNED_INT, nullptr);
	});
	m_pickingShader.unbind();
	m_cube.unbind();
	m_pickingBuffer.unbind();
	
	/// Fill GBuffer
	m_gbuffer.bind();
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	m_gbufferShader.bind();
	m_gbufferShader.get("mProjection").set(projMat);
	m_gbufferShader.get("mView").set(viewMat);
	
	if (m_activeCameraTransform) {
		m_gbufferShader.get("uEye").set(m_activeCameraTransform->worldPosition());
	}
	
	world.each([&](Entity& ent, Transform& T, Drawable3D& D) {
		Mat4 modelMat = T.localToWorldMatrix();
		m_gbufferShader.get("mModel").set(modelMat);
		
		m_gbufferShader.get("material.roughness").set(D.material.roughness);
		m_gbufferShader.get("material.metallic").set(D.material.metallic);
		m_gbufferShader.get("material.emission").set(D.material.emission);
		m_gbufferShader.get("material.baseColor").set(D.material.baseColor);
		m_gbufferShader.get("material.heightScale").set(D.material.heightScale);
		m_gbufferShader.get("material.discardEdges").set(D.material.discardParallaxEdges);
		
		int sloti = 0;
		for (TextureSlot slot : D.material.textures) {
			if (!slot.enabled|| slot.texture.id() == 0) continue;
			
			String tname = "";
			switch (slot.type) {
				case TextureSlotType::NormalMap: tname = "tNormalMap"; break;
				case TextureSlotType::RougnessMetallicEmission: tname = "tRMEMap"; break;
				case TextureSlotType::Albedo0: tname = "tAlbedo0"; break;
				case TextureSlotType::Albedo1: tname = "tAlbedo1"; break;
				case TextureSlotType::HeightMap: tname = "tHeightMap"; break;
				default: break;
			}
			
			if (!tname.empty()) {
				slot.texture.bind(slot.sampler, sloti);
				m_gbufferShader.get(tname + String(".img")).set(sloti);
				m_gbufferShader.get(tname + String(".opt.enabled")).set(true);
				m_gbufferShader.get(tname + String(".opt.uv_transform")).set(slot.uvTransform);
				sloti++;
			}
		}
		
		D.mesh.bind();
		glDrawElements(GL_TRIANGLES, D.mesh.indexCount(), GL_UNSIGNED_INT, nullptr);
	});
	
	m_gbufferShader.unbind();
	m_gbuffer.unbind();
	
	glDisable(GL_DEPTH_TEST);
	
	// Lights
	m_finalBuffer.bind();
	
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	m_lightingShader.bind();
	m_lightingShader.get("mProjection").set(projMat);
	m_lightingShader.get("mView").set(viewMat);
	
	m_gbuffer.getColorAttachment(0).bind(m_screenTextureSampler, 0); // Normals
	m_gbuffer.getColorAttachment(1).bind(m_screenTextureSampler, 1); // Albedo	
	m_gbuffer.getColorAttachment(2).bind(m_screenTextureSampler, 2); // RME
	//m_gbuffer.getDepthAttachment().bind(m_screenDepthSampler, 3); // Depth
	
	m_lightingShader.get("tNormals").set(0);
	m_lightingShader.get("tAlbedo").set(1);
	m_lightingShader.get("tRME").set(2);
	//m_lightingShader.get("tDepth").set(3);
	
	if (m_activeCameraTransform) {
		m_lightingShader.get("uEye").set(m_activeCameraTransform->worldPosition());
	}
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	
	m_plane.bind();
	
	m_lightingShader.get("uEmit").set(false);
	m_lightingShader.get("uIBL").set(false);
	
	// IBL
	if (m_IBLGenerated) {
//		m_brdf.bind(m_screenTextureSampler, 4);
		m_irradiance.bind(m_cubeMapSamplerNoMip, 4);
		m_radiance.bind(m_cubeMapSampler, 5);
		m_lightingShader.get("uIBL").set(true);
//		m_lightingShader.get("tBRDFLUT").set(4);
		m_lightingShader.get("tIrradiance").set(4);
		m_lightingShader.get("tRadiance").set(5);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
		m_lightingShader.get("uIBL").set(false);
	}
	
	// Emit
	m_lightingShader.get("uEmit").set(true);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
	m_lightingShader.get("uEmit").set(false);
	
	// Directional Lights
	world.each([&](Entity& ent, Transform& T, DirectionalLight& L) {
		m_lightingShader.get("uLight.type").set(L.getType());
		m_lightingShader.get("uLight.color").set(L.color);
		m_lightingShader.get("uLight.intensity").set(L.intensity);
		
		m_lightingShader.get("uLight.direction").set(T.forward());
		
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
	});
	
	// Point Lights
	world.each([&](Entity& ent, Transform& T, PointLight& L) {
		m_lightingShader.get("uLight.type").set(L.getType());
		m_lightingShader.get("uLight.color").set(L.color);
		m_lightingShader.get("uLight.intensity").set(L.intensity);
		
		m_lightingShader.get("uLight.position").set(T.worldPosition());
		m_lightingShader.get("uLight.radius").set(L.radius);
		m_lightingShader.get("uLight.lightCutoff").set(L.lightCutOff);
		
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
	});
	
	// Spot Lights
	world.each([&](Entity& ent, Transform& T, SpotLight& L) {
		m_lightingShader.get("uLight.type").set(L.getType());
		m_lightingShader.get("uLight.color").set(L.color);
		m_lightingShader.get("uLight.intensity").set(L.intensity);
		
		m_lightingShader.get("uLight.position").set(T.worldPosition());
		m_lightingShader.get("uLight.direction").set(T.forward());
		m_lightingShader.get("uLight.radius").set(L.radius);
		m_lightingShader.get("uLight.lightCutoff").set(L.lightCutOff);
		m_lightingShader.get("uLight.spotCutoff").set(L.spotCutOff);
		
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
	});
	
	m_plane.unbind();
	m_lightingShader.unbind();
	
	glDisable(GL_BLEND);

	if (m_envMap.id() != 0) {	
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDepthFunc(GL_LEQUAL);
		glClear(GL_DEPTH_BUFFER_BIT);
		
		m_gbuffer.bind(FrameBufferTarget::ReadFramebuffer);
		m_finalBuffer.blit(
				0, 0, m_gbuffer.width(), m_gbuffer.height(),
				0, 0, m_gbuffer.width(), m_gbuffer.height(),
				ClearBufferMask::DepthBuffer,
				TextureFilter::Nearest
		);
		
		m_cube.bind();
		m_cubeMapShader.bind();
		m_cubeMapShader.get("mProjection").set(projMat);
		m_cubeMapShader.get("mView").set(viewMat);
		
		m_envMap.bind(m_cubeMapSampler, 0);
		m_cubeMapShader.get("tCubeMap").set(0);
		
		glDrawElements(GL_TRIANGLES, m_cube.indexCount(), GL_UNSIGNED_INT, nullptr);
		
		m_gbuffer.unbind();
		m_envMap.unbind();
		m_cube.unbind();
		m_cubeMapShader.unbind();
		
		glDepthFunc(GL_LESS);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
	}
	
	m_finalBuffer.unbind();
	
	m_finalBuffer.getColorAttachment(0).bind(m_screenMipSampler, 0);
	m_finalBuffer.getColorAttachment(0).generateMipmaps();
	m_finalBuffer.getColorAttachment(0).unbind();
	
	/// Final render
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	
	m_plane.bind();
	
	/// Post processing
	if (m_postEffects.empty()) {
		m_finalShader.bind();

		m_finalBuffer.getColorAttachment(0).bind(m_screenMipSampler, 0);
		m_finalShader.get("tTex").set(0);

		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

		m_finalShader.unbind();
	} else {
		int currActive = 0;
		bool first = true;
		
		m_pingPongBuffer.bind();
		for (ShaderProgram& shd : m_postEffects) {
			int src = currActive;
			int dest = 1 - currActive;
			
			m_pingPongBuffer.setDrawBuffer(dest);
			
			shd.bind();
			
			if (first) {
				m_finalBuffer.getColorAttachment(0).bind(m_screenMipSampler, 0);
			} else {
				m_pingPongBuffer.getColorAttachment(src).generateMipmaps();
				m_pingPongBuffer.getColorAttachment(src).bind(m_screenMipSampler, 0);
			}
			
			shd.get("tScreen").set(0);
			shd.get("tTime").set(m_time);
			shd.get("tResolution").set(Vec2(m_finalBuffer.width(), m_finalBuffer.height()));
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
			
			if (first) {
				m_finalBuffer.getColorAttachment(0).unbind();
				first = false;
			} else {
				m_pingPongBuffer.getColorAttachment(src).unbind();
			}
			
			shd.unbind();
			
			currActive = 1 - currActive;
		}
		m_pingPongBuffer.unbind();
		
		m_finalShader.bind();

		m_pingPongBuffer.getColorAttachment(currActive).bind(m_screenTextureSampler, 0);
		m_finalShader.get("tTex").set(0);

		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

		m_finalShader.unbind();
	}
	
	m_plane.unbind();
	
	m_gbuffer.bind(FrameBufferTarget::ReadFramebuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(
			0, 0, m_gbuffer.width(), m_gbuffer.height(),
			0, 0, m_gbuffer.width(), m_gbuffer.height(),
			ClearBufferMask::DepthBuffer,
			TextureFilter::Nearest
	);
	m_gbuffer.unbind();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RendererSystem::computeIrradiance() {
	if (m_envMap.id() == 0) return;
	
	const Mat4 capProj = Mat4::perspective(radians(45.0f), 1.0f, 0.1f, 10.0f);
	const Mat4 capViews[6] = {
		Mat4::lookAt(Vec3(0, 0, 0), Vec3(1, 0, 0), Vec3(0, -1, 0)),
		Mat4::lookAt(Vec3(0, 0, 0), Vec3(-1, 0, 0), Vec3(0, -1, 0)),
		Mat4::lookAt(Vec3(0, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1)),
		Mat4::lookAt(Vec3(0, 0, 0), Vec3(0, -1, 0), Vec3(0, 0, -1)),
		Mat4::lookAt(Vec3(0, 0, 0), Vec3(0, 0, 1), Vec3(0, -1, 0)),
		Mat4::lookAt(Vec3(0, 0, 0), Vec3(0, 0, -1), Vec3(0, -1, 0))
	};
	
	if (m_irradiance.id() != 0) {
		Builder<Texture>::dispose(m_irradiance);
	}
	m_irradiance = Builder<Texture>::build()
			.bind(TextureTarget::CubeMap)
			.setCubemapNull(32, 32, TextureFormat::RGBf);
	
	m_irradianceShader.bind();
	m_irradianceShader.get("mProjection").set(capProj);
	
	m_envMap.bind(m_cubeMapSampler, 0);
	m_irradianceShader.get("tCubeMap").set(0);
	
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	
	m_captureBuffer.bind();
	glViewport(0, 0, 32, 32);
	m_cube.bind();
	for (u32 i = 0; i < 6; i++) {
		m_irradianceShader.get("mView").set(capViews[i]);
		m_captureBuffer.setColorAttachment(0, (TextureTarget)(TextureTarget::CubeMapPX+i), m_irradiance);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDrawElements(GL_TRIANGLES, m_cube.indexCount(), GL_UNSIGNED_INT, nullptr);
	}	
	m_cube.unbind();
	m_captureBuffer.unbind();
	
	m_irradianceShader.unbind();
	
	glEnable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
}

void RendererSystem::computeRadiance() {
	if (m_envMap.id() == 0) return;
	
	const Mat4 capProj = Mat4::perspective(radians(45.0f), 1.0f, 0.1f, 10.0f);
	const Mat4 capViews[6] = {
		Mat4::lookAt(Vec3(0, 0, 0), Vec3(1, 0, 0), Vec3(0, -1, 0)),
		Mat4::lookAt(Vec3(0, 0, 0), Vec3(-1, 0, 0), Vec3(0, -1, 0)),
		Mat4::lookAt(Vec3(0, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1)),
		Mat4::lookAt(Vec3(0, 0, 0), Vec3(0, -1, 0), Vec3(0, 0, -1)),
		Mat4::lookAt(Vec3(0, 0, 0), Vec3(0, 0, 1), Vec3(0, -1, 0)),
		Mat4::lookAt(Vec3(0, 0, 0), Vec3(0, 0, -1), Vec3(0, -1, 0))
	};
	
	const u32 maxMip = 8;
	
	if (m_radiance.id() != 0) {
		Builder<Texture>::dispose(m_radiance);
	}
	m_radiance = Builder<Texture>::build()
			.bind(TextureTarget::CubeMap)
			.setCubemapNull(128, 128, TextureFormat::RGBf)
			.generateMipmaps();
	
	m_preFilterShader.bind();
	m_preFilterShader.get("mProjection").set(capProj);
	
	m_envMap.bind(m_cubeMapSampler, 0);
	m_preFilterShader.get("tCubeMap").set(0);
	
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	
	m_captureBuffer.bind();
	m_cube.bind();
	for (u32 i = 0; i < maxMip; i++) {
		
		u32 mipWidth = 128 * std::pow(0.5, i);
		u32 mipHeight = 128 * std::pow(0.5, i);
		
		m_captureBuffer.setRenderBufferStorage(TextureFormat::Depth, mipWidth, mipHeight);
		glViewport(0, 0, mipWidth, mipHeight);

		float roughness = float(i) / float(maxMip - 1);
		m_preFilterShader.get("uRoughness").set(roughness);

		for (u32 j = 0; j < 6; j++) {
			m_preFilterShader.get("mView").set(capViews[j]);
			m_captureBuffer.setColorAttachment(0, (TextureTarget)(TextureTarget::CubeMapPX+j), m_radiance, i);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glDrawElements(GL_TRIANGLES, m_cube.indexCount(), GL_UNSIGNED_INT, nullptr);
		}
	}
	m_cube.unbind();
	m_captureBuffer.unbind();
	
	m_preFilterShader.unbind();
	
	glEnable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
}

void RendererSystem::computeBRDF() {
	if (m_envMap.id() == 0) return;
	
	if (m_brdf.id() != 0) {
		Builder<Texture>::dispose(m_brdf);
	}
	m_brdf = Builder<Texture>::build()
			.bind(TextureTarget::Texture2D)
			.setNull(512, 512, TextureFormat::RGf);
	
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	
	m_captureBuffer.bind();
	m_captureBuffer.setRenderBufferStorage(TextureFormat::Depth, 512, 512);
	glViewport(0, 0, 512, 512);
	
	m_plane.bind();
	m_brdfLUTShader.bind();
	
	m_captureBuffer.setColorAttachment(0, TextureTarget::Texture2D, m_brdf);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
	
	m_brdfLUTShader.unbind();
	m_captureBuffer.unbind();
	m_plane.unbind();
	
	glEnable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
}

void RendererSystem::computeIBL() {
	computeIrradiance();
	computeRadiance();
//	computeBRDF();
}

RendererSystem& RendererSystem::addPostEffect(ShaderProgram effect) {
	m_postEffects.push_back(effect);
	return *this;
}

RendererSystem& RendererSystem::removePostEffect(u32 index) {
	m_postEffects.erase(m_postEffects.begin() + index);
	return *this;
}

RendererSystem& RendererSystem::setEnvironmentMap(const Texture& tex) {
	if (tex.target() != TextureTarget::CubeMap) {
		LogError("Texture is not a Cube Map.");
		return *this;
	}
	m_envMap = tex;
	m_IBLGenerated = false;
	return *this;
}

NS_END