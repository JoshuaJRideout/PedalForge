#include "test_framework.h"
#include "render/raycast.h"

using namespace vox;

TEST(png_encoder_produces_valid_signature) {
    Image img(8, 8);
    img.put(3, 3, 255, 0, 0);
    const std::vector<uint8_t> png = encodePng(img);
    CHECK(png.size() > 50);
    const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    for (int i = 0; i < 8; ++i) CHECK(png[static_cast<size_t>(i)] == sig[i]);
}

TEST(raycast_renders_terrain_not_just_sky) {
    VoxelWorld w({ 64, 32, 64 });
    w.generate(3);
    PreviewCamera cam;
    cam.position = { 8.0f, 28.0f, 8.0f };
    cam.lookAt = { 32.0f, 10.0f, 32.0f };
    const Image img = renderWorld(w, cam, 64, 48);
    // Bottom half of the frame should be ground, not sky-blue.
    int groundish = 0;
    for (int y = 30; y < 48; ++y)
        for (int x = 0; x < 64; ++x) {
            const size_t i = (static_cast<size_t>(y) * 64 + x) * 3;
            if (img.rgb[i] >= img.rgb[i + 2]) ++groundish; // r >= b: not sky
        }
    CHECK(groundish > 300);
}

TEST(raycast_vehicle_damage_is_visible) {
    const VehicleTemplate& tmpl = VehicleTemplate::waspFighter();
    Vehicle intact(tmpl);
    Vehicle damaged(tmpl);
    Rng rng(1);
    damaged.applyHit({ 10, 2, 4 }, 60, DamageType::Kinetic, rng); // wing off

    const Image a = renderVehicle(tmpl, intact, 128, 96);
    const Image b = renderVehicle(tmpl, damaged, 128, 96);
    CHECK(a.rgb != b.rgb); // the missing wing changes the picture
}
