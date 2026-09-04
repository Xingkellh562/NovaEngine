#include "mesh_cache.h"
#include <voxel/chunk/chunk.h>
#include <render/mesh_data.h>
#include <chrono>
#include <iostream>

GLFWwindow* MeshCacheTest::window = nullptr;

nova_engine::render::MeshData GenerateLargePlane(int segX, int segZ, float size = 200.0f) {
    std::vector<float> vertices;
    std::vector<int> indices;

    // 预分配容量（提升性能）
    vertices.reserve((segX + 1) * (segZ + 1) * 6); // 位置(xyz) + 法线(xyz) = 6 floats
    indices.reserve(segX * segZ * 6); // 每个四边形 2 个三角形 = 6 个索引

    float half = size * 0.5f;
    float stepX = size / static_cast<float>(segX);
    float stepZ = size / static_cast<float>(segZ);

    // 1. 生成顶点（带轻微高度起伏，便于视觉观察渲染效果）
    for (int z = 0; z <= segZ; ++z) {
        for (int x = 0; x <= segX; ++x) {
            float px = -half + x * stepX;
            float pz = -half + z * stepZ;
            // 高度图：用正弦制造山丘，模拟地形（仅用于压力测试视觉验证）
            float py = 5.0f * std::sin(px * 0.05f) * std::cos(pz * 0.05f);

            // 法线（粗略估算，对于平面测试，直接朝上即可；带高度时建议重新计算，但为了压力测试省事，统一朝上）
            // 注意：如果高度变化剧烈，法线朝上会导致光照奇怪，但不影响压力测试结果
            vertices.push_back(px);
            vertices.push_back(py);
            vertices.push_back(pz);
            vertices.push_back(0.0f);
            vertices.push_back(1.0f);
            vertices.push_back(0.0f); // 法线朝上
        }
    }

    // 2. 生成索引（两个三角形构成一个四边形）
    for (int z = 0; z < segZ; ++z) {
        for (int x = 0; x < segX; ++x) {
            int top_left = z * (segX + 1) + x;
            int top_right = top_left + 1;
            int bottom_left = (z + 1) * (segX + 1) + x;
            int bottom_right = bottom_left + 1;

            // 三角形 1（左上 -> 右上 -> 左下）
            indices.push_back(top_left);
            indices.push_back(top_right);
            indices.push_back(bottom_left);

            // 三角形 2（右上 -> 右下 -> 左下）
            indices.push_back(top_right);
            indices.push_back(bottom_right);
            indices.push_back(bottom_left);
        }
    }

    // 包装成 MeshData（假设 MeshData 接受 vector<float> 和 vector<uint32_t>）
    nova_engine::render::MeshData data;
    data.vertices = std::move(vertices); // 注意：你的 MeshData 目前可能是 vector<Vertex> 而非 float
    data.indices = std::move(indices);
    return data;
}


TEST_F(MeshCacheTest, SubmitAndFindMesh) {

	nova_engine::render::MeshData data{ 
        {
            // X     Y     Z      NX    NY    NZ
             0.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,  // v0 (中心)
            -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  // v1 (左下)
             1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  // v2 (右下)
             1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  // v3 (右上)
            -1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f   // v4 (左上)
        },{
            0, 1, 2,  // 三角形 1 (左下)
            0, 2, 3,  // 三角形 2 (右下)
            0, 3, 4,  // 三角形 3 (右上)
            0, 4, 1   // 三角形 4 (左上)
        } };
    meshs.SubmitMesh(0, 0, 0, data);

    EXPECT_EQ(meshs.Exist(0,0,0),true);
}

//TEST_F(MeshCacheTest, SubmitAndFind512LargeMesh) {
//    nova_engine::render::MeshData large_mesh = GenerateLargePlane(512, 512, 500.0f);
//    for (int x = 0;x < 8;x++)
//        for (int y = 0;y < 8;y++)
//            for (int z = 0;z < 8;z++) {
//                
//                meshs.SubmitMesh(x, y, z, large_mesh);
//            }
//
//    for (int x = 0;x < 8;x++)
//        for (int y = 0;y < 8;y++)
//            for (int z = 0;z < 8;z++)
//                EXPECT_EQ(meshs.Exist(x, y, z), true);
//    
//}

TEST_F(MeshCacheTest, TestMeshRecycle) {
    nova_engine::render::MeshData large_mesh = GenerateLargePlane(512, 512, 500.0f);
    for (int x = -2;x < 6;x++)
        for (int y = -2;y < 6;y++)
            for (int z = -2;z < 6;z++) {

                meshs.SubmitMesh(x * 32 + 5, y * 32 + 3, z * 32 + 5, large_mesh);
            }

    for (int x = -2;x < 6;x++)
        for (int y = -2;y < 6;y++)
            for (int z = -2;z < 6;z++)
                meshs.FreeMesh(x * 32 + 5, y * 32 + 6, z * 32 + 6);

    for (int x = -2;x < 6;x++)
        for (int y = -2;y < 6;y++)
            for (int z = -2;z < 6;z++)
                EXPECT_EQ(meshs.Exist(x * 32, y * 32, z * 32), false);

    for (int x = -2;x < 6;x++)
        for (int y = -2;y < 6;y++)
            for (int z = -2;z < 6;z++)
                meshs.SubmitMesh(x * 32, y * 32, z * 32, large_mesh);


    for (int x = -2;x < 6;x++)
        for (int y = -2;y < 6;y++)
            for (int z = -2;z < 6;z++)
                EXPECT_EQ(meshs.Exist(x * 32 + 2, y * 32 +4, z * 32 + 4), true);
}