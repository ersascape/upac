#pragma once
//
// upac/pac_reader.h — Read and extract .pac firmware files
//

#include "upac/pac.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace upac {

class PacReader {
public:
    /// Open a .pac file. Returns nullopt with error printed on failure.
    static std::optional<PacReader> open(const std::filesystem::path& path);

    // --- Accessors ---

    const PacHeader& header() const { return header_; }
    const std::vector<PacFileEntry>& raw_files() const { return raw_files_; }

    /// Get human-readable product info
    ProductInfo product_info() const;

    /// Get human-readable file info for all entries
    std::vector<FileInfo> file_infos() const;

    /// Get the embedded XML config as a UTF-8 string.
    /// Returns empty string if no XML was found.
    std::string xml_config() const;

    // --- Operations ---

    /// Extract a single file by index to the given directory.
    /// Returns true on success.
    bool extract(size_t index, const std::filesystem::path& out_dir) const;

    /// Extract all files to the given directory.
    /// Returns true if all succeeded.
    bool extract_all(const std::filesystem::path& out_dir) const;

    /// Pretty-print header summary to stream.
    void print_info(std::ostream& os) const;

    /// Print tabulated file list to stream.
    void print_file_list(std::ostream& os) const;

    /// Verify CRC integrity. Returns true if valid.
    bool verify_crc() const;

private:
    PacReader() = default;

    bool read_file_data(const PacFileEntry& entry, std::vector<uint8_t>& out) const;

    std::filesystem::path path_;
    PacHeader header_{};
    std::vector<PacFileEntry> raw_files_;
    std::vector<uint8_t> xml_data_;  // raw XML bytes from PAC
    uint64_t actual_file_size_ = 0;
    size_t xml_file_index_ = static_cast<size_t>(-1);
    mutable std::optional<std::vector<FileInfo>> file_infos_cache_;
    mutable std::optional<std::string> xml_config_cache_;
};

} // namespace upac
