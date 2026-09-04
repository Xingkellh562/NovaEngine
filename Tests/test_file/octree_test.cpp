#include <gtest/gtest.h>
#include <collection/data_structure/octree.h>
#include <voxel/chunk/chunk.h>

using namespace nova_engine::collection::data_structure;
using namespace nova_engine::voxel::chunk;

class OctreeTest : public ::testing::Test {
protected:
	void SetUp() override {
		// 在每个测试之前运行的代码
	}
	void TearDown() override {
		// 在每个测试之后运行的代码
	}

	Octree<Chunk, 2048, 32> chunkOctree;
};

TEST_F(OctreeTest, AddAndFindChunk) {
	Chunk chunk = Chunk(32);

	chunk.SetVoxel(0, 0, 0, 1);
	
	chunkOctree.Add(100, 100, 100, std::move(chunk));
	Chunk* foundChunk = chunkOctree.Find(100, 100, 100);
	EXPECT_NE(foundChunk, nullptr);
	if (foundChunk != nullptr) {
		int blockI = foundChunk->GetVoxel(0, 0, 0); // 测试访问块��据
		EXPECT_EQ(blockI, 1);
	}

	chunkOctree.Remove(100, 100, 100);
	EXPECT_EQ(chunkOctree.Find(100, 100, 100), nullptr);
}

TEST_F(OctreeTest, AddAndFindChunkInZeroCoo) {
	Chunk chunk = Chunk(32);

	chunk.SetVoxel(0, 0, 0, 1);

	chunkOctree.Add(0, 0, 0, std::move(chunk));
	Chunk* foundChunk = chunkOctree.Find(0, 0, 0);
	EXPECT_NE(foundChunk, nullptr);
	if (foundChunk != nullptr) {
		int blockI = foundChunk->GetVoxel(0, 0, 0); // 测试访问块��据
		EXPECT_EQ(blockI, 1);
	}

	chunkOctree.Remove(0, 0, 0);
	EXPECT_EQ(chunkOctree.Find(0, 0, 0), nullptr);
}