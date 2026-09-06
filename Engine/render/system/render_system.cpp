#include "render_system.h"
#include <render/opengl/context/chunk_mesh_cache.h>
#include <render/utils/mesh_builder.h>
#include <render/opengl/library/camera_library.h>
#include <utils/spatial/chunk_range_finder.h>
#include <glm/gtx/norm.hpp>

namespace nova_engine::render::system {
	void RenderSystem::Render(opengl::context::RenderContext& renderContext) const
	{
		const std::vector<opengl::GLMesh>& meshs = renderContext.chunkMeshCache.GetAllMesh();
		for (const opengl::GLMesh& mesh : meshs) {
			if (mesh.GetElementLength()) {
				renderContext.blockRenderer.Render(mesh);
			}
		}
	}

	static const std::vector<glm::ivec3> chunksPos = nova_engine::utils::spatial::FindChunkInRange(320);

	void RenderSystem::Update(
		const nova_engine::core::world::World& world, 
		opengl::context::RenderContext& context)
	{

		glm::ivec3 playerPos = world.playerPos;

		auto& meshs = context.chunkMeshCache.GetAllMesh();
		for (const auto& mesh : meshs) {
			auto chunk = world.chunkData.FindChunk(mesh.position.x, mesh.position.y, mesh.position.z);
			int dis = glm::length2(mesh.position - (glm::vec3)playerPos);
			if ((dis > 272 * 272 || !chunk)) {
				context.chunkMeshCache.FreeMesh(mesh.position.x, mesh.position.y, mesh.position.z);
			}
		}
		const int MAXCOUNT = 6;
		int count = 0;
		for (const glm::ivec3& chunkPos : chunksPos) {
			int cx = chunkPos.x + (int)playerPos.x,
				cy = chunkPos.y + (int)playerPos.y,
				cz = chunkPos.z + (int)playerPos.z;
			auto chunk = world.chunkData.FindChunk(cx, cy, cz);
			bool exist = context.chunkMeshCache.Exist(cx, cy, cz);
			const glm::ivec3& cp = chunkPos;
			//((cp.x * cp.x + cp.y * cp.y + cp.z * cp.z) < 96 * 96) &&
			if (chunk && !exist) {
				const std::array<nova_engine::voxel::chunk::Chunk* ,6>& nei = world.chunkData.GetChunkNeighbours(cx, cy, cz);
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

				const MeshData& mesh = 
					utils::MeshBuilder::BuildMeshFromChunk(*chunk, nei,voxel::block::BlockLibrary::GetInstance());
				context.chunkMeshCache.SubmitMesh(cx, cy, cz, mesh);
				count++;
			}
			/*if((((cp.x * cp.x + cp.y * cp.y + cp.z * cp.z) >= 96 * 96) || !chunk) && exist){
				context.chunkMeshCache.FreeMesh(cx, cy, cz);
			}*/

			if (count >= MAXCOUNT) break;
			
		}

		
		
		opengl::Camera* camera = opengl::CameraLibrary::GetInstance().GetCamera("MainCamera");
		if (camera) {
			camera->position = world.playerPos;
			camera->rotation = world.playerRot;
		}
	}
}