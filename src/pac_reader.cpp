#include "upac/pac_reader.h"

#include <algorithm>
#include <cstring>
#include <iomanip>

namespace upac {

// ============================================================================
// Open + Parse
// ============================================================================

std::optional<PacReader> PacReader::open(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "error: cannot open file: " << path << "\n";
        return std::nullopt;
    }

    PacReader reader;
    reader.path_ = path;
    reader.actual_file_size_ = static_cast<uint64_t>(file.tellg());
    file.seekg(0);

    // --- Read header ---
    if (reader.actual_file_size_ < sizeof(PacHeader)) {
        std::cerr << "error: file too small to contain PAC header ("
                  << reader.actual_file_size_ << " bytes)\n";
        return std::nullopt;
    }

    file.read(reinterpret_cast<char*>(&reader.header_), sizeof(PacHeader));
    if (!file) {
        std::cerr << "error: failed to read PAC header\n";
        return std::nullopt;
    }

    // --- Validate magic ---
    if (reader.header_.magic != PAC_MAGIC) {
        std::cerr << "error: invalid PAC magic (expected 0x"
                  << std::hex << PAC_MAGIC << ", got 0x"
                  << reader.header_.magic << std::dec << ")\n";
        return std::nullopt;
    }

    // --- Read file entries ---
    int32_t count = reader.header_.file_count;
    if (count < 0 || count > 1000) {
        std::cerr << "error: suspicious file count: " << count << "\n";
        return std::nullopt;
    }

    if (reader.header_.file_offset == 0 || count == 0) {
        // No files is technically valid but unusual
        return reader;
    }

    file.seekg(reader.header_.file_offset);
    reader.raw_files_.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        file.read(reinterpret_cast<char*>(&reader.raw_files_[i]),
                  sizeof(PacFileEntry));
        if (!file) {
            std::cerr << "error: failed to read file entry " << i << "\n";
            return std::nullopt;
        }
    }

    // --- Locate and read XML config ---
    // The XML config is stored after the FILE_T array and before the first
    // file's data. We find it by looking at the gap between end-of-FILE_T
    // array and the earliest data_offset.
    uint64_t entries_end = static_cast<uint64_t>(reader.header_.file_offset)
                         + static_cast<uint64_t>(count) * sizeof(PacFileEntry);

    // Find the earliest data offset among all files
    uint64_t earliest_data = reader.actual_file_size_;
    for (const auto& fe : reader.raw_files_) {
        if (fe.file_flag != 0 && fe.data_offset != 0 && fe.file_size > 0) {
            earliest_data = std::min(earliest_data,
                                     static_cast<uint64_t>(fe.data_offset));
        }
    }

    if (earliest_data > entries_end) {
        uint64_t xml_size = earliest_data - entries_end;
        if (xml_size > 0 && xml_size < 10 * 1024 * 1024) { // sanity: <10MB
            reader.xml_data_.resize(static_cast<size_t>(xml_size));
            file.seekg(static_cast<std::streamoff>(entries_end));
            file.read(reinterpret_cast<char*>(reader.xml_data_.data()),
                      static_cast<std::streamsize>(xml_size));
        }
    }

    return reader;
}

// ============================================================================
// Accessors
// ============================================================================

ProductInfo PacReader::product_info() const {
    ProductInfo pi;
    pi.version_str   = from_utf16(header_.version, 24);
    pi.name          = from_utf16(header_.product_name, 256);
    pi.version       = from_utf16(header_.product_version, 256);
    pi.alias         = from_utf16(header_.product_alias, 100);
    pi.pac_size      = header_.size;
    pi.file_count    = header_.file_count;
    pi.mode          = header_.mode;
    pi.flash_type    = header_.flash_type;
    pi.nand_strategy = header_.nand_strategy;
    pi.nv_backup     = header_.nv_backup;
    pi.nand_page_type = header_.nand_page_type;
    return pi;
}

std::vector<FileInfo> PacReader::file_infos() const {
    std::vector<FileInfo> infos;
    infos.reserve(raw_files_.size());
    for (size_t i = 0; i < raw_files_.size(); ++i) {
        const auto& fe = raw_files_[i];
        FileInfo fi;
        fi.index       = i;
        fi.id          = from_utf16(fe.file_id, 256);
        fi.name        = from_utf16(fe.file_name, 256);
        fi.version     = from_utf16(fe.file_version, 256);
        fi.size        = fe.file_size;
        fi.data_offset = fe.data_offset;
        fi.file_flag   = fe.file_flag;
        fi.check_flag  = fe.check_flag;
        fi.omit_flag   = fe.omit_flag;
        fi.addr_count  = fe.addr_count;
        std::memcpy(fi.addr, fe.addr, sizeof(fi.addr));
        infos.push_back(std::move(fi));
    }
    return infos;
}

std::string PacReader::xml_config() const {
    if (xml_data_.empty()) return {};

    // The XML data in PAC is UTF-16LE encoded. Convert to UTF-8.
    // Check for BOM (0xFFFE or 0xFEFF) to detect encoding.
    const uint8_t* data = xml_data_.data();
    size_t len = xml_data_.size();

    // Check for UTF-16LE BOM
    if (len >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        // UTF-16LE with BOM — convert
        const char16_t* u16 = reinterpret_cast<const char16_t*>(data + 2);
        size_t u16_len = (len - 2) / 2;
        return from_utf16(u16, u16_len);
    }

    // Check for UTF-16LE without BOM (common in PAC files):
    // If second byte is 0x00 and first byte is '<', likely UTF-16LE
    if (len >= 2 && data[0] == '<' && data[1] == 0x00) {
        const char16_t* u16 = reinterpret_cast<const char16_t*>(data);
        size_t u16_len = len / 2;
        return from_utf16(u16, u16_len);
    }

    // Otherwise assume UTF-8 / ASCII
    // Strip any trailing nulls
    while (len > 0 && data[len - 1] == 0) --len;
    return std::string(reinterpret_cast<const char*>(data), len);
}

// ============================================================================
// Extraction
// ============================================================================

bool PacReader::extract(size_t index,
                        const std::filesystem::path& out_dir) const {
    if (index >= raw_files_.size()) {
        std::cerr << "error: file index " << index << " out of range\n";
        return false;
    }

    const auto& fe = raw_files_[index];
    std::string name = from_utf16(fe.file_name, 256);
    std::string id   = from_utf16(fe.file_id, 256);

    if (fe.file_flag == 0 || fe.file_size == 0) {
        // Operation-only entry (no file data)
        std::cout << "  [skip] " << id << " — operation only, no file data\n";
        return true;
    }

    // Use file_name if available, fallback to file_id
    std::string out_name = name.empty() ? id : name;
    // Strip any directory components from the stored filename
    auto slash_pos = out_name.find_last_of("/\\");
    if (slash_pos != std::string::npos) {
        out_name = out_name.substr(slash_pos + 1);
    }

    std::filesystem::create_directories(out_dir);
    auto out_path = out_dir / out_name;

    // Read from PAC and write to output
    std::ifstream pac(path_, std::ios::binary);
    if (!pac) {
        std::cerr << "error: cannot reopen PAC file\n";
        return false;
    }

    pac.seekg(fe.data_offset);
    std::ofstream out(out_path, std::ios::binary);
    if (!out) {
        std::cerr << "error: cannot create output file: " << out_path << "\n";
        return false;
    }

    // Stream in chunks (handle large files)
    constexpr size_t CHUNK = 1024 * 1024; // 1MB
    uint32_t remaining = fe.file_size;
    std::vector<char> buf(CHUNK);

    while (remaining > 0) {
        size_t to_read = std::min(static_cast<size_t>(remaining), CHUNK);
        pac.read(buf.data(), static_cast<std::streamsize>(to_read));
        auto actually_read = pac.gcount();
        if (actually_read <= 0) {
            std::cerr << "error: read failed for " << id << "\n";
            return false;
        }
        out.write(buf.data(), actually_read);
        remaining -= static_cast<uint32_t>(actually_read);
    }

    std::cout << "  [ok] " << id << " → " << out_path.string()
              << " (" << fe.file_size << " bytes)\n";
    return true;
}

bool PacReader::extract_all(const std::filesystem::path& out_dir) const {
    bool all_ok = true;
    for (size_t i = 0; i < raw_files_.size(); ++i) {
        if (!extract(i, out_dir)) {
            all_ok = false;
        }
    }
    return all_ok;
}

// ============================================================================
// Pretty printing
// ============================================================================

static std::string format_size(uint64_t bytes) {
    if (bytes >= 1024ULL * 1024 * 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f GB",
                 static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
        return buf;
    } else if (bytes >= 1024 * 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f MB",
                 static_cast<double>(bytes) / (1024.0 * 1024.0));
        return buf;
    } else if (bytes >= 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f KB",
                 static_cast<double>(bytes) / 1024.0);
        return buf;
    } else {
        return std::to_string(bytes) + " B";
    }
}

void PacReader::print_info(std::ostream& os) const {
    auto pi = product_info();

    os << "PAC File Info\n";
    os << "-------------------------------------\n";
    os << "  File:           " << path_.filename().string() << "\n";
    os << "  Actual size:    " << format_size(actual_file_size_) << "\n";
    os << "  Header size:    " << format_size(pi.pac_size) << "\n";
    os << "  Version:        " << pi.version_str << "\n";
    os << "  Product:        " << pi.name << "\n";
    os << "  Product ver:    " << pi.version << "\n";
    if (!pi.alias.empty()) {
        os << "  Alias:          " << pi.alias << "\n";
    }
    os << "  File count:     " << pi.file_count << "\n";
    os << "  Flash type:     " << flash_type_str(pi.flash_type)
       << " (" << pi.flash_type << ")\n";
    os << "  Flash mode:     " << flash_mode_str(pi.mode)
       << " (" << pi.mode << ")\n";
    os << "  NAND strategy:  " << pi.nand_strategy << "\n";
    os << "  NAND page type: " << pi.nand_page_type << "\n";
    os << "  NV backup:      " << (pi.nv_backup ? "Yes" : "No") << "\n";
    os << "  CRC1 (header):  0x" << std::hex << std::setw(4)
       << std::setfill('0') << header_.crc1 << std::dec << "\n";
    os << "  CRC2 (file):    0x" << std::hex << std::setw(4)
       << std::setfill('0') << header_.crc2 << std::dec << "\n";
    os << "  Magic:          0x" << std::hex << std::setw(8)
       << std::setfill('0') << header_.magic << std::dec << "\n";
    if (!xml_data_.empty()) {
        os << "  XML config:     " << format_size(xml_data_.size()) << "\n";
    }
}

void PacReader::print_file_list(std::ostream& os) const {
    auto infos = file_infos();

    os << "\n";
    // Header
    os << std::left
       << std::setw(4)  << "#"
       << std::setw(20) << "ID"
       << std::setw(32) << "Filename"
       << std::right
       << std::setw(12) << "Size"
       << std::setw(12) << "Offset"
       << "  Flags\n";
    os << std::string(84, '-') << "\n";

    for (const auto& fi : infos) {
        // Truncate long filenames for display
        std::string display_name = fi.name;
        if (display_name.length() > 30) {
            display_name = "..." + display_name.substr(display_name.length() - 27);
        }

        os << std::left
           << std::setw(4)  << fi.index
           << std::setw(20) << fi.id
           << std::setw(32) << display_name
           << std::right
           << std::setw(12) << format_size(fi.size)
           << "  0x" << std::hex << std::setw(8) << std::setfill('0')
           << fi.data_offset << std::dec << std::setfill(' ')
           << "  ";

        // Flags
        if (fi.file_flag == 0)   os << "[op] ";
        if (fi.check_flag == 1)  os << "[req] ";
        if (fi.omit_flag == 1)   os << "[omit] ";

        // Addresses
        if (fi.addr_count > 0) {
            os << " @";
            for (uint32_t a = 0; a < fi.addr_count && a < 5; ++a) {
                os << " 0x" << std::hex << fi.addr[a] << std::dec;
            }
        }

        os << "\n";
    }
}

// ============================================================================
// CRC Verification
// ============================================================================

bool PacReader::verify_crc() const {
    // CRC1 covers the header, excluding magic + crc1 + crc2 (last 8 bytes)
    constexpr size_t crc1_len = sizeof(PacHeader) - sizeof(uint32_t) - 2 * sizeof(uint16_t);
    uint16_t computed_crc1 = crc16(0,
        reinterpret_cast<const uint8_t*>(&header_), crc1_len);

    bool crc1_ok = (computed_crc1 == header_.crc1);
    std::cout << "  CRC1 (header): computed=0x" << std::hex << std::setw(4)
              << std::setfill('0') << computed_crc1
              << " stored=0x" << std::setw(4) << header_.crc1
              << std::dec << std::setfill(' ')
              << (crc1_ok ? " [OK]" : " [FAILED] MISMATCH") << "\n";

    // Note: CRC2 covers the entire file, which can be very large.
    // For now we just report CRC1 status.
    // Full CRC2 verification would require reading the entire PAC.

    return crc1_ok;
}

} // namespace upac
