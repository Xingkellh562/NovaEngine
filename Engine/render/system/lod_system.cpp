#include "lod_system.h"
#include <core/config/config.h>
#include <render/utils/mesh_builder.h>
#include <utils/spatial/chunk_range_finder.h>
#include <log/logger.h>
#include <unordered_set>
#include <glm/vec4.hpp>

namespace nova_engine::render::system{

	bool first = false;
	//更新Lod网格：根据预加载表决定更新位置
    void LodSystem::Update(opengl::context::RenderContext& context,glm::vec3 playerTruePos) {
        constexpr int maxDepth = core::config::OCTREE_BIT1 - core::config::CHUNK_BIT1;
        constexpr int layerCount = maxDepth + 1;

        // 1. 构造“所有可能位置”的并集，并记录每个位置的最深目标层级
        //    缓存起来，只在玩家移动跨过某个粒度时重建
        static voxel::terrain::TerrainGenerator generator(123);

        static std::array<std::vector<glm::ivec3>, layerCount> coordSubTable = {};
        static std::array<std::vector<glm::ivec3>, layerCount> coordComTable = {};

        static std::once_flag flag;
        std::call_once(flag, [] {
            int range = core::config::OCTREE_SIZE * 8;
            for (int i = 0; i < layerCount; ++i) {
                int size = core::config::OCTREE_SIZE >> i;

                coordSubTable[i] = nova_engine::utils::spatial::GetLatticeCooInRange(range >> i, size << 1);
                coordComTable[i] = nova_engine::utils::spatial::GetLatticeCooInRange(range >> i, size);
            }
        });

        auto chunkMesh = context.chunkMesh.GetLayer(Default);
        if (!chunkMesh) return;

        glm::ivec3 playerPos = static_cast<glm::ivec3>(playerTruePos);
        int shift = core::config::CHUNK_BIT1;

        glm::ivec3 anchor = {
            (playerPos.x >> shift) << shift,
            (playerPos.y >> shift) << shift,
            (playerPos.z >> shift) << shift
        };

        for (const auto& p : coordComTable[maxDepth]) {
            int x = p.x + anchor.x;
            int y = p.y + anchor.y;
            int z = p.z + anchor.z;

            auto lodMesh = context.lodMeshCache.Find(x, y, z, nullptr);
            auto cMesh = chunkMesh->FindMesh(x, y, z);
            if (cMesh) {
                if (lodMesh && cMesh->Awake()) lodMesh->Sleep();
                else if (lodMesh && !cMesh->Awake()) lodMesh->Wake();
            }
            else {
                if (lodMesh) lodMesh->Wake();
            }
        }

        int budget = 2;
        for (int d = 0; d <= maxDepth && budget > 0; ++d) {
            for (const auto& p : coordSubTable[d]) {
                if (budget <= 0) break;

                int x = p.x + anchor.x;
                int y = p.y + anchor.y;
                int z = p.z + anchor.z;

                int currentDepth = 0;
                
                context.lodMeshCache.Find(x, y, z, &currentDepth);
                //细分
                if (currentDepth < d) {
                    
                    Subdivide(context.lodMeshCache, generator, x, y, z, currentDepth + 1);
                    --budget;
                }
            }
        }

        budget = 2;

        //坐标检查表：防止上一级遍历误合并下一级坐标
        std::unordered_set<glm::ivec3> existedCoords;
        for (int d = maxDepth; d >= 0 && budget > 0; --d) {
            for (const auto& p : coordComTable[d]) {
                if (budget <= 0) break;

                int x = p.x + anchor.x;
                int y = p.y + anchor.y;
                int z = p.z + anchor.z;

                int currentDepth = 0;

                context.lodMeshCache.Find(x, y, z, &currentDepth);
                auto it = existedCoords.find(p);
                //合并
                if (currentDepth > d && it == existedCoords.end()) {
                    Combine(context.lodMeshCache, generator, x, y, z, currentDepth - 1);
                    --budget;
                }
                existedCoords.insert(p);
            }
        }

        for (auto& octree : context.lodMeshCache.AllOctree()) {
            int depth = -1;
            octree.second.Find(0,0,0,&depth);
            if (depth == 0) {
                const int bit = core::config::OCTREE_BIT1;
                int x = octree.first.x << bit;
                int y = octree.first.y << bit;
                int z = octree.first.z << bit;
                context.lodMeshCache.TryDeleteOctree(x,y,z);
            }
        }
    }

	//创建网格
    void LodSystem::Subdivide(
        opengl::context::LodMeshCache& cache,
        const voxel::terrain::TerrainGenerator& generator,
        int x, int y, int z, int depth)
    {
        static const std::array<glm::ivec3, 8> NeiOffCoord = { {
            {0,0,0},{1,0,0},{0,1,0},{1,1,0},
            {0,0,1},{1,0,1},{0,1,1},{1,1,1}
        } };

        const int maxDepth = core::config::OCTREE_BIT1 - core::config::CHUNK_BIT1;
        if (depth <= 0 || depth > maxDepth) return;

        int childSize = core::config::OCTREE_SIZE >> depth;
        int parentSize = childSize << 1;
        int scale = 1 << (maxDepth - depth);

        // 父节点原点
        int px = x & ~(parentSize - 1);
        int py = y & ~(parentSize - 1);
        int pz = z & ~(parentSize - 1);

        std::array<MeshData, 8> data = {};
        std::array<MeshData*, 8> dataPtr = {};

        for (int i = 0; i < 8; ++i) {
            int fx = px+ NeiOffCoord[i].x * childSize;
            int fy = py+ NeiOffCoord[i].y * childSize;
            int fz = pz+ NeiOffCoord[i].z * childSize;
            
            auto lodData = generator.SampleLodData(fx, fy, fz, childSize);
            data[i] = utils::MeshBuilder::BuildLodMesh(
                lodData, scale, voxel::block::BlockLibrary::GetInstance());
            dataPtr[i] = &data[i];
        }

        cache.TrySubdivide(px, py, pz, depth, dataPtr);
    }

    void LodSystem::Combine(
        opengl::context::LodMeshCache& cache,
        const voxel::terrain::TerrainGenerator& generator,
        int x, int y, int z, int depth)
    {
        const int maxDepth = core::config::OCTREE_BIT1 - core::config::CHUNK_BIT1;
        if (depth < 0 || depth >= maxDepth) return;

        int parentSize = core::config::OCTREE_SIZE >> depth;
        int scale = 1 << (maxDepth - depth);

        int px = x & ~(parentSize - 1);
        int py = y & ~(parentSize - 1);
        int pz = z & ~(parentSize - 1);

        auto lodData = generator.SampleLodData(px, py, pz, parentSize);
        MeshData data = utils::MeshBuilder::BuildLodMesh(
            lodData, scale, voxel::block::BlockLibrary::GetInstance());

        cache.TryCombine(px, py, pz, depth, &data);
    }
}