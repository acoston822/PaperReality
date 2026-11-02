#include "PM64TextureFactory.h"
#include "UpgradeOrbMask.h"
#include "fast/resource/factory/TextureFactory.h"
#include "libultraship/bridge/consolevariablebridge.h"
#include "fast/resource/type/Texture.h"
#include "spdlog/spdlog.h"
#include <iomanip>
#include <sstream>

namespace PM64 {

std::shared_ptr<Ship::IResource> ResourceFactoryBinaryTextureV0::ReadResource(
    std::shared_ptr<Ship::File> file, std::shared_ptr<Ship::ResourceInitData> initData
) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto texture = std::make_shared<Fast::Texture>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);

    uint32_t type = reader->ReadUInt32();
    uint32_t width = reader->ReadUInt32();
    uint32_t height = reader->ReadUInt32();
    uint32_t imageDataSize = reader->ReadUInt32();

    texture->Type = (Fast::TextureType) type;
    texture->Width = static_cast<uint16_t>(width);
    texture->Height = static_cast<uint16_t>(height);
    texture->ImageDataSize = imageDataSize;

    texture->ImageData = new uint8_t[texture->ImageDataSize];
    reader->Read((char*) texture->ImageData, texture->ImageDataSize);

    return texture;
}


std::shared_ptr<Ship::IResource> ResourceFactoryBinaryTextureV1::ReadResource(
    std::shared_ptr<Ship::File> file, std::shared_ptr<Ship::ResourceInitData> initData
) {
    Fast::ResourceFactoryBinaryTextureV1 base;
    auto resource = base.ReadResource(file, initData);
    auto texture = std::dynamic_pointer_cast<Fast::Texture>(resource);
    if (texture == nullptr || initData == nullptr ||
        CVarGetInteger("gDeveloper.UpgradeOrbHDAlpha", 1) == 0) {
        return resource;
    }
    std::string_view path = initData->Path;
    if (path.starts_with("alt/")) {
        path.remove_prefix(4);
    }
    if (path != "effects/effect_gfx_motion_blur_flame/tex_0" ||
        texture->Type != Fast::TextureType::RGBA32bpp ||
        texture->Flags != TEX_FLAG_LOAD_AS_RAW ||
        texture->Width == 0 || texture->Height == 0 ||
        texture->ImageData == nullptr ||
        texture->ImageDataSize != (size_t)texture->Width * texture->Height * 4) {
        return resource;
    }
    // The shared base factory stores a view into File::Buffer. Check the view
    // before reading pixels, and normalize a private copy so archive bytes and
    // other consumers of that file buffer stay unchanged.
    if (file == nullptr || file->Buffer == nullptr || texture->mImageBuffer != file->Buffer) {
        return resource;
    }
    const auto offset = reinterpret_cast<const char*>(texture->ImageData) - file->Buffer->data();
    if (offset < 0 || (size_t)offset > file->Buffer->size() ||
        texture->ImageDataSize > file->Buffer->size() - (size_t)offset) {
        return resource;
    }
    auto corrected = std::make_shared<std::vector<char>>(
        texture->ImageData, texture->ImageData + texture->ImageDataSize
    );
    if (NormalizeUpgradeOrbMaskAlpha(path, reinterpret_cast<uint8_t*>(corrected->data()), corrected->size())) {
        texture->mImageBuffer = corrected;
        texture->ImageData = reinterpret_cast<uint8_t*>(corrected->data());
        SPDLOG_INFO("[upgrade-orb] HD intensity coverage corrected ({}x{}); RGB and scale retained",
                    texture->Width, texture->Height);
    }
    return resource;
}

} // namespace PM64
