#pragma once
//
// upac/xml_config.h — Parse the XML configuration embedded in PAC files
//
// The XML describes the per-file operation sequence for flashing.
// Example operations: CheckBaud, Connect, Download, EraseFlash, Reset, etc.
//

#include <iostream>
#include <string>
#include <vector>

namespace upac {

/// A single operation in the flash sequence for a file
struct FileOperation {
    std::string name;   // e.g. "CheckBaud", "Connect", "Download"
};

/// Configuration for one file/partition in the PAC
struct XmlFileConfig {
    std::string id;             // File ID (e.g. "FDL", "FDL2", "NV")
    std::string type;           // File type (e.g. "FDL", "CODE", "NV")
    std::string id_name;        // ID name / block attribute
    uint64_t    base_address = 0;
    uint64_t    size = 0;
    uint32_t    flag = 0;
    uint32_t    check_flag = 0;
    std::vector<FileOperation> operations;
};

/// Parsed product configuration from the PAC's embedded XML
struct XmlProductConfig {
    std::string name;
    std::vector<XmlFileConfig> files;
};

/// Parse PAC-embedded XML configuration.
/// Returns true on success.
bool parse_xml_config(const std::string& xml_utf8,
                      XmlProductConfig& out_config);

/// Print parsed operation sequences.
void print_operations(std::ostream& os, const XmlProductConfig& config);

} // namespace upac
