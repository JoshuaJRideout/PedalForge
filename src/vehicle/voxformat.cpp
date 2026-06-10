#include "vehicle/voxformat.h"
#include <map>
#include <sstream>
#include "core/bytes.h"

namespace vox {

namespace {

uint32_t fourCC(const char* s) {
    return static_cast<uint32_t>(s[0]) | (static_cast<uint32_t>(s[1]) << 8)
         | (static_cast<uint32_t>(s[2]) << 16) | (static_cast<uint32_t>(s[3]) << 24);
}

std::optional<PartType> partTypeFromName(const std::string& s) {
    if (s == "hull") return PartType::Hull;
    if (s == "engine") return PartType::Engine;
    if (s == "wing") return PartType::Wing;
    if (s == "track") return PartType::Track;
    if (s == "weapon") return PartType::Weapon;
    if (s == "sensor") return PartType::Sensor;
    if (s == "shieldgen") return PartType::ShieldGen;
    if (s == "cargo") return PartType::Cargo;
    if (s == "power") return PartType::Power;
    if (s == "cockpit") return PartType::Cockpit;
    if (s == "leg") return PartType::Leg;
    if (s == "jumpjets") return PartType::JumpJets;
    return std::nullopt;
}

std::optional<LocomotionClass> locomotionFromName(const std::string& s) {
    if (s == "tracked") return LocomotionClass::Tracked;
    if (s == "jet") return LocomotionClass::Jet;
    if (s == "walker") return LocomotionClass::Walker;
    if (s == "pilot") return LocomotionClass::Pilot;
    if (s == "static") return LocomotionClass::Static;
    return std::nullopt;
}

} // namespace

std::optional<VoxModel> parseVox(const std::vector<uint8_t>& bytes) {
    ByteReader r(bytes);
    if (r.u32() != fourCC("VOX ")) return std::nullopt;
    r.u32(); // version (150/200 both fine for SIZE/XYZI/RGBA)

    VoxModel model;
    bool haveSize = false, haveVoxels = false;

    // Chunk stream: id, contentBytes, childrenBytes, content... We walk the
    // whole file flat; MAIN's children are just subsequent chunks.
    while (r.ok && r.pos + 12 <= bytes.size()) {
        const uint32_t id = r.u32();
        const uint32_t contentBytes = r.u32();
        r.u32(); // childrenBytes
        if (!r.ok || r.pos + contentBytes > bytes.size()) return std::nullopt;
        const size_t contentEnd = r.pos + contentBytes;

        if (id == fourCC("SIZE") && !haveSize) {
            const int x = r.i32(), y = r.i32(), z = r.i32();
            if (x <= 0 || y <= 0 || z <= 0 || x > 256 || y > 256 || z > 256)
                return std::nullopt;
            model.dims = { x, y, z };
            haveSize = true;
        } else if (id == fourCC("XYZI") && haveSize && !haveVoxels) {
            const uint32_t count = r.u32();
            if (!r.ok || count > 256u * 256u * 256u) return std::nullopt;
            if (r.pos + static_cast<size_t>(count) * 4 > bytes.size()) return std::nullopt;
            model.voxels.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                const uint8_t x = r.u8(), y = r.u8(), z = r.u8(), color = r.u8();
                if (x >= model.dims.x || y >= model.dims.y || z >= model.dims.z || color == 0)
                    return std::nullopt;
                model.voxels.push_back({ Int3{ x, y, z }, color });
            }
            haveVoxels = true;
        } else if (id == fourCC("RGBA")) {
            if (contentBytes < 256 * 4) return std::nullopt;
            for (int i = 0; i < 256; ++i) model.palette[i] = r.u32();
        }
        r.pos = contentEnd; // skip any unread remainder of this chunk
    }
    if (!haveSize || !haveVoxels) return std::nullopt;
    return model;
}

std::optional<VehicleTemplate> templateFromVox(const VoxModel& model, const std::string& sidecar) {
    VehicleTemplate t;
    t.dims = model.dims;
    t.partIndex.assign(static_cast<size_t>(t.dims.x) * t.dims.y * t.dims.z, kEmptySubvoxel);

    std::map<int, int> colorToPart;
    std::istringstream in(sidecar);
    std::string line;
    while (std::getline(in, line)) {
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        std::istringstream ls(line);
        std::string keyword;
        if (!(ls >> keyword)) continue;

        if (keyword == "name") {
            ls >> t.name;
        } else if (keyword == "locomotion") {
            std::string value;
            ls >> value;
            const std::optional<LocomotionClass> loco = locomotionFromName(value);
            if (!loco) return std::nullopt;
            t.locomotion = *loco;
        } else if (keyword == "part") {
            int color = 0, hp = 0;
            std::string partName, typeName;
            float armor = 1.0f;
            if (!(ls >> color >> partName >> typeName >> hp)) return std::nullopt;
            ls >> armor; // optional
            const std::optional<PartType> type = partTypeFromName(typeName);
            if (!type || color < 1 || color > 255 || hp <= 0) return std::nullopt;
            if (colorToPart.count(color)) return std::nullopt; // duplicate mapping
            colorToPart[color] = t.addPart(partName, *type, hp, armor);
        } else {
            return std::nullopt; // unknown keyword: refuse, don't guess
        }
    }

    if (t.name.empty() || t.parts.empty() || t.corePart < 0) return std::nullopt;
    int hullCount = 0;
    for (const PartDef& p : t.parts)
        if (p.type == PartType::Hull) ++hullCount;
    if (hullCount != 1) return std::nullopt;

    for (const auto& [pos, color] : model.voxels) {
        const auto it = colorToPart.find(color);
        if (it == colorToPart.end()) return std::nullopt; // unmapped color
        t.partIndex[t.index(pos)] = static_cast<uint8_t>(it->second);
    }

    t.finalize();
    // Every part must actually own voxels and touch the rest of the craft.
    for (size_t i = 0; i < t.parts.size(); ++i)
        if (t.parts.size() > 1 && t.adjacency[i].empty()) return std::nullopt;
    return t;
}

} // namespace vox
