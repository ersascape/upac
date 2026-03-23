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

    // Attributes vs Child Nodes: Try both
    fc.id = file_node.attribute("id").as_string(
            file_node.attribute("ID").as_string(
            file_node.child_value("id")));
    if (fc.id.empty()) fc.id = file_node.child_value("ID");

    fc.type = file_node.attribute("type").as_string(
              file_node.attribute("Type").as_string(
              file_node.child_value("type")));
    if (fc.type.empty()) fc.type = file_node.child_value("Type");

    // Parse Block — handles both <Block base="0x..." /> and <Block><Base>0x...</Base></Block>
    auto block = file_node.child("Block");
    if (block) {
        fc.id_name = block.attribute("id").as_string(
                     block.attribute("ID").as_string(
                     block.child_value("id")));
        if (fc.id_name.empty()) fc.id_name = block.child_value("ID");

        fc.base_address = parse_hex_or_dec(
            block.attribute("base").as_string(
            block.attribute("Base").as_string(
            block.child_value("base"))));
        if (fc.base_address == 0) {
            fc.base_address = parse_hex_or_dec(block.child_value("Base"));
        }

        fc.size = parse_hex_or_dec(
            block.attribute("size").as_string(
            block.attribute("Size").as_string(
            block.child_value("size"))));
        if (fc.size == 0) {
            fc.size = parse_hex_or_dec(block.child_value("Size"));
        }
    }

    fc.flag       = file_node.attribute("flag").as_uint(
                    file_node.attribute("Flag").as_uint(
                    file_node.child("flag").text().as_uint(
                    file_node.child("Flag").text().as_uint(0))));
    
    fc.check_flag = file_node.attribute("check_flag").as_uint(
                    file_node.attribute("CheckFlag").as_uint(
                    file_node.child("check_flag").text().as_uint(
                    file_node.child("CheckFlag").text().as_uint(0))));

    // Parse Operation children
    for (auto op : file_node.children()) {
        std::string node_name = op.name();
        // std::cerr << "  debug: found node " << node_name << " under " << fc.id << "\n";
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

    // Parse file/packet entries — handle both <File> and <Packet> tags,
    // potentially wrapped in a <PacketList> or directly under <Product>.
    
    auto parse_children = [&](const pugi::xml_node& parent) {
        for (auto node : parent.children()) {
            std::string name = node.name();
            if (name == "File" || name == "Packet") {
                out_config.files.push_back(parse_file_node(node));
            }
        }
    };

    // Try directly under product node
    parse_children(product_node);

    // Try under <PacketList>
    if (auto packet_list = product_node.child("PacketList")) {
        parse_children(packet_list);
    }

    // NEW: Try under <SchemeList><Scheme>
    if (out_config.files.empty()) {
        std::string scheme_name = product_node.child_value("SchemeName");
        if (scheme_name.empty()) scheme_name = out_config.name;

        auto scheme_list = doc.child("BMAConfig").child("SchemeList");
        if (!scheme_list) scheme_list = doc.child("SchemeList");

        if (scheme_list) {
            for (auto scheme : scheme_list.children("Scheme")) {
                std::string sname = scheme.attribute("name").as_string(
                                    scheme.attribute("Name").as_string(""));
                if (sname == scheme_name) {
                    parse_children(scheme);
                    break;
                }
            }
        }
    }

    // Fallback: if still empty, try all children that look like entries
    if (out_config.files.empty()) {
        for (auto child : product_node.children()) {
            if (child.attribute("id") || child.attribute("ID") || child.child("ID") || child.child("id")) {
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
