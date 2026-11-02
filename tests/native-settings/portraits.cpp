// Synthetic pixels only; no game data or texture pack is needed.
#include "port/ui/NativePortrait.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace N = Paperboat::NativeSettings;
namespace P = N::Portrait;
static void Require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
static N::Image Pattern(int width, int height) {
    N::Image image { width, height, { } };
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            image.rgba.insert(image.rgba.end(), { uint8_t(x * 29), uint8_t(y * 31), uint8_t((x + y) * 17), 255 });
    return image;
}
static std::array<uint8_t, 4> Pixel(const N::Image& image, int x, int y) {
    const auto* p = &image.rgba.at((size_t(y) * image.width + x) * 4);
    return { p[0], p[1], p[2], p[3] };
}
int main() {
    const auto hd = Pattern(8, 4);
    const auto exact = P::Assemble({ { 0, 0, 0, 2, 1, hd } });
    Require(
        exact.width == 8 && exact.height == 4 && exact.rgba == hd.rgba,
        "HD component was downsampled to its original game dimensions"
    );

    const auto low = Pattern(2, 2), detail = Pattern(4, 4);
    auto mixed = P::Assemble({ { -2, 0, 0, 2, 2, low }, { 0, -1, 0, 1, 1, detail } });
    Require(mixed.width == 12 && mixed.height == 12, "Mixed-resolution component bounds drifted");
    Require(
        Pixel(mixed, 0, 4) == Pixel(low, 0, 0) && Pixel(mixed, 7, 11) == Pixel(low, 1, 1),
        "Original-resolution component lost its position or proportions"
    );
    Require(Pixel(mixed, 11, 3) == Pixel(detail, 3, 3), "HD detail was discarded or misplaced");
    Require(Pixel(mixed, 0, 0)[3] == 0, "Empty canvas must remain transparent");

    const N::Image red { 1, 1, { 255, 0, 0, 128 } }, blue { 1, 1, { 0, 0, 255, 128 } };
    auto blended = P::Assemble({ { 0, 0, 1, 1, 1, red }, { 0, 0, 0, 1, 1, blue } });
    Require(
        Pixel(blended, 0, 0) == std::array<uint8_t, 4> { 170, 0, 85, 192 },
        "Antialiased overlapping parts must use source-over in depth order"
    );
    auto stable = P::Assemble({ { 0, 0, 0, 1, 1, blue }, { 0, 0, 0, 1, 1, red } });
    Require(stable.rgba == blended.rgba, "Equal-depth parts changed their original order");

    auto tall = Pattern(4, 8);
    auto stretched = P::Assemble({ { 0, 0, 0, 2, 2, tall } });
    Require(
        stretched.width == 8 && stretched.height == 8 && Pixel(stretched, 7, 7) == Pixel(tall, 3, 7),
        "Non-square replacement scale lost source rows or changed logical proportions"
    );
    auto bounded = P::Assemble({ { 0, 0, 0, 4096, 1, Pattern(4096, 1) } });
    Require(bounded.width == 2048 && bounded.height == 1, "Oversized output must respect the memory limit");
    Require(P::Assemble({ }).rgba.empty(), "Missing assets must stay blank");
    Require(P::Assemble({ { 0, 0, 0, 1, 1, { 8, 8, { 1 } } } }).rgba.empty(), "Truncated texture was accepted");
    Require(P::Assemble({ { 0, 0, 0, 0, 1, hd } }).rgba.empty(), "Invalid component bounds were accepted");
    std::cout << "Portrait checks passed: HD detail, mixed-scale alignment, alpha/depth, aspect and memory bounds.\n";
}
