#include <cstdint>

#include <pci/MSI.hpp>

namespace PCI {
    bool MSI::IsEnabled() const {
        return MessageControl & ENABLE;
    }

    void MSI::Enable() const {
        MessageControl |= ENABLE;
    }

    void MSI::Disable() const {
        MessageControl &= ~ENABLE;
    }

    void MSI::ConfigureVectors(uint8_t implemented_vectors) const {
        if (implemented_vectors == 0) {
            Disable();
        }
        else {
            const uint8_t requested = (MessageControl & ENABLED_VECTORS) >> REQUESTED_SHIFT;

            if (implemented_vectors > requested) {
                implemented_vectors = requested;
            }

            uint8_t implemented = IMPL1;

            if (implemented_vectors < 2) {
                implemented = IMPL1;
            }
            else if (implemented_vectors < 4) {
                implemented = IMPL2;
            }
            else if (implemented_vectors < 8) {
                implemented = IMPL4;
            }
            else if (implemented_vectors < 16) {
                implemented = IMPL8;
            }
            else if (implemented_vectors < 32) {
                implemented = IMPL16;
            }
            else {
                implemented = IMPL32;
            }

            implemented = (implemented << ENABLED_SHIFT) & ENABLED_VECTORS;

            MessageControl = (MessageControl & ~ENABLED_VECTORS) | implemented;
        }
    }

    void MSI::ConfigureMSI(const MSI* msi, const MSIConfiguration& config) {
        if (msi->MessageControl & ADDRESS64) {
            reinterpret_cast<const MSIx64*>(msi)->Configure(config);
        }
        else {
            reinterpret_cast<const MSIx32*>(msi)->Configure(config);
        }
    }

    void MSIx32::Configure(const MSIConfiguration& config) const {
        if (config.implemented_vectors > 0) {
            bool enabled = IsEnabled();

            if (enabled) {
                Disable();
            }
            
            MessageAddress = static_cast<uint32_t>(config.address);
            MessageData = config.data;

            ConfigureVectors(config.implemented_vectors);

            if (enabled) {
                Enable();
            }
        }
        else {
            Disable();
        }
    }

    void MSIx64::Configure(const MSIConfiguration& config) const {
        if (config.implemented_vectors > 0) {
            bool enabled = IsEnabled();

            if (enabled) {
                Disable();
            }

            MessageAddress = static_cast<uint32_t>(config.address);
            MessageUpperAddress = static_cast<uint32_t>(config.address >> 32);
            MessageData = config.data;

            ConfigureVectors(config.implemented_vectors);

            if (enabled) {
                Enable();
            }
        }
        else {
            Disable();
        }
    }

    MSI* GetMSI(const Interface& interface) {
        static constexpr uint8_t MSI_CAPABILITY_ID = 5;
        return reinterpret_cast<MSI*>(interface.FindCapability(MSI_CAPABILITY_ID));
    }
}
