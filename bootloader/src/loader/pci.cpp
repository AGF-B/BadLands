#include <cstdint>

#include <efi/efi_misc.hpp>
#include <shared/efi/efi.h>

#include <ldstdio.hpp>
#include <ldstdlib.hpp>

#include <loader/system_config.hpp>
#include <loader/pci.hpp>

#include <acpi.hpp>

namespace {
    static constexpr uint8_t MCFG_Sig[4] = { 0x4D, 0x43, 0x46, 0x47 }; // "MCFG" in ASCII
}

EFI_PHYSICAL_ADDRESS Loader::LocatePCI(const EFI_SYSTEM_CONFIGURATION& sysconfig) {
    if (sysconfig.ACPI_20 == nullptr) {
        Loader::puts(u"LOADER PANIC: NO ACPI 2.0+ DATA FOUND\n\r");
        EFI::Terminate();
    }

    ACPI_RSDP* RSDP = static_cast<ACPI_RSDP*>(sysconfig.ACPI_20);
    ACPI_RSDT* RSDT = reinterpret_cast<ACPI_RSDT*>(static_cast<uint64_t>(RSDP->RsdtAddress));

    size_t entries_count = (RSDT->Header.Length -
        (reinterpret_cast<uint8_t*>(&RSDT->Entry) - reinterpret_cast<uint8_t*>(RSDT))
    ) / sizeof(uint32_t);

    ACPI_MCFG* MCFG = nullptr;

    for (size_t i = 0; i < entries_count; ++i) {
        uint32_t* raw_sdth_ptr = &RSDT->Entry + i;
        ACPI_SDTH* SDTH = reinterpret_cast<ACPI_SDTH*>(static_cast<uint64_t>(*raw_sdth_ptr));

        if (Loader::memcmp(
            SDTH->Signature,
            MCFG_Sig,
            4
        )) {
            MCFG = (ACPI_MCFG*)SDTH;
        }
    }

    if (MCFG == nullptr) {
        Loader::puts(u"LOADER PANIC: COULD NOT LOCATE PCI MCFG TABLE\n\r");
        EFI::Terminate();
    }

    size_t mcfg_entries = (MCFG->Header.Length -
        (reinterpret_cast<uint8_t*>(&MCFG->Entry) - reinterpret_cast<uint8_t*>(MCFG))
    ) / sizeof(PCI_CSBA);

    if (mcfg_entries == 0) {
        Loader::puts(u"LOADER PANIC: CORRUPTED/INVALID MCFG TABLE\n\r");
        EFI::Terminate();
    }
    else if (mcfg_entries > 1) {
        Loader::puts(u"LOADER WARNING: IGNORING ADDITIONAL ENTRIES IN MCFG TABLE\n\r");
    }

    for (size_t i = 0; i < mcfg_entries; ++i) {
        PCI_CSBA* csba_ptr = &MCFG->Entry + i;

        if (csba_ptr->StartBusNumber != 0) {
            Loader::puts(u"LOADER PANIC: CORRUPTED/INVALID MCFG TABLE (2)\n\r");
            EFI::Terminate();
        }
    }

    return reinterpret_cast<EFI_PHYSICAL_ADDRESS>(reinterpret_cast<PCI_CSBA*>(&MCFG->Entry)->BaseAddress);
}
