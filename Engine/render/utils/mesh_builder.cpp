#include "mesh_builder.h"
#include <log/logger.h>


namespace nova_engine::render::utils {
	const std::array<std::array<int, 3>, 6> MeshBuilder::neighbours_ = {
		{{-1,0,0},{1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}
	};

	const std::array<std::array<std::array<int, 3>, 12>, 6> MeshBuilder::aos_ = {{
		{{{-1,1,0},{-1,1,1},{-1,0,1},{-1,1,0},{-1,1,-1},{-1,0,-1},{-1,0,-1},{-1,-1,-1},{-1,-1,0},{-1,0,1},{-1,-1,1},{-1,-1,0}}},
		{{{1,0,-1},{1,-1,-1},{1,-1,0},{1,1,0},{1,1,-1},{1,0,-1},{1,1,0},{1,1,1},{1,0,1},{1,0,1},{1,-1,1},{1,-1,0}}},
		{{{1,1,1},{0,1,1},{1,1,0},{1,1,0},{1,1,-1},{0,1,-1},{0,1,-1},{-1,1,-1},{-1,1,0},{-1,1,0},{-1,1,1},{0,1,1}}},
		{{{-1,-1,0},{-1,-1,-1},{0,-1,-1},{0,-1,-1},{1,-1,-1},{1,-1,0},{1,-1,0},{1,-1,1},{0,-1,1},{0,-1,1},{-1,-1,1},{-1,-1,0}}},
		{{{-1,0,1},{-1,-1,1},{0,-1,1},{0,-1,1},{1,-1,1},{1,0,1},{1,0,1},{1,1,1},{0,1,1},{0,1,1},{-1,1,1},{-1,0,1}}},
		{{{0,1,-1},{1,1,-1},{1,0,-1},{1,0,-1},{1,-1,-1},{0,-1,-1},{0,-1,-1},{-1,-1,-1},{-1,0,-1},{-1,0,-1},{-1,1,-1},{0,1,-1}}}
	}};

	void MeshBuilder::AddQuadMesh(const voxel::block::QuadFace& face,
								std::vector<float>& vertices,
								std::vector<float>& uvws,
								std::vector<float>& aos,
								std::vector<float>& normals,
								std::vector<int>& indices,
								int& vertexCount,
								const std::array<float, 4>& aoArray,
								int x, int y, int z, int nx, int ny, int nz, int scale)
	{
		for (int i = 0;i < 6;i++) {
			indices.push_back(face.indices[i] + vertexCount);
		}

		for (int j = 0;j < 4;j++) {
			vertices.push_back((face.vertices[j][0] + x) * scale);
			vertices.push_back((face.vertices[j][1] + y) * scale);
			vertices.push_back((face.vertices[j][2] + z) * scale);
			uvws.push_back(face.uvs[j][0]);
			uvws.push_back(face.uvs[j][1]);
			uvws.push_back(face.uvs[j][2]);
			normals.push_back(nx);
			normals.push_back(ny);
			normals.push_back(nz);
			aos.push_back(aoArray[j]);
		}
		vertexCount += 4;
	}

	const std::array<float, 4>& MeshBuilder::GetAOValueForMesh(int idx, int dir) {
		const int CHUNK_LENGTH = 32768;
		const int MASK = 31;
	}

	std::array<std::optional<MeshData>,2> MeshBuilder::BuildMeshFromChunk(
		const voxel::chunk::Chunk& chunk, 
		const std::array<voxel::chunk::Chunk*, 6> neighbours,
		const voxel::block::BlockLibrary& library) 
	{
		const int CHUNK_LENGTH = 32768;
		const int SIZE = 32;
		const int MASK = 31;
		//制表(仅在第一次执行函数时调用一次)
		static int16_t neighborTable[CHUNK_LENGTH][6];
		static int aoTable[CHUNK_LENGTH][6][12];
		static std::once_flag flag;
		std::call_once(flag, [] {
			//static int aoTable[CHUNK_LENGTH][6][12];
			int x = 0, y = 0, z = 0;
			for (int i = 0;i < CHUNK_LENGTH;i++) {
				//位运算更新xyz值
				z = i & 31;
				y = (i >> 5) & 31;
				x = (i >> 10) & 31;

				for (int face = 0;face < 6;face++)
					for (int ver = 0;ver < 12;ver++) {
						int fx = x + aos_[face][ver][0];
						int fy = y + aos_[face][ver][1];
						int fz = z + aos_[face][ver][2];

						int mx = fx & ~MASK;
						int my = fy & ~MASK;
						int mz = fz & ~MASK;

						//有至少两个邻居越界:暂时占位
						if ((mx && my) ||
							(my && mz) ||
							(mx && mz))
						{
							//-1作为占位符
							aoTable[i][face][ver] = -1;
						}
						else if (mx || my || mz) { //只有一个邻居越界:计算邻居索引
							int ffx = fx & MASK, ffy = fy & MASK, ffz = fz & MASK;
							int ori = 0;
							if (mx) ori = (fx < 0 ? 0 : 1);
							else if (my) ori = (fy < 0 ? 3 : 2);
							else if (mz) ori = (fz < 0 ? 5 : 4);
						
							int fi = (1 << 31) + (ori << 15) + (ffx << 10) + (ffy << 5) + ffz;
							aoTable[i][face][ver] = fi;
						}
						else {//无邻居越界:直接计算索引
							int fi = (fx << 10) + (fy << 5) + fz;
							aoTable[i][face][ver] = fi;
						}

					}

			}

			const int AREA = SIZE * SIZE;
			//遍历整个体素
			x = 0, y = 0, z = 0;
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
		//暂时只有一份，加入透明网格后继续添加
		std::vector<float> vertices,lucencyVer;
		std::vector<float> uvws, lucencyUvws;
		std::vector<float> aos, lucencyAos;
		std::vector<float> normals, lucencyNormals;
		std::vector<int> indices , lucencyIdx;

		constexpr int MAX_VERTS_FLOATS = 32 * 32 * 32 * 6 * 4; // 6个float/顶点
		constexpr int MAX_INDICES = 32 * 32 * 32 * 6 * 6; // 最多6个面，每个面6个索引
		vertices.reserve(MAX_VERTS_FLOATS * 3);
		uvws.reserve(MAX_VERTS_FLOATS * 3);
		normals.reserve(MAX_VERTS_FLOATS * 3);
		aos.reserve(MAX_VERTS_FLOATS);
		indices.reserve(MAX_INDICES);
		

		int vertexCount = 0, lucencyVertexCount = 0;

		//预加载方块模型
		int blockCount = library.Size();
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
			std::array<float, 4> aoArray = { 1,1,1,1 };
			for (size_t i = 0; i < CHUNK_LENGTH; i++)
			{
				//通过纯位运算更新xyz的值
				z = i & 31;
				y = (i >> 5) & MASK;
				x = (i >> 10) & MASK;

				//获取方块id与mesh
				int blockId = voxels[i];
				if (blockId == 0) continue; //空气，直接跳过

				const voxel::block::BlockMesh* blockMesh = blockMeshs[blockId];
				if (!blockMesh) continue;

				for (int j = 0;j < blockMesh->faces.size();j++) {
					//获取每个面的方向
					int ori = blockMesh->faces[j].orientation;
					//如果是内部面: 面方向枚举 == None == 6则不进行遮挡面剔除，直接构建
					if (ori == 6) {
						AddQuadMesh(blockMesh->faces[j], vertices, uvws, aos,normals, indices, vertexCount, {1,1,1,1}, x, y, z,0,0,0);
					}
					else { //如果不是(面含有方向属性->进入遮挡面剔除流程)
						int16_t nIdx = neighborTable[i][ori];
						aoArray = { 1,1,1,1 };
						int nx = neighbours_[ori][0], ny = neighbours_[ori][1], nz = neighbours_[ori][2];
						switch (blockMesh->modelType)
						{
						case 0: //默认网格
							//计算ao值
							for (int a = 0; a < 4;a++) {
								int aoIdx0 = aoTable[i][ori][a * 3];
								int aoIdx1 = aoTable[i][ori][a * 3 + 1];
								int aoIdx2 = aoTable[i][ori][a * 3 + 2];

								bool blockAoId0{}, blockAoId1{}, blockAoId2{};

								blockAoId0 = aoIdx0 != -1 ? blockMeshs[(aoIdx0 < 0 && nei[(aoIdx0 >> 15) & 7] ?
									*nei[(aoIdx0 >> 15) & 7] : voxels)[aoIdx0 & 0x00007FFF]]->modelType == 0 : 0;
								blockAoId1 = aoIdx1 != -1 ? blockMeshs[(aoIdx1 < 0 && nei[(aoIdx1 >> 15) & 7] ?
									*nei[(aoIdx1 >> 15) & 7] : voxels)[aoIdx1 & 0x00007FFF]]->modelType == 0 : 0;
								blockAoId2 = aoIdx2 != -1 ? blockMeshs[(aoIdx2 < 0 && nei[(aoIdx2 >> 15) & 7] ?
									*nei[(aoIdx2 >> 15) & 7] : voxels)[aoIdx2 & 0x00007FFF]]->modelType == 0 : 0;

								aoArray[a] = 1 - (blockAoId0 + blockAoId1 + blockAoId2) * 0.2;
							}

							if (nIdx < 0) {
								if (nei[ori] && blockMeshs[(*nei[ori])[nIdx & 0x7FFF]]->modelType) {
									AddQuadMesh(blockMesh->faces[j], vertices, uvws, aos, normals, indices, vertexCount, aoArray, x, y, z, nx, ny, nz);
								}
								else if (!nei[ori]) {
									AddQuadMesh(blockMesh->faces[j], vertices, uvws, aos, normals, indices, vertexCount, aoArray, x, y, z, nx, ny, nz);
								}
							}
							else if (blockMeshs[voxels[nIdx]]->modelType) {
								AddQuadMesh(blockMesh->faces[j], vertices, uvws, aos, normals, indices, vertexCount, aoArray, x, y, z, nx, ny, nz);
							}
							break;
						case 1: //透明网格
							//这里将要写透明网格的面剔除逻辑
							if (nIdx < 0) {
								if (nei[ori] && (*nei[ori])[nIdx & 0x7FFF] != blockId) {
									AddQuadMesh(blockMesh->faces[j], lucencyVer, lucencyUvws, 
										lucencyAos, lucencyNormals,lucencyIdx, lucencyVertexCount, aoArray, x, y, z, nx, ny, nz);
								}
								else if (!nei[ori]) {
									AddQuadMesh(blockMesh->faces[j], lucencyVer, lucencyUvws, 
										lucencyAos, lucencyNormals,lucencyIdx, lucencyVertexCount, aoArray, x, y, z, nx, ny, nz);
								}
							}
							else if (voxels[nIdx] != blockId) {
								AddQuadMesh(blockMesh->faces[j], lucencyVer, lucencyUvws,
									lucencyAos, lucencyNormals, lucencyIdx, lucencyVertexCount, aoArray, x, y, z, nx, ny, nz);
							}
							break;
						default: //什么都不做，就当是空气
							break;
						}
					}
				}
			}
		}
		catch (std::exception& e) {
			NE_CORE_ERROR("MeshBuilder", "Failed to build mesh : '{}'", e.what());
			return { {{{std::move(vertices), std::move(uvws), std::move(aos),{}, std::move(indices)}},
				{{std::move(lucencyVer), std::move(lucencyUvws), std::move(lucencyAos),{}, std::move(lucencyIdx)}}} };
		}

		return { {{{std::move(vertices), std::move(uvws), std::move(aos),std::move(normals), std::move(indices)}},
			{{std::move(lucencyVer), std::move(lucencyUvws), std::move(lucencyAos),std::move(lucencyNormals), std::move(lucencyIdx)}}} };
	}


	MeshData MeshBuilder::BuildLodMesh(const voxel::chunk::Chunk& chunk,size_t scale, const voxel::block::BlockLibrary& library) {
		std::vector<float> vertices;
		std::vector<float> uvws;
		std::vector<float> aos;
		std::vector<float> normals;
		std::vector<int> indices;

		const int CHUNK_LENGTH = 32768;
		const int SIZE = 32;
		const int MASK = 31;

		constexpr int MAX_VERTS_FLOATS = 0;//32 * 32 * 32 * 6 * 4; // 6个float/顶点
		constexpr int MAX_INDICES = 0;//32 * 32 * 32 * 6 * 6; // 最多6个面，每个面6个索引
		vertices.reserve(MAX_VERTS_FLOATS * 3 / 4);
		uvws.reserve(MAX_VERTS_FLOATS * 3 / 4);
		aos.reserve(MAX_VERTS_FLOATS / 4);
		normals.reserve(MAX_VERTS_FLOATS * 3 / 4);
		indices.reserve(MAX_INDICES / 4);

		

		int blockCount = library.Size();
		std::vector<const voxel::block::BlockMesh*> blockMeshs;
		for (int idx = 0;idx < blockCount; idx++) {
			auto block = library.FindBlock(idx);
			if (block)
				blockMeshs.push_back(&block->GetMesh());
			else blockMeshs.push_back(nullptr);
		}

		

		try
		{
			const std::vector<int>& voxels = chunk.GetVoxelSpan();
			int x = 0, y = 0, z = 0;
			std::array<float, 4> aoArray = { 1,1,1,1 };
			int vertexCount = 0;
			for (size_t i = 0; i < CHUNK_LENGTH; i++)
			{
				//通过纯位运算更新xyz的值
				z = i & 31;
				y = (i >> 5) & MASK;
				x = (i >> 10) & MASK;

				//获取方块id与mesh
				int blockId = voxels[i];
				if (blockId == 0) continue; //空气，直接跳过

				const voxel::block::BlockMesh* blockMesh = blockMeshs[blockId];
				if (!blockMesh) continue;

				for (int j = 0;j < blockMesh->faces.size();j++) {
					//获取每个面的方向
					int ori = blockMesh->faces[j].orientation;
					//如果是内部面: 面方向枚举 == None == 6则不进行遮挡面剔除，直接构建
					
					if (ori == 6) {
						AddQuadMesh(blockMesh->faces[j], vertices, uvws, aos, normals,indices, vertexCount, aoArray, x, y, z, 0,0,0,scale);
					}
					else { //如果不是(面含有方向属性->进入遮挡面剔除流程)
						int nx = neighbours_[ori][0] + x;
						int ny = neighbours_[ori][1] + y;
						int nz = neighbours_[ori][2] + z;

						int nIdx = (nx << 10) + (ny << 5) + nz;

						switch (blockMesh->modelType)
						{
						case 0: //默认网格
							if (nx < 0 || nx >= 32 ||
								ny < 0 || ny >= 32 ||
								nz < 0 || nz >= 32) {
								//边界，单独判断
								
							}
							else if (blockMeshs[voxels[nIdx]]->modelType) {
								AddQuadMesh(blockMesh->faces[j], vertices, uvws, aos, normals, indices, vertexCount, aoArray, x, y, z, 0, 0, 0, scale);
							}
							break;
						default: //什么都不做，就当是空气
							break;
						}
					}
				}
			}
		}
		catch (const std::exception& e)
		{
			NE_CORE_ERROR("MeshBuilder", "Failed to build mesh : '{}'", e.what());
			return {std::move(vertices), std::move(uvws), std::move(aos),std::move(normals), std::move(indices)};
		}
		return { std::move(vertices), std::move(uvws), std::move(aos), std::move(normals),std::move(indices) };
	}

	MeshData MeshBuilder::BuildLodMesh(const voxel::lod::LodVoxelData& lodData, size_t scale, const voxel::block::BlockLibrary& library) {
		std::vector<float> vertices;
		std::vector<float> uvws;
		std::vector<float> aos;
		std::vector<float> normals;
		std::vector<int> indices;

		const int CHUNK_LENGTH = 32768;
		const int SIZE = 32;
		const int MASK = 31;

		constexpr int MAX_VERTS_FLOATS = 0; // 6个float/顶点
		constexpr int MAX_INDICES = 0;//32 * 32 * 32 * 6 * 6; // 最多6个面，每个面6个索引
		vertices.reserve(MAX_VERTS_FLOATS * 3 / 4);
		uvws.reserve(MAX_VERTS_FLOATS * 3 / 4);
		aos.reserve(MAX_VERTS_FLOATS / 4);
		normals.reserve(MAX_VERTS_FLOATS * 3 / 4);
		indices.reserve(MAX_INDICES / 4);



		int blockCount = library.Size();
		std::vector<const voxel::block::BlockMesh*> blockMeshs;
		for (int idx = 0;idx < blockCount; idx++) {
			auto block = library.FindBlock(idx);
			if (block)
				blockMeshs.push_back(&block->GetMesh());
			else blockMeshs.push_back(nullptr);
		}

		try
		{
			const std::vector<unsigned int>& voxels = lodData.datas_;
			int x = 0, y = 0, z = 0;
			std::array<float, 4> aoArray = { 1,1,1,1 };
			int vertexCount = 0;
			for (size_t i = 0; i < CHUNK_LENGTH; i++)
			{
				//通过纯位运算更新xyz的值
				z = i & 31;
				y = (i >> 5) & MASK;
				x = (i >> 10) & MASK;

				//获取方块id与mesh
				int blockId = voxels[i];
				if (blockId == 0) continue; //空气，直接跳过

				const voxel::block::BlockMesh* blockMesh = blockMeshs[blockId];
				if (!blockMesh) continue;

				for (int j = 0;j < blockMesh->faces.size();j++) {
					//获取每个面的方向
					int ori = blockMesh->faces[j].orientation;
					//如果是内部面: 面方向枚举 == None == 6则不进行遮挡面剔除，直接构建

					if (ori == 6) {
						AddQuadMesh(blockMesh->faces[j], vertices, uvws, aos, normals,indices, vertexCount, aoArray, x, y, z, 0,0,0,scale);
					}
					else { //如果不是(面含有方向属性->进入遮挡面剔除流程)
						int nx = neighbours_[ori][0] + x;
						int ny = neighbours_[ori][1] + y;
						int nz = neighbours_[ori][2] + z;

						int nox = neighbours_[ori][0], noy = neighbours_[ori][1], noz = neighbours_[ori][2];

						int nIdx = (nx << 10) + (ny << 5) + nz;

						switch (blockMesh->modelType)
						{
						case 0: //默认网格
							if (nx < 0 || nx >= SIZE ||
								ny < 0 || ny >= SIZE ||
								nz < 0 || nz >= SIZE) {
								//边界，单独判断
								//计算边界索引
								int borderFaceIdx = 0;
								if (nx < 0 || nx >= SIZE) borderFaceIdx = ny * SIZE + nz;
								else if (ny < 0 || ny >= SIZE) borderFaceIdx = nx * SIZE + nz;
								else if (nz < 0 || nz >= SIZE) borderFaceIdx = nx * SIZE + ny;

								if(!lodData.neighbours_[ori][borderFaceIdx])
									AddQuadMesh(blockMesh->faces[j], vertices, uvws, aos, normals, indices, vertexCount, aoArray, x, y, z,nox, noy, noz, scale);
							}
							else if (blockMeshs[voxels[nIdx]]->modelType) {
								AddQuadMesh(blockMesh->faces[j], vertices, uvws, aos, normals, indices, vertexCount, aoArray, x, y, z, nox, noy, noz, scale);
							}
							break;
						default: //什么都不做，就当是空气
							break;
						}
					}
				}
			}
		}
		catch (const std::exception& e)
		{
			NE_CORE_ERROR("MeshBuilder", "Failed to build mesh : '{}'", e.what());
			return { std::move(vertices), std::move(uvws), std::move(aos),std::move(normals), std::move(indices) };
		}
		return { std::move(vertices), std::move(uvws), std::move(aos),std::move(normals), std::move(indices) };
	}
}