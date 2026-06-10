#include "test_framework.h"
#include "sim/sim.h"

using namespace vox;

TEST(faction_rosters_are_well_formed) {
    for (int f = 0; f < static_cast<int>(Faction::Count); ++f) {
        for (int c = 0; c < static_cast<int>(UnitClass::Count); ++c) {
            const VehicleTemplate& t =
                factionTemplate(static_cast<Faction>(f), static_cast<UnitClass>(c));
            CHECK(!t.name.empty());
            CHECK(t.id != TemplateId::Count);
            CHECK(t.corePart >= 0);
            CHECK(t.occupiedCount() > 100);
            CHECK(!t.paint.empty()); // every faction unit is painted
            // No floating parts.
            for (size_t i = 0; i < t.parts.size(); ++i)
                if (t.parts.size() > 1) CHECK(!t.adjacency[i].empty());
            // byId round-trips the wire id.
            CHECK(VehicleTemplate::byId(t.id).id == t.id);
            // Class sanity.
            const UnitClass uc = static_cast<UnitClass>(c);
            if (uc == UnitClass::Fighter) CHECK(t.locomotion == LocomotionClass::Jet);
            if (uc == UnitClass::Mech) CHECK(t.locomotion == LocomotionClass::Walker);
            if (uc == UnitClass::PilotUnit) CHECK(t.locomotion == LocomotionClass::Pilot);
        }
    }
}

TEST(faction_fighters_lose_wings_like_anyone) {
    Rng rng(1);
    for (int f = 0; f < 4; ++f) {
        const VehicleTemplate& t =
            factionTemplate(static_cast<Faction>(f), UnitClass::Fighter);
        Vehicle v(t);
        CHECK(v.alivePartCountOfType(PartType::Wing) == 2);
        // Find a wing voxel and pound it until the part detaches.
        Int3 wingVoxel{ -1, -1, -1 };
        for (int z = 0; z < t.dims.z && wingVoxel.x < 0; ++z)
            for (int y = 0; y < t.dims.y && wingVoxel.x < 0; ++y)
                for (int x = 0; x < t.dims.x && wingVoxel.x < 0; ++x) {
                    const int part = t.partAt({ x, y, z });
                    if (part >= 0 && t.parts[static_cast<size_t>(part)].type == PartType::Wing)
                        wingVoxel = { x, y, z };
                }
        CHECK(wingVoxel.x >= 0);
        for (int i = 0; i < 20 && v.alivePartCountOfType(PartType::Wing) == 2; ++i)
            v.applyHit(wingVoxel, 30, DamageType::Kinetic, rng);
        CHECK(v.alivePartCountOfType(PartType::Wing) == 1);
        CHECK(!v.destroyed());
    }
}

TEST(faction_eject_spawns_faction_pilot) {
    VoxelWorld w({ 64, 32, 64 });
    for (int x = 0; x < 64; ++x)
        for (int z = 0; z < 64; ++z)
            for (int y = 0; y < 10; ++y) w.set({ x, y, z }, Material::Rock);
    Sim sim(std::move(w), 1);

    // Team 1 = Kessler: its tank ejects a Kessler pilot.
    const uint32_t tank = sim.spawnVehicle(TemplateId::KesselTank, 1, { 30.0f, 10.0f, 30.0f }, 0.0f);
    const uint32_t pilot = sim.eject(tank);
    CHECK(pilot != 0);
    CHECK(sim.find(pilot)->tmpl->id == TemplateId::KesselPilot);
    // And the pilot can steal a Choir grav-tank parked nearby.
    const uint32_t tide = sim.spawnVehicle(TemplateId::ChoirTank, 3, { 32.0f, 10.0f, 30.0f }, 0.0f);
    sim.eject(tide);
    CHECK(sim.board(pilot, tide));
}
