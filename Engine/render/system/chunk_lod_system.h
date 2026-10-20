#pragma once

#include <voxel/terrain/planet_terrain_generator.h>
#include <render/scene/chunk_mesh_registry.h>
#include <render/scene/lod_mesh_registry.h>
#include <core/concurrency/worker.h>
#include <unordered_set>

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
		ChunkLodSystem() {
			subdivideWorker_.Start();
			combineWorker_.Start();
		}

		~ChunkLodSystem() {
			subdivideWorker_.Stop();
			combineWorker_.Stop();
		}

		void UpdateLod(
			glm::vec3 playerTruePos,
			const scene::ChunkMeshRegistry& chunkMeshRegistry,
			scene::LodMeshRegistry& lodMeshRegistry,
			const voxel::terrain::PlanetTerrainGenerator& generator);
	private:
		nova_engine::core::concurrency::Worker<SubdivideResult> subdivideWorker_{ "SubdivideWorker",2048,2048 };
		nova_engine::core::concurrency::Worker<CombineResult> combineWorker_{ "CombineWorker",2048,2048 };

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