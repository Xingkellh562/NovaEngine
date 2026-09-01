#include <gtest/gtest.h>
#include <utils/loaders/model_loader.h>
#include <filesystem>
#include <voxel/block/block_mesh.h>

using namespace nova_engine::utils::loader;
using namespace nova_engine::voxel::block;

class ModelLoaderTest : public ::testing::Test {
protected:
	void SetUp() override {
		// 在每个测试之前运行的代码
	}
	void TearDown() override {
		// 在每个测试之后运行的代码
	}
};

TEST_F(ModelLoaderTest, LoadValidBlockModel) {
	std::filesystem::path test_file = __FILE__;
	auto test_dir = test_file.parent_path();
	auto json_path = test_dir / "test_data" / "valid_block_model.json";

	BlockMesh mesh = ModelLoader::LoadBlockModelFromJsonPath(json_path.string().c_str(), nullptr);

	ASSERT_EQ(mesh.faces.size(), 1);
	const auto& face = mesh.faces[0];

	// 验证顶点数量（四边形应该有 4 个顶点）
	EXPECT_EQ(face.vertices.size(), 4);
	// 验证第一个顶点位置（假设 JSON 中第一个顶点是 [-0.5, -0.5, 0.0]）
	const auto& v0 = face.vertices[0];
	EXPECT_FLOAT_EQ(v0[0], -0.5f);
	EXPECT_FLOAT_EQ(v0[1], -0.5f);
	EXPECT_FLOAT_EQ(v0[2], 0.0f);

	// 验证 UV 和纹理索引（由于传入 nullptr，索引应为 0）
	const auto& uv0 = face.uvs[0];
	EXPECT_FLOAT_EQ(uv0[0], 0.0f);
	EXPECT_FLOAT_EQ(uv0[1], 0.0f);
	EXPECT_EQ(static_cast<int>(uv0[2]), 0);   // 纹理索引
}

TEST_F(ModelLoaderTest, LoadInvalidMismatchVertices) {
	auto test_file = std::filesystem::path(__FILE__);
	auto test_dir = test_file.parent_path();
	auto json_path = test_dir / "test_data" / "invalid_mismatch_vertices.json";

	BlockMesh mesh = ModelLoader::LoadBlockModelFromJsonPath(json_path.string().c_str(), nullptr);

	// 期望返回空的 BlockMesh（即 faces 为空）
	EXPECT_TRUE(mesh.faces.empty());
}

// 测试文件不存在的情况（确保不崩溃，返回空）
TEST_F(ModelLoaderTest, LoadNonExistentFile) {
	BlockMesh mesh = ModelLoader::LoadBlockModelFromJsonPath("non_existent_file.json", nullptr);
	EXPECT_TRUE(mesh.faces.empty());
}

TEST_F(ModelLoaderTest, LoadValidComplexBlockModel) {
	auto test_file = std::filesystem::path(__FILE__);
	auto test_dir = test_file.parent_path();
	auto json_path = test_dir / "test_data" / "complex_block_model.json";

	BlockMesh mesh = ModelLoader::LoadBlockModelFromJsonPath(json_path.string().c_str(), nullptr);
	// 期望返回完整正确的 BlockMesh
	// 应该有 7 个面（6 个立方体面 + 1 个额外的面）
	ASSERT_EQ(mesh.faces.size(), 7);
	// 验证第一个面的顶点和 UV
	const auto& face = mesh.faces[0];
	ASSERT_EQ(face.vertices.size(), 4);
	ASSERT_EQ(face.vertices[0].size(), 3);

	// 直接验证第七个面
	const auto& face7 = mesh.faces[6];
	ASSERT_FLOAT_EQ(face7.vertices[0][0], -0.5f);
	ASSERT_FLOAT_EQ(face7.vertices[0][1], -0.5f);
	ASSERT_FLOAT_EQ(face7.vertices[0][2],  0.0f);
}