#include <gtest/gtest.h>
#include <render/utils/smooth_mesher.h>
#include <utils/assist/des_packer.h>
#include <random>

class MeshBuildTest : public ::testing::Test {
protected:
	nova_engine::voxel::chunk::Chunk<float> chunk{32};
	void SetUp() override {
		// 在每个测试之前运行的代码
		for(int x = 0;x<32;x++)
			for (int y = 0;y < 32;y++)
				for (int z = 0;z < 32;z++) {
					int r = RandomInt(0, 100);
					float density = 2.0f / r - 1.0f;
					chunk.SetVoxel(x,y,z,nova_engine::utils::assist::Pack(density,0));
				}
	}
	void TearDown() override {
		// 在每个测试之后运行的代码
	}

	int RandomInt(int a, int b) {
		static std::mt19937 gen(std::random_device{}());
		return std::uniform_int_distribution<int>(a, b)(gen);
	}


};

TEST_F(MeshBuildTest, BuildOneSNMesh) {
	nova_engine::render::utils::BuildMeshFromChunk(chunk, {});
}

TEST_F(MeshBuildTest, BuildTwoSNMesh) {
	for (int i = 0;i < 2;i++)
		nova_engine::render::utils::BuildMeshFromChunk(chunk, {});
}

TEST_F(MeshBuildTest, BuildFiftySNMesh) {
	for(int i = 0;i<50;i++)
		nova_engine::render::utils::BuildMeshFromChunk(chunk, {});
}

TEST_F(MeshBuildTest, BuildHundredSNMesh) {
	for (int i = 0;i < 100;i++)
		nova_engine::render::utils::BuildMeshFromChunk(chunk, {});
}

