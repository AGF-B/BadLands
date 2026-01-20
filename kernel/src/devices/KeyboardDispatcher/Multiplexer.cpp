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

#include <cstdint>

#include <new>

#include <shared/Lock.hpp>
#include <shared/LockGuard.hpp>
#include <shared/SimpleAtomic.hpp>

#include <devices/KeyboardDispatcher/Keypacket.hpp>
#include <devices/KeyboardDispatcher/Multiplexer.hpp>

#include <fs/IFNode.hpp>
#include <fs/VFS.hpp>

#include <interrupts/Panic.hpp>

#include <mm/Heap.hpp>
#include <mm/Utils.hpp>

#include <screen/Log.hpp>

namespace {
	static class KeyboardOwner : public FS::Owner {} keyboard_owner;

	template<size_t BUFFER_SIZE> class GenericKeyboardBuffer final : public FS::File {
	private:
		using BasicKeyPacket = Devices::KeyboardDispatcher::BasicKeyPacket;

		static constexpr size_t PACKET_SIZE = sizeof(BasicKeyPacket);
		static constexpr size_t CAPACITY = BUFFER_SIZE / PACKET_SIZE;
		
		uint8_t* buffer;
		size_t location;

		Utils::Lock write_lock;
		Utils::Lock read_lock;
		Utils::SimpleAtomic<size_t> available_packets_count;

	public:
		GenericKeyboardBuffer(uint8_t* buffer) : FS::File(&keyboard_owner), buffer {buffer}, location{0}, available_packets_count{0} {
			static_assert(BUFFER_SIZE % PACKET_SIZE == 0);
		}

		virtual FS::Response<size_t> Read([[maybe_unused]] size_t offset, size_t count, uint8_t* buffer) final {
			Utils::LockGuard _{read_lock};
			
			const size_t available_packets = available_packets_count;

			if (available_packets == 0) {
				return FS::Response<size_t>(0);
			}

			if (count % PACKET_SIZE != 0) {
				return FS::Response<size_t>(FS::Status::INVALID_PARAMETER);
			}

			const size_t packets = count / PACKET_SIZE;
			const size_t read_packets = packets > available_packets ? available_packets : packets;

			for (size_t i = 0; i < read_packets; ++i, location = (location + 1) % CAPACITY, buffer += PACKET_SIZE) {
				Utils::memcpy(buffer, this->buffer + location * PACKET_SIZE, PACKET_SIZE);
			}

			available_packets_count -= read_packets;

			return FS::Response(read_packets * PACKET_SIZE);
		}

		virtual FS::Response<size_t> Write([[maybe_unused]] size_t offset, size_t count, const uint8_t* buffer) final {
			Utils::LockGuard _{write_lock};

			if (count % PACKET_SIZE != 0) {
				return FS::Response<size_t>(FS::Status::INVALID_PARAMETER);
			}

			const size_t available_packets = available_packets_count;
			const size_t remaining_space = CAPACITY - available_packets;

			if (remaining_space == 0) {
				return FS::Response<size_t>(0);
			}

			const size_t packets = count / PACKET_SIZE;
			const size_t written_packets = packets > remaining_space ? remaining_space : packets;			
			available_packets_count += written_packets;

			for (size_t i = 0; i < written_packets; ++i, buffer += PACKET_SIZE) {
				Utils::memcpy(this->buffer + ((location + available_packets + i) % CAPACITY) * PACKET_SIZE, buffer, PACKET_SIZE);
			}

			return FS::Response(written_packets * PACKET_SIZE);
		}

		virtual void Destroy() final {
			Heap::Free(buffer);
		}
	};
}

namespace Devices::KeyboardDispatcher {
	FS::IFNode* Initialize(FS::IFNode* deviceInterface) {
		Log::putsSafe("[GENKBD] Initializing generic keyboard multiplexer...\n\r");

		static constexpr size_t bufferSize = 0x800 * sizeof(BasicKeyPacket);

		using MultiplexerInterface = GenericKeyboardBuffer<bufferSize>;

		void* buffer = Heap::Allocate(bufferSize);

		if (buffer == nullptr) {
			Panic::PanicShutdown("(GENKBD) COULD NOT ALLOCATE A SUITABLE BUFFER FOR THE KEYBOARD MULTIPLEXER\n\r");
		}

		void* mem = Heap::Allocate(sizeof(MultiplexerInterface));

		if (mem == nullptr) {
			Panic::PanicShutdown("(GENKBD) COULD NOT ALLOCATE MEMMORY TO CREATE THE KEYBOARD MULTIPLEXER INTERFACE\n\r");
		}

		MultiplexerInterface* multiplexer = new (mem) MultiplexerInterface(static_cast<uint8_t*>(buffer));

		static constexpr const char nameReference[] = "keyboard";
		static constexpr FS::DirectoryEntry multiplexerEntry = { .NameLength = sizeof(nameReference) - 1, .Name = nameReference };

		auto status = deviceInterface->AddNode(multiplexerEntry, multiplexer);

		if (status != FS::Status::SUCCESS) {
			Panic::PanicShutdown("(GENKBD) COULD NOT ADD KEYBOARD MULTIPLEXER TO VFS\n\r");
		}

		Log::putsSafe("[GENKBD] Generic keyboard multiplexer created\n\r");

		return multiplexer;
	}
}
