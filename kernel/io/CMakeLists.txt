yak_add_sources(
    init.c
    console.c

    Array.cc
    object.cc
    Device.cc
    String.cc
    Dictionary.cc
    IoRegistry.cc
    test.cc

    acpi/AcpiDevice.cc
    acpi/AcpiPersonality.cc
    acpi/glue.c
    acpi/acpi.c
    acpi/event.c
    acpi/sleep.c

    pci/PciBus.cc
    pci/PciDevice.cc
    pci/PciPersonality.cc
    pci/pci.c
    pci/pci_ecam.c
)
