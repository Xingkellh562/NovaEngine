#include "lod_system.h"
#include <core/config/config.h>
#include <render/utils/mesh_builder.h>
#include <utils/spatial/chunk_range_finder.h>
#include <log/logger.h>
#include <unordered_set>
#include <glm/vec4.hpp>

namespace nova_engine::render::system{
    struct SubmitTask {
        int px, py, pz, size, depth, scale;
        const voxel::terrain::TerrainGenerator& generator;
    };



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

        int budget = 32;
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
                    
                    Subdivide(context.lodMeshCache, generator, x, y, z, currentDepth + 1, currentDepth >= d - 1);
                    --budget;
                }
            }
        }

        budget = 32;

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
                    //--budget;
                }
                existedCoords.insert(p);
            }
        }

        std::vector<glm::ivec3> needDelete_;
        for (auto& octree : context.lodMeshCache.AllOctree()) {
            int depth = -1;
            octree.second.Find(0,0,0,&depth);
            if (depth == 0) {
                const int bit = core::config::OCTREE_BIT1;
                int x = octree.first.x << bit;
                int y = octree.first.y << bit;
                int z = octree.first.z << bit;
                needDelete_.push_back({x,y,z});
                context.lodMeshCache.TryDeleteOctree(x,y,z);
            }
        }

        for(auto p : needDelete_)
            context.lodMeshCache.TryDeleteOctree(p.x, p.y, p.z);

        core::concurrency::Result <SubdivideResult> out;
        while (subdivideWorker_.TryPopResult(out)) {
            int px = out.value.x;
            int py = out.value.y;
            int pz = out.value.z;
            int depth = out.value.depth;
            std::array<MeshData*, 8> dataPtr{};
            for (int i = 0;i < 8;i++) {
                //如果顶点数量为零->空网格->传空指针->不分配索引，减小开销
                if (out.value.datas[i].vertices.size())
                    dataPtr[i] = &out.value.datas[i];
                else dataPtr[i] = nullptr;
            }
            context.lodMeshCache.TrySubdivide(px, py, pz, depth, dataPtr);
            subdivideExists_.erase({ px,py,pz });
        }

        core::concurrency::Result<CombineResult> out2;
        while (combineWorker_.TryPopResult(out2)) {
            int px = out2.value.x;
            int py = out2.value.y;
            int pz = out2.value.z;
            int depth = out2.value.depth;
            if(out2.value.data.vertices.size())
                context.lodMeshCache.TryCombine(px, py, pz, depth, &out2.value.data);
            else 
                context.lodMeshCache.TryCombine(px, py, pz, depth, nullptr);
            combineExists_.erase({ px,py,pz });
        }
    }

	//创建网格
    void LodSystem::Subdivide(
        opengl::context::LodMeshCache& cache,
        const voxel::terrain::TerrainGenerator& generator,
        int x, int y, int z, int depth,bool generatorMesh)
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

        //将下面提交到多线程
        if (subdivideExists_.find({ px,py,pz }) != subdivideExists_.end())
            return;

        subdivideExists_.insert({ px,py,pz });
        try {
            auto cancel = std::make_shared<core::concurrency::CancelToken>();

            SubmitTask submit{px,py,pz,childSize,depth,scale,generator};

            bool success = subdivideWorker_.TrySubmitTask(core::concurrency::Task<SubdivideResult>{
                .id = 0, .priority = 0, .cancel = cancel, 
                .fn = [submit](core::concurrency::TaskContext<SubdivideResult>& ctx) {
                    try {
                        std::array<MeshData, 8> data = {};
                        for (int i = 0; i < 8; ++i) {
                            int fx = submit.px + NeiOffCoord[i].x * submit.size;
                            int fy = submit.py + NeiOffCoord[i].y * submit.size;
                            int fz = submit.pz + NeiOffCoord[i].z * submit.size;

                            auto lodData = submit.generator.SampleLodData(fx, fy, fz, submit.size);
                            data[i] = utils::MeshBuilder::BuildLodMesh(
                                lodData, submit.scale, voxel::block::BlockLibrary::GetInstance());
                        }
                        SubdivideResult result{ submit.px,submit.py,submit.pz,submit.depth,std::move(data) };
                        ctx.emit(std::move(result));
                        //ctx.emit({ px,py,pz,depth,{} });
                    }
                    catch (...) {
                        ctx.emit({ submit.px, submit.py, submit.pz, submit.depth,{} });
                    }

                }
            });

            if (!success) subdivideExists_.erase({ px,py,pz });
        }
        catch (const std::exception& e) {
            NE_CORE_ERROR("LodSystem", "Failed to submit LOD mesh build task, catch error:{}", e.what());
            subdivideExists_.erase({ px,py,pz });
        }
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

        //提交

        if (combineExists_.find({ px,py,pz }) != combineExists_.end())
            return;

        combineExists_.insert({ px,py,pz });
        try {
            auto cancel = std::make_shared<core::concurrency::CancelToken>();

            SubmitTask submit{ px,py,pz,parentSize,depth,scale,generator };

            bool success = combineWorker_.TrySubmitTask(core::concurrency::Task<CombineResult>{
                .id = 0, .priority = 0, .cancel = cancel,
                .fn = [submit](core::concurrency::TaskContext<CombineResult>& ctx) {
                    try {
                        auto lodData = submit.generator.SampleLodData(submit.px, submit.py, submit.pz, submit.size);
                        MeshData data = utils::MeshBuilder::BuildLodMesh(
                            lodData, submit.scale, voxel::block::BlockLibrary::GetInstance());

                        CombineResult result{ submit.px,submit.py,submit.pz,submit.depth,std::move(data) };

                        ctx.emit(std::move(result));
                        //ctx.emit({ px,py,pz,depth,{} });
                    }
                    catch (...) {
                        ctx.emit({ submit.px,submit.py,submit.pz,submit.depth,{} });
                    }
                }
            });

            if (!success) combineExists_.erase({ px,py,pz });
        }
        catch (const std::exception& e) {
            NE_CORE_ERROR("LodSystem", "Failed to submit LOD mesh build task, catch error:{}", e.what());
            combineExists_.erase({ px,py,pz });
        }
    }
}