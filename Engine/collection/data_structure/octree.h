#pragma once

#include <vector>
#include <cmath>
#include <optional>
#include <memory>

namespace nova_engine::collection::data_structure {
	struct OctreeNode {
		char childMask = 0;// 低八位表示子节点是否存在，1表示存在，0表示不存在
		int childFirstIndex = -1;// 子节点在节点数组中的起始索引
		int chunkIndex = -1;// 节点对应的块索引，没有则为-1
		int parentIndex = -1;// 父节点在节点数组中的索引，没有则为-1
		int depth = 0;// 节点的深度，根节点为0
	};

	template <typename T>
	class Octree {
	public:
		//要求:rootSize必须是2的幂次方，leafSize必须是2的幂次方，且rootSize必须大于等于leafSize
		Octree(int rootSize,int leafSize) : m_rootSize(rootSize), m_leafSize(leafSize) {
			auto isPowerOfTwo = [](int v) { return v > 0 && (v & (v - 1)) == 0; };
			if (!isPowerOfTwo(rootSize) || !isPowerOfTwo(leafSize) || rootSize < leafSize) {
				throw std::invalid_argument("rootSize and leafSize must be powers of two and rootSize >= leafSize");
			}
			// 改用移位计算深度，避免浮点误差
			int depth = 0;
			int temp = rootSize;
			while (temp > leafSize) {
				temp >>= 1;
				++depth;
			}
			m_maxDepth = depth;

			m_nodes.reserve(2048); // 预估节点数量，避免频繁扩容
			m_chunks.reserve(512); // 预估块数量，避免频繁扩容

			OctreeNode rootNode;
			m_nodes.push_back(rootNode);
		}
		~Octree() = default;

		Octree(const Octree&) = delete;
		Octree& operator=(const Octree&) = delete;

		int GetChildIndexWithPos(int x,int y,int z,int depth) const {
			if (depth < 0 || depth > m_maxDepth) return -1;
			assert(depth >= 0 && depth < m_maxDepth);
			int half = m_rootSize >> (depth + 1);
			int index = 0;
			if ((half & x) != 0) index |= 1;
			if ((half & y) != 0) index |= 2;
			if ((half & z) != 0) index |= 4;
			
			return index;
		}

		// 查找指定坐标的块，如果不存在则返回nullptr
		T* Find(int x,int y,int z) const {
			// 检查坐标是否在八叉树范围内
			if (x < 0 || y < 0 || z < 0 || x >= m_rootSize || y >= m_rootSize || z >= m_rootSize) {
				return nullptr;
			}
			//往下递归八叉树，找到坐标对应的叶子节点，如果中间断掉或叶子节点没有对应的块，则返回nullptr
			int nodeIndex = 0;
			for (int d = 0;d < m_maxDepth; ++d) {
				const OctreeNode& node = m_nodes[nodeIndex];
				if (node.childFirstIndex < 0) return nullptr;
				int childIndex = GetChildIndexWithPos(x, y, z, node.depth);
				
				if (node.childMask & (1 << childIndex))
					nodeIndex = node.childFirstIndex + childIndex;
				else return nullptr;
			}
			//此时nodeIndex已经指向叶子节点，返回对应的块
			const OctreeNode& leafNode = m_nodes[nodeIndex];
			if (leafNode.chunkIndex < 0) return nullptr;
			else return m_chunks[leafNode.chunkIndex].get();
		}

		void Add(int x,int y,int z, T&& chunk) {
			// 检查坐标是否在八叉树范围内
			if (x < 0 || y < 0 || z < 0 || x >= m_rootSize || y >= m_rootSize || z >= m_rootSize) {
				return;
			}
			//逐渐往下递归八叉树，直到到达叶子节点
			int nodeIndex = 0;
			for (int d = 0; d < m_maxDepth; ++d) {
				//if ( nodeIndex >= m_nodes.size()) return;
				nodeIndex = AddChildNode(nodeIndex, x, y, z);
			}
			//此时nodeIndex已经指向叶子节点，将chunk存入
			OctreeNode& leafNode = m_nodes[nodeIndex];
			if (leafNode.chunkIndex == -1) {
				leafNode.chunkIndex = (int)m_chunks.size();
				m_chunks.push_back(std::make_unique<T>(std::move(chunk)));
			} else {
				auto newPtr = std::make_unique<T>(std::move(chunk));
				m_chunks[leafNode.chunkIndex] = std::move(newPtr);
			}
		}

		std::optional<T> Remove(int x,int y,int z) {
			if (x < 0 || y < 0 || z < 0 || x >= m_rootSize || y >= m_rootSize || z >= m_rootSize) {
				return std::nullopt;
			}
			//往下递归八叉树，找到坐标对应的叶子节点，如果中间断掉或叶子节点没有对应的块，则返回null
			int nodeIndex = 0;
			for (int d = 0;d < m_maxDepth; ++d) {
				//if (nodeIndex < 0 || nodeIndex >= m_nodes.size()) return std::nullopt;
				OctreeNode& node = m_nodes[nodeIndex];
				if (node.childFirstIndex == -1) return std::nullopt;
				int childIndex = GetChildIndexWithPos(x, y, z, node.depth);

				if (node.childMask & (1 << childIndex))
					nodeIndex = node.childFirstIndex + childIndex;
				else return std::nullopt;
			}
			//此时nodeIndex已经指向叶子节点，删除对应的块
			OctreeNode& leafNode = m_nodes[nodeIndex];
			if (leafNode.chunkIndex == -1) return std::nullopt;

			std::unique_ptr<T> ptr = std::move(m_chunks[leafNode.chunkIndex]);
			T value = std::move(*ptr);   // 移动数据

			leafNode.chunkIndex = -1;
			//往上递归八叉树,如果父节点没有子节点了,则移除父节点Mask
			PruneEmptyAncestors(nodeIndex);
			return value;
		}

	private:
		std::vector<OctreeNode> m_nodes;
		std::vector<std::unique_ptr<T>> m_chunks;

		int m_maxDepth = 0;
		int m_rootSize;
		int m_leafSize;

		int AddChildNode(int nodeIndex,int x,int y,int z) {
			EnsureOctNodes(nodeIndex);
			OctreeNode& node = m_nodes[nodeIndex];
			int childIndex = GetChildIndexWithPos(x, y, z, node.depth);
			if (!(node.childMask & (1 << childIndex))) {
				node.childMask |= (1 << childIndex);
			}
			return node.childFirstIndex + childIndex;
		}

		//确保八个子节点位置存在，如果不存在则创建
		void EnsureOctNodes(int nodeIndex) {
			if (m_nodes[nodeIndex].childFirstIndex == -1) {
				int childIndex = (int)m_nodes.size();
				int parentDepth = m_nodes[nodeIndex].depth;

				for (int i = 0; i < 8; ++i) {
					OctreeNode childNode;
					childNode.parentIndex = nodeIndex;
					childNode.depth = parentDepth + 1;
					m_nodes.push_back(childNode);
				}

				m_nodes[nodeIndex].childFirstIndex = childIndex;
			}
		}

		void PruneEmptyAncestors(int nodeIndex) {
			int cur = nodeIndex;
			while (cur != 0) {  // 根节点无父节点，索引固定为 0
				OctreeNode& node = m_nodes[cur];
				// 如果当前节点非空（有数据或仍有子节点），停止清理
				if (node.chunkIndex != -1 || node.childMask != 0) {
					break;
				}
				int parentIdx = node.parentIndex;
				if (parentIdx == -1) break;  // 防御性检查（根节点已在循环条件排除）

				OctreeNode& parent = m_nodes[parentIdx];
				int childIdx = cur - parent.childFirstIndex;  // 0~7
				parent.childMask &= ~(1 << childIdx);

				cur = parentIdx;  // 继续向上
			}
		}
	};
}