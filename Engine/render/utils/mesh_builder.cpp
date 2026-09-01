#include "mesh_builder.h"
#include <log/logger.h>

namespace nova_engine::render::utils {
	const std::array<std::array<int, 3>, 6> MeshBuilder::neighbours_ = {
		{{-1,0,0},{1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}
	};

	void MeshBuilder::AddQuadMesh(const voxel::block::QuadFace& face,
										std::vector<float>& vertices,
										std::vector<int>& indices,
										int& vertexCount,
										int x,int y,int z) 
	{
		for (int i = 0;i < 6;i++) {
			indices.push_back(face.indices[i] + vertexCount);
		}

		for (int j = 0;j < 4;j++) {
			vertices.push_back(face.vertices[j][0] + x);
			vertices.push_back(face.vertices[j][1] + y);
			vertices.push_back(face.vertices[j][2] + z);
			vertices.push_back(face.uvs[j][0]);
			vertices.push_back(face.uvs[j][1]);
			vertices.push_back(face.uvs[j][2]);
		}
		vertexCount += 4;
	}

	MeshData MeshBuilder::BuildMeshFromChunk(const voxel::chunk::Chunk& chunk, const voxel::block::BlockLibrary& library) {
		//准备收集数据的容器
		std::vector<float> vertices;
		std::vector<int> indices;
		int vertexCount = 0;
		//遍历区块
		try {
			for (int x = 0; x < chunk.Size(); x++){
				for (int y = 0; y < chunk.Size(); y++){
					for (int z = 0; z < chunk.Size(); z++){
						//获取方块mesh
						int blockId = chunk.GetVoxel(x,y,z);
						if (blockId == 0) continue;
						auto block = library.FindBlock(blockId);
						if (block == nullptr) continue;
						voxel::block::BlockMesh blockMesh = block->GetMesh();
						//遮挡面剔除
						//遍历blockMesh中所有的面
						for (int i = 0;i < blockMesh.faces.size();i++) {
							int ori = blockMesh.faces[i].orientation;
							if (ori == 6) {
								AddQuadMesh(blockMesh.faces[i],vertices, indices, vertexCount,x, y, z);
							}
							else {
								int nx = neighbours_[ori][0] + x, 
									ny = neighbours_[ori][1] + y, 
									nz = neighbours_[ori][2] + z;

								if (nx < 0 || nx >= chunk.Size() ||
									ny < 0 || ny >= chunk.Size() ||
									nz < 0 || nz >= chunk.Size() ||
									chunk.GetVoxel(nx, ny, nz) == 0) {
									AddQuadMesh(blockMesh.faces[i], vertices, indices, vertexCount,x,y,z);
								}
							}
						}

					}
				}
			}
		}
		catch (std::exception& e) {
			NE_CORE_ERROR("MeshBuilder","Failed to build mesh : '{}'",e.what());
			return MeshData(std::move(vertices), std::move(indices));
		}

		return MeshData(std::move(vertices), std::move(indices));
	}
}