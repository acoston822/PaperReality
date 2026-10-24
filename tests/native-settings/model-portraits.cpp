// Generated geometry only; never imports game models or artwork.
#include "port/ui/NativeModelPortrait.h"
#include <iostream>

namespace M = Paperboat::NativeSettings::ModelPortrait;
namespace N = Paperboat::NativeSettings;
static void Require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
static void Word(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    std::memcpy(bytes.data() + offset, &value, 4);
}
static std::vector<uint8_t> Shape() {
    std::vector<uint8_t> bytes(512);
    const auto word = [&](size_t offset, uint32_t value) { Word(bytes, offset, value); };
    word(0, 32);
    word(32, 7);
    word(48, 52);
    word(52, 352);
    word(64, 2);
    word(68, 72);
    word(72, 80);
    word(76, 100);
    word(80, 2);
    word(84, 128);
    word(88, 1);
    word(92, 160);
    word(100, 2);
    word(104, 136);
    word(108, 1);
    word(112, 172);
    word(128, 192);
    word(136, 224);
    word(160, 0x5e);
    word(168, 320);
    word(172, 0x5e);
    word(180, 320);
    bytes[320] = 't';
    word(192, 0x01003006);
    word(196, 0);
    word(200, 0x05000204);
    word(208, 0xdf000000);
    word(224, 0x01003006);
    word(228, 72);
    word(232, 0x05000204);
    word(240, 0xdf000000);
    auto m = M::Identity();
    m[3][0] = 2;
    std::memcpy(bytes.data() + 352, m.data(), 64);
    return bytes;
}
static std::array<uint8_t, 4> Center(const N::Image& image) {
    Require(!image.rgba.empty(), "Model did not render");
    const auto* p = image.rgba.data() + (size_t(image.height / 2) * image.width + image.width / 2) * 4;
    return { p[0], p[1], p[2], p[3] };
}
int main() {
    const M::Vertex a { { -8, -8, 0 }, 0, 0 }, b { { 8, -8, 0 }, 2, 0 }, c { { 0, 8, 0 }, 1, 2 };
    std::vector<M::Vertex> vertices { a, b, c, a, b, c };
    auto bytes = Shape();
    const auto original = bytes;
    int loads = 0;
    const auto material = [&](const auto&) {
        ++loads;
        return M::Material { { 1, 1, { 255, 0, 0, 255 } }, 2, 2, 0, 0 };
    };
    auto mesh = M::ShapeReader(bytes, vertices, 24, true).Read(2, -1, material);
    Require(
        mesh.triangles.size() == 2 && mesh.materials.size() == 1 && loads == 1,
        "Shared model materials were repeatedly decoded"
    );
    Require(mesh.triangles[0].vertices[0].position[0] == -6, "Parent model transform was lost");
    auto single = M::ShapeReader(bytes, vertices, 24, true).Read(0, -1, material);
    Require(single.triangles.size() == 1, "Subtree selection included the surrounding stage");
    auto excluded = M::ShapeReader(bytes, vertices, 24, true).Read(2, 0, material);
    Require(excluded.triangles.size() == 1, "Hidden model part was included");
    Require(bytes == original, "Thumbnail reader modified imported model data");
    auto image = M::Render(single);
    Require(image.width == 256 && image.height == 256 && image.rgba[3] == 0, "Thumbnail size/transparency incorrect");
    Require(Center(image) == std::array<uint8_t, 4> { 255, 0, 0, 255 }, "Vertex color or texture sampling incorrect");
    auto reversed = single;
    std::swap(reversed.triangles[0].vertices[0], reversed.triangles[0].vertices[1]);
    Require(M::Render(reversed).rgba == image.rgba, "Back-facing geometry disappeared");
    auto front = single.triangles[0];
    front.material = 1;
    for (auto& v : front.vertices)
        v.position[2] += .01f;
    single.materials.push_back({ { 1, 1, { 0, 0, 255, 255 } }, 2, 2, 0, 0 });
    single.triangles.insert(single.triangles.begin(), front);
    Require(
        Center(M::Render(single)) == std::array<uint8_t, 4> { 0, 0, 255, 255 }, "Depth depended on submission order"
    );
    single.materials[1].image.rgba[3] = 128;
    const auto blended = Center(M::Render(single));
    Require(
        blended[0] >= 126 && blended[0] <= 128 && blended[2] == 128 && blended[3] == 255,
        "Transparent model parts did not blend over opaque geometry"
    );
    M::Mesh square;
    square.materials.push_back({ { 1, 1, { 255, 255, 255, 128 } }, 1, 1, 0, 0 });
    const M::Vertex d { { 8, 8, 0 } }, e { { -8, 8, 0 } };
    square.triangles = { { { a, b, d }, 0 }, { { a, d, e }, 0 } };
    const auto glass = M::Render(square);
    for (size_t i = 3; i < glass.rgba.size(); i += 4)
        Require(glass.rgba[i] <= 128, "Shared triangle edge blended twice");
    auto fixed = bytes;
    for (size_t i = 352; i < 416; i += 4)
        Word(fixed, i, 0);
    Word(fixed, 352, 0x00010000);
    Word(fixed, 360, 1);
    Word(fixed, 372, 0x00010000);
    Word(fixed, 376, 0x00020001);
    const auto fixedMesh = M::ShapeReader(fixed, vertices, 24, false).Read(0, -1, material);
    Require(fixedMesh.triangles[0].vertices[0].position[0] == -6, "Fixed-point model matrix was misread");
    Require(
        M::Wrap(-.25f, 0) == .75f && M::Wrap(1.25f, 1) == .75f && M::Wrap(-.25f, 2) == 0,
        "Clamp/mirror/wrap texture modes changed"
    );
    Require(M::Render(mesh, 1024).rgba.empty() && M::Render({}).rgba.empty(), "Output allocation was not bounded");
    auto broken = mesh;
    broken.materials[0].image.rgba.clear();
    Require(M::Render(broken).rgba.empty(), "Partial model material was accepted");
    const auto rejected = [&](std::vector<uint8_t> invalid) {
        try {
            M::ShapeReader(invalid, vertices, 24, true).Read(2, -1, material);
            return false;
        } catch (const std::runtime_error&) {
            return true;
        }
    };
    auto invalid = bytes;
    Word(invalid, 72, 32);
    Require(rejected(invalid), "Cyclic model tree was accepted");
    invalid = bytes;
    Word(invalid, 192, 0xde000000);
    Word(invalid, 196, 192);
    Require(rejected(invalid), "Cyclic display list was accepted");
    invalid = bytes;
    Word(invalid, 196, 1000000);
    Require(rejected(invalid), "Out-of-bounds vertices were accepted");
    invalid = bytes;
    Word(invalid, 48, 0xfffffff0);
    Require(rejected(invalid), "Out-of-bounds model record was accepted");
    invalid.resize(14);
    Require(rejected(invalid), "Truncated shape was accepted");
    std::cout << "Model portraits passed: selection, transforms, shared materials, depth/alpha, wrapping and malformed "
                 "data.\n";
}
