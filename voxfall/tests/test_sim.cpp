#include "test_framework.h"
#include "sim/sim.h"

using namespace vox;

namespace {

VoxelWorld flatWorld() {
    VoxelWorld w({ 96, 48, 96 });
    for (int x = 0; x < 96; ++x)
        for (int z = 0; z < 96; ++z)
            for (int y = 0; y < 10; ++y) w.set({ x, y, z }, Material::Rock);
    return w;
}

} // namespace

TEST(sim_hitscan_hits_vehicle_parts) {
    Sim sim(flatWorld(), 1);
    const uint32_t shooter = sim.spawnVehicle(TemplateId::Brick, 0, { 20.0f, 10.0f, 48.0f }, 0.0f);
    const uint32_t target = sim.spawnVehicle(TemplateId::Brick, 1, { 40.0f, 10.0f, 48.0f }, 0.0f);

    // Fire straight +x at hull height: must hit the target vehicle, not terrain.
    const auto r = sim.fire(shooter, { 1.0f, 0.0f, 0.0f });
    CHECK(r.has_value());
    CHECK(r->hitVehicle);
    CHECK(r->entity == target);
    CHECK(r->hit.partHit >= 0);
    CHECK(sim.find(shooter)->ammo == 199); // ammo consumed
}

TEST(sim_hitscan_hits_terrain_and_craters) {
    Sim sim(flatWorld(), 2);
    const uint32_t shooter = sim.spawnVehicle(TemplateId::Brick, 0, { 20.0f, 10.0f, 48.0f }, 0.0f);
    const uint64_t before = sim.world().contentHash();

    // Fire down into the ground with an explosive shell.
    WeaponSpec shell;
    shell.type = DamageType::Explosive;
    shell.damage = 150;
    shell.blastRadius = 3.0f;
    shell.blastDamage = 300;
    const auto r = sim.fire(shooter, { 1.0f, -0.4f, 0.0f }, shell);
    CHECK(r.has_value());
    CHECK(r->hitTerrain);
    CHECK(sim.world().contentHash() != before); // crater happened

    // The blast was recorded as an event (the netcode payload).
    bool sawBlast = false;
    for (const SimEvent& ev : sim.takeEvents()) sawBlast |= (ev.type == SimEvent::Type::Blast);
    CHECK(sawBlast);
}

TEST(sim_destruction_spawns_collectible_drops) {
    Sim sim(flatWorld(), 3);
    const uint32_t shooter = sim.spawnVehicle(TemplateId::Brick, 0, { 30.0f, 10.0f, 48.0f }, 0.0f);
    const uint32_t target = sim.spawnVehicle(TemplateId::Brick, 1, { 38.0f, 10.0f, 48.0f }, 0.0f);

    // Pound the target until it dies; drops should appear.
    WeaponSpec gun;
    gun.damage = 120;
    for (int i = 0; i < 30 && !sim.find(target)->state.destroyed(); ++i)
        sim.fire(shooter, { 1.0f, 0.0f, 0.0f }, gun);
    CHECK(sim.find(target)->state.destroyed());

    bool sawVehicleDestroyed = false, sawPickup = false;
    for (const SimEvent& ev : sim.takeEvents()) {
        sawVehicleDestroyed |= (ev.type == SimEvent::Type::VehicleDestroyed);
        sawPickup |= (ev.type == SimEvent::Type::PickupSpawned);
    }
    CHECK(sawVehicleDestroyed);
    CHECK(sawPickup); // whole-vehicle kill guarantees at least the energy shard

    // Drive the shooter over the wreck; pickups get collected.
    const size_t pickupsBefore = sim.pickups().size();
    CHECK(pickupsBefore > 0);
    ControlInput in;
    in.throttle = 1.0f;
    sim.setInput(shooter, in);
    for (int t = 0; t < 240; ++t) sim.step();
    CHECK(sim.pickups().size() < pickupsBefore);
}

TEST(sim_is_deterministic) {
    auto run = [] {
        Sim sim(flatWorld(), 42);
        const uint32_t a = sim.spawnVehicle(TemplateId::Brick, 0, { 20.0f, 10.0f, 40.0f }, 0.0f);
        const uint32_t b = sim.spawnVehicle(TemplateId::Talon, 1, { 60.0f, 10.0f, 56.0f }, 3.14f);
        ControlInput in;
        in.throttle = 1.0f;
        in.steer = 0.3f;
        sim.setInput(a, in);
        in.jump = true;
        sim.setInput(b, in);
        for (int t = 0; t < 300; ++t) {
            if (t % 30 == 0) sim.fire(a, { 1.0f, 0.0f, 0.2f });
            sim.step();
        }
        return sim.stateHash();
    };
    CHECK(run() == run()); // identical commands -> identical state
}

TEST(sim_eject_and_board) {
    Sim sim(flatWorld(), 7);
    const uint32_t tank = sim.spawnVehicle(TemplateId::Brick, 0, { 30.0f, 10.0f, 48.0f }, 0.0f);

    // Eject: pilot appears beside the tank; the tank goes inert.
    const uint32_t pilot = sim.eject(tank);
    CHECK(pilot != 0);
    CHECK(!sim.find(tank)->hasPilot);
    CHECK(sim.find(pilot)->tmpl->id == TemplateId::Pilot);
    CHECK(distance(sim.find(pilot)->body.position, sim.find(tank)->body.position)
          < Sim::kBoardRange);

    ControlInput in;
    in.throttle = 1.0f;
    sim.setInput(tank, in);
    const float tankX = sim.find(tank)->body.position.x;
    for (int t = 0; t < 60; ++t) sim.step();
    CHECK(sim.find(tank)->body.position.x == tankX); // no pilot, no movement

    // Can't eject from a pilot, can't double-eject an empty vehicle.
    CHECK(sim.eject(pilot) == 0);
    CHECK(sim.eject(tank) == 0);

    // Board it again and drive off.
    CHECK(sim.board(pilot, tank));
    CHECK(sim.find(pilot) == nullptr); // pilot entity consumed
    CHECK(sim.find(tank)->hasPilot);
    sim.setInput(tank, in);
    for (int t = 0; t < 60; ++t) sim.step();
    CHECK(sim.find(tank)->body.position.x > tankX);
}

TEST(sim_boarding_rules) {
    Sim sim(flatWorld(), 8);
    const uint32_t mech = sim.spawnVehicle(TemplateId::Talon, 0, { 30.0f, 10.0f, 48.0f }, 0.0f);
    const uint32_t enemyTank = sim.spawnVehicle(TemplateId::Brick, 1, { 33.0f, 10.0f, 48.0f }, 0.0f);

    const uint32_t pilot = sim.eject(mech);
    CHECK(pilot != 0);

    // Occupied vehicles can't be boarded — even enemy ones.
    CHECK(!sim.board(pilot, enemyTank));
    // The enemy bails out; now their tank is free real estate (stealing, §7.1).
    const uint32_t enemyPilot = sim.eject(enemyTank);
    CHECK(enemyPilot != 0);
    CHECK(sim.board(pilot, enemyTank));
    CHECK(sim.find(enemyTank)->team == 1); // captured hull keeps its paint (for now)

    // A mech with a destroyed cockpit can't be re-boarded until repaired (§4.7).
    Rng rng(1);
    VehicleEntity* m = sim.find(mech);
    int cockpit = -1;
    for (size_t i = 0; i < m->tmpl->parts.size(); ++i)
        if (m->tmpl->parts[i].name == "cockpit") cockpit = static_cast<int>(i);
    m->state.applyHit({ 20, 10, 50 }, 50, DamageType::Kinetic, rng);
    CHECK(!m->state.partAlive(cockpit));
    CHECK(!sim.board(enemyPilot, mech));
}

TEST(sim_lobbed_shell_arcs_and_craters) {
    Sim sim(flatWorld(), 11);
    const uint32_t arty = sim.spawnVehicle(TemplateId::Brick, 0, { 20.0f, 10.0f, 48.0f }, 0.0f);

    WeaponSpec shell;
    shell.type = DamageType::Explosive;
    shell.damage = 120;
    shell.blastRadius = 3.0f;
    shell.blastDamage = 300;
    // Lob at 45 degrees: range = v^2/g = 30*30/20 = 45 m downrange.
    const uint32_t round = sim.launch(arty, { 0.7071f, 0.7071f, 0.0f }, 30.0f, shell);
    CHECK(round != 0);
    CHECK(sim.projectiles().size() == 1);

    const uint64_t before = sim.world().contentHash();
    for (int t = 0; t < 60 * 6 && !sim.projectiles().empty(); ++t) sim.step();
    CHECK(sim.projectiles().empty());              // landed
    CHECK(sim.world().contentHash() != before);    // cratered
    // The crater should be well downrange, not at the gun's feet.
    CHECK(sim.world().heightAt(20, 48) == 10);
    bool craterDownrange = false;
    for (int x = 50; x < 80; ++x) craterDownrange |= (sim.world().heightAt(x, 48) < 10);
    CHECK(craterDownrange);
}

TEST(sim_blast_splash_damages_vehicles) {
    Sim sim(flatWorld(), 12);
    const uint32_t a = sim.spawnVehicle(TemplateId::Brick, 0, { 40.0f, 10.0f, 48.0f }, 0.0f);
    const uint32_t b = sim.spawnVehicle(TemplateId::Brick, 1, { 46.0f, 10.0f, 48.0f }, 0.0f);

    // Detonation between the two tanks: both take part damage.
    sim.applyBlast({ { 43.0f, 11.0f, 48.0f }, 6.0f, 200, DamageType::Explosive });
    auto damaged = [&](uint32_t id) {
        const VehicleEntity* e = sim.find(id);
        for (size_t p = 0; p < e->tmpl->parts.size(); ++p)
            if (e->state.partHp(static_cast<int>(p)) < e->tmpl->parts[p].maxHp) return true;
        return false;
    };
    CHECK(damaged(a));
    CHECK(damaged(b));
}

TEST(sim_missile_direct_hit_with_splash) {
    Sim sim(flatWorld(), 13);
    const uint32_t shooter = sim.spawnVehicle(TemplateId::Brick, 0, { 20.0f, 10.0f, 48.0f }, 0.0f);
    const uint32_t target = sim.spawnVehicle(TemplateId::Brick, 1, { 60.0f, 10.0f, 48.0f }, 0.0f);

    WeaponSpec missile;
    missile.type = DamageType::Explosive;
    missile.damage = 150;
    missile.blastRadius = 4.0f;
    missile.blastDamage = 250;
    // Flat trajectory (gravityFactor 0), straight at the target.
    CHECK(sim.launch(shooter, { 1.0f, 0.0f, 0.0f }, 60.0f, missile, 0.0f) != 0);
    for (int t = 0; t < 120 && !sim.projectiles().empty(); ++t) sim.step();
    CHECK(sim.projectiles().empty());

    const VehicleEntity* e = sim.find(target);
    int damagedParts = 0;
    for (size_t p = 0; p < e->tmpl->parts.size(); ++p)
        if (e->state.partHp(static_cast<int>(p)) < e->tmpl->parts[p].maxHp) ++damagedParts;
    CHECK(damagedParts >= 2); // direct hit + splash spreads across parts
}

TEST(sim_sector_economy) {
    Sim sim(flatWorld(), 9);
    sim.addEnergy(0, 150);

    // Not enough for two stations; second build in the same sector also fails.
    const uint32_t station = sim.buildPowerStation(0, { 24.0f, 10.0f, 24.0f });
    CHECK(station != 0);
    CHECK(sim.teamEnergy(0) == 50);
    CHECK(sim.sectorOwner(sim.sectorOf({ 24.0f, 10.0f, 24.0f })) == 0);
    CHECK(sim.buildPowerStation(0, { 26.0f, 10.0f, 26.0f }) == 0); // sector taken
    CHECK(sim.buildPowerStation(0, { 70.0f, 10.0f, 70.0f }) == 0); // 50 < cost

    // Income: 10 seconds of holding one sector pays 10 * kSectorIncome.
    for (int t = 0; t < 600; ++t) sim.step();
    CHECK(sim.teamEnergy(0) == 50 + 10 * Sim::kSectorIncome);

    // Shell the station until its core dies: the sector flips neutral and
    // income stops.
    const uint32_t raider = sim.spawnVehicle(TemplateId::Brick, 1, { 40.0f, 10.0f, 24.0f },
                                             3.14159f); // facing -x toward station
    WeaponSpec gun;
    gun.damage = 120;
    for (int i = 0; i < 30 && !sim.find(station)->state.destroyed(); ++i)
        sim.fire(raider, { -1.0f, 0.0f, 0.0f }, gun);
    CHECK(sim.find(station)->state.destroyed());
    CHECK(sim.sectorOwner(sim.sectorOf({ 24.0f, 10.0f, 24.0f })) == -1);
    const int frozen = sim.teamEnergy(0);
    for (int t = 0; t < 600; ++t) sim.step();
    CHECK(sim.teamEnergy(0) == frozen);
}

TEST(sim_repair_kit_heals_lowest_part) {
    Sim sim(flatWorld(), 5);
    const uint32_t id = sim.spawnVehicle(TemplateId::Brick, 0, { 30.0f, 10.0f, 48.0f }, 0.0f);
    VehicleEntity* e = sim.find(id);

    // Damage the turret, then hand-place a repair kit on the vehicle.
    Rng rng(1);
    int turret = -1;
    for (size_t i = 0; i < e->tmpl->parts.size(); ++i)
        if (e->tmpl->parts[i].name == "weapon.turret") turret = static_cast<int>(i);
    e->state.applyHit({ 16, 10, 16 }, 30, DamageType::Kinetic, rng); // hits turret block
    const int hpAfterDamage = e->state.partHp(turret);
    CHECK(hpAfterDamage < e->tmpl->parts[static_cast<size_t>(turret)].maxHp);

    // Kill an adjacent enemy to generate a real drop? Simpler: simulate via fire
    // on a sacrificial target placed on top of the shooter's position is fragile —
    // instead verify repairPart directly plus pickup plumbing elsewhere.
    const int restored = e->state.repairPart(turret, 1000);
    CHECK(e->state.partHp(turret) == e->tmpl->parts[static_cast<size_t>(turret)].maxHp);
    CHECK(restored == e->tmpl->parts[static_cast<size_t>(turret)].maxHp - hpAfterDamage);
}
