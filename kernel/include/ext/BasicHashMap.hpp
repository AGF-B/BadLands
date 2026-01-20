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

#pragma once

#include <cstddef>
#include <cstdint>

#include <new>

#include <ext/FNV1A.hpp>

#include <mm/Heap.hpp>

namespace Ext {
	template<class K, class V, uint32_t capacity = 0x100, uint32_t aggregate_ratio = 4>
	class BasicHashMap {
	private:
		struct Aggregate {
			static_assert(aggregate_ratio > 0);

			V values[aggregate_ratio];
			K keys[aggregate_ratio] = { 0 };
			uint32_t presence[(aggregate_ratio + 31) / 32] = { 0 };
			Aggregate* next = nullptr;

			V* access(const K& key) {
				for (size_t i = 0; i < aggregate_ratio; ++i) {
					if (presence[i / 32] == 0) {
						i += 31;
					}
					else if (((presence[i / 32] >> (i % 32)) & 1) != 0 && keys[i] == key) {
						return &values[i];
					}
				}

				return nullptr;
			}

			V* insert(const K& key, const V& value) {
				for (size_t i = 0; i < aggregate_ratio; ++i) {
					if (presence[i / 32] == 0xFFFFFFFF) {
						i += 31;
					}
					else if (((presence[i / 32] >> (i % 32)) & 1) == 0) {
						values[i] = value;
						keys[i] = key;
						presence[i / 32] |= (1 << (i % 32));
						return &values[i];
					}
				}

				return nullptr;
			}
		};

		Aggregate* entries[capacity] = { nullptr };
		size_t elements_count = 0;

	public:
		class Iterator {
			bool error;

			size_t ref;

			BasicHashMap* owner;

			uint32_t entry;
			Aggregate* agg;
			uint32_t i;

		public:
			explicit Iterator(size_t ref, BasicHashMap* owner) : error{false}, ref{ref}, owner{owner} { }
			explicit Iterator(size_t ref, BasicHashMap* owner, uint32_t entry, Aggregate* agg, uint32_t index)
					: ref{ref}, owner{owner}, entry{entry}, agg{agg}, i{index}
			{
				if (agg == nullptr || entry >= capacity) {
					error = true;
				}
				else {
					error = false;
				}
			}

			Iterator& operator++() {
				if (!error) {
					++i;

					while (entry < capacity) {
						while (agg != nullptr) {
							for (; i < aggregate_ratio; ++i) {
								if (agg->presence[i / 32] == 0) {
									i += 31;
								}
								else if (((agg->presence[i / 32] >> (i % 32)) & 1) != 0) {
									++ref;
									return *this;
								}
							}
							agg = agg->next;
							i = 0;
						}

						agg = ++entry < capacity ? owner->entries[entry] : nullptr;
					}

					error = true;
				}

				return *this;
			}

			bool operator ==(const Iterator& other) const {
				if (this->error || other.error) {
					return true;
				}

				return ref == other.ref && owner == other.owner;
			}

			bool operator !=(const Iterator& other) const {
				return !(*this == other);
			}

			V* operator *() const {
				if (error) {
					return nullptr;
				}

				return &agg->values[i];
			}
		};

		constexpr size_t size() const {
			return elements_count;
		}

		V* at(const K& key) const {
			uint32_t hash = FNV1A32(key) % capacity;

			Aggregate* agg = entries[hash];

			while (agg != nullptr) {
				V* ptr = agg->access(key);

				if (ptr != nullptr) {
					return ptr;
				}

				agg = agg->next;
			}

			return nullptr;
		}

		V* insert(const K& key, const V& value) {
			uint32_t hash = FNV1A32(key) % capacity;

			if (entries[hash] == nullptr) {
				void* mem = Heap::Allocate(sizeof(Aggregate));

				if (mem == nullptr) {
					return nullptr;
				}

				Aggregate* agg = new (mem) Aggregate;

				agg->values[0] = value;
				agg->keys[0] = key;
				agg->presence[0] = 1;

				entries[hash] = agg;

				elements_count++;

				return &agg->values[0];
			}
			else {
				V* ptr = at(key);

				if (ptr != nullptr) {
					ptr->~V();
					*ptr = value;
					return ptr;
				}
			}

			Aggregate* agg = entries[hash];
			Aggregate* prev = nullptr;

			while (agg != nullptr) {
				V* ptr = agg->insert(key, value);

				if (ptr != nullptr) {
					elements_count++;
					return ptr;
				}

				prev = agg;
				agg = agg->next;
			}

			void* mem = Heap::Allocate(sizeof(Aggregate));

			if (mem == nullptr) {
				return nullptr;
			}

			agg = new (mem) Aggregate;
			prev->next = agg;

			elements_count++;

			return agg->insert(key, value);
		}

		Iterator begin() {
			if (size() == 0) {
				return end();
			}

			uint32_t entry = 0;

			while (entry < capacity && entries[entry] == nullptr) { ++entry; }

			if (entry > capacity) {
				return end();
			}

			while (entry < capacity) {
				Aggregate* agg = entries[entry];

				while (agg != nullptr) {
					for (size_t i = 0; i < aggregate_ratio; ++i) {
						if (agg->presence[i / 32] == 0) {
							i += 31;
						}
						else if (((agg->presence[i / 32] >> (i % 32)) & 1) != 0) {
							return Iterator(0, this, entry, agg, i);
						}
					}

					agg = agg->next;
				}

				++entry;
			}

			return end();
		}

		Iterator end() {
			return Iterator(size(), this);
		}
	};
}
