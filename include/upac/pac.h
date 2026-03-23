#pragma once
//
// upac/pac.h — PAC firmware container format structures
//
// These structures directly mirror the on-disk binary layout of Spreadtrum/Unisoc
// .pac firmware files. They MUST be packed and use char16_t for the wide-string
// fields (the original tool uses Windows _TCHAR = wchar_t = 2 bytes).
//
// Reference: SPD Flash Tool Source — BinPack.h
//

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace upac {

// ============================================================================
// Constants
// ============================================================================

constexpr uint32_t PAC_MAGIC  = 0xFFFAFFFA;
constexpr uint32_t ZIP_MAGIC  = 0x04034B50;
constexpr int      MAX_BLOCKS = 5;

// ============================================================================
// On-disk structures — packed, matching Windows sizeof exactly
// ============================================================================

#pragma pack(push, 1)

/// PAC file header — 2124 bytes on disk
/// Lives at offset 0. Magic is at the END (last 4 bytes before CRCs).
struct PacHeader {
    char16_t version[24];          // Packet struct version string
    uint32_t size;                 // Entire PAC file size (bytes)
    char16_t product_name[256];    // Product name
    char16_t product_version[256]; // Product version
    int32_t  file_count;           // Number of FILE_T entries
    uint32_t file_offset;          // Offset to FILE_T array from start
    uint32_t mode;                 // Flash mode
    uint32_t flash_type;           // Flash type (NOR/NAND/eMMC)
    uint32_t nand_strategy;        // NAND bad-block strategy
    uint32_t nv_backup;            // NV backup flag (1=yes)
    uint32_t nand_page_type;       // NAND page size type
    char16_t product_alias[100];   // Product alias
    uint32_t oma_dm_product_flag;
    uint32_t is_oma_dm;
    uint32_t is_preload;
    uint32_t reserved[200];        // Reserved for future use
    uint32_t magic;                // Must be PAC_MAGIC (0xFFFAFFFA)
    uint16_t crc1;                 // CRC of header
    uint16_t crc2;                 // CRC of entire file
};

/// File entry in the PAC — 2580 bytes on disk
struct PacFileEntry {
    uint32_t struct_size;          // Size of this struct itself
    char16_t file_id[256];         // File ID: "FDL", "FDL2", "NV", etc.
    char16_t file_name[256];       // Original filename
    char16_t file_version[256];    // Reserved
    uint32_t file_size;            // Actual file data size (bytes)
    int32_t  file_flag;            // 0=operation only, 1=has file data
    uint32_t check_flag;           // 1=must download, 0=optional
    uint32_t data_offset;          // Offset from PAC start to file data
    uint32_t omit_flag;            // 1=can be omitted in "All files" mode
    uint32_t addr_count;           // Number of address entries used
    uint32_t addr[5];              // Download base addresses
    uint32_t reserved[249];        // Reserved
};

#pragma pack(pop)

// Compile-time size checks — these MUST match the Windows tool
static_assert(sizeof(PacHeader)    == 2124, "PacHeader size mismatch — binary layout is wrong");
static_assert(sizeof(PacFileEntry) == 2580, "PacFileEntry size mismatch — binary layout is wrong");

// ============================================================================
// Helper types (non-packed, for in-memory use)
// ============================================================================

/// Human-friendly file info extracted from PacFileEntry
struct FileInfo {
    size_t      index;          // Index in the PAC
    std::string id;             // File ID (UTF-8)
    std::string name;           // Filename (UTF-8)
    std::string version;        // Version string (UTF-8)
    uint32_t    size;           // File data size
    uint32_t    data_offset;    // Offset in PAC
    int32_t     file_flag;      // 0=op only, 1=has data
    uint32_t    check_flag;     // 1=required, 0=optional
    uint32_t    omit_flag;      // 1=omittable
    uint32_t    addr_count;
    uint32_t    addr[5];
};

/// Product info extracted from PacHeader
struct ProductInfo {
    std::string version_str;    // PAC struct version
    std::string name;           // Product name
    std::string version;        // Product version
    std::string alias;          // Product alias
    uint32_t    pac_size;       // Total PAC file size
    int32_t     file_count;
    uint32_t    mode;
    uint32_t    flash_type;
    uint32_t    nand_strategy;
    uint32_t    nv_backup;
    uint32_t    nand_page_type;
};

// ============================================================================
// Utility functions
// ============================================================================

/// CRC-16 (poly 0x8005) — compatible with the SPD Flash Tool's crc16.c
uint16_t crc16(uint16_t crc, const uint8_t* data, size_t len);

/// Convert a fixed-size char16_t array to UTF-8 std::string.
/// Stops at the first null char16_t or at max_len elements.
std::string from_utf16(const char16_t* str, size_t max_len);

/// Human-readable flash type string
const char* flash_type_str(uint32_t type);

/// Human-readable flash mode string
const char* flash_mode_str(uint32_t mode);

} // namespace upac
