#pragma once

#include <render/opengl/context/render_context.h>
#include <core/world/world.h>
#include <voxel/terrain/terrain_generator.h>

namespace nova_engine::render::system {
	class LodSystem {
	public:
		void Update(opengl::context::RenderContext&,glm::vec3);
	private:
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