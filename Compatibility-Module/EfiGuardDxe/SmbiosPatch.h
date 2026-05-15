#pragma once

#include <Uefi.h>
#include <Protocol/Smbios.h>
#include <Protocol/ScootwareConfig.h>

// SMBIOS Entry Point Structure (32-bit)
#pragma pack(push, 1)
typedef struct {
    CHAR8   AnchorString[4];        // "_SM_"
    UINT8   EntryPointLength;
    UINT8   MajorVersion;
    UINT8   MinorVersion;
    UINT16  MaxStructureSize;
    UINT8   EntryPointRevision;
    UINT8   FormattedArea[5];
    CHAR8   IntermediateAnchorString[5]; // "_DMI_"
    UINT8   IntermediateChecksum;
    UINT16  StructureTableLength;
    UINT32  StructureTableAddress;
    UINT16  NumberOfStructures;
    UINT8   BCDRevision;
} SMBIOS_STRUCTURE_TABLE_CUSTOM;

typedef struct {
    UINT8   Type;
    UINT8   Length;
    UINT16  Handle;
} SMBIOS_HEADER_CUSTOM;

typedef struct {
    SMBIOS_HEADER_CUSTOM  Header;
    UINT8          Manufacturer;
    UINT8          ProductName;
    UINT8          Version;
    UINT8          SerialNumber;
    UINT8          UUID[16];
    UINT8          WakeUpType;
    UINT8          SKUNumber;
    UINT8          Family;
} SMBIOS_TYPE1_CUSTOM;

typedef struct {
    SMBIOS_HEADER_CUSTOM  Header;
    UINT8          Manufacturer;
    UINT8          ProductName;
    UINT8          Version;
    UINT8          SerialNumber;
    UINT8          AssetTag;
    UINT8          FeatureFlags;
    UINT8          LocationInChassis;
    UINT16         ChassisHandle;
    UINT8          BoardType;
} SMBIOS_TYPE2_CUSTOM;

typedef struct {
    SMBIOS_HEADER_CUSTOM  Header;
    UINT8          SocketDesignation;
    UINT8          ProcessorType;
    UINT8          ProcessorFamily;
    UINT8          ProcessorManufacturer;
    UINT64         ProcessorId;
    UINT8          ProcessorVersion;
    UINT8          Voltage;
    UINT16         ExternalClock;
    UINT16         MaxSpeed;
    UINT16         CurrentSpeed;
    UINT8          Status;
    UINT8          ProcessorUpgrade;
    UINT16         L1CacheHandle;
    UINT16         L2CacheHandle;
    UINT16         L3CacheHandle;
    UINT8          SerialNumber;
    UINT8          AssetTag;
} SMBIOS_TYPE4_CUSTOM;
#pragma pack(pop)

typedef union {
    UINT8*                Raw;
    SMBIOS_HEADER_CUSTOM* Hdr;
    SMBIOS_TYPE1_CUSTOM*  Type1;
    SMBIOS_TYPE2_CUSTOM*  Type2;
    SMBIOS_TYPE4_CUSTOM*  Type4;
} SMBIOS_STRUCTURE_POINTER_CUSTOM;

// Prototypes
SMBIOS_STRUCTURE_TABLE_CUSTOM* SmbiosFindEntry(VOID);

VOID SmbiosCaptureCurrent(IN OUT SCOOTWARE_EFI_CONFIG* Cfg);

VOID SmbiosApplySpoofs(IN CONST SCOOTWARE_EFI_CONFIG* Cfg);
