#include "render_system.h"
#include <render/opengl/context/chunk_mesh_cache.h>
#include <render/utils/mesh_builder.h>
#include <render/opengl/library/camera_library.h>
#include <render/opengl/library/frame_buffer_library.h>
#include <utils/spatial/chunk_range_finder.h>
#include <glm/gtx/norm.hpp>
#include <log/logger.h>
#include <voxel/terrain/terrain_generator.h>

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


		
		//---------------------------------平行光阴影渲染器--------------------------------------
		
		if(opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("ShadowFrame"))
			opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("ShadowFrame")->Bind();

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LESS);

		if (renderContext.useShadow) {
			for (const opengl::GLMesh& mesh : meshs) {
				if (mesh.Awake()) {
					renderContext.parallelLightRenderer.Render(mesh);
				}
			}
		}

		//----------------------------------冯氏光照渲染器--------------------------------------
		if (opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("LightFrame"))
			opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("LightFrame")->Bind();
		GLfloat clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LESS);
		for (const opengl::GLMesh& mesh : meshs) {
			if (mesh.Awake()) {
				renderContext.lightRenderer.Render(mesh);
			}
		}
		if (renderContext.lodMesh) {
			for (const auto& lodMesh : renderContext.lodMeshCache.AllData()) {
				if (lodMesh.Awake()) {
					renderContext.lightRenderer.Render(lodMesh);
				}
			}
		}
		glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
		
		//----------------------------------方块纹理渲染器--------------------------------------
		if(opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("MainFrame"))
			opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("MainFrame")->Bind();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LESS);
		renderContext.blockRenderer.Render(emptyMesh);
		for (const opengl::GLMesh& mesh : meshs) {
			if (mesh.Awake()) {
				renderContext.blockRenderer.Render(mesh);
			}
		}

		if (renderContext.lodMesh) {
			for (const auto& lodMesh : renderContext.lodMeshCache.AllData()) {
				if (lodMesh.Awake()) {
					renderContext.blockRenderer.Render(lodMesh);
				}
			}
		}

		//----------------------------------WBOIT渲染器--------------------------------------
		if (opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("OITFrame"))
			opengl::FrameBufferLibrary::GetInstance().FindFrameBuffer("OITFrame")->Bind();

		const opengl::GLTextureArray* textures
			= opengl::TextureArrayLibrary::GetInstance().GetTextureArray("BlockTextures");
		if (textures != nullptr) {
			glBindTexture(GL_TEXTURE_2D_ARRAY, textures->get());
			glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		}
		
		// Accumulation 清 0
		const GLfloat accumDefault[]{ 0.0f, 0.0f, 0.0f, 0.0f };
		const GLfloat revealDefault[]{ 1.0f, 0.0f, 0.0f, 0.0f };
		glClearBufferfv(GL_COLOR, 0, accumDefault);
		// Revealage 清 1（乘性累积的初始值）
		glClearBufferfv(GL_COLOR, 1, revealDefault);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		renderContext.semiTranRenderer.Render(emptyMesh);
		if (renderContext.lucencyMesh) {
			//首先需要进行距离排序
			//准备临时容器
			std::vector<const opengl::GLMesh*> temMesh;
			//收集网格
			for (const opengl::GLMesh& mesh : meshs2) {
				if (mesh.Awake()) {
					temMesh.push_back(&mesh);
				}
			}
			//排序
			std::sort(temMesh.begin(), temMesh.end(),
				[](const opengl::GLMesh* a, const opengl::GLMesh* b) {
					if (a && b)
						return glm::length2(a->position) > glm::length2(b->position);
				});
			for (const opengl::GLMesh* mesh : temMesh)
			{
				if (mesh)
					renderContext.semiTranRenderer.Render(*mesh);
			}
		}
		
		if (textures != nullptr) {
			glBindTexture(GL_TEXTURE_2D_ARRAY, textures->get());
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
		}
		//----------------------------------后处理渲染器--------------------------------------
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		renderContext.postProcessRenderer.Render(emptyMesh);
	
	}

	static const std::vector<glm::ivec3> chunksPos = nova_engine::utils::spatial::FindChunkInRange(256);

	bool firstLod = true;
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
				if ((dis > 192 * 192 || !chunk)) {
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

					//多线程异步提交
					//SubmitMeshRequire(chunk,nei,cx,cy,cz);

					auto defaultMesh = utils::MeshBuilder::BuildMeshFromChunk(*chunk, nei, voxel::block::BlockLibrary::GetInstance());
					const MeshData& mesh = defaultMesh[0].value();
					const MeshData& mesh2 = defaultMesh[1].value();

					meshCache->SubmitMesh(cx, cy, cz, mesh);
					meshCache2->SubmitMesh(cx, cy, cz, mesh2);
					count++;
				}

				if (count >= MAXCOUNT) break;
			}


			glm::ivec3 out;
			while (meshCache->GetUpdateRequirement(out)) {
				int cx = out.x;
				int cy = out.y;
				int cz = out.z;

				auto chunk = world.chunkData.FindChunk(cx, cy, cz);
				const std::array<nova_engine::voxel::chunk::Chunk*, 6>& nei = world.chunkData.GetChunkNeighbours(cx, cy, cz);

				if (chunk)
					SubmitMeshRequire(chunk, nei, cx, cy, cz);
			}
		}

		

		core::concurrency::Result<MeshBuildResult> out;
		while (meshBuildWorker_.TryPopResult(out)) {
			const MeshData& mesh = out.value.value[0].value();
			const MeshData& mesh2 = out.value.value[1].value();
			int cx = out.value.x,cy =  out.value.y, cz = out.value.z;
			

			meshCache->SubmitMesh(cx, cy, cz, mesh);
			meshCache2->SubmitMesh(cx, cy, cz, mesh2);
			existTasks_.erase({ cx,cy,cz });
		}

		lodSystem_.Update(context, world.playerPos);
		
		
		opengl::Camera* camera = opengl::CameraLibrary::GetInstance().GetCamera("MainCamera");
		if (camera) {
			camera->position = world.playerPos;
			camera->rotation = world.playerRot;
		}

		opengl::Camera* shadowCamera = opengl::CameraLibrary::GetInstance().GetCamera("ShadowCamera");
		if (shadowCamera) {
			float orthoSize = 64.0f;
			int shadowResolution = 2048;
			float texelWorldPos = (2.0 * orthoSize) / shadowResolution;

			glm::vec3 lightDir = glm::normalize(glm::vec3(0.3, 0.8, 0.5));
			float distance = 128.0f;
			//glm::vec3 fixedPos = glm::floor(world.playerPos / 1.0f) * 1.0f + 0.5f;
			glm::vec3 lightPos = world.playerPos + lightDir * distance;

			glm::mat4 lightView = glm::lookAt(lightPos, world.playerPos,glm::vec3(0,1,0));
			glm::vec4 playerInLightSpace = lightView * glm::vec4(world.playerPos, 1.0);

			playerInLightSpace.x = glm::floor(playerInLightSpace.x / texelWorldPos) * texelWorldPos;
			playerInLightSpace.y = glm::floor(playerInLightSpace.y / texelWorldPos) * texelWorldPos;

			glm::mat4 invLightView = glm::inverse(lightView);
			glm::vec4 alignedPlayer = invLightView * playerInLightSpace;
			glm::vec3 alignedTarget = glm::vec3(alignedPlayer);

			shadowCamera->position = alignedTarget + lightDir * distance;
			shadowCamera->rotation = -lightDir;
		}
	}

	void RenderSystem::SubmitMeshRequire(voxel::chunk::Chunk* chunk, 
		const std::array<nova_engine::voxel::chunk::Chunk*, 6>& nei,
		int cx,int cy,int cz){
		if (existTasks_.find({ cx, cy, cz }) != existTasks_.end()) return;

		auto cancel = std::make_shared<core::concurrency::CancelToken>();
		existTasks_.insert({ cx, cy, cz });

		try {
			bool success = meshBuildWorker_.TrySubmitTask(core::concurrency::Task<MeshBuildResult>{
				.id = 0, .priority = 0, .cancel = cancel,
					.fn = [chunk, nei, cx, cy, cz](core::concurrency::TaskContext<MeshBuildResult>& ctx) mutable {
					MeshBuildResult result;
					result.x = cx;
					result.y = cy;
					result.z = cz;

					auto defaultMesh = utils::MeshBuilder::BuildMeshFromChunk(*chunk, nei, voxel::block::BlockLibrary::GetInstance());
					result.value = std::move(defaultMesh);
					ctx.emit(std::move(result));
					}
			});

			if (!success) existTasks_.erase({ cx,cy,cz });
		}
		catch (const std::exception& e) {
			NE_CORE_ERROR("RenderSystem", "Failed to submit chunk build task, catch error:{}", e.what());
		}
	}
}