#pragma once

#include <cstdint>

#include <pci/Interface.hpp>

namespace PCI {
    struct MSIConfiguration {
        uint64_t address;
        uint16_t data;
        uint8_t implemented_vectors;
    };

    class MSI : public Capability {
    protected:
        static constexpr uint16_t ENABLE            = 0x0001;
        static constexpr uint16_t REQUESTED_VECTORS = 0x000E;
        static constexpr uint16_t ENABLED_VECTORS   = 0x0070;
        static constexpr uint16_t ADDRESS64         = 0x0080;

        static constexpr uint8_t REQUESTED_SHIFT = 1;
        static constexpr uint8_t ENABLED_SHIFT = 4;

        static constexpr uint8_t IMPL1  = 0x0;
        static constexpr uint8_t IMPL2  = 0x1;
        static constexpr uint8_t IMPL4  = 0x2;
        static constexpr uint8_t IMPL8  = 0x3;
        static constexpr uint8_t IMPL16 = 0x4;
        static constexpr uint8_t IMPL32 = 0x5;

        void ConfigureVectors(uint8_t implemented_vectors) const;
    
    public:
        mutable uint16_t MessageControl;

        bool IsEnabled() const;
        void Enable() const;
        void Disable() const;

        static void ConfigureMSI(const MSI* msi, const MSIConfiguration& config);
    };

    class MSIx32 : public MSI {
    public:
        mutable uint32_t MessageAddress;
        mutable uint16_t MessageData;

        void Configure(const MSIConfiguration& config) const;
    };

    class MSIx64 : public MSI {
    public:
        mutable uint32_t MessageAddress;
        mutable uint32_t MessageUpperAddress;
        mutable uint16_t MessageData;

        void Configure(const MSIConfiguration& config) const;
    };

    MSI* GetMSI(const Interface& interface);
}
