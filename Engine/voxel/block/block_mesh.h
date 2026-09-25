#pragma once
#include <vector>
#include <array>

namespace nova_engine::voxel::block {
	enum QuadFaceOrientation {
		North = 0, 
		South = 1,
		Up    = 2, 
		Down  = 3, 
		East  = 4, 
		West  = 5, 
		None  = 6//-x,+x,+y,-y,+z,-z,none
	};

	//这一个对象代表一个四边形面
	struct QuadFace {
		std::vector<std::array<float, 3>> vertices;
		std::vector<std::array<float, 3>> uvs;
		std::array<int, 6> indices = {0,1,2,0,2,3};
		QuadFaceOrientation orientation;

		QuadFace(std::vector<std::array<float, 3>> vertices,
			std::vector<std::array<float, 3>> uvs,
			QuadFaceOrientation orientation = None) :
			vertices(std::move(vertices)),
			uvs(std::move(uvs)),
			orientation(orientation) {
		}
	};

	//一个方块模型包含多个四边形面
	struct BlockMesh
	{
		int modelType = 0;
		std::vector<QuadFace> faces;
		BlockMesh(const std::vector<QuadFace>& faces,int type) :faces(faces),modelType(type){}
	};
}