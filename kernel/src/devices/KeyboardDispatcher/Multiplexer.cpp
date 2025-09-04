#include <cstdint>

#include <shared/Lock.hpp>
#include <shared/LockGuard.hpp>

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
		
		size_t available_packets;
		uint8_t* buffer;
		size_t location;

		Utils::Lock lock;

	public:
		GenericKeyboardBuffer(uint8_t* buffer) : FS::File(&keyboard_owner), available_packets{0}, buffer {buffer}, location{0} {
			static_assert(BUFFER_SIZE % PACKET_SIZE == 0);
		}

		virtual FS::Response<size_t> Read([[maybe_unused]] size_t offset, size_t count, uint8_t* buffer) final {
			Utils::LockGuard _{lock};

			if (count % PACKET_SIZE != 0) {
				return FS::Response<size_t>(FS::Status::INVALID_PARAMETER);
			}

			size_t packets = count / PACKET_SIZE;

			if (available_packets == 0) {
				return FS::Response<size_t>(0);
			}

			size_t read_packets = packets > available_packets ? available_packets : packets;

			for (size_t i = 0; i < read_packets; ++i, location = (location + 1) % CAPACITY, buffer += PACKET_SIZE) {
				Utils::memcpy(buffer, this->buffer + location * PACKET_SIZE, PACKET_SIZE);
			}

			available_packets -= read_packets;

			return FS::Response(read_packets * PACKET_SIZE);
		}

		virtual FS::Response<size_t> Write([[maybe_unused]] size_t offset, size_t count, const uint8_t* buffer) final {
			Utils::LockGuard _{lock};

			if (count % PACKET_SIZE != 0) {
				return FS::Response<size_t>(FS::Status::INVALID_PARAMETER);
			}

			size_t remaining_space = CAPACITY - available_packets;

			if (remaining_space == 0) {
				return FS::Response<size_t>(0);
			}

			size_t packets = count / PACKET_SIZE;
			size_t written_packets = packets > remaining_space ? remaining_space : packets;

			for (size_t i = 0; i < written_packets; ++i, buffer += PACKET_SIZE) {
				Utils::memcpy(this->buffer + ((location + available_packets + i) % CAPACITY) * PACKET_SIZE, buffer, PACKET_SIZE);
			}

			available_packets += written_packets;

			return FS::Response(written_packets * PACKET_SIZE);
		}

		virtual void Destroy() final {
			Heap::Free(buffer);
		}
	};
}

namespace Devices::KeyboardDispatcher {
	FS::IFNode* Initialize(FS::IFNode* deviceInterface) {
		Log::puts("[GENKBD] Initializing generic keyboard multiplexer...\n\r");

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

		Log::puts("[GENKBD] Generic keyboard multiplexer created\n\r");

		return multiplexer;
	}
}
