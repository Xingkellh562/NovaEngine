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

	//遗留版本，几乎不使用
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

	MeshData MeshBuilder::BuildMeshFromChunk(
		const voxel::chunk::Chunk& chunk, 
		const std::array<voxel::chunk::Chunk*, 6> neighbours,
		const voxel::block::BlockLibrary& library) 
	{
		const int CHUNK_LENGTH = 32768;
		const int SIZE = 32;
		const int MASK = 31;
		//制表(仅在第一次执行函数时调用一次)
		static int16_t neighborTable[CHUNK_LENGTH][6];
		static std::once_flag flag;
		std::call_once(flag, [] {
			const int AREA = SIZE * SIZE;

			//遍历整个体素
			int x = 0, y = 0, z = 0;
			for (int i = 0;i < AREA * SIZE;i++) {
				//位运算更新xyz值
				z = i & 31;
				y = (i >> 5) & 31;
				x = (i >> 10) & 31;

				for (int j = 0;j < 6;j++) {
					int nx = neighbours_[j][0] + x;
					int ny = neighbours_[j][1] + y;
					int nz = neighbours_[j][2] + z;

					if (nx & ~MASK || ny & ~MASK || nz & ~MASK) {
						int fnx = nx & MASK, fny = ny & MASK, fnz = nz & MASK;
						int16_t fni = (fnx << 10) + (fny << 5) + fnz;
						neighborTable[i][j] = fni | (1 << 15);
					}
					else {
						int16_t ni = (nx << 10) + (ny << 5) + nz;
						neighborTable[i][j] = ni;
					}
				}
			}
	    });

		//准备收集数据的容器
		std::vector<float> vertices;
		std::vector<int> indices;

		constexpr int MAX_VERTS_FLOATS = 32 * 32 * 32 * 6 * 4 * 6; // 6个float/顶点
		constexpr int MAX_INDICES = 32 * 32 * 32 * 6 * 6; // 最多6个面，每个面6个索引
		vertices.reserve(MAX_VERTS_FLOATS);
		indices.reserve(MAX_INDICES);

		int vertexCount = 0;

		//预加载方块模型，暂时硬编码方块数量，后续会完善方块数量查询入口
		int blockCount = 16;
		std::vector<const voxel::block::BlockMesh*> blockMeshs;
		for (int idx = 0;idx < blockCount; idx++) {
			auto block = library.FindBlock(idx);
			if (block)
				blockMeshs.push_back(&block->GetMesh());
			else blockMeshs.push_back(nullptr);
		}

		//遍历区块
		try {
			const std::vector<int>& voxels = chunk.GetVoxelSpan();
			std::array<const std::vector<int>*, 6> nei{};
			for (int n = 0;n < 6;n++) {
				if (neighbours[n])
					nei[n] = &neighbours[n]->GetVoxelSpan();
				else nei[n] = nullptr;
			}

			int x = 0, y = 0, z = 0;
			for (size_t i = 0; i < CHUNK_LENGTH; i++)
			{
				//通过纯位运算更新xyz的值
				z = i & 31;
				y = (i >> 5) & MASK;
				x = (i >> 10) & MASK;

				//获取方块id与mesh
				int blockId = voxels[i];
				if (blockId == 0) continue;

				const voxel::block::BlockMesh* blockMesh = blockMeshs[blockId];
				if (!blockMesh) continue;

				for (int j = 0;j < blockMesh->faces.size();j++) {
					//获取每个面的方向
					int ori = blockMesh->faces[j].orientation;
					//如果是内部面: 面方向枚举 == None == 6则不进行遮挡面剔除，直接构建
					if (ori == 6)
						AddQuadMesh(blockMesh->faces[j], vertices, indices, vertexCount, x, y, z);
					else { //如果不是(面含有方向属性->进入遮挡面剔除流程)
						int16_t nIdx = neighborTable[i][ori];
						if (nIdx < 0) {
							if (nei[ori] && !(*nei[ori])[nIdx & 0x7FFF]) {
								AddQuadMesh(blockMesh->faces[j], vertices, indices, vertexCount, x, y, z);
							}
							else if (!nei[ori]) {
								AddQuadMesh(blockMesh->faces[j], vertices, indices, vertexCount, x, y, z);
							}
						}
						else if (!voxels[nIdx]) {
							AddQuadMesh(blockMesh->faces[j], vertices, indices, vertexCount, x, y, z);
						}
					}
				}
			}
		}
		catch (std::exception& e) {
			NE_CORE_ERROR("MeshBuilder", "Failed to build mesh : '{}'", e.what());
			return MeshData(std::move(vertices), std::move(indices));
		}

		return MeshData(std::move(vertices), std::move(indices));
	}
}