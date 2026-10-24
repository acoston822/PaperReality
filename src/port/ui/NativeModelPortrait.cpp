#include "NativeModelPortrait.h"
#include <fast/resource/type/Vertex.h>
#include <libultraship/libultra/gbi.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/resource/type/Blob.h>

#include "common.h"
#include "port/patches/MapTextureMeta.inc.c"

namespace Paperboat::NativeSettings {
namespace {
    struct ModelSpec {
        int actor;
        const char* shape;
        const char* archive;
        int model, excluded;
    };
    // Tree IDs from mapfs/omo_bt07_shape.h and mapfs/trd_bt00_shape.h.
    // Select only the subject's subtree, never the surrounding battle stage.
    constexpr ModelSpec models[] = {
        { ACTOR_TYPE_LIGHT_BULB, "omo_bt07_shape", "omo_tex", 0x2a, -1 },  // MODEL_kyu
        { ACTOR_TYPE_TOY_TANK, "omo_bt07_shape", "omo_tex", 0x40, 0x3e },  // MODEL_sensya, hide MODEL_shy
        { ACTOR_TYPE_FAKE_BOWSER, "trd_bt00_shape", "trd_tex", 0x2f, -1 }, // MODEL_nise_koppa
    };
    struct CachedModel {
        std::weak_ptr<Ship::ResourceManager> manager;
        std::vector<std::weak_ptr<Ship::Archive>> archives;
        bool alternate = false;
        Image image;
    };
    std::array<CachedModel, std::size(models)> cache;

    // A 256px portrait does not need full-size copies of every 4K material.
    // Bound temporary image memory while retaining more detail than its final
    // 44pt UIKit cell. Original resources and pack files are never rewritten.
    Image ThumbnailMaterial(Image source) {
        const int largest = std::max(source.width, source.height);
        if (largest <= 512 || !ModelPortrait::ValidImage(source))
            return source;
        const int width = std::max(1, source.width * 512 / largest);
        const int height = std::max(1, source.height * 512 / largest);
        Image result { width, height, std::vector<uint8_t>(size_t(width) * height * 4) };
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const size_t src = (size_t(y * source.height / height) * source.width + x * source.width / width) * 4;
                std::copy_n(source.rgba.data() + src, 4, result.rgba.data() + (size_t(y) * width + x) * 4);
            }
        return result;
    }
    ModelPortrait::Material LoadMaterial(const ModelSpec& spec, const std::string& name) {
        if (name.empty())
            return { { 1, 1, { 255, 255, 255, 255 } }, 1, 1, 0, 0 };
        for (const auto& archive : gMapTexArchives) {
            if (std::string_view(archive.archive) != spec.archive)
                continue;
            for (uint32_t i = 0; i < archive.count; ++i) {
                const auto& meta = archive.textures[i];
                if (name != meta.name)
                    continue;
                const std::string palette = std::string(meta.otrPath) + "_tlut";
                auto image = LoadImage(meta.otrPath, meta.mainFmt == G_IM_FMT_CI ? palette.c_str() : "");
                return { ThumbnailMaterial(std::move(image)), meta.mainW, meta.mainH, meta.mainWrapW, meta.mainWrapH };
            }
        }
        // Keep the placeholder when a material is unavailable, rather than
        // presenting an untextured or partially reconstructed actor.
        throw std::runtime_error("Missing thumbnail material");
    }
}

Image LoadModelPortrait(int actorType) {
    const auto* context = Ship::Context::GetRawInstance();
    auto manager = context ? context->GetResourceManager() : nullptr;
    auto archives = manager ? manager->GetArchiveManager() : nullptr;
    if (!archives)
        return {};
    for (size_t index = 0; index < std::size(models); ++index) {
        const auto& spec = models[index];
        if (spec.actor != actorType)
            continue;
        const auto mounted = archives->GetArchives();
        auto& cached = cache[index];
        bool same = cached.manager.lock() == manager && cached.alternate == manager->IsAltAssetsEnabled()
            && cached.archives.size() == mounted->size();
        if (same)
            for (size_t i = 0; i < mounted->size(); ++i)
                same = same && cached.archives[i].lock() == (*mounted)[i];
        if (same && !cached.image.rgba.empty())
            return cached.image;
        cached = {};
        const std::string shapePath = std::string("shapes/") + spec.shape;
        if (!archives->HasFile(shapePath) || !archives->HasFile(shapePath + "/vtx"))
            return {};
        try {
            auto blob = std::dynamic_pointer_cast<Ship::Blob>(manager->LoadResource(shapePath, true));
            auto vertices = std::dynamic_pointer_cast<Fast::Vertex>(manager->LoadResource(shapePath + "/vtx", true));
            if (!blob || !vertices || blob->Data.size() > 1024 * 1024 || vertices->VertexList.size() > 16384)
                return {};
            std::vector<ModelPortrait::Vertex> converted;
            converted.reserve(vertices->VertexList.size());
            for (const auto& vertex : vertices->VertexList)
                converted.push_back(
                    { { float(vertex.v.ob[0]), float(vertex.v.ob[1]), float(vertex.v.ob[2]) },
                      vertex.v.tc[0] / 32.f,
                      vertex.v.tc[1] / 32.f,
                      { float(vertex.v.cn[0]), float(vertex.v.cn[1]), float(vertex.v.cn[2]), float(vertex.v.cn[3]) } }
                );
#ifdef GBI_FLOATS
            constexpr bool floats = true;
#else
            constexpr bool floats = false;
#endif
            ModelPortrait::ShapeReader reader(blob->Data, converted, sizeof(Vtx), floats);
            const auto mesh =
                reader.Read(spec.model, spec.excluded, [&](const auto& name) { return LoadMaterial(spec, name); });
            auto image = ModelPortrait::Render(mesh);
            if (!image.rgba.empty()) {
                cached.manager = manager;
                cached.alternate = manager->IsAltAssetsEnabled();
                for (const auto& archive : *mounted)
                    cached.archives.push_back(archive);
                cached.image = image;
            }
            return image;
        } catch (const std::exception&) {
            return {};
        }
    }
    return {};
}
} // namespace Paperboat::NativeSettings
