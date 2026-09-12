#include "render_system.h"
#include <render/opengl/context/chunk_mesh_cache.h>
#include <render/utils/mesh_builder.h>
#include <render/opengl/library/camera_library.h>
#include <utils/spatial/chunk_range_finder.h>
#include <glm/gtx/norm.hpp>
#include <log/logger.h>

namespace nova_engine::render::system {
	void RenderSystem::Render(opengl::context::RenderContext& renderContext) const
	{
		static opengl::GLMesh emptyMesh = {};
		opengl::context::ChunkMeshCache* meshCache = renderContext.chunkMesh.GetLayer(Default);
		opengl::context::ChunkMeshCache* meshCache2 = renderContext.chunkMesh.GetLayer(Lucency);
		if (!meshCache) return;

		const std::vector<opengl::GLMesh>& meshs = meshCache->GetAllMesh();
		const std::vector<opengl::GLMesh>& meshs2 = meshCache2->GetAllMesh();

		glBlendFunc(GL_ONE, GL_ZERO);
		glBlendEquation(GL_FUNC_ADD);
		glEnable(GL_BLEND);



		//----------------------------------基础渲染器--------------------------------------

		glBindFramebuffer(GL_FRAMEBUFFER, renderContext.parallelLightRenderer.GetFBO());
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LESS);
		for (const opengl::GLMesh& mesh : meshs) {
			if (mesh.GetElementLength()) {
				renderContext.parallelLightRenderer.Render(mesh);
			}
		}

		

		//----------------------------------方块纹理渲染器--------------------------------------

		GLuint mainFbo = renderContext.blockRenderer.GetFBO();
		glBindFramebuffer(GL_FRAMEBUFFER, mainFbo);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LESS);
		for (const opengl::GLMesh& mesh : meshs) {
			if (mesh.GetElementLength()) {
				renderContext.blockRenderer.Render(mesh);
			}
		}
		GLuint depthTex = renderContext.blockRenderer.GetDepthTex();
		if (depthTex)
			renderContext.semiTranRenderer.SetDepthTex(depthTex);


		GLenum err = glGetError();
		if (err != GL_NO_ERROR) {
			NE_CORE_DEBUG("RenderSystem", "GL error: {}", err);
		}

		//----------------------------------WBOIT渲染器--------------------------------------

		glBindFramebuffer(GL_FRAMEBUFFER, renderContext.semiTranRenderer.GetFBO());

		renderContext.semiTranRenderer.SetMainFbo(mainFbo);
		// Accumulation 清 0
		const GLfloat accumDefault[]{ 0.0f, 0.0f, 0.0f, 0.0f };
		const GLfloat revealDefault[]{ 1.0f, 0.0f, 0.0f, 0.0f };
		glClearBufferfv(GL_COLOR, 0, accumDefault);
		// Revealage 清 1（乘性累积的初始值）
		glClearBufferfv(GL_COLOR, 1, revealDefault);
		// 深度清空由主 FBO 处理
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		for (const opengl::GLMesh& mesh : meshs2) {
			if (mesh.GetElementLength()) {
				renderContext.semiTranRenderer.Render(mesh);
			}
		}

		/*glReadBuffer(GL_COLOR_ATTACHMENT0);
		float pixel[4];
		glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixel);

		NE_CORE_DEBUG("RenderSystem", "After clear, accum center : {},{},{},{}", pixel[0], pixel[1], pixel[2], pixel[3]);*/
		
		//----------------------------------后处理渲染器--------------------------------------
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		GLuint colorTex = renderContext.blockRenderer.GetColorTex();
		GLuint accumTex = renderContext.semiTranRenderer.GetAccumTex();
		GLuint revealTex = renderContext.semiTranRenderer.GetRevealTex();

		GLuint shadowMap = renderContext.parallelLightRenderer.GetDepthMap();
		if (colorTex && depthTex && shadowMap)
			renderContext.postProcessRenderer.SetColorTex(colorTex, depthTex, shadowMap);

		if (accumTex && revealTex)
			renderContext.postProcessRenderer.SetOitTex(accumTex, revealTex);

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		renderContext.postProcessRenderer.Render(emptyMesh);
	}

	static const std::vector<glm::ivec3> chunksPos = nova_engine::utils::spatial::FindChunkInRange(320);

	void RenderSystem::Update(
		const nova_engine::core::world::World& world, 
		opengl::context::RenderContext& context)
	{
		glm::ivec3 playerPos = world.playerPos;

		opengl::context::ChunkMeshCache* meshCache = context.chunkMesh.GetLayer(Default);
		opengl::context::ChunkMeshCache* meshCache2 = context.chunkMesh.GetLayer(Lucency);
		if (meshCache && meshCache2) {
			auto& meshs = meshCache->GetAllMesh();

			for (const auto& mesh : meshs) {
				auto chunk = world.chunkData.FindChunk(mesh.position.x, mesh.position.y, mesh.position.z);
				int dis = glm::length2(mesh.position - (glm::vec3)playerPos);
				if ((dis > 272 * 272 || !chunk)) {
					meshCache->FreeMesh(mesh.position.x, mesh.position.y, mesh.position.z);
					meshCache2->FreeMesh(mesh.position.x, mesh.position.y, mesh.position.z);
				}
			}
			const int MAXCOUNT = 6;
			int count = 0;
			for (const glm::ivec3& chunkPos : chunksPos) {
				int cx = chunkPos.x + (int)playerPos.x,
					cy = chunkPos.y + (int)playerPos.y,
					cz = chunkPos.z + (int)playerPos.z;
				auto chunk = world.chunkData.FindChunk(cx, cy, cz);
				bool exist = meshCache->Exist(cx, cy, cz);
				const glm::ivec3& cp = chunkPos;
				//((cp.x * cp.x + cp.y * cp.y + cp.z * cp.z) < 96 * 96) &&
				if (chunk && !exist) {
					const std::array<nova_engine::voxel::chunk::Chunk*, 6>& nei = world.chunkData.GetChunkNeighbours(cx, cy, cz);
					//只要有邻居不存在，不构建网格
					bool build = true;
					for (const nova_engine::voxel::chunk::Chunk* c : nei) {
						if (!c) {
							build = false;
							break;
						}
					}
					if (!build) continue;
					int i = 0;

					auto defaultMesh = utils::MeshBuilder::BuildMeshFromChunk(*chunk, nei, voxel::block::BlockLibrary::GetInstance());

					const MeshData& mesh = defaultMesh[0].value();
					const MeshData& mesh2 = defaultMesh[1].value();
						
					meshCache->SubmitMesh(cx, cy, cz, mesh);
					meshCache2->SubmitMesh(cx, cy, cz, mesh2);
					count++;
				}
				/*if((((cp.x * cp.x + cp.y * cp.y + cp.z * cp.z) >= 96 * 96) || !chunk) && exist){
					context.chunkMeshCache.FreeMesh(cx, cy, cz);
				}*/

				if (count >= MAXCOUNT) break;

			}

		}
		
		opengl::Camera* camera = opengl::CameraLibrary::GetInstance().GetCamera("MainCamera");
		if (camera) {
			camera->position = world.playerPos;
			camera->rotation = world.playerRot;
		}

		opengl::Camera* shadowCamera = opengl::CameraLibrary::GetInstance().GetCamera("ShadowCamera");
		if (shadowCamera) {
			glm::vec3 lightDir = glm::normalize(glm::vec3(0.3, 0.8, 0.5));
			float distance = 128.0f;
			glm::vec3 fixedPos = glm::floor(world.playerPos / 4.0f) * 4.0f;
			glm::vec3 lightPos = fixedPos + lightDir * distance;
			shadowCamera->position = lightPos;
			shadowCamera->rotation = fixedPos - lightPos; // 指向玩家的向量
		}
	}
}