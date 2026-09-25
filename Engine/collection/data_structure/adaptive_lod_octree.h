#pragma once

#include "octree.h"
#include <cmath>

namespace nova_engine::collection::data_structure {
	//专门用来实现Lod功能的八叉树数据结构
	template<typename T,size_t RootSize, size_t LeafSize>
	class AdaptiveLodOctree {
	public:
		AdaptiveLodOctree() {
			static_assert(RootSize > 0 && LeafSize > 0, "RootSize and LeafSize must be positive");
			rootSize_ = RootSize, leafSize_ = LeafSize;
			rootSize_--, leafSize_--;
			for (int i = 0;i < 6;i++) {
				rootSize_ |= rootSize_ >> 1;
				leafSize_ |= leafSize_ >> 1;
			}
			rootSize_++, leafSize_++;

			if (rootSize_ < leafSize_) {
				rootSize_ = leafSize_;
				maxDepth_ = 0;
			}		
			else
				maxDepth_ = static_cast<int>(std::log2(rootSize_) - std::log2(leafSize_));

			//添加一个空网格
			OctreeNode rootNode;
			rootNode.chunkIndex = 0;
			nodes_.push_back(rootNode);
			datas_.push_back(nullptr);
		}

		~AdaptiveLodOctree() = default;

		AdaptiveLodOctree(const AdaptiveLodOctree&) = delete;
		AdaptiveLodOctree& operator=(const AdaptiveLodOctree&) = delete;

		AdaptiveLodOctree(AdaptiveLodOctree&&) noexcept = default;
		AdaptiveLodOctree& operator=(AdaptiveLodOctree&&) noexcept = default;

		//增删查改->

		//查:基于坐标的查找，返回对象指针，失败返回空指针
		//depth返回参数可选
		T* Find(int x, int y, int z, int* depth = nullptr) const{
			if (!CheckCooBorder(x, y, z)) return nullptr;
			int nodeIdx = 0;
			for (int d = 0;d < maxDepth_;d++) {
				const OctreeNode& node = nodes_[nodeIdx];
				if (depth) *depth = node.depth;

				if (node.childFirstIndex < 0)
					return node.chunkIndex >= 0 ? datas_[node.chunkIndex].get() : nullptr;
				int childIdx = GetChildIdxWithCoo(x, y, z, d);
				if (node.childMask & (1 << childIdx))
					nodeIdx = node.childFirstIndex + childIdx;
				else
					return node.chunkIndex >= 0 ? datas_[node.chunkIndex].get() : nullptr;
			}
			//最后是depth == maxDepth_的情况
			const OctreeNode& node = nodes_[nodeIdx];
			if (depth) *depth = node.depth;
			return node.chunkIndex >= 0 ? datas_[node.chunkIndex].get() : nullptr;
		}

		//改:原地替换数据，不涉及到八叉树节点的改变
		bool TryEmplace(int x,int y,int z ,std::unique_ptr<T> data) {
			if (!CheckCooBorder(x, y, z)) return false;
			int nodeIdx = 0;
			bool ground = false;
			for (int d = 0;d < maxDepth_;d++) {
				const OctreeNode& node = nodes_[nodeIdx];

				if (node.childFirstIndex < 0) ground = true;
				
				int childIdx = GetChildIdxWithCoo(x, y, z, d);
				if (node.childMask & (1 << childIdx))
					nodeIdx = node.childFirstIndex + childIdx;
				else ground = true;

				if (ground) {
					if (node.chunkIndex < 0) return false;
					//修改:只修改已经存在的数据，若数据不存在(chunkIndex >= 0)不分配新索引
					if (!datas_[node.chunkIndex])
						datas_[node.chunkIndex] = std::move(data);
					else
						datas_[node.chunkIndex] = std::move(data);
					return true;
				}
			}

			const OctreeNode& node = nodes_[nodeIdx];
			if (node.chunkIndex >= 0) {
				datas_[node.chunkIndex] = std::move(data);
				return true;
			}
			return false;
		}

		//增:将该位置的深度向下增加一，填充所有的八个子节点数据，同时删除父节点数据(细分)
		//保证对于同一个位置，有一个确定的深度，只有且所有处于该深度的节点持有数据
		//数据本身可以为空，只作为占位表示，内部视为数据存在
		bool TrySubdivide(int x, int y, int z,std::array<std::unique_ptr<T>,8>&& datas) {
			if (!CheckCooBorder(x, y, z)) return false;
			int nodeIdx = 0;
			bool subdivide = false;
			for (int d = 0;d < maxDepth_;d++) {
				OctreeNode& node = nodes_[nodeIdx];
				if (node.childFirstIndex < 0)
					subdivide = true;
				int childIdx = GetChildIdxWithCoo(x, y, z, d);
				if (node.childMask & (1 << childIdx))
					nodeIdx = node.childFirstIndex + childIdx;
				else
					subdivide = true;

				if (subdivide) {
					//如果node的子节点不存在，就更新八个子节点
					EnsureOctNodes(nodeIdx);
					//为八个子节点附上数据
					for (int idx = 0; idx < 8;idx++) {
						OctreeNode& childNode =
							nodes_[nodes_[nodeIdx].childFirstIndex + idx];

						int dataIdx = GetFreeIndex();
						//如果有空闲索引:直接使用空闲索引
						if (dataIdx != -1) {
							datas_[dataIdx] = std::move(datas[idx]);
							ReleaseIndex(childNode.chunkIndex);
							childNode.chunkIndex = dataIdx;
						}//如果没有:分配一个新索引
						else {
							ReleaseIndex(childNode.chunkIndex);
							childNode.chunkIndex = datas_.size();
							datas_.push_back(std::move(datas[idx]));
						}
					}
					//更新标记位
					nodes_[nodeIdx].childMask = 255;
					//删除当前节点数据
					if (nodes_[nodeIdx].chunkIndex != -1) {
						//置空
						datas_[nodes_[nodeIdx].chunkIndex] = nullptr;
						//索引移入空闲队列
						freeIdxs_.push_back(nodes_[nodeIdx].chunkIndex);
						nodes_[nodeIdx].chunkIndex = -1;
					}

					return true;
				}
			}
			//此时已经是最大深度了，再往下细分没有意义，故直接返回false
			return false;
		}
		//删:将该位置的深度减小一，完全删除该位置所有的最底层叶子节点，并填充次级节点数据(合并)
		bool TryCombine(int x, int y, int z, std::unique_ptr<T>&& data) {
			if (!CheckCooBorder(x, y, z)) return false;
			int nodeIdx = 0;
			for (int d = 0;d < maxDepth_;d++) {
				OctreeNode& node = nodes_[nodeIdx];
				if (node.childFirstIndex < 0) break;
				int childIdx = GetChildIdxWithCoo(x, y, z,d);
				if (node.childMask & (1 << childIdx))
					nodeIdx = node.childFirstIndex + childIdx;
				else break;
			}

			OctreeNode& node = nodes_[nodeIdx];
			//找到父节点
			if (node.parentIndex == -1) return false;
			OctreeNode& parentNode = nodes_[node.parentIndex];

			//添加父节点数据
			int dataIdx = GetFreeIndex();
			//如果有空闲索引:直接使用空闲索引
			if (dataIdx != -1) {
				datas_[dataIdx] = std::move(data);
				ReleaseIndex(parentNode.chunkIndex);
				parentNode.chunkIndex = dataIdx;
			}//如果没有:分配一个新索引
			else {
				ReleaseIndex(parentNode.chunkIndex);
				parentNode.chunkIndex = datas_.size();
				datas_.push_back(std::move(data));
			}

			//删除八个子节点
			for (int idx = 0; idx < 8;idx++) {
				OctreeNode& childNode =
					nodes_[parentNode.childFirstIndex + idx];

				if (childNode.chunkIndex != -1) {
					//置空
					datas_[childNode.chunkIndex] = nullptr;
					//索引移入空闲队列
					freeIdxs_.push_back(childNode.chunkIndex);
					childNode.chunkIndex = -1;
				}
				//此时node是空节点
				//下一次细分网格时会自动复用，现在暂时不管
			}
			//更新标记位
			parentNode.childMask = 0;
			return true;
		}

		const std::vector<std::unique_ptr<T>>& Data() const {
			return datas_;
		}
	private:
		//用于实现八叉树索引查找的核心
		//仅维护八叉树索引，不存储实际数据
		std::vector<OctreeNode> nodes_;
		//这里才是数据实际的存储位置，nodes_中存储的索引指向该数组中的数据
		std::vector<std::unique_ptr<T>> datas_;
		//空闲索引，用于回收那些被释放的索引，避免数据数组中产生空洞
		std::vector<size_t> freeIdxs_;
		//最大深度，从0开始
		size_t maxDepth_;

		size_t rootSize_, leafSize_;

		//返回空闲索引
		int GetFreeIndex() {
			if (freeIdxs_.size()) {
				size_t idx = freeIdxs_.back();
				freeIdxs_.pop_back();
				return idx;
			}
			return -1;
		}


		bool CheckCooBorder(int x, int y, int z) const {
			return (x >= 0 && x < rootSize_) &&
				   (y >= 0 && y < rootSize_) &&
				   (z >= 0 && z < rootSize_);
		}

		int GetChildIdxWithCoo(int x, int y, int z,int depth)const {
			if (depth < 0 || depth > maxDepth_) return 0;
			int half = rootSize_ >> (depth + 1);
			int idx = 0;
			if (x & half) idx |= 1;
			if (y & half) idx |= 2;
			if (z & half) idx |= 4;
			return idx;
		}
		//确保八个子节点位置存在，如果不存在则创建
		void EnsureOctNodes(int nodeIndex) {
			if (nodes_[nodeIndex].childFirstIndex == -1) {
				int childIndex = (int)nodes_.size();
				int parentDepth = nodes_[nodeIndex].depth;

				for (int i = 0; i < 8; ++i) {
					OctreeNode childNode;
					childNode.parentIndex = nodeIndex;
					childNode.depth = parentDepth + 1;
					nodes_.push_back(childNode);
				}

				nodes_[nodeIndex].childFirstIndex = childIndex;
			}
		}

		void ReleaseIndex(int idx) {
			if (idx < 0) return;
			datas_[idx] = nullptr;          // 关键
			freeIdxs_.push_back(idx);
		}
	};
}