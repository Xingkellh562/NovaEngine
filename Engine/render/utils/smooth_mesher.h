#pragma once

#include <voxel/chunk/chunk.h>
#include <render/mesh_data.h>

namespace nova_engine::render::utils {
	//从有符号密度场中构建网格，使用MC算法
	MeshData BuildMeshFromChunk(
		const voxel::chunk::Chunk<float>& chunk, 
		const std::array<voxel::chunk::Chunk<float>*, 6> neighbours, int scale = 1);

	//从有符号密度场中构建网格，使用SN算法
	//chunk：包含密度场采样数据，中心点采样
	//neighbours：正方向上的7个邻居
	//scale：区块大小缩放
	MeshData BuildSNMeshFromChunk( //<-这个就是出问题的函数，本来是重载函数我改了个名以防混淆，cpp名称同步修改
		const voxel::chunk::Chunk<float>& chunk,
		const std::array<const voxel::chunk::Chunk<float>*, 7> neighbours, int scale = 1);
}