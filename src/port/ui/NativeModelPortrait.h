#pragma once

#include "NativeSettings.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string_view>

namespace Paperboat::NativeSettings {
Image LoadModelPortrait(int actorType, std::ostream* report = nullptr);

// A small, isolated renderer for static editor thumbnails. It never submits
// commands to Fast3D or touches the currently loaded map/model animator.
namespace ModelPortrait {
    class ReadError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };
    struct Vertex {
        std::array<float, 3> position {};
        float u = 0, v = 0;
        std::array<float, 4> color { 255, 255, 255, 255 };
    };
    struct Material {
        Image image;
        int width = 1, height = 1, wrapS = 0, wrapT = 0;
    };
    enum class AlphaMode {
        Vertex,
        Opaque,
        Cutout
    };
    struct Triangle {
        std::array<Vertex, 3> vertices;
        size_t material = 0;
        AlphaMode alpha = AlphaMode::Vertex;
    };
    struct Mesh {
        std::vector<Triangle> triangles;
        std::vector<Material> materials;
    };
    using Matrix = std::array<std::array<float, 4>, 4>;
    inline Matrix Identity() {
        Matrix m {};
        for (int i = 0; i < 4; ++i)
            m[i][i] = 1;
        return m;
    }
    inline Matrix Multiply(const Matrix& a, const Matrix& b) {
        Matrix result {};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                for (int k = 0; k < 4; ++k)
                    result[i][j] += a[i][k] * b[k][j];
        return result;
    }

    class ShapeReader {
    public:
        ShapeReader(
            std::span<const uint8_t> blob,
            std::span<const Vertex> vertices,
            size_t vertexStride,
            bool floats,
            std::ostream* report = nullptr
        )
            : blob(blob)
            , vertices(vertices)
            , vertexStride(vertexStride)
            , floats(floats)
            , report(report) {
        }
        Mesh Read(int model, int excluded, const std::function<Material(const std::string&)>& loadMaterial) {
            // Imported shapes retain native-endian N64-sized records. Vertex
            // commands use the port's exported Vtx stride; the vertex resource has
            // already converted positions and texture coordinates to native types.
            Walk(Word(0), Identity(), 0, model);
            if (report)
                *report << "  Shape tree nodes: " << index << "; selected range: " << first << ".." << model << '\n';
            if (first < 0)
                return {};
            Mesh mesh;
            std::vector<std::string> names;
            size_t materialBytes = 0;
            for (const auto& part : parts) {
                if (part.id < first || part.id > model || part.id == excluded)
                    continue;
                if (report)
                    *report << "  Model part " << part.id << "; display list offset " << part.displayList
                            << "; render mode " << part.renderMode << '\n';
                auto found = std::find(names.begin(), names.end(), part.texture);
                const size_t material = size_t(found - names.begin());
                if (found == names.end()) {
                    names.push_back(part.texture);
                    mesh.materials.push_back(loadMaterial(part.texture));
                    materialBytes += mesh.materials.back().image.rgba.size();
                    if (materialBytes > 32 * 1024 * 1024)
                        Invalid();
                }
                std::array<Vertex, 32> cache {};
                std::array<bool, 32> valid {};
                ReadList(part.displayList, part.world, cache, valid, mesh, material, part.renderMode, 0);
            }
            return mesh;
        }

    private:
        struct Part {
            int id;
            uint32_t displayList;
            Matrix world;
            std::string texture;
            uint32_t renderMode;
        };
        std::span<const uint8_t> blob;
        std::span<const Vertex> vertices;
        size_t vertexStride;
        bool floats;
        std::ostream* report;
        std::vector<Part> parts;
        int index = 0, first = -1;
        size_t commands = 0;
        [[noreturn]] static void Invalid(const std::string& reason = "Invalid thumbnail model") {
            throw ReadError(reason);
        }
        void Bounds(size_t offset, size_t size) const {
            if (offset > blob.size() || size > blob.size() - offset)
                Invalid("Model record outside shape buffer");
        }
        uint32_t Word(size_t offset) const {
            Bounds(offset, 4);
            uint32_t word;
            std::memcpy(&word, blob.data() + offset, 4);
            return word;
        }
        std::string Text(uint32_t offset) const {
            if (!offset)
                return {};
            std::string result;
            for (size_t i = 0; i < 128; ++i) {
                Bounds(size_t(offset) + i, 1);
                const char c = char(blob[offset + i]);
                if (!c)
                    return result;
                result += c;
            }
            Invalid();
        }
        Matrix ReadMatrix(uint32_t offset) const {
            if (!offset)
                return Identity();
            Bounds(offset, 64);
            Matrix m {};
            if (floats)
                std::memcpy(m.data(), blob.data() + offset, 64);
            else
                for (int i = 0; i < 4; ++i)
                    for (int j = 0; j < 2; ++j) {
                        const uint32_t whole = Word(offset + (i * 2 + j) * 4);
                        const uint32_t fraction = Word(offset + 32 + (i * 2 + j) * 4);
                        m[i][j * 2] = int32_t((whole & 0xffff0000) | (fraction >> 16)) / 65536.f;
                        m[i][j * 2 + 1] = int32_t((whole << 16) | (fraction & 0xffff)) / 65536.f;
                    }
            for (const auto& row : m)
                for (float v : row)
                    if (!std::isfinite(v) || std::abs(v) > 1e6f)
                        Invalid();
            return m;
        }
        void Walk(uint32_t offset, const Matrix& parent, int depth, int model) {
            if (!offset || depth > 32 || index >= 1024)
                Invalid();
            Bounds(offset, 20);
            const int start = index;
            Matrix world = parent;
            const uint32_t group = Word(offset + 16);
            if (group) {
                Bounds(group, 20);
                world = Multiply(ReadMatrix(Word(group)), parent);
                const uint32_t count = Word(group + 12), children = Word(group + 16);
                if (count > 512)
                    Invalid();
                Bounds(children, size_t(count) * 4);
                for (uint32_t i = 0; i < count; ++i)
                    Walk(Word(children + i * 4), world, depth + 1, model);
            }
            const int id = index++;
            if (id == model)
                first = start;
            if (Word(offset) != 2)
                return;
            const uint32_t display = Word(offset + 4), count = Word(offset + 8), properties = Word(offset + 12);
            if (count > 128)
                Invalid();
            Bounds(properties, size_t(count) * 12);
            std::string texture;
            uint32_t renderMode = 1;
            for (uint32_t i = 0; i < count; ++i) {
                if (Word(properties + i * 12) == 0x5e)
                    texture = Text(Word(properties + i * 12 + 8));
                else if (Word(properties + i * 12) == 0x5c)
                    renderMode = Word(properties + i * 12 + 8);
            }
            if (display && Word(display))
                parts.push_back({ id, Word(display), world, std::move(texture), renderMode });
        }
        void ReadList(
            uint32_t offset,
            const Matrix& world,
            std::array<Vertex, 32>& cache,
            std::array<bool, 32>& valid,
            Mesh& mesh,
            size_t material,
            uint32_t renderMode,
            int depth
        ) {
            if (!offset || depth > 16 || vertexStride == 0)
                Invalid();
            const auto triangle = [&](uint32_t word) {
                Triangle t;
                t.material = material;
                // Solid render modes ignore vertex alpha in the game's blender.
                // Cutout surfaces use texture coverage; translucent surfaces
                // retain the interpolated vertex opacity.
                if (renderMode <= 0x0c || renderMode == 0x2a)
                    t.alpha = AlphaMode::Opaque;
                else if (renderMode == 0x0d || renderMode == 0x0f || renderMode == 0x10 || renderMode == 0x2b)
                    t.alpha = AlphaMode::Cutout;
                for (int i = 0; i < 3; ++i) {
                    const int slot = ((word >> (16 - i * 8)) & 0xff) / 2;
                    if (slot >= 32 || !valid[slot])
                        Invalid();
                    t.vertices[i] = cache[slot];
                }
                if (mesh.triangles.size() >= 8192)
                    Invalid();
                mesh.triangles.push_back(std::move(t));
            };
            for (size_t cursor = offset;; cursor += 8) {
                if (++commands > 20000)
                    Invalid();
                const uint32_t w0 = Word(cursor), w1 = Word(cursor + 4), op = w0 >> 24;
                if (op == 0xdf)
                    return;
                if (op == 0xde) {
                    ReadList(w1, world, cache, valid, mesh, material, renderMode, depth + 1);
                    if ((w0 >> 16) & 1)
                        return;
                } else if (op == 1) {
                    const int count = (w0 >> 12) & 0xff, end = (w0 >> 1) & 0x7f, start = end - count;
                    const size_t source = w1 / vertexStride;
                    if (w1 % vertexStride || !count || start < 0 || end > 32 || source > vertices.size()
                        || size_t(count) > vertices.size() - source)
                        Invalid();
                    for (int i = 0; i < count; ++i) {
                        Vertex vertex = vertices[source + i];
                        const auto p = vertex.position;
                        for (int j = 0; j < 3; ++j)
                            vertex.position[j] =
                                p[0] * world[0][j] + p[1] * world[1][j] + p[2] * world[2][j] + world[3][j];
                        cache[start + i] = vertex;
                        valid[start + i] = true;
                    }
                } else if (op == 5 || op == 6 || op == 7) {
                    triangle(w0);
                    if (op != 5)
                        triangle(w1);
                } else if (op == 2) {
                    const uint32_t slot = (w0 & 0xffff) / 2, field = (w0 >> 16) & 0xff;
                    if (slot >= cache.size() || !valid[slot])
                        Invalid("ModifyVertex refers to an unloaded vertex");
                    if (field == 0x10) {
                        for (int channel = 0; channel < 4; ++channel)
                            cache[slot].color[channel] = (w1 >> (24 - channel * 8)) & 0xff;
                    } else if (field == 0x14) {
                        cache[slot].u = int16_t(w1 >> 16) / 32.f;
                        cache[slot].v = int16_t(w1 & 0xffff) / 32.f;
                    } else
                        Invalid("Unsupported ModifyVertex field " + std::to_string(field));
                } else if (op == 4 || op == 0xda) {
                    Invalid(
                        "Unsupported geometry opcode " + std::to_string(op) + " at offset " + std::to_string(cursor)
                    );
                }
            }
        }
    };

    inline bool ValidImage(const Image& image) {
        return image.width > 0 && image.height > 0 && image.width <= 4096 && image.height <= 4096
            && image.rgba.size() >= size_t(image.width) * image.height * 4;
    }
    inline float Wrap(float coordinate, int mode) {
        if (mode & 2)
            return std::clamp(coordinate, 0.f, std::nextafter(1.f, 0.f));
        const float tile = std::floor(coordinate);
        coordinate -= tile;
        if ((mode & 1) && std::fmod(std::abs(tile), 2.f) >= 1)
            coordinate = 1 - coordinate;
        return std::clamp(coordinate, 0.f, std::nextafter(1.f, 0.f));
    }
    inline Image Render(const Mesh& mesh, int size = 256) {
        if (mesh.triangles.empty() || mesh.triangles.size() > 8192 || size < 16 || size > 512)
            return {};
        auto triangles = mesh.triangles;
        std::array<float, 3> minimum { INFINITY, INFINITY, INFINITY }, maximum { -INFINITY, -INFINITY, -INFINITY };
        // A slight overhead, three-quarter view. Shape colors are already baked
        // into the vertices; retain them rather than applying scene lighting twice.
        for (auto& triangle : triangles) {
            if (triangle.material >= mesh.materials.size())
                return {};
            const auto& material = mesh.materials[triangle.material];
            if (!ValidImage(material.image) || material.width <= 0 || material.height <= 0)
                return {};
            for (auto& v : triangle.vertices) {
                const auto p = v.position;
                v.position = { .966f * p[0] + .259f * p[2], .978f * p[1] - .208f * (-.259f * p[0] + .966f * p[2]),
                               .208f * p[1] + .978f * (-.259f * p[0] + .966f * p[2]) };
                for (int i = 0; i < 3; ++i) {
                    if (!std::isfinite(v.position[i]) || std::abs(v.position[i]) > 1e8f)
                        return {};
                    minimum[i] = std::min(minimum[i], v.position[i]);
                    maximum[i] = std::max(maximum[i], v.position[i]);
                }
                if (!std::isfinite(v.u) || !std::isfinite(v.v) || std::abs(v.u) > 1e8f || std::abs(v.v) > 1e8f)
                    return {};
                for (float c : v.color)
                    if (!std::isfinite(c))
                        return {};
            }
        }
        const float extent = std::max(maximum[0] - minimum[0], maximum[1] - minimum[1]);
        if (extent < .0001f)
            return {};
        const float scale = (size - 12.f) / extent;
        for (auto& triangle : triangles)
            for (auto& v : triangle.vertices) {
                v.position[0] = size / 2.f + (v.position[0] - (minimum[0] + maximum[0]) / 2) * scale;
                v.position[1] = size / 2.f - (v.position[1] - (minimum[1] + maximum[1]) / 2) * scale;
            }
        const auto depth = [](const Triangle& t) {
            return t.vertices[0].position[2] + t.vertices[1].position[2] + t.vertices[2].position[2];
        };
        std::stable_sort(triangles.begin(), triangles.end(), [&](const auto& a, const auto& b) {
            return depth(a) < depth(b);
        });
        Image image { size, size, std::vector<uint8_t>(size_t(size) * size * 4) };
        std::vector<float> depths(size_t(size) * size, -INFINITY);
        const auto edge = [](const Vertex& a, const Vertex& b, float x, float y) {
            return (b.position[0] - a.position[0]) * (y - a.position[1])
                - (b.position[1] - a.position[1]) * (x - a.position[0]);
        };
        bool visible = false;
        size_t pixelBudget = 16 * 1024 * 1024;
        for (const auto& triangle : triangles) {
            const auto& a = triangle.vertices[0];
            const float signedArea =
                edge(a, triangle.vertices[1], triangle.vertices[2].position[0], triangle.vertices[2].position[1]);
            const auto& b = triangle.vertices[signedArea < 0 ? 2 : 1];
            const auto& c = triangle.vertices[signedArea < 0 ? 1 : 2];
            const auto& material = mesh.materials[triangle.material];
            const auto& tex = material.image;
            const float area = std::abs(signedArea);
            if (area < .0001f)
                continue;
            const auto covered = [](float value, const Vertex& p, const Vertex& q) {
                const float dy = q.position[1] - p.position[1], dx = q.position[0] - p.position[0];
                return value > 0 || (value == 0 && (dy < 0 || (dy == 0 && dx > 0)));
            };
            const int left = std::max(0, int(std::floor(std::min({ a.position[0], b.position[0], c.position[0] }))));
            const int right =
                std::min(size - 1, int(std::ceil(std::max({ a.position[0], b.position[0], c.position[0] }))));
            const int top = std::max(0, int(std::floor(std::min({ a.position[1], b.position[1], c.position[1] }))));
            const int bottom =
                std::min(size - 1, int(std::ceil(std::max({ a.position[1], b.position[1], c.position[1] }))));
            const size_t pixels = size_t(std::max(0, right - left + 1)) * std::max(0, bottom - top + 1);
            if (pixels > pixelBudget)
                return {};
            pixelBudget -= pixels;
            for (int y = top; y <= bottom; ++y)
                for (int x = left; x <= right; ++x) {
                    const float ea = edge(b, c, x + .5f, y + .5f);
                    const float eb = edge(c, a, x + .5f, y + .5f);
                    const float ec = edge(a, b, x + .5f, y + .5f);
                    if (!covered(ea, b, c) || !covered(eb, c, a) || !covered(ec, a, b))
                        continue;
                    const float wa = ea / area, wb = eb / area, wc = ec / area;
                    const size_t pixel = size_t(y) * size + x;
                    const float z = wa * a.position[2] + wb * b.position[2] + wc * c.position[2];
                    if (z < depths[pixel] - .0001f)
                        continue;
                    const int tx =
                        int(Wrap((wa * a.u + wb * b.u + wc * c.u) / material.width, material.wrapS) * tex.width);
                    const int ty =
                        int(Wrap((wa * a.v + wb * b.v + wc * c.v) / material.height, material.wrapT) * tex.height);
                    const size_t source = (size_t(ty) * tex.width + tx) * 4;
                    std::array<float, 4> color;
                    for (int channel = 0; channel < 4; ++channel)
                        color[channel] = tex.rgba[source + channel]
                            * std::clamp((wa * a.color[channel] + wb * b.color[channel] + wc * c.color[channel]) / 255,
                                         0.f, 1.f);
                    if (triangle.alpha == AlphaMode::Opaque)
                        color[3] = 255;
                    else if (triangle.alpha == AlphaMode::Cutout)
                        color[3] = tex.rgba[source + 3] >= 128 ? 255 : 0;
                    const float alpha = color[3] / 255;
                    if (alpha <= 0)
                        continue;
                    const float oldAlpha = image.rgba[pixel * 4 + 3] / 255.f;
                    const float resultAlpha = alpha + oldAlpha * (1 - alpha);
                    for (int channel = 0; channel < 3; ++channel)
                        image.rgba[pixel * 4 + channel] = uint8_t(
                            std::clamp(
                                std::lround(
                                    (color[channel] * alpha + image.rgba[pixel * 4 + channel] * oldAlpha * (1 - alpha))
                                    / resultAlpha
                                ),
                                0L, 255L
                            )
                        );
                    image.rgba[pixel * 4 + 3] = uint8_t(std::lround(resultAlpha * 255));
                    if (alpha >= .999f)
                        depths[pixel] = z;
                    visible = true;
                }
        }
        return visible ? image : Image {};
    }
} // namespace ModelPortrait
} // namespace Paperboat::NativeSettings
