#include "resources/tibia_dat.hpp"
#include "resources/tibia_pic.hpp"
#include "resources/tibia_spr.hpp"
#include "render/object_compositor.hpp"
#include "render/outfit_compositor.hpp"
#include "render/pic_compositor.hpp"
#include "render/animation_compositor.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open input file: " + path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write_u16(std::ostream& out, std::uint16_t v) {
    out.put(static_cast<char>(v)); out.put(static_cast<char>(v >> 8U));
}
void write_u32(std::ostream& out, std::uint32_t v) {
    for (unsigned shift = 0; shift < 32; shift += 8) out.put(static_cast<char>(v >> shift));
}

void write_bmp(const std::string& path, const tibia::resources::TibiaSprImage& image) {
    constexpr std::uint32_t pixel_bytes = 32U * 32U * 4U;
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot create output file: " + path);
    out.put('B'); out.put('M');
    write_u32(out, 54U + pixel_bytes); write_u16(out, 0); write_u16(out, 0); write_u32(out, 54);
    write_u32(out, 40); write_u32(out, 32); write_u32(out, 32); write_u16(out, 1); write_u16(out, 32);
    write_u32(out, 0); write_u32(out, pixel_bytes); write_u32(out, 2835); write_u32(out, 2835);
    write_u32(out, 0); write_u32(out, 0);
    for (int y = 31; y >= 0; --y) {
        for (int x = 0; x < 32; ++x) {
            const auto i = (static_cast<std::size_t>(y) * 32 + x) * 4;
            out.put(static_cast<char>(image.rgba[i + 2]));
            out.put(static_cast<char>(image.rgba[i + 1]));
            out.put(static_cast<char>(image.rgba[i]));
            out.put(static_cast<char>(image.rgba[i + 3]));
        }
    }
    if (!out) throw std::runtime_error("failed while writing BMP: " + path);
}

void write_bmp(const std::string& path, const tibia::render::SoftwareSurface& surface) {
    const auto width = surface.width();
    const auto height = surface.height();
    const auto pixel_bytes = static_cast<std::uint32_t>(static_cast<std::uint64_t>(width) * height * 4U);
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot create output file: " + path);
    out.put('B'); out.put('M');
    write_u32(out, 54U + pixel_bytes); write_u16(out, 0); write_u16(out, 0); write_u32(out, 54);
    write_u32(out, 40); write_u32(out, width); write_u32(out, height);
    write_u16(out, 1); write_u16(out, 32); write_u32(out, 0); write_u32(out, pixel_bytes);
    write_u32(out, 2835); write_u32(out, 2835); write_u32(out, 0); write_u32(out, 0);
    const auto& pixels = surface.pixels();
    for (std::uint32_t row = height; row > 0; --row) {
        const auto y = row - 1;
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto i = (static_cast<std::size_t>(y) * width + x) * 4;
            out.put(static_cast<char>(pixels[i + 2])); out.put(static_cast<char>(pixels[i + 1]));
            out.put(static_cast<char>(pixels[i])); out.put(static_cast<char>(pixels[i + 3]));
        }
    }
    if (!out) throw std::runtime_error("failed while writing BMP: " + path);
}

std::uint16_t parse_id(const char* text) {
    const auto value = std::stoul(text);
    if (value > 65535) throw std::out_of_range("ID must be between 0 and 65535");
    return static_cast<std::uint16_t>(value);
}

const tibia::resources::TibiaDatCategory& select_category(
    const tibia::resources::TibiaDatFile& dat, const std::string& category) {
    if (category == "items") return dat.items;
    if (category == "outfits") return dat.outfits;
    if (category == "effects") return dat.effects;
    if (category == "missiles") return dat.missiles;
    throw std::invalid_argument("CATEGORY must be items, outfits, effects, or missiles");
}

tibia::render::SoftwareSurface compose_grid_object(const std::vector<std::uint8_t>& spr_bytes,
    const tibia::resources::TibiaSprFile& spr, const tibia::resources::TibiaDatObject& object,
    bool is_outfit) {
    if (!is_outfit)
        return tibia::render::compose_tibia_dat_object(spr_bytes, spr, object);
    tibia::render::OutfitPalette palette{};
    for (auto& color : palette) color = tibia::render::tibia_outfit_color(0);
    tibia::render::OutfitRenderOptions options;
    return tibia::render::compose_tibia_outfit(spr_bytes, spr, object, options, palette);
}

void usage() {
    std::cerr << "Usage:\n"
        << "  tibia_asset_tool dat-info Tibia.dat\n"
        << "  tibia_asset_tool object-list Tibia.dat CATEGORY|all\n"
        << "  tibia_asset_tool outfit-list Tibia.dat\n"
        << "  tibia_asset_tool spr-info Tibia.spr\n"
        << "  tibia_asset_tool export-sprite Tibia.spr ID output.bmp\n"
        << "  tibia_asset_tool pic-info Tibia.pic\n"
        << "  tibia_asset_tool pic-list Tibia.pic\n"
        << "  tibia_asset_tool export-pic-tile Tibia.pic PICTURE_ID X Y output.bmp\n"
        << "  tibia_asset_tool export-pic Tibia.pic PICTURE_ID output.bmp\n"
        << "  tibia_asset_tool export-all-pics Tibia.pic OUTPUT_DIRECTORY\n"
        << "  tibia_asset_tool export-object-layer Tibia.dat Tibia.spr CATEGORY ID LAYER PX PY PZ PHASE output.bmp\n"
        << "  tibia_asset_tool export-object Tibia.dat Tibia.spr CATEGORY ID PX PY PZ PHASE output.bmp\n";
    std::cerr << "  tibia_asset_tool export-outfit Tibia.dat Tibia.spr ID DIRECTION ADDON_MASK PZ PHASE COLOR0 COLOR1 COLOR2 COLOR3 output.bmp\n";
    std::cerr << "  tibia_asset_tool export-animation Tibia.dat Tibia.spr CATEGORY ID PX PY PZ output.bmp\n"
        << "  tibia_asset_tool export-object-grid Tibia.dat Tibia.spr CATEGORY START COUNT COLUMNS output.bmp\n";
    std::cerr << "  tibia_asset_tool export-outfit-animation Tibia.dat Tibia.spr ID DIRECTION ADDON_MASK PZ COLOR0 COLOR1 COLOR2 COLOR3 output.bmp\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 3) { usage(); return 2; }
        const std::string command = argv[1];
        const auto bytes = read_file(argv[2]);
        if (command == "dat-info") {
            const auto file = tibia::resources::parse_tibia_dat(bytes);
            std::cout << "signature=0x" << std::hex << file.header.signature << std::dec
                << " items=" << file.items.objects.size() << " outfits=" << file.outfits.objects.size()
                << " effects=" << file.effects.objects.size() << " missiles=" << file.missiles.objects.size()
                << " parsed_bytes=" << file.parsed_bytes << "\n";
        } else if (command == "object-list" && argc == 4) {
            const auto dat = tibia::resources::parse_tibia_dat(bytes);
            const std::string requested = argv[3];
            const std::array<std::pair<const char*, const tibia::resources::TibiaDatCategory*>, 4> categories{{
                {"items", &dat.items}, {"outfits", &dat.outfits},
                {"effects", &dat.effects}, {"missiles", &dat.missiles}}};
            bool matched = false;
            for (const auto& [name, category] : categories) {
                if (requested != "all" && requested != name) continue;
                matched = true;
                std::cout << "[" << name << "] first_id=" << category->first_id
                    << " count=" << category->objects.size() << "\n";
                for (std::size_t i = 0; i < category->objects.size(); ++i) {
                    const auto& object = category->objects[i];
                    std::cout << (category->first_id + i) << " tiles="
                        << static_cast<unsigned>(object.width) << "x"
                        << static_cast<unsigned>(object.height) << " layers="
                        << static_cast<unsigned>(object.layers) << " patterns="
                        << static_cast<unsigned>(object.pattern_x) << "x"
                        << static_cast<unsigned>(object.pattern_y) << "x"
                        << static_cast<unsigned>(object.pattern_z) << " phases="
                        << static_cast<unsigned>(object.animation_frames) << " attrs=";
                    for (std::size_t a = 0; a < object.attributes.size(); ++a) {
                        if (a) std::cout << ',';
                        std::cout << static_cast<unsigned>(object.attributes[a].id);
                    }
                    std::cout << "\n";
                }
            }
            if (!matched) throw std::invalid_argument("CATEGORY must be items, outfits, effects, missiles, or all");
        } else if (command == "outfit-list") {
            const auto dat = tibia::resources::parse_tibia_dat(bytes);
            std::cout << "first_id=" << dat.outfits.first_id << " outfits="
                << dat.outfits.objects.size() << "\n";
            for (std::size_t i = 0; i < dat.outfits.objects.size(); ++i) {
                const auto& outfit = dat.outfits.objects[i];
                if (outfit.layers == 2)
                    std::cout << (dat.outfits.first_id + i) << " tiles="
                        << static_cast<unsigned>(outfit.width) << "x"
                        << static_cast<unsigned>(outfit.height) << " patterns="
                        << static_cast<unsigned>(outfit.pattern_x) << "x"
                        << static_cast<unsigned>(outfit.pattern_y) << "x"
                        << static_cast<unsigned>(outfit.pattern_z) << " phases="
                        << static_cast<unsigned>(outfit.animation_frames) << "\n";
            }
        } else if (command == "spr-info") {
            const auto file = tibia::resources::parse_tibia_spr(bytes);
            std::size_t present = 0;
            std::uint32_t first = 0;
            for (std::size_t i = 0; i < file.sprite_offsets.size(); ++i) {
                if (file.sprite_offsets[i] != 0) { ++present; if (!first) first = static_cast<std::uint32_t>(i + 1); }
            }
            std::cout << "signature=0x" << std::hex << file.signature << std::dec
                << " sprites=" << file.sprite_offsets.size() << " present=" << present
                << " first_present_id=" << first << " index_bytes=" << file.parsed_index_bytes << "\n";
        } else if (command == "export-sprite" && argc == 5) {
            const auto file = tibia::resources::parse_tibia_spr(bytes);
            const auto image = tibia::resources::decode_tibia_spr_image(bytes, file, parse_id(argv[3]));
            write_bmp(argv[4], image);
            std::cout << "exported sprite " << argv[3] << " to " << argv[4] << "\n";
        } else if (command == "pic-info") {
            const auto file = tibia::resources::parse_tibia_pic(bytes);
            std::cout << "signature=0x" << std::hex << file.signature << std::dec
                << " pictures=" << file.pictures.size() << " parsed_bytes=" << file.parsed_index_bytes << "\n";
        } else if (command == "pic-list") {
            const auto file = tibia::resources::parse_tibia_pic(bytes);
            std::cout << "signature=0x" << std::hex << file.signature << std::dec
                << " pictures=" << file.pictures.size() << "\n";
            for (std::size_t i = 0; i < file.pictures.size(); ++i) {
                const auto& picture = file.pictures[i];
                std::cout << (i + 1) << " tiles=" << static_cast<unsigned>(picture.tiles_x)
                    << "x" << static_cast<unsigned>(picture.tiles_y) << " pixels="
                    << static_cast<unsigned>(picture.tiles_x) * 32U << "x"
                    << static_cast<unsigned>(picture.tiles_y) * 32U << "\n";
            }
        } else if (command == "export-pic-tile" && argc == 7) {
            const auto file = tibia::resources::parse_tibia_pic(bytes);
            const auto image = tibia::resources::decode_tibia_pic_tile(bytes, file,
                parse_id(argv[3]), parse_id(argv[4]), parse_id(argv[5]));
            write_bmp(argv[6], image);
            std::cout << "exported picture tile to " << argv[6] << "\n";
        } else if (command == "export-pic" && argc == 5) {
            const auto file = tibia::resources::parse_tibia_pic(bytes);
            const auto picture_id = parse_id(argv[3]);
            const auto surface = tibia::render::compose_tibia_pic_picture(bytes, file, picture_id);
            write_bmp(argv[4], surface);
            std::cout << "exported complete picture " << picture_id << " to " << argv[4] << "\n";
        } else if (command == "export-all-pics" && argc == 4) {
            const auto file = tibia::resources::parse_tibia_pic(bytes);
            const std::filesystem::path output_directory(argv[3]);
            std::filesystem::create_directories(output_directory);
            for (std::size_t i = 0; i < file.pictures.size(); ++i) {
                const auto id = i + 1;
                const auto filename = output_directory /
                    (std::string("picture-") + (id < 10 ? "0" : "") + std::to_string(id) + ".bmp");
                const auto surface = tibia::render::compose_tibia_pic_picture(
                    bytes, file, static_cast<std::uint16_t>(id));
                write_bmp(filename.string(), surface);
            }
            std::cout << "exported " << file.pictures.size() << " complete pictures to "
                << output_directory.string() << "\n";
        } else if (command == "export-outfit-animation" && argc == 13) {
            const auto dat = tibia::resources::parse_tibia_dat(bytes);
            const auto spr_bytes = read_file(argv[3]);
            const auto spr = tibia::resources::parse_tibia_spr(spr_bytes);
            const auto id = parse_id(argv[4]);
            if (id < dat.outfits.first_id || id - dat.outfits.first_id >= dat.outfits.objects.size())
                throw std::out_of_range("outfit ID is outside the parsed outfit category");
            const auto& outfit = dat.outfits.objects[id - dat.outfits.first_id];
            const auto direction = parse_id(argv[5]);
            const auto addon_mask = parse_id(argv[6]);
            const auto pattern_z = parse_id(argv[7]);
            tibia::render::OutfitPalette palette{};
            for (std::size_t color = 0; color < palette.size(); ++color) {
                const auto selector = parse_id(argv[8 + color]);
                if (selector > 132)
                    throw std::out_of_range("outfit color selectors must be between 0 and 132");
                palette[color] = tibia::render::tibia_outfit_color(selector);
            }
            const auto surface = tibia::render::compose_tibia_outfit_animation(
                spr_bytes, spr, outfit, direction, addon_mask, pattern_z, palette);
            write_bmp(argv[12], surface);
            std::cout << "exported outfit animation strip for " << id << " to " << argv[12] << "\n";
        } else if (command == "export-animation" && argc == 10) {
            const auto dat = tibia::resources::parse_tibia_dat(bytes);
            const auto spr_bytes = read_file(argv[3]);
            const auto spr = tibia::resources::parse_tibia_spr(spr_bytes);
            const std::string category_name = argv[4];
            if (category_name == "outfits")
                throw std::invalid_argument("use export-outfit for outfit color masks");
            const auto& category = select_category(dat, category_name);
            const auto id = parse_id(argv[5]);
            if (id < category.first_id || id - category.first_id >= category.objects.size())
                throw std::out_of_range("object ID is outside the selected category");
            const auto surface = tibia::render::compose_tibia_dat_animation(spr_bytes, spr,
                category.objects[id - category.first_id], parse_id(argv[6]),
                parse_id(argv[7]), parse_id(argv[8]));
            write_bmp(argv[9], surface);
            std::cout << "exported animation strip for " << category_name << " " << id
                << " to " << argv[9] << "\n";
        } else if (command == "export-object-grid" && argc == 9) {
            const auto dat = tibia::resources::parse_tibia_dat(bytes);
            const auto spr_bytes = read_file(argv[3]);
            const auto spr = tibia::resources::parse_tibia_spr(spr_bytes);
            const std::string category_name = argv[4];
            const auto& category = select_category(dat, category_name);
            const auto start = parse_id(argv[5]);
            const auto count = parse_id(argv[6]);
            const auto columns = parse_id(argv[7]);
            if (count == 0 || columns == 0 || start < category.first_id ||
                static_cast<std::uint32_t>(start - category.first_id) + count > category.objects.size())
                throw std::out_of_range("object grid range or column count is invalid");
            std::uint16_t max_width = 1;
            std::uint16_t max_height = 1;
            for (std::uint32_t i = 0; i < count; ++i) {
                const auto& object = category.objects[start - category.first_id + i];
                max_width = std::max<std::uint16_t>(max_width, object.width);
                max_height = std::max<std::uint16_t>(max_height, object.height);
            }
            constexpr std::uint32_t gap = 4;
            const auto cell_width = static_cast<std::uint32_t>(max_width) * 32U + gap;
            const auto cell_height = static_cast<std::uint32_t>(max_height) * 32U + gap;
            const auto rows = (static_cast<std::uint32_t>(count) + columns - 1U) / columns;
            const auto atlas_width = cell_width * columns;
            const auto atlas_height = cell_height * rows;
            if (static_cast<std::uint64_t>(atlas_width) * atlas_height > 64ULL * 1024ULL * 1024ULL)
                throw std::length_error("object grid exceeds the 64-megapixel export limit");
            tibia::render::SoftwareSurface atlas(atlas_width, atlas_height, {0, 0, 0, 0});
            for (std::uint32_t i = 0; i < count; ++i) {
                const auto& object = category.objects[start - category.first_id + i];
                if (object.width == 0 || object.height == 0 || object.layers == 0) continue;
                const auto image = compose_grid_object(spr_bytes, spr, object,
                    category_name == "outfits");
                const auto column = i % columns;
                const auto row = i / columns;
                const auto x = column * cell_width + (cell_width - image.width()) / 2U;
                const auto y = row * cell_height + (cell_height - image.height()) / 2U;
                atlas.draw(image, static_cast<int>(x), static_cast<int>(y));
            }
            write_bmp(argv[8], atlas);
            std::cout << "exported grid for " << category_name << " IDs " << start << ".."
                << static_cast<std::uint32_t>(start) + count - 1U << " to " << argv[8] << "\n";
        } else if (command == "export-object-layer" && argc == 12) {
            const auto dat = tibia::resources::parse_tibia_dat(bytes);
            const auto spr_bytes = read_file(argv[3]);
            const auto spr = tibia::resources::parse_tibia_spr(spr_bytes);
            const std::string category = argv[4];
            const auto id = parse_id(argv[5]);
            const auto layer = parse_id(argv[6]);
            const auto px = parse_id(argv[7]);
            const auto py = parse_id(argv[8]);
            const auto pz = parse_id(argv[9]);
            const auto phase = parse_id(argv[10]);
            const tibia::resources::TibiaDatCategory* selected{};
            if (category == "items") selected = &dat.items;
            else if (category == "outfits") selected = &dat.outfits;
            else if (category == "effects") selected = &dat.effects;
            else if (category == "missiles") selected = &dat.missiles;
            else throw std::invalid_argument("CATEGORY must be items, outfits, effects, or missiles");
            if (id < selected->first_id || id - selected->first_id >= selected->objects.size())
                throw std::out_of_range("object ID is outside the selected category");
            const auto& object = selected->objects[id - selected->first_id];
            const auto surface = tibia::render::compose_tibia_dat_object_layer(
                spr_bytes, spr, object, layer, px, py, pz, phase);
            write_bmp(argv[11], surface);
            std::cout << "exported " << category << " object " << id << " layer " << layer
                << " to " << argv[11] << "\n";
        } else if (command == "export-object" && argc == 11) {
            const auto dat = tibia::resources::parse_tibia_dat(bytes);
            const auto spr_bytes = read_file(argv[3]);
            const auto spr = tibia::resources::parse_tibia_spr(spr_bytes);
            const std::string category = argv[4];
            const auto id = parse_id(argv[5]);
            const auto px = parse_id(argv[6]);
            const auto py = parse_id(argv[7]);
            const auto pz = parse_id(argv[8]);
            const auto phase = parse_id(argv[9]);
            const tibia::resources::TibiaDatCategory* selected{};
            if (category == "items") selected = &dat.items;
            else if (category == "outfits") selected = &dat.outfits;
            else if (category == "effects") selected = &dat.effects;
            else if (category == "missiles") selected = &dat.missiles;
            else throw std::invalid_argument("CATEGORY must be items, outfits, effects, or missiles");
            if (id < selected->first_id || id - selected->first_id >= selected->objects.size())
                throw std::out_of_range("object ID is outside the selected category");
            const auto& object = selected->objects[id - selected->first_id];
            const auto surface = tibia::render::compose_tibia_dat_object(
                spr_bytes, spr, object, px, py, pz, phase);
            write_bmp(argv[10], surface);
            std::cout << "exported complete " << category << " object " << id
                << " to " << argv[10] << "\n";
        } else if (command == "export-outfit" && argc == 14) {
            const auto dat = tibia::resources::parse_tibia_dat(bytes);
            const auto spr_bytes = read_file(argv[3]);
            const auto spr = tibia::resources::parse_tibia_spr(spr_bytes);
            const auto id = parse_id(argv[4]);
            if (id < dat.outfits.first_id || id - dat.outfits.first_id >= dat.outfits.objects.size())
                throw std::out_of_range("outfit ID is outside the parsed outfit category");
            const auto& outfit = dat.outfits.objects[id - dat.outfits.first_id];
            tibia::render::OutfitRenderOptions options;
            options.direction = parse_id(argv[5]);
            options.addon_mask = parse_id(argv[6]);
            options.pattern_z = parse_id(argv[7]);
            options.phase = parse_id(argv[8]);
            tibia::render::OutfitPalette palette{};
            for (std::size_t color = 0; color < palette.size(); ++color) {
                const auto selector = parse_id(argv[9 + color]);
                if (selector > 132)
                    throw std::out_of_range("outfit color selectors must be between 0 and 132");
                palette[color] = tibia::render::tibia_outfit_color(selector);
            }
            const auto surface = tibia::render::compose_tibia_outfit(
                spr_bytes, spr, outfit, options, palette);
            write_bmp(argv[13], surface);
            std::cout << "exported outfit " << id << " (direction " << options.direction
                << ") to " << argv[13] << "\n";
        } else {
            usage(); return 2;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return 1;
    }
}
