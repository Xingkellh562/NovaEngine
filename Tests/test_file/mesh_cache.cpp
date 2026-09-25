#include "mesh_cache.h"

#include <voxel/chunk/chunk.h>
#include <render/mesh_data.h>
#include <chrono>
#include <iostream>
#include <render/utils/mesh_builder.h>
#include <voxel/terrain/terrain_generator.h>
#include <utils/spatial/chunk_range_finder.h>
#include <core/config/config.h>
#include <voxel/block/block_library.h>
#include <unordered_set>

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
    std::array<nova_engine::render::MeshData*, 8> datas = {};
    for (int i = 0;i < 8;i++) {

        datas[i] = &data;
    }

    meshs.TrySubdivide(0, 0, 0,0,datas);

    int depth = 0;
    ASSERT_NE(meshs.Find(0,0,0,&depth), nullptr);
    
    EXPECT_EQ(depth,1);

    meshs.TrySubdivide(1024, 1024, 1024,1, datas);
    ASSERT_NE(meshs.Find(0, 0, 0, &depth), nullptr);
    EXPECT_EQ(depth, 1);

    ASSERT_NE(meshs.Find(1024, 1024, 1024, &depth), nullptr);
    EXPECT_EQ(depth, 2);
}

TEST_F(MeshCacheTest, MoreCooTest) {
    
    std::array<nova_engine::render::MeshData*, 8> datas = {};
    for (int i = 0;i < 8;i++) {
        datas[i] = &data;
    }

    meshs.TrySubdivide(12432, 24355, 23234,1, datas);

    int depth = 0;
    auto mesh = meshs.Find(12435, 24355, 23234, &depth);
    ASSERT_NE(mesh, nullptr);

    EXPECT_EQ(depth, 1);

    if (mesh) {
         EXPECT_EQ(mesh->GetElementLength(), 12);
    }

    auto mesh2 = meshs.Find(0, 0,0, &depth);
    EXPECT_EQ(mesh2,nullptr);

    meshs.TryCombine(12432, 24355, 23234,0, &data);
    meshs.Find(12435, 24355, 23234, &depth);
    EXPECT_EQ(depth, 0);
}
TEST_F(MeshCacheTest, TraverseTest) {
    std::array<nova_engine::render::MeshData*, 8> datas = {};
    for (int i = 0;i < 8;i++) {
        datas[i] = &data;
    }

    meshs.TrySubdivide(0, 0, 0,0 ,datas);
    meshs.TrySubdivide(1024, 1024, 1024,1, datas);

    int size = 0;
    for (const auto& mesh : meshs.AllData()) {
        EXPECT_EQ(mesh.GetElementLength(), 12);
        size++;
    }
    EXPECT_EQ(size, 15);
}

static void Subdivide(
    nova_engine::render::opengl::context::LodMeshCache& cache,
    const nova_engine::voxel::terrain::TerrainGenerator& generator,
    int x, int y, int z, int depth);

static void Combine(
    nova_engine::render::opengl::context::LodMeshCache& cache,
    const nova_engine::voxel::terrain::TerrainGenerator& generator,
    int x, int y, int z, int depth);

TEST_F(MeshCacheTest, AnalogLodLoading) {
    constexpr int maxDepth = nova_engine::core::config::OCTREE_BIT1 - nova_engine::core::config::CHUNK_BIT1; // = 6
    constexpr int layerCount = maxDepth + 1;

    static nova_engine::voxel::terrain::TerrainGenerator generator(123);
    static std::array<std::vector<glm::ivec3>, layerCount> coordTable = {};

    static std::once_flag flag;
    std::call_once(flag, [] {
        int range = nova_engine::core::config::OCTREE_SIZE * 8;
        for (int i = 0; i < layerCount; ++i) {
            int size = nova_engine::core::config::OCTREE_SIZE >> i;
            coordTable[i] = nova_engine::utils::spatial::GetLatticeCooInRange(range >> i, size);
        }
        });

    //动态更新Lod网格
    glm::ivec3 playerPos = {0,0,0};

    int budget = 256;

    // 粗 -> 细：细分
    for (int d = 0; d <= maxDepth && budget > 0; ++d) {
        int size = nova_engine::core::config::OCTREE_SIZE >> d;
        int shift = nova_engine::core::config::OCTREE_BIT1 - d;

        glm::ivec3 anchor = {
            (playerPos.x >> shift) << shift,
            (playerPos.y >> shift) << shift,
            (playerPos.z >> shift) << shift
        };

        for (const auto& off : coordTable[d]) {
            if (budget <= 0) break;

            int x = anchor.x + off.x;
            int y = anchor.y + off.y;
            int z = anchor.z + off.z;

            int depth = 0;
            meshs.Find(x, y, z, &depth);
            

            if (depth < d && depth < maxDepth) {
                Subdivide(meshs, generator, x, y, z, depth + 1);
                //--budget;
            }
        }
    }

    budget = 8;

    // 细 -> 粗：合并
    //for (int d = maxDepth; d >= 0 && budget > 0; --d) {
    //    int shift = nova_engine::core::config::OCTREE_BIT1 - d;
    //    glm::ivec3 anchor = {
    //        (playerPos.x >> shift) << shift,
    //        (playerPos.y >> shift) << shift,
    //        (playerPos.z >> shift) << shift
    //    };

    //    for (const auto& off : coordTable[d]) {
    //        if (budget <= 0) break;

    //        int x = anchor.x + off.x;
    //        int y = anchor.y + off.y;
    //        int z = anchor.z + off.z;

    //        int depth = 0;
    //        meshs.Find(x, y, z, &depth);
    //        
    //        if (depth > d && depth > 0) {
    //            //Combine(meshs, generator, x, y, z, depth - 1);
    //            //--budget;
    //        }
    //    }
    //}
    int mistake = 0;
    int count = 0;

    std::unordered_set<glm::ivec3> coos;
    //让我康康
    for (int d = maxDepth; d >= 0; --d) {
        int size = nova_engine::core::config::OCTREE_SIZE >> d;
        int shift = nova_engine::core::config::OCTREE_BIT1 - d;

        glm::ivec3 anchor = {
            (playerPos.x >> shift) << shift,
            (playerPos.y >> shift) << shift,
            (playerPos.z >> shift) << shift
        };

        
        for (const auto& off : coordTable[d]) {
            if (budget <= 0) break;

            int x = anchor.x + off.x;
            int y = anchor.y + off.y;
            int z = anchor.z + off.z;

            int depth = 0;
            meshs.Find(x, y, z, &depth);
            count++;
            auto it = coos.find({ x,y,z });
            if (it == coos.end()) {
                EXPECT_EQ(depth ,d) << "Coord:" << x << "," << y << "," << z
                    << "  mistake=" << ++mistake << "   count = " << count;
                coos.insert({ x,y,z });
            }
            else {
                EXPECT_TRUE(depth >= d);
            }
        }
    }
}

static nova_engine::render::MeshData meshData{
        {
        // X     Y     Z      NX    NY    NZ
         0.0f,  0.0f, 0.0f,   // v0 (中心)
        -1.0f, -1.0f, 0.0f,    // v1 (左下)
         1.0f, -1.0f, 0.0f,    // v2 (右下)
         1.0f,  1.0f, 0.0f,    // v3 (右上)
        -1.0f,  1.0f, 0.0f,     // v4 (左上)
    },{
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f
    },{
        1,1,1,1,1
    },{
        0, 1, 2,  // 三角形 1 (左下)
        0, 2, 3,  // 三角形 2 (右下)
        0, 3, 4,  // 三角形 3 (右上)
        0, 4, 1   // 三角形 4 (左上)
    } };

void Subdivide(
    nova_engine::render::opengl::context::LodMeshCache& cache,
    const nova_engine::voxel::terrain::TerrainGenerator& generator,
    int x, int y, int z, int depth)
{
    static const std::array<glm::ivec3, 8> NeiOffCoord = { {
        {0,0,0},{1,0,0},{1,0,1},{0,0,1},
        {0,1,0},{1,1,0},{1,1,1},{0,1,1}
    } };

    const int maxDepth = nova_engine::core::config::OCTREE_BIT1 - nova_engine::core::config::CHUNK_BIT1;
    if (depth <= 0 || depth > maxDepth) return;

    int childSize = nova_engine::core::config::OCTREE_SIZE >> depth;
    int parentSize = childSize << 1;
    int scale = 1 << (maxDepth - depth);

    // 父节点原点
    int px = x & ~(parentSize - 1);
    int py = y & ~(parentSize - 1);
    int pz = z & ~(parentSize - 1);

    std::array<nova_engine::render::MeshData, 8> datas = {};
    std::array<nova_engine::render::MeshData*, 8> dataPtr = {};

    for (int i = 0; i < 8; ++i) {
        int fx = px + NeiOffCoord[i].x * childSize;
        int fy = py + NeiOffCoord[i].y * childSize;
        int fz = pz + NeiOffCoord[i].z * childSize;

        nova_engine::voxel::chunk::Chunk chunk(nova_engine::core::config::CHUNK_SIZE);
        generator.BuildChunk(chunk, fx, fy, fz, childSize);
        datas[i] = meshData;
        dataPtr[i] = &datas[i];
    }

    cache.TrySubdivide(px, py, pz, depth, dataPtr);
}

void Combine(
    nova_engine::render::opengl::context::LodMeshCache& cache,
    const nova_engine::voxel::terrain::TerrainGenerator& generator,
    int x, int y, int z, int depth)
{
    const int maxDepth = nova_engine::core::config::OCTREE_BIT1 - nova_engine::core::config::CHUNK_BIT1;
    if (depth < 0 || depth >= maxDepth) return;

    int parentSize = nova_engine::core::config::OCTREE_SIZE >> depth;
    int scale = 1 << (maxDepth - depth);

    int px = x & ~(parentSize - 1);
    int py = y & ~(parentSize - 1);
    int pz = z & ~(parentSize - 1);

    nova_engine::voxel::chunk::Chunk chunk(nova_engine::core::config::CHUNK_SIZE);
    generator.BuildChunk(chunk, px, py, pz, parentSize);
    nova_engine::render::MeshData data = meshData;

    cache.TryCombine(px, py, pz, depth, &data);
}