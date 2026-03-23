#include "upac/pac_reader.h"
#include "upac/xml_config.h"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

void print_usage() {
    std::cout << "upac — Unisoc PAC Firmware Utility\n\n"
              << "Usage: upac <command> [arguments]\n\n"
              << "Commands:\n"
              << "  info <file.pac>           Show PAC header and product information\n"
              << "  list <file.pac>           List all files contained in the PAC\n"
              << "  extract <file.pac> [dir]  Extract all files to a directory (default: ./extracted)\n"
              << "  xml <file.pac>            Dump the embedded XML configuration to stdout\n"
              << "  ops <file.pac>            Show the flash operation sequence per file\n"
              << "  verify <file.pac>         Verify PAC integrity (CRC1)\n"
              << "  help                      Show this help message\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "help" || cmd == "-h" || cmd == "--help") {
        print_usage();
        return 0;
    }

    if (argc < 3) {
        std::cerr << "error: missing PAC file argument for command '" << cmd << "'\n";
        return 1;
    }

    fs::path pac_path = argv[2];
    auto reader_opt = upac::PacReader::open(pac_path);
    if (!reader_opt) {
        return 1;
    }

    const auto& reader = *reader_opt;

    if (cmd == "info") {
        reader.print_info(std::cout);
    } 
    else if (cmd == "list") {
        reader.print_file_list(std::cout);
    } 
    else if (cmd == "extract") {
        fs::path out_dir = (argc >= 4) ? argv[3] : "extracted";
        std::cout << "Extracting all files from " << pac_path.filename().string() << " to " << out_dir.string() << "...\n";
        if (reader.extract_all(out_dir)) {
            std::cout << "\nExtraction complete.\n";
        } else {
            std::cerr << "\nExtraction failed for some files.\n";
            return 1;
        }
    } 
    else if (cmd == "xml") {
        std::string xml = reader.xml_config();
        if (xml.empty()) {
            std::cerr << "error: no XML configuration found in PAC\n";
            return 1;
        }
        std::cout << xml << std::endl;
    } 
    else if (cmd == "ops") {
        std::string xml = reader.xml_config();
        if (xml.empty()) {
            std::cerr << "error: no XML configuration found in PAC\n";
            return 1;
        }
        upac::XmlProductConfig config;
        if (upac::parse_xml_config(xml, config)) {
            upac::print_operations(std::cout, config);
        } else {
            return 1;
        }
    } 
    else if (cmd == "verify") {
        if (reader.verify_crc()) {
            std::cout << "\nIntegrity check passed.\n";
        } else {
            std::cerr << "\nIntegrity check FAILED.\n";
            return 1;
        }
    } 
    else {
        std::cerr << "error: unknown command '" << cmd << "'\n";
        print_usage();
        return 1;
    }

    return 0;
}
