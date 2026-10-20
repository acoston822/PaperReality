"""Synthetic texture archive: generated pixels only, no game or pack assets."""
import json
import struct
import zipfile


def write_archive(path):
    with zipfile.ZipFile(path, 'w') as archive:
        def texture(name, width, height, pixels, kind=1, flags=0, claimed_size=None):
            # OTR header + Texture V1 body (the same factory used by HD packs).
            header = struct.pack('<4BIIQ', 0, 1, 0, 0, 0x4F544558, 1, 0).ljust(64, b'\0')
            body = struct.pack('<IIIIffI', kind, width, height, flags, 1.0, 1.0,
                               len(pixels) if claimed_size is None else claimed_size)
            archive.writestr(name, header + body + pixels)

        def rgba(name, width, height, color, flags=1, kind=1):
            texture(name, width, height, bytes(color) * width * height, kind, flags)

        def alias(name, target):
            archive.writestr(name + '.meta', json.dumps({
                'path': target, 'format': 'Binary', 'type': 'Texture', 'version': 1
            }))

        prefix = 'test-native/'
        palette = b'\xf8\x01\x07\xc1' + b'\0\0' * 14  # opaque red, green
        texture(prefix + 'colors', 16, 1, palette, kind=2)
        # An incompatible HD palette must fall back to the original CI palette.
        rgba('alt/' + prefix + 'colors', 16, 1, (0, 0, 255, 255))
        for name in ('cached', 'variant', 'partial', 'unsupported', 'short',
                     'claimed', 'zero', 'oversized', 'missing-target', 'bad-meta'):
            texture(prefix + name, 2, 1, b'\x01', kind=3)
        rgba('alt/' + prefix + 'cached', 4, 2, (0, 0, 255, 255))
        rgba('alt/' + prefix + 'variant', 4, 2, (0, 0, 255, 255))
        rgba('alt/' + prefix + 'variant@colors', 8, 4, (255, 192, 0, 255))
        # Raw RGBA data may retain the raster's original CI type.
        rgba('alt/' + prefix + 'raw-ci', 4, 2, (0, 255, 255, 128), kind=3)
        rgba(prefix + 'aliases/target', 12, 6, (255, 0, 255, 255), flags=0)
        alias('alt/' + prefix + 'alias-only', prefix + 'aliases/target')
        alias('alt/' + prefix + 'variant-alias@colors', prefix + 'aliases/target')
        alias(prefix + 'base-alias', prefix + 'aliases/target')
        texture('alt/' + prefix + 'unsupported', 4, 2, b'\xff' * 8, kind=6)
        texture('alt/' + prefix + 'short', 4, 2, b'\xff', flags=1)
        texture('alt/' + prefix + 'claimed', 4, 2, b'\xff', flags=1, claimed_size=32)
        texture('alt/' + prefix + 'zero', 0, 2, b'\xff', flags=1)
        texture('alt/' + prefix + 'oversized', 5000, 5000, b'\xff', flags=1)
        alias('alt/' + prefix + 'missing-target', prefix + 'absent')
        archive.writestr('alt/' + prefix + 'bad-meta.meta', '{invalid json}')
        texture(prefix + 'ci8-colors', 256, 1, b'\0\0' * 255 + b'\xff\xff', kind=2)
        texture(prefix + 'ci8', 1, 1, b'\xff', kind=4)
