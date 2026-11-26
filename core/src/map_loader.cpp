#include <rwrl/map_loader.h>
#include <rwrl/pathfinder.h>

#include <expat.h>
#include <zlib.h>

#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace rwrl {
namespace {

namespace fs = std::filesystem;

struct XmlNode {
    Str name;
    Map<Str, Str> attributes;
    Str text;
    Vec<std::unique_ptr<XmlNode>> children;
};

struct XmlBuildState {
    std::unique_ptr<XmlNode> root;
    Vec<XmlNode*> stack;
};

Str lower(Str value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

const Str& attr(const XmlNode& node, const Str& key) {
    static const Str empty;
    auto it = node.attributes.find(key);
    return it == node.attributes.end() ? empty : it->second;
}

i32 parse_i32(const Str& value, const Str& context) {
    i32 result = 0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    auto [ptr, error] = std::from_chars(begin, end, result);
    if (error != std::errc{} || ptr != end) {
        throw std::runtime_error("invalid integer for " + context + ": " + value);
    }
    return result;
}

u32 parse_u32(const Str& value, const Str& context) {
    u32 result = 0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    auto [ptr, error] = std::from_chars(begin, end, result);
    if (error != std::errc{} || ptr != end) {
        throw std::runtime_error("invalid unsigned integer for " + context + ": " + value);
    }
    return result;
}

void XMLCALL start_element(void* user_data, const XML_Char* name,
                           const XML_Char** attributes) {
    auto& state = *static_cast<XmlBuildState*>(user_data);
    auto node = std::make_unique<XmlNode>();
    node->name = name;
    for (const XML_Char** item = attributes; item && *item; item += 2) {
        node->attributes.emplace(item[0], item[1] ? item[1] : "");
    }
    XmlNode* raw = node.get();
    if (state.stack.empty()) {
        state.root = std::move(node);
    } else {
        state.stack.back()->children.push_back(std::move(node));
    }
    state.stack.push_back(raw);
}

void XMLCALL end_element(void* user_data, const XML_Char*) {
    auto& state = *static_cast<XmlBuildState*>(user_data);
    if (!state.stack.empty()) state.stack.pop_back();
}

void XMLCALL character_data(void* user_data, const XML_Char* value, int length) {
    auto& state = *static_cast<XmlBuildState*>(user_data);
    if (!state.stack.empty()) state.stack.back()->text.append(value, length);
}

Str read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open map resource: " + path.string());
    input.seekg(0, std::ios::end);
    const std::streamoff length = input.tellg();
    input.seekg(0, std::ios::beg);
    Str contents(static_cast<std::size_t>(std::max<std::streamoff>(0, length)), '\0');
    if (!contents.empty()) input.read(contents.data(), length);
    if (!input && !contents.empty()) {
        throw std::runtime_error("cannot read map resource: " + path.string());
    }
    return contents;
}

std::unique_ptr<XmlNode> parse_xml(const fs::path& path) {
    const Str contents = read_file(path);
    XML_Parser parser = XML_ParserCreate(nullptr);
    if (!parser) throw std::runtime_error("cannot create Expat parser");

    XmlBuildState state;
    XML_SetUserData(parser, &state);
    XML_SetElementHandler(parser, start_element, end_element);
    XML_SetCharacterDataHandler(parser, character_data);
    const int parsed = XML_Parse(parser, contents.data(),
                                 static_cast<int>(contents.size()), XML_TRUE);
    if (parsed == XML_STATUS_ERROR) {
        const Str error = XML_ErrorString(XML_GetErrorCode(parser));
        const auto line = XML_GetCurrentLineNumber(parser);
        XML_ParserFree(parser);
        throw std::runtime_error(path.string() + ":" + std::to_string(line) +
                                 ": XML parse error: " + error);
    }
    XML_ParserFree(parser);
    if (!state.root) throw std::runtime_error("empty XML document: " + path.string());
    return std::move(state.root);
}

Map<Str, Str> properties(const XmlNode& node) {
    Map<Str, Str> result;
    for (const auto& child : node.children) {
        if (child->name != "properties") continue;
        for (const auto& property : child->children) {
            if (property->name != "property") continue;
            const Str name = attr(*property, "name");
            if (name.empty()) continue;
            const Str value = property->attributes.contains("value")
                ? attr(*property, "value") : property->text;
            result[name] = value;
        }
    }
    return result;
}

struct TileInfo {
    u8 terrain = TERRAIN_NONE;
    bool resource_pool = false;
    bool block_buildings = false;
    i8 path_cost = 0;
    Str unit;
    Str custom_unit;
    Str team;
    bool random_rotate = false;
    Str subtype;
};

TileInfo make_tile_info(const Map<Str, Str>& props) {
    TileInfo info;
    auto has = [&](const char* name) { return props.contains(name); };
    if (has("water")) info.terrain |= TERRAIN_WATER;
    if (has("water-bridge")) info.terrain |= TERRAIN_WATER_BRIDGE;
    if (has("lava") || has("lava-cliff")) info.terrain |= TERRAIN_LAVA;
    if (has("lava-cliff") || has("cliff-soft") || has("cliff")) {
        info.terrain |= TERRAIN_CLIFF;
    }
    if (has("large-cliff")) info.terrain |= TERRAIN_LARGE_CLIFF;
    if (has("trees")) info.terrain |= TERRAIN_TREES;
    if (has("block-land")) {
        info.terrain |= TERRAIN_BLOCK_LAND;
        info.path_cost = -1;
    } else if (has("large-rock")) {
        info.path_cost = -1;
    } else if (has("small-rock")) {
        info.path_cost = 40;
    }
    info.resource_pool = has("res_pool");
    info.block_buildings = has("block-buildings");
    auto copy = [&](const char* name, Str& destination) {
        auto it = props.find(name);
        if (it != props.end()) destination = it->second;
    };
    copy("unit", info.unit);
    copy("customUnit", info.custom_unit);
    copy("team", info.team);
    copy("type", info.subtype);
    info.random_rotate = has("randomRotate");
    return info;
}

struct TilesetInfo {
    u32 first_gid = 0;
    Map<u32, TileInfo> tiles;
};

fs::path resolve_tileset_path(const fs::path& tmx_path, const Str& source) {
    const fs::path relative = fs::path(source);
    Vec<fs::path> candidates;
    candidates.push_back((tmx_path.parent_path() / relative).lexically_normal());

    for (fs::path cursor = tmx_path.parent_path(); !cursor.empty();
         cursor = cursor.parent_path()) {
        if (cursor.filename() == "maps") {
            const fs::path tileset_root = cursor.parent_path() / "tilesets";
            candidates.push_back((tileset_root / relative).lexically_normal());
            // A few shipped maps retain their source-repository prefix, for
            // example ../rustedWarfareMaps-testing/misc.tsx. Packaged assets
            // flatten that shared file to assets/tilesets/misc.tsx.
            candidates.push_back((tileset_root / relative.filename()).lexically_normal());
        }
        const fs::path parent = cursor.parent_path();
        if (parent == cursor) break;
    }
    for (const fs::path& candidate : candidates) {
        std::error_code error;
        if (fs::is_regular_file(candidate, error)) return candidate;
    }
    throw std::runtime_error("cannot resolve external tileset '" + source +
                             "' from " + tmx_path.string());
}

void read_tiles(const XmlNode& tileset_node, TilesetInfo& result) {
    for (const auto& tile : tileset_node.children) {
        if (tile->name != "tile") continue;
        const u32 id = parse_u32(attr(*tile, "id"), "tileset tile id");
        result.tiles[id] = make_tile_info(properties(*tile));
    }
}

TilesetInfo parse_tileset(const XmlNode& reference, const fs::path& tmx_path) {
    TilesetInfo result;
    result.first_gid = parse_u32(attr(reference, "firstgid"), "tileset firstgid");
    const Str source = attr(reference, "source");
    if (source.empty()) {
        read_tiles(reference, result);
    } else {
        const fs::path source_path = resolve_tileset_path(tmx_path, source);
        std::unique_ptr<XmlNode> root = parse_xml(source_path);
        if (root->name != "tileset") {
            throw std::runtime_error("external tileset root is not <tileset>: " +
                                     source_path.string());
        }
        read_tiles(*root, result);
    }
    return result;
}

Vec<u8> decode_base64(const Str& input) {
    static constexpr i8 invalid = -1;
    static constexpr i8 padding = -2;
    i8 table[256];
    std::fill(std::begin(table), std::end(table), invalid);
    const Str alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (std::size_t i = 0; i < alphabet.size(); ++i) {
        table[static_cast<u8>(alphabet[i])] = static_cast<i8>(i);
    }
    table[static_cast<u8>('=')] = padding;

    Vec<u8> output;
    u32 accumulator = 0;
    i32 bits = -8;
    bool saw_padding = false;
    for (unsigned char c : input) {
        if (std::isspace(c)) continue;
        const i8 value = table[c];
        if (value == padding) {
            saw_padding = true;
            continue;
        }
        if (value == invalid || saw_padding) {
            throw std::runtime_error("invalid base64 layer data");
        }
        accumulator = (accumulator << 6) | static_cast<u32>(value);
        bits += 6;
        if (bits >= 0) {
            output.push_back(static_cast<u8>((accumulator >> bits) & 0xff));
            bits -= 8;
        }
    }
    return output;
}

Vec<u8> inflate_layer(const Vec<u8>& compressed, std::size_t expected_size) {
    Vec<u8> output(expected_size);
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(compressed.data());
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());
    if (inflateInit2(&stream, 15 + 32) != Z_OK) {
        throw std::runtime_error("cannot initialize zlib layer decoder");
    }
    const int status = inflate(&stream, Z_FINISH);
    const std::size_t decoded_size = stream.total_out;
    inflateEnd(&stream);
    if (status != Z_STREAM_END || decoded_size != expected_size) {
        throw std::runtime_error("compressed layer has the wrong decoded size");
    }
    return output;
}

Vec<u32> decode_csv(const Str& text, std::size_t expected_count) {
    Vec<u32> gids;
    const char* cursor = text.data();
    const char* end = cursor + text.size();
    while (cursor < end) {
        while (cursor < end && (std::isspace(static_cast<unsigned char>(*cursor)) ||
                                *cursor == ',')) ++cursor;
        if (cursor == end) break;
        u32 gid = 0;
        auto [next, error] = std::from_chars(cursor, end, gid);
        if (error != std::errc{}) throw std::runtime_error("invalid CSV layer data");
        gids.push_back(gid & 0x1fffffffu);
        cursor = next;
        while (cursor < end && std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
        if (cursor < end && *cursor != ',') {
            throw std::runtime_error("invalid CSV layer separator");
        }
    }
    if (gids.size() != expected_count) {
        throw std::runtime_error("CSV layer has the wrong tile count");
    }
    return gids;
}

Vec<u32> decode_layer(const XmlNode& layer, std::size_t expected_count) {
    const XmlNode* data = nullptr;
    for (const auto& child : layer.children) {
        if (child->name == "data") {
            data = child.get();
            break;
        }
    }
    if (!data) throw std::runtime_error("TMX layer has no data element");
    const Str encoding = lower(attr(*data, "encoding"));
    const Str compression = lower(attr(*data, "compression"));
    if (encoding == "csv") {
        if (!compression.empty()) {
            throw std::runtime_error("compressed CSV TMX layers are unsupported");
        }
        return decode_csv(data->text, expected_count);
    }

    if (encoding.empty()) {
        Vec<u32> gids;
        for (const auto& tile : data->children) {
            if (tile->name == "tile") {
                gids.push_back(parse_u32(attr(*tile, "gid"), "layer gid") &
                               0x1fffffffu);
            }
        }
        if (gids.size() != expected_count) {
            throw std::runtime_error("XML tile layer has the wrong tile count");
        }
        return gids;
    }
    if (encoding != "base64") {
        throw std::runtime_error("unsupported TMX layer encoding: " + encoding);
    }

    Vec<u8> bytes = decode_base64(data->text);
    const std::size_t expected_bytes = expected_count * sizeof(u32);
    if (!compression.empty()) {
        if (compression != "gzip" && compression != "zlib") {
            throw std::runtime_error("unsupported TMX layer compression: " + compression);
        }
        bytes = inflate_layer(bytes, expected_bytes);
    }
    if (bytes.size() != expected_bytes) {
        throw std::runtime_error("base64 layer has the wrong decoded size");
    }

    Vec<u32> gids(expected_count);
    for (std::size_t i = 0; i < expected_count; ++i) {
        const std::size_t offset = i * 4;
        gids[i] = (static_cast<u32>(bytes[offset]) |
                   (static_cast<u32>(bytes[offset + 1]) << 8) |
                   (static_cast<u32>(bytes[offset + 2]) << 16) |
                   (static_cast<u32>(bytes[offset + 3]) << 24)) & 0x1fffffffu;
    }
    return gids;
}

const TileInfo* find_tile(const Vec<TilesetInfo>& tilesets, u32 gid) {
    if (gid == 0) return nullptr;
    const TilesetInfo* selected = nullptr;
    for (const TilesetInfo& tileset : tilesets) {
        if (tileset.first_gid <= gid &&
            (!selected || tileset.first_gid > selected->first_gid)) {
            selected = &tileset;
        }
    }
    if (!selected) return nullptr;
    auto it = selected->tiles.find(gid - selected->first_gid);
    return it == selected->tiles.end() ? nullptr : &it->second;
}

i32 parse_team(const Str& value, const Str& unit) {
    if (lower(value) == "none" || value == "-1") return -1;
    if (value.empty()) {
        throw std::runtime_error("map unit has no team property: " + unit);
    }
    return parse_i32(value, "map unit team");
}

void merge_tile(MapData& map, std::size_t index, const TileInfo& tile,
                bool replace) {
    if (replace) {
        map.terrain[index] = tile.terrain;
        map.resource_pool[index] = tile.resource_pool;
        map.block_buildings[index] = tile.block_buildings;
        map.path_cost[index] = tile.path_cost;
    } else {
        map.terrain[index] |= tile.terrain;
        map.resource_pool[index] = map.resource_pool[index] || tile.resource_pool;
        map.block_buildings[index] =
            map.block_buildings[index] || tile.block_buildings;
        // The Items/Objects tile cost wins over Ground when it is non-zero.
        if (tile.path_cost != 0) map.path_cost[index] = tile.path_cost;
    }
    map.terrain_set[index] = true;
}

void collect_fog_property(const XmlNode& node, MapData& map) {
    const auto props = properties(node);
    auto it = props.find("fog");
    if (it != props.end()) {
        const Str mode = lower(it->second);
        if (mode == "none") map.fog_mode = FogMode::NONE;
        else if (mode == "map") map.fog_mode = FogMode::MAP;
        else if (mode == "los") map.fog_mode = FogMode::LOS;
        else throw std::runtime_error("unknown map fog mode: " + it->second);
    }
    for (const auto& child : node.children) {
        if (child->name == "object" && lower(attr(*child, "name")) == "map_info") {
            collect_fog_property(*child, map);
        }
    }
}

} // namespace

MapData load_tmx_map(const Str& path_string) {
    const fs::path path = fs::absolute(fs::path(path_string)).lexically_normal();
    std::unique_ptr<XmlNode> root = parse_xml(path);
    if (root->name != "map") throw std::runtime_error("TMX root is not <map>");
    if (lower(attr(*root, "orientation")) != "orthogonal") {
        throw std::runtime_error("only orthogonal TMX maps are supported");
    }

    MapData map;
    map.width = parse_i32(attr(*root, "width"), "map width");
    map.height = parse_i32(attr(*root, "height"), "map height");
    if (map.width <= 0 || map.height <= 0) {
        throw std::runtime_error("TMX map dimensions must be positive");
    }
    const std::size_t tile_count =
        static_cast<std::size_t>(map.width) * static_cast<std::size_t>(map.height);
    if (tile_count > static_cast<std::size_t>(std::numeric_limits<i32>::max())) {
        throw std::runtime_error("TMX map is too large");
    }
    map.terrain.assign(tile_count, TERRAIN_NONE);
    map.terrain_set.assign(tile_count, false);
    map.resource_pool.assign(tile_count, false);
    map.block_buildings.assign(tile_count, false);
    map.path_cost.assign(tile_count, 0);

    Vec<TilesetInfo> tilesets;
    for (const auto& child : root->children) {
        if (child->name == "tileset") tilesets.push_back(parse_tileset(*child, path));
    }
    std::sort(tilesets.begin(), tilesets.end(),
              [](const TilesetInfo& a, const TilesetInfo& b) {
                  return a.first_gid < b.first_gid;
              });

    bool saw_ground = false;
    for (const auto& child : root->children) {
        if (child->name != "layer") continue;
        const Str layer_name = lower(attr(*child, "name"));
        const i32 layer_width = attr(*child, "width").empty()
            ? map.width : parse_i32(attr(*child, "width"), "layer width");
        const i32 layer_height = attr(*child, "height").empty()
            ? map.height : parse_i32(attr(*child, "height"), "layer height");
        if (layer_width != map.width || layer_height != map.height) {
            throw std::runtime_error("TMX layer dimensions differ from the map");
        }
        const Vec<u32> gids = decode_layer(*child, tile_count);
        const bool ground = layer_name == "ground";
        const bool overlay = layer_name == "items" || layer_name == "objects";
        const bool override_layer = layer_name == "pathingoverride";
        if (ground) saw_ground = true;

        for (std::size_t index = 0; index < gids.size(); ++index) {
            if (ground && gids[index] == 0) {
                throw std::runtime_error("Ground layer contains an empty tile");
            }
            const TileInfo* tile = find_tile(tilesets, gids[index]);
            if ((ground || overlay || override_layer) && gids[index] != 0) {
                if (tile) merge_tile(map, index, *tile, override_layer);
                else if (ground) map.terrain_set[index] = true;
            }
            if (!tile) continue;
            const bool custom = !tile->custom_unit.empty();
            const Str& unit = custom ? tile->custom_unit : tile->unit;
            if (unit.empty()) continue;
            const i32 tx = static_cast<i32>(index % static_cast<std::size_t>(map.width));
            const i32 ty = static_cast<i32>(index / static_cast<std::size_t>(map.width));
            map.units.push_back({
                unit,
                custom,
                parse_team(tile->team, unit),
                (static_cast<f32>(tx) + 0.5f) * map.tile_size,
                (static_cast<f32>(ty) + 0.5f) * map.tile_size,
                tile->random_rotate,
                tile->subtype,
            });
        }
    }
    if (!saw_ground) throw std::runtime_error("TMX map has no Ground layer");

    collect_fog_property(*root, map);
    for (const auto& child : root->children) {
        if (child->name == "objectgroup") collect_fog_property(*child, map);
    }
    return map;
}

} // namespace rwrl
