#pragma once

#include <voxel/terrain/planet_terrain_generator.h>
#include <render/scene/chunk_mesh_registry.h>
#include <render/scene/lod_mesh_registry.h>
#include <core/concurrency/worker.h>
#include <unordered_set>
#include <deque>

namespace nova_engine::render::system {
	struct SubdivideResult {
		int x = 0, y = 0, z = 0;
		int depth = 0;

		std::array<core::MeshData, 8> datas;
	};

	struct CombineResult {
		int x = 0, y = 0, z = 0;
		int depth = 0;

		core::MeshData data;
	};

	class ChunkLodSystem {
	public:
		ChunkLodSystem(int workerCount = 3) {
			//一个工作线程消费不过来那就给我开三个啊啊啊
			for (int i = 0;i < workerCount;i++) {
				subdivideWorkers_.emplace_back(
					std::string("SubdivideWorker") + std::to_string(i), 2048, 2048 );

				subdivideWorkers_.back().Start();

				combineWorkers_.emplace_back(
					std::string("CombineWorker") + std::to_string(i),2048,2048 );

				combineWorkers_.back().Start();
			}
		}

		~ChunkLodSystem() {
			for (int i = 0;i < subdivideWorkers_.size();i++) {
				subdivideWorkers_[i].Stop();
				combineWorkers_[i].Stop();
			}
		}

		void UpdateLod(
			glm::vec3 playerTruePos,
			const scene::ChunkMeshRegistry& chunkMeshRegistry,
			scene::LodMeshRegistry& lodMeshRegistry,
			const voxel::terrain::PlanetTerrainGenerator& generator);
	private:
		std::deque<nova_engine::core::concurrency::Worker<SubdivideResult>> subdivideWorkers_;
		std::deque<nova_engine::core::concurrency::Worker<CombineResult>> combineWorkers_;

		std::unordered_set<glm::ivec3> subdivideExists_;
		std::unordered_set<glm::ivec3> combineExists_;

		void SubmitSubdivide(
			scene::LodMeshRegistry& lodMeshRegistry,
			const voxel::terrain::PlanetTerrainGenerator& generator,
			int x, int y, int z, int depth, bool generatorMesh);

		void SubmitCombine(
			scene::LodMeshRegistry& lodMeshRegistry,
			const voxel::terrain::PlanetTerrainGenerator& generator,
			int x, int y, int z, int depth);
	};
}