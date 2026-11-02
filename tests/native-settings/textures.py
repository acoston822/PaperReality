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
        texture('alt/' + prefix + 'unsupported', 4, 2, b'\xff' * 8, kind=0)
        texture(prefix + 'i4', 2, 1, b'\xf0', kind=5)
        texture(prefix + 'i8', 1, 1, b'\x80', kind=6)
        texture(prefix + 'ia4', 2, 1, b'\xf6', kind=7)
        texture(prefix + 'ia8', 1, 1, b'\xc8', kind=8)
        texture(prefix + 'ia16', 1, 1, b'\x40\x80', kind=9)
        texture(prefix + 'ia16-short', 2, 1, b'\xff', kind=9)
        texture('alt/' + prefix + 'short', 4, 2, b'\xff', flags=1)
        texture('alt/' + prefix + 'claimed', 4, 2, b'\xff', flags=1, claimed_size=32)
        texture('alt/' + prefix + 'zero', 0, 2, b'\xff', flags=1)
        texture('alt/' + prefix + 'oversized', 5000, 5000, b'\xff', flags=1)
        alias('alt/' + prefix + 'missing-target', prefix + 'absent')
        archive.writestr('alt/' + prefix + 'bad-meta.meta', '{invalid json}')
        texture(prefix + 'ci8-colors', 256, 1, b'\0\0' * 255 + b'\xff\xff', kind=2)
        texture(prefix + 'ci8', 1, 1, b'\xff', kind=4)
        # Real lookup names, generated colors only, for the on-device report.
        for name in ('AutoJump', 'PowerJump', 'SuperJump', 'Berserker', 'Kaiden'):
            texture('icons/badge/' + name, 2, 1, b'\x01', kind=3)
            texture('icons/badge/' + name + '.pal', 16, 1, palette, kind=2)
        rgba('alt/icons/badge/PowerJump', 4, 2, (0, 0, 255, 255))
        alias('alt/icons/badge/SuperJump', prefix + 'aliases/target')
        texture('alt/icons/badge/Berserker', 4, 2, b'\xff', flags=1)
        # Not part of the detailed sample: the complete UI listing must find it.
        rgba('ui/input/report-test', 2, 2, (255, 255, 255, 255))
        rgba('alt/ui/input/report-test', 4, 4, (255, 255, 255, 255))

        # Synthetic shape trees at the production model paths. Every record,
        # triangle and material below is generated; no game bytes are included.
        def resource(name, resource_type, body):
            header = struct.pack('<4BIIQ', 0, 1, 0, 0, resource_type, 0, 0).ljust(64, b'\0')
            archive.writestr(name, header + body)

        def model(shape, count, material):
            blob = bytearray(32)

            def words(*values):
                offset = len(blob)
                blob.extend(struct.pack('<' + 'I' * len(values), *values))
                return offset

            root = words(7, 0, 0, 0, 0)
            group = words(0, 0, 0, count, 0)
            children = words(*([0] * count))
            text = len(blob)
            blob.extend(material.encode() + b'\0')
            while len(blob) % 4:
                blob.append(0)
            props = words(0x5e, 2, text)
            dlist = words(0x01003006, 0, 0x02140000, 0, 0x05000204, 0, 0xdf000000, 0)
            display = words(dlist, 0)
            for index in range(count):
                node = words(2, display, 1, props, 0)
                struct.pack_into('<I', blob, children + index * 4, node)
            struct.pack_into('<I', blob, 0, root)
            struct.pack_into('<I', blob, root + 16, group)
            struct.pack_into('<I', blob, group + 16, children)
            path = 'shapes/' + shape
            resource(path, 0x4F424C42, struct.pack('<I', len(blob)) + blob)
            vertices = b''.join(struct.pack('<hhhHhhBBBB', x, y, 0, 0, 0, 0, 255, 255, 255, 0)
                                for x, y in ((-8, -8), (8, -8), (0, 8)))
            resource(path + '/vtx', 0x4F565458, struct.pack('<I', 3) + vertices)

        model('omo_bt07_shape', 65, 'omo_denkitif')
        # Use an actual texture metadata key, with generated pixels only.
        model('trd_bt00_shape', 48, 'trd_komatif')
        for archive_name, name in (('omo_tex', 'omo_denkitif'), ('trd_tex', 'trd_komatif')):
            path = 'textures/' + archive_name + '/' + name
            rgba(path, 2, 2, (255, 0, 0, 255))
            rgba('alt/' + path, 8, 8, (0, 0, 255, 255))
