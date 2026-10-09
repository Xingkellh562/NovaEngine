#pragma once

#include <render/opengl/context/render_context.h>
#include <core/world/world.h>
#include <voxel/terrain/terrain_generator.h>
#include <core/concurrency/worker.h>
#include <unordered_set>

namespace nova_engine::render::system {
	struct SubdivideResult {
		int x = 0, y = 0, z = 0;
		int depth = 0;

		std::array<MeshData, 8> datas;
	};

	struct CombineResult {
		int x = 0, y = 0, z = 0;
		int depth = 0;

		MeshData data;
	};

	class LodSystem {
	public:
		LodSystem() {
			subdivideWorker_.Start();
			combineWorker_.Start();
		}

		~LodSystem() {
			subdivideWorker_.Stop();
			combineWorker_.Stop();
		}

		void Update(opengl::context::RenderContext&,glm::vec3);

		size_t GetSubSubmitCount() const { return subdivideWorker_.GetSubmitTaskCount(); }
		size_t GetSubResultCount() const { return subdivideWorker_.GetResultCount(); }
		size_t GetCobSubmitCount() const { return combineWorker_.GetSubmitTaskCount(); }
		size_t GetCobResultCount() const { return combineWorker_.GetResultCount(); }
	private:
		core::concurrency::Worker<SubdivideResult> subdivideWorker_{"SubdivideWorker",2048,2048};
		core::concurrency::Worker<CombineResult> combineWorker_{ "CombineWorker",2048,2048 };

		std::unordered_set<glm::ivec3> subdivideExists_;
		std::unordered_set<glm::ivec3> combineExists_;

		void Subdivide(
			opengl::context::LodMeshCache&,
			const voxel::terrain::TerrainGenerator& generator, 
			int x, int y, int z, int depth, bool generatorMesh);

		void Combine(
			opengl::context::LodMeshCache&, 
			const voxel::terrain::TerrainGenerator& generator,
			int x, int y, int z, int depth);
	};
}