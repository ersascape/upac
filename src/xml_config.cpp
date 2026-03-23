#include "upac/xml_config.h"
#include "pugixml.hpp"

#include <iomanip>
#include <sstream>

namespace upac {

// ============================================================================
// XML Config Parser
// ============================================================================
//
// The PAC-embedded XML has a structure like:
//
//   <BMAConfig>
//     <ProductList>
//       <Product name="...">
//         <File id="FDL" type="FDL" ...>
//           <Block id="..." base="0x..." size="0x..." />
//           <Operation type="CheckBaud" />
//           <Operation type="Connect" />
//           <Operation type="Download" />
//           ...
//         </File>
//         ...
//       </Product>
//     </ProductList>
//   </BMAConfig>
//
// Different PAC versions may have slightly different schemas, so we
// parse defensively and extract what we can.

static uint64_t parse_hex_or_dec(const char* str) {
    if (!str || !*str) return 0;
    // Handle "0x" prefix
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        return std::strtoull(str + 2, nullptr, 16);
    }
    return std::strtoull(str, nullptr, 10);
}

/// Try to parse a File node and its children
static XmlFileConfig parse_file_node(const pugi::xml_node& file_node) {
    XmlFileConfig fc;

    // Attributes vary: "id", "ID", "name", "Name", etc.
    // Try common patterns
    fc.id   = file_node.attribute("id").as_string(
              file_node.attribute("ID").as_string(""));
    fc.type = file_node.attribute("type").as_string(
              file_node.attribute("Type").as_string(""));

    // Parse Block children (base address, size)
    for (auto block : file_node.children("Block")) {
        fc.id_name = block.attribute("id").as_string("");
        fc.base_address = parse_hex_or_dec(
            block.attribute("base").as_string(
            block.attribute("Base").as_string("")));
        fc.size = parse_hex_or_dec(
            block.attribute("size").as_string(
            block.attribute("Size").as_string("")));
    }

    fc.flag       = file_node.attribute("flag").as_uint(
                    file_node.attribute("Flag").as_uint(0));
    fc.check_flag = file_node.attribute("check_flag").as_uint(
                    file_node.attribute("CheckFlag").as_uint(0));

    // Parse Operation children
    for (auto op : file_node.children()) {
        std::string node_name = op.name();
        // Accept <Operation type="..."> or <Scheme name="...">
        if (node_name == "Operation" || node_name == "Scheme") {
            FileOperation fop;
            fop.name = op.attribute("type").as_string(
                       op.attribute("Type").as_string(
                       op.attribute("name").as_string(
                       op.attribute("Name").as_string(""))));
            if (!fop.name.empty()) {
                fc.operations.push_back(std::move(fop));
            }
        }
    }

    return fc;
}

bool parse_xml_config(const std::string& xml_utf8,
                      XmlProductConfig& out_config) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xml_utf8.c_str());
    if (!result) {
        std::cerr << "XML parse error: " << result.description()
                  << " at offset " << result.offset << "\n";
        return false;
    }

    // Navigate to the product — try several known root structures
    pugi::xml_node product_node;

    // Pattern 1: <BMAConfig><ProductList><Product>
    product_node = doc.child("BMAConfig").child("ProductList").first_child();

    // Pattern 2: <ProductList><Product>
    if (!product_node) {
        product_node = doc.child("ProductList").first_child();
    }

    // Pattern 3: <BMAConfig><Product>
    if (!product_node) {
        product_node = doc.child("BMAConfig").first_child();
    }

    // Pattern 4: root is the product itself
    if (!product_node) {
        product_node = doc.first_child();
    }

    if (!product_node) {
        std::cerr << "warning: could not find product node in XML\n";
        return false;
    }

    out_config.name = product_node.attribute("name").as_string(
                      product_node.attribute("Name").as_string("Unknown"));

    // Parse each file node
    for (auto file_node : product_node.children("File")) {
        out_config.files.push_back(parse_file_node(file_node));
    }

    // Also try <NVBackup>, <Scheme>, and other patterns
    // Some PAC XMLs put file entries under different tags
    if (out_config.files.empty()) {
        // Try all children that have an "id" or "ID" attribute
        for (auto child : product_node.children()) {
            if (child.attribute("id") || child.attribute("ID")) {
                out_config.files.push_back(parse_file_node(child));
            }
        }
    }

    return true;
}

// ============================================================================
// Pretty print
// ============================================================================

void print_operations(std::ostream& os, const XmlProductConfig& config) {
    os << "Product: " << config.name << "\n\n";

    for (const auto& file : config.files) {
        os << "  " << std::left << std::setw(16) << file.id;
        if (!file.type.empty()) {
            os << " [" << file.type << "]";
        }
        if (file.base_address != 0) {
            os << " @0x" << std::hex << file.base_address << std::dec;
        }
        os << "\n";

        if (!file.operations.empty()) {
            os << "    ";
            for (size_t i = 0; i < file.operations.size(); ++i) {
                if (i > 0) os << " → ";
                os << file.operations[i].name;
            }
            os << "\n";
        }
        os << "\n";
    }
}

} // namespace upac
