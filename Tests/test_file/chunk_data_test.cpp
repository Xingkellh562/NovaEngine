#include <gtest/gtest.h>
#include <voxel/chunk/chunk_data.h>

using namespace nova_engine::collection::data_structure;
using namespace nova_engine::voxel::chunk;

class ChunkDataTest : public ::testing::Test {
protected:
	void SetUp() override {
		// 在每个测试之前运行的代码
	}
	void TearDown() override {
		// 在每个测试之后运行的代码
	}

	ChunkData chunkData;
};

TEST_F(ChunkDataTest, SubmitAndFindChunk) {
	Chunk chunk(32);
	chunk.SetVoxel(0, 0, 0, 1);

	chunkData.SubmitChunk(100, 100, 100, std::make_unique<Chunk>(std::move(chunk)));

	Chunk* found = chunkData.FindChunk(100, 100, 100);
	ASSERT_NE(found,nullptr);
	EXPECT_EQ(found->GetVoxel(0, 0, 0), 1);

	chunkData.RemoveChunk(100,100,100);
	Chunk* foundNull = chunkData.FindChunk(100, 100, 100);
	EXPECT_EQ(foundNull, nullptr);
}

TEST_F(ChunkDataTest, SubmitAndFindChunkWithNegativeCoo) {
	Chunk chunk(32);
	chunk.SetVoxel(0, 0, 0, 1);

	chunkData.SubmitChunk(-234, 78, -938, std::make_unique<Chunk>(std::move(chunk)));
	Chunk* found = chunkData.FindChunk(-234, 78, -938);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->GetVoxel(0, 0, 0), 1);
}

TEST_F(ChunkDataTest, SubmitAndFind32768Chunks) {
	for(int x = 0;x < 4096;x += 128)
		for (int y = 0;y < 4096;y += 128)
			for (int z = 0;z < 4096;z += 128) {
				auto chunk = chunkData.AcquireChunk();
				chunk->SetVoxel(0, 0, 0, 1);

				chunkData.SubmitChunk(x, y, z, std::move(chunk));
			}
	for (int x = 0;x < 4096;x += 128)
		for (int y = 0;y < 4096;y += 128)
			for (int z = 0;z < 4096;z += 128) {
				Chunk* found = chunkData.FindChunk(x,y,z);
				ASSERT_NE(found, nullptr);
				EXPECT_EQ(found->GetVoxel(0, 0, 0), 1);
			}
}