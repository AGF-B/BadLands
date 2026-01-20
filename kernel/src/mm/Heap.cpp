// SPDX-License-Identifier: GPL-3.0-only
//
// Copyright (C) 2026 Alexandre Boissiere
// This file is part of the BadLands operating system.
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, version 3.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>. 

#include <cstddef>
#include <cstdint>

#include <shared/Lock.hpp>
#include <shared/LockGuard.hpp>
#include <shared/memory/defs.hpp>

#include <mm/PhysicalMemory.hpp>
#include <mm/VirtualMemory.hpp>
#include <mm/Heap.hpp>
#include <mm/Utils.hpp>

namespace ShdMem = Shared::Memory;

namespace {
	static constexpr size_t ARENA_ALIGNMENT = 8;

	template<class T>
	static constexpr T HostedMax(const T& x, const T& y) {
		return x > y ? x : y;
	}

	struct AllocatedNode {
		uint64_t size;
	};

	class AVLHeap {
	public:
		AVLHeap();
		AVLHeap(uint8_t* arena, size_t size);

		void* Allocate(uint64_t size);
		void Free(void* ptr);

		struct Node;

	private:
		Node* arenaRoot = nullptr;
	};

	struct AVLHeap::Node{
	private:
		static uint64_t GetHeight(const Node* n) {
			return n == nullptr ? 0 : n->height;
		}

	public:
		uint64_t size;
		Node* left;
		Node* right;
		Node* parent;
		uint64_t height;

		void UpdateHeight() {
			height = HostedMax(GetHeight(left), GetHeight(right)) + 1;
		}

		int64_t GetBalanceFactor() const {
			return GetHeight(left) - GetHeight(right);
		}

		static Node* RotateLeft(Node* root) {
			Node* newRoot;

			if (root == nullptr || (newRoot = root->right) == nullptr) {
				return root;
			}

			root->right = newRoot->left;
			newRoot->left = root;

			if (root->right != nullptr) {
				root->right->parent = root;
			}

			Node* parent = root->parent;
			root->parent = newRoot;
			newRoot->parent = parent;

			if (parent != nullptr) {
				if (root < parent) {
					parent->left = newRoot;
				}
				else {
					parent->right = newRoot;
				}
			}

			root->UpdateHeight();
			newRoot->UpdateHeight();

			return newRoot;
		}

		static Node* RotateRight(Node* root) {
			Node* newRoot;

			if (root == nullptr || (newRoot = root->left) == nullptr) {
				return root;
			}

			root->left = newRoot->right;
			newRoot->right = root;

			if (root->left != nullptr) {
				root->left->parent = root;
			}

			Node* parent = root->parent;
			root->parent = newRoot;
			newRoot->parent = parent;

			if (parent != nullptr) {
				if (root < parent) {
					parent->left = newRoot;
				}
				else {
					parent->right = newRoot;
				}
			}

			root->UpdateHeight();
			newRoot->UpdateHeight();

			return newRoot;
		}

		static Node* RotateRightLeft(Node* root) {
			Node* newSubRoot = RotateRight(root->right);
			root->right = newSubRoot;
			return RotateLeft(root);
		}

		static Node* RotateLeftRight(Node* root) {
			Node* newSubRoot = RotateLeft(root->left);
			root->left = newSubRoot;
			return RotateRight(root);
		}
	};

	using Node = AVLHeap::Node;
	using Tree = Node;

	static Tree* Rebalance(Tree* root, Tree* ptr) {
		root->UpdateHeight();

		const int64_t balanceFactor = root->GetBalanceFactor();

		if (balanceFactor < -1 || balanceFactor > 1) {
			if (ptr > root && ptr > root->right) {
				root = Node::RotateLeft(root);
			}
			else if (ptr < root && ptr < root->left) {
				root = Node::RotateRight(root);
			}
			else if (ptr > root && ptr < root->right) {
				root = Node::RotateRightLeft(root);
			}
			else if (ptr < root && ptr > root->left) {
				root = Node::RotateLeftRight(root);
			}
		}

		return root;
	}

	static Tree* Insert(Tree* root, AllocatedNode* _n) {
		Node* n = (Node*)_n;

		if (root == nullptr) {
			*n = Node{
				.size = _n->size,
				.left = nullptr,
				.right = nullptr,
				.parent = nullptr,
				.height = 1
			};
			return n;
		}

		Node* prev = root;
		Node* curr = root;

		while (curr != nullptr) {
			prev = curr;

			if (n < curr) {
				curr = curr->left;
			}
			else if (n > curr) {
				curr = curr->right;
			}
			else {
				return root;
			}
		}

		*n = {
			.size = _n->size,
			.left = nullptr,
			.right = nullptr,
			.parent = prev,
			.height = 1
		};

		if (n < prev) {
			prev->left = n;
		}
		else {
			prev->right = n;
		}

		for (; prev != nullptr; curr = prev, prev = prev->parent) {
			prev = Rebalance(prev, n);
		}

		return curr;
	}

	static Tree* Delete(Tree* root, Node* node) {
		if (root == nullptr || node == nullptr) {
			return root;
		}

		Node* prev = node->parent;

		Node* left = node->left;
		Node* right = node->right;

		if (left == nullptr && right == nullptr) {
			if (prev == nullptr) {
				root = nullptr;
			}
			else if (node > prev) {
				prev->right = nullptr;
			}
			else {
				prev->left = nullptr;
			}
		}
		else if (right == nullptr) {
			left->parent = prev;

			if (prev == nullptr) {
				root = left;
			}
			else if (node > prev) {
				prev->right = left;
			}
			else {
				prev->left = left;
			}
		}
		else if (left == nullptr) {
			right->parent = prev;

			if (prev == nullptr) {
				root = right;
			}
			else if (node > prev) {
				prev->right = right;
			}
			else {
				prev->left = right;
			}
		}
		else {
			Node* ptr = right;
			Node* successor = ptr;

			while (ptr != nullptr) {
				successor = ptr;
				ptr = ptr->left;
			}

			Node* new_prev;

			if (successor->parent != node) {
				successor->parent->left = successor->right;

				if (successor->right != nullptr) {
					successor->right->parent = successor->parent;
				}

				new_prev = successor->parent;

				successor->right = right;
				right->parent = successor;
			}
			else {
				new_prev = successor;
			}

			successor->left = left;
			left->parent = successor;
			successor->parent = prev;

			successor->UpdateHeight();

			if (prev == nullptr) {
				root = successor;
			}
			else {
				if (successor < prev) {
					prev->left = successor;
				}
				else {
					prev->right = successor;
				}
			}

			prev = new_prev;
		}

		if (prev == nullptr) {
			if (root == nullptr) {
				return nullptr;
			}
			else {
				return Rebalance(root, node);
			}
		}
		else {
			Node* subroot = nullptr;

			for (; prev != nullptr; subroot = prev, prev = prev->parent) {
				prev->UpdateHeight();

				const int64_t balanceFactor = prev->GetBalanceFactor();

				if (balanceFactor > 1) {
					if (prev->left->GetBalanceFactor() >= 0) {
						prev = Node::RotateRight(prev);
					}
					else {
						prev = Node::RotateLeftRight(prev);
					}
				}
				else if (balanceFactor < -1) {
					if (prev->right->GetBalanceFactor() > 0) {
						prev = Node::RotateRightLeft(prev);
					}
					else {
						prev = Node::RotateLeft(prev);
					}
				}
			}

			return subroot;
		}
	}

	static Node* BestFind(Tree* root, uint64_t size) {
		if (root == nullptr) {
			return nullptr;
		}

		if (root->size == size) {
			return root;
		}

		Node* left = BestFind(root->left, size);

		if (left != nullptr && left->size == size) {
			return left;
		}
			
		Node* right = BestFind(root->right, size);

		Node* nodes[3] = { root, left, right };
		uint64_t sizes[3] = { root->size, left != nullptr ? left->size : 0, right != nullptr ? right->size : 0 };

		for (size_t i = 0; i < 3; ++i) {
			if (sizes[i] < size) {
				sizes[i] = 0;
				nodes[i] = nullptr;
			}
		}

		size_t best_fit = 0;

		for (size_t i = 0; i < 3; ++i) {
			if (sizes[i] > 0 && sizes[i] < sizes[best_fit] && sizes[i] >= size) {
				best_fit = i;
			}
		}

		return nodes[best_fit];
	}

	static AllocatedNode* ExtendArena(size_t size) {
		const size_t effective_size = 2 * size;
		const size_t allocated_pages = (effective_size + ShdMem::PAGE_SIZE - 1) / ShdMem::PAGE_SIZE;
		const size_t allocated_size = allocated_pages * ShdMem::PAGE_SIZE;

		void* ptr = VirtualMemory::AllocateKernelHeap(allocated_pages);

		if (ptr == nullptr) {
			return nullptr;
		}

		Utils::memset(ptr, 0, allocated_size);

		AllocatedNode* node = (AllocatedNode*)ptr;
		node->size = allocated_size;
		return node;
	}

	static void* Allocate(Tree*& root, uint64_t size) {
		size += sizeof(AllocatedNode);

		if (size < sizeof(Node)) {
			size = sizeof(Node);
		}

		uint64_t misalignment = size % ARENA_ALIGNMENT;

		if (misalignment != 0) {
			size += ARENA_ALIGNMENT - misalignment;
		}

		Node* node = BestFind(root, size);

		if (node == nullptr) {
			AllocatedNode* node = ExtendArena(size);

			if (node == nullptr) {
				return nullptr;
			}

			uint8_t* pointer = (uint8_t*)node + node->size - size;
			node->size -= size;
			((AllocatedNode*)pointer)->size = size;

			root = Insert(root, node);
			return pointer + sizeof(AllocatedNode);
		}

		if (node->size > size && node->size - size >= sizeof(Node)) {
			uint8_t* pointer = (uint8_t*)node + node->size - size;
			((AllocatedNode*)pointer)->size = size;

			node->size -= size;

			return pointer + sizeof(AllocatedNode);
		}
		else {
			root = Delete(root, node);

			((AllocatedNode*)node)->size = node->size;
			return ((uint8_t*)node) + sizeof(AllocatedNode);
		}
	}

	static void Free(Tree*& root, void* pointer) {
		AllocatedNode* allocatedNode = (AllocatedNode*)((uint8_t*)pointer - sizeof(AllocatedNode));

		root = Insert(root, allocatedNode);
	}

	AVLHeap::AVLHeap() {
		arenaRoot = nullptr;
	}

	AVLHeap::AVLHeap(uint8_t* arena, size_t size) {
		arenaRoot = (Node*)arena;
		*arenaRoot = Node{
			.size = size,
			.left = nullptr,
			.right = nullptr,
			.parent = nullptr,
			.height = 1
		};
	}

	void* AVLHeap::Allocate(uint64_t size) {
		return ::Allocate(arenaRoot, size);
	}

	void AVLHeap::Free(void* ptr) {
		return ::Free(arenaRoot, ptr);
	}

	static bool CreateAVLHeap(AVLHeap& heap) {
		const size_t allocated_pages = 16;
		const size_t allocated_memory = allocated_pages * ShdMem::PAGE_SIZE;

		void* pages = VirtualMemory::AllocateKernelHeap(allocated_pages);

		if (pages == nullptr) {
			return false;
		}

		Utils::memset(pages, 0, allocated_memory);

		heap = AVLHeap((uint8_t*)pages, allocated_memory);

		return true;
	}

	static Utils::Lock heapLock;
	static AVLHeap heap;
}

bool Heap::Create() {
	return CreateAVLHeap(heap);
}

void* Heap::Allocate(size_t size) {
	Utils::LockGuard _{heapLock};
	return heap.Allocate(size);
}

void Heap::Free(void* ptr) {
	Utils::LockGuard _{heapLock};
	return heap.Free(ptr);
}
