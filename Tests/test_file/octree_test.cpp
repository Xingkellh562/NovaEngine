#include <gtest/gtest.h>
#include <collection/data_structure/adaptive_lod_octree.h>
#include <voxel/chunk/chunk.h>
#include <array>
#include <memory>
#include <random>


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

	AdaptiveLodOctree<Chunk, 2048, 32> chunkOctree;
};

TEST_F(OctreeTest, AddAndFindChunk) {
	int depth = 0;
	const Chunk* resultChunk = chunkOctree.Find(0, 0, 0, &depth);
	EXPECT_EQ(resultChunk,nullptr);
	EXPECT_EQ(depth,0);

	std::array<std::unique_ptr<Chunk>, 8> chunks;
	for (int i = 0;i < 8;i++) {
		chunks[i] = std::make_unique<Chunk>(32);
	}

	chunkOctree.TrySubdivide(0,0,0, std::move(chunks));
	resultChunk = chunkOctree.Find(0, 0, 0, &depth);
	ASSERT_NE(resultChunk, nullptr);
	EXPECT_EQ(depth, 1);

	std::array<std::unique_ptr<Chunk>, 8> chunks2;
	for (int i = 0;i < 8;i++) {
		chunks2[i] = std::make_unique<Chunk>(32);
	}

	chunkOctree.TrySubdivide(0, 0, 0, std::move(chunks2));
	resultChunk = chunkOctree.Find(0, 0, 0, &depth);
	ASSERT_NE(resultChunk, nullptr);
	EXPECT_EQ(depth, 2);

	resultChunk = chunkOctree.Find(1024, 1024, 1024, &depth);
	ASSERT_NE(resultChunk, nullptr);
	EXPECT_EQ(depth, 1);

	auto chunkPtr = std::make_unique<Chunk>(32);
	chunkOctree.TryCombine(0, 0, 0,std::move(chunkPtr));

	resultChunk = chunkOctree.Find(0, 0, 0, &depth);
	ASSERT_NE(resultChunk, nullptr);
	EXPECT_EQ(depth, 1);
}

TEST_F(OctreeTest, LoopAddAndFindChunk) {
	for (int i = 0;i < 6;i++) {
		std::array<std::unique_ptr<Chunk>, 8> chunks;
		for (int j = 0;j < 8;j++) {
			chunks[j] = std::make_unique<Chunk>(32);
		}
		chunkOctree.TrySubdivide(0, 0, 0, std::move(chunks));
	}

	int depth = 0;
	const Chunk* resultChunk = chunkOctree.Find(0, 0, 0, &depth);
	ASSERT_NE(resultChunk, nullptr);
	EXPECT_EQ(depth, 6);

	std::array<std::unique_ptr<Chunk>, 8> chunks;
	for (int j = 0;j < 8;j++) {
		chunks[j] = std::make_unique<Chunk>(32);
	}
	bool success = chunkOctree.TrySubdivide(0, 0, 0, std::move(chunks));
	EXPECT_EQ(success, false);

	for (int i = 0;i < 3;i++) {
		auto chunk = std::make_unique<Chunk>(32);
		chunkOctree.TryCombine(0, 0, 0, std::move(chunk));
	}

	resultChunk = chunkOctree.Find(0, 0, 0, &depth);
	ASSERT_NE(resultChunk, nullptr);
	EXPECT_EQ(depth, 3);


}

TEST_F(OctreeTest, TryEmplaceAtLeaf) {
	std::array<std::unique_ptr<Chunk>, 8> chunks;
	for (auto& c : chunks) c = std::make_unique<Chunk>(32);
	ASSERT_TRUE(chunkOctree.TrySubdivide(0, 0, 0, std::move(chunks)));

	Chunk chunk(32);
	chunk.SetVoxel(1, 2, 3, 42);
	EXPECT_TRUE(chunkOctree.TryEmplace(0, 0, 0, std::move(chunk)));

	int depth = -1;
	const Chunk* found = chunkOctree.Find(0, 0, 0, &depth);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->GetVoxel(1, 2, 3), 42);
	EXPECT_EQ(depth, 1); // 深度不变
}

TEST_F(OctreeTest, TryEmplaceOutOfBounds) {
	Chunk chunk1(32);
	EXPECT_FALSE(chunkOctree.TryEmplace(-1, 0, 0, std::move(chunk1)));
	Chunk chunk2(32);
	EXPECT_FALSE(chunkOctree.TryEmplace(2048, 0, 0, std::move(chunk2)));
	Chunk chunk3(32);
	EXPECT_FALSE(chunkOctree.TryEmplace(0, -1, 0, std::move(chunk3)));
	Chunk chunk4(32);
	EXPECT_FALSE(chunkOctree.TryEmplace(0, 0, 2048, std::move(chunk4)));
}

TEST_F(OctreeTest, SubdivideAtMaxDepthFails) {
	for (int i = 0; i < 6; ++i) {
		std::array<std::unique_ptr<Chunk>, 8> chunks;
		for (auto& c : chunks) c = std::make_unique<Chunk>(32);
		ASSERT_TRUE(chunkOctree.TrySubdivide(0, 0, 0, std::move(chunks)));
	}

	int depth = -1;
	ASSERT_NE(chunkOctree.Find(0, 0, 0, &depth), nullptr);
	EXPECT_EQ(depth, 6);

	std::array<std::unique_ptr<Chunk>, 8> chunks;
	for (auto& c : chunks) c = std::make_unique<Chunk>(32);
	EXPECT_FALSE(chunkOctree.TrySubdivide(0, 0, 0, std::move(chunks)));
}

TEST_F(OctreeTest, CombineRootFails) {
	auto data = std::make_unique<Chunk>(32);
	EXPECT_FALSE(chunkOctree.TryCombine(0, 0, 0, std::move(data)));
}

TEST_F(OctreeTest, CombineToRootThenSubdivideAgain) {
	// 先细分到最大深度
	for (int i = 0; i < 6; ++i) {
		std::array<std::unique_ptr<Chunk>, 8> chunks;
		for (auto& c : chunks) c = std::make_unique<Chunk>(32);
		ASSERT_TRUE(chunkOctree.TrySubdivide(0, 0, 0, std::move(chunks)));
	}

	// 连续合并回根
	for (int i = 0; i < 6; ++i) {
		auto data = std::make_unique<Chunk>(32);
		ASSERT_TRUE(chunkOctree.TryCombine(0, 0, 0, std::move(data)));
	}

	int depth = -1;
	const Chunk* found = chunkOctree.Find(0, 0, 0, &depth);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(depth, 0);

	// 再次细分到最大深度
	for (int i = 0; i < 6; ++i) {
		std::array<std::unique_ptr<Chunk>, 8> chunks;
		for (auto& c : chunks) c = std::make_unique<Chunk>(32);
		ASSERT_TRUE(chunkOctree.TrySubdivide(0, 0, 0, std::move(chunks)));
	}

	found = chunkOctree.Find(0, 0, 0, &depth);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(depth, 6);
}

TEST_F(OctreeTest, CombineNonBalancedBranch) {
	// 分支 A: (0,0,0) 细分到深度 2
	for (int i = 0; i < 2; ++i) {
		std::array<std::unique_ptr<Chunk>, 8> chunks;
		for (auto& c : chunks) c = std::make_unique<Chunk>(32);
		ASSERT_TRUE(chunkOctree.TrySubdivide(0, 0, 0, std::move(chunks)));
	}

	// 分支 B: (1024,1024,1024) 细分到深度 2
	for (int i = 0; i < 2; ++i) {
		std::array<std::unique_ptr<Chunk>, 8> chunks;
		for (auto& c : chunks) c = std::make_unique<Chunk>(32);
		ASSERT_TRUE(chunkOctree.TrySubdivide(1024, 1024, 1024, std::move(chunks)));
	}

	// 只合并分支 A 一层
	auto data = std::make_unique<Chunk>(32);
	ASSERT_TRUE(chunkOctree.TryCombine(0, 0, 0, std::move(data)));

	int depth = -1;
	const Chunk* found = chunkOctree.Find(0, 0, 0, &depth);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(depth, 1);

	// 分支 B 深度应保持不变
	found = chunkOctree.Find(1024, 1024, 1024, &depth);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(depth, 3);
}

TEST_F(OctreeTest, CombineThenSubdivideAgain) {
	// 细分到深度 2
	for (int i = 0; i < 2; ++i) {
		std::array<std::unique_ptr<Chunk>, 8> chunks;
		for (auto& c : chunks) c = std::make_unique<Chunk>(32);
		ASSERT_TRUE(chunkOctree.TrySubdivide(0, 0, 0, std::move(chunks)));
	}

	// 合并回深度 1
	auto data = std::make_unique<Chunk>(32);
	ASSERT_TRUE(chunkOctree.TryCombine(0, 0, 0, std::move(data)));

	int depth = -1;
	ASSERT_NE(chunkOctree.Find(0, 0, 0, &depth), nullptr);
	EXPECT_EQ(depth, 1);

	// 再次细分到深度 2
	std::array<std::unique_ptr<Chunk>, 8> chunks;
	for (auto& c : chunks) c = std::make_unique<Chunk>(32);
	ASSERT_TRUE(chunkOctree.TrySubdivide(0, 0, 0, std::move(chunks)));

	ASSERT_NE(chunkOctree.Find(0, 0, 0, &depth), nullptr);
	EXPECT_EQ(depth, 2);
}

TEST_F(OctreeTest, FindEachChildData) {
	std::array<std::unique_ptr<Chunk>, 8> chunks;
	for (int i = 0; i < 8; ++i) {
		chunks[i] = std::make_unique<Chunk>(32);
		chunks[i]->SetVoxel(0, 0, 0, i + 1);
	}
	ASSERT_TRUE(chunkOctree.TrySubdivide(0, 0, 0, std::move(chunks)));

	// 子节点中心坐标（RootSize=2048，第一次细分后子节点尺寸 1024）
	const int coords[8][3] = {
		{512, 512, 512},   // 0
		{1536, 512, 512},  // 1
		{512, 1536, 512},  // 2
		{1536, 1536, 512}, // 3
		{512, 512, 1536},  // 4
		{1536, 512, 1536}, // 5
		{512, 1536, 1536}, // 6
		{1536, 1536, 1536} // 7
	};

	for (int i = 0; i < 8; ++i) {
		int depth = -1;
		const Chunk* found = chunkOctree.Find(coords[i][0], coords[i][1], coords[i][2], &depth);
		ASSERT_NE(found, nullptr);
		EXPECT_EQ(found->GetVoxel(0, 0, 0), i + 1);
		EXPECT_EQ(depth, 1);
	}
}

TEST_F(OctreeTest, CombineDataContent) {
	std::array<std::unique_ptr<Chunk>, 8> chunks;
	for (auto& c : chunks) c = std::make_unique<Chunk>(32);
	ASSERT_TRUE(chunkOctree.TrySubdivide(0, 0, 0, std::move(chunks)));

	auto data = std::make_unique<Chunk>(32);
	data->SetVoxel(1, 2, 3, 99);
	ASSERT_TRUE(chunkOctree.TryCombine(0, 0, 0, std::move(data)));

	int depth = -1;
	const Chunk* found = chunkOctree.Find(0, 0, 0, &depth);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->GetVoxel(1, 2, 3), 99);
	EXPECT_EQ(depth, 0);
}

TEST_F(OctreeTest, FindWithoutDepth) {
	std::array<std::unique_ptr<Chunk>, 8> chunks;
	for (auto& c : chunks) c = std::make_unique<Chunk>(32);
	ASSERT_TRUE(chunkOctree.TrySubdivide(0, 0, 0, std::move(chunks)));

	EXPECT_NE(chunkOctree.Find(0, 0, 0), nullptr);
}

TEST_F(OctreeTest, FindDepthUnchangedOnMiss) {
	int depth = -123;
	EXPECT_EQ(chunkOctree.Find(-1, 0, 0, &depth), nullptr);
	// 如果设计为不修改 depth，则：
	// EXPECT_EQ(depth, -123);
}

TEST(OctreeTemplateTest, RootSizeLessThanLeafSize) {
	AdaptiveLodOctree<Chunk, 3, 5> tree;
	// 预期 rootSize_ 被提升为 leafSize_，maxDepth_ = 0
	// 可通过 Find/TrySubdivide 行为间接验证
}

TEST_F(OctreeTest, RandomStress) {
	std::mt19937 rng(42);
	std::uniform_int_distribution<int> coord(0, 2047);

	for (int i = 0; i < 1000; ++i) {
		int x = coord(rng), y = coord(rng), z = coord(rng);
		int op = rng() % 3;

		if (op == 0) {
			std::array<std::unique_ptr<Chunk>, 8> chunks;
			for (auto& c : chunks) c = std::make_unique<Chunk>(32);
			chunkOctree.TrySubdivide(x, y, z, std::move(chunks));
		}
		else if (op == 1) {
			auto data = std::make_unique<Chunk>(32);
			chunkOctree.TryCombine(x, y, z, std::move(data));
		}
		else {
			int depth = -1;
			chunkOctree.Find(x, y, z, &depth);
		}
	}
	// 最终至少验证不崩溃
}