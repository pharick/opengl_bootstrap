#!/usr/bin/env python3
"""Convert a PNG (or anything Pillow reads) into an uncompressed KTX1 texture.

The project loads textures with gli, which reads KTX and DDS but not PNG, so
source images are converted offline and the .ktx is what ships.

    python3 tools/png_to_ktx.py assets/textures/checker.png
    python3 tools/png_to_ktx.py assets/textures/checker.png --srgb -o out.ktx

Two things this handles that are easy to get wrong:

  * Orientation. PNG stores the top row first; OpenGL's texture origin is
    lower-left. The image is flipped on the way in, so nothing has to flip at
    load time, and KTXorientation is written to say so.

  * Row padding. KTX1 stores uncompressed data at GL_UNPACK_ALIGNMENT = 4, so
    each row is padded up to a multiple of 4 bytes. (A 256px RGB row is 768
    bytes and needs none, but a 255px one would.)

Only single-level 2D textures are emitted; numberOfMipmapLevels = 1 and the
loader calls glGenerateMipmap. Baking a mip chain here would be the next step
if you ever want filtered mips authored offline.
"""

import argparse
import struct
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    sys.exit("Pillow is required: python3 -m pip install Pillow")

# GL enums, spelled out so this script needs no GL binding.
GL_UNSIGNED_BYTE = 0x1401
GL_RGB = 0x1907
GL_RGBA = 0x1908
GL_RGB8 = 0x8051
GL_RGBA8 = 0x8058
GL_SRGB8 = 0x8C41
GL_SRGB8_ALPHA8 = 0x8C43

KTX_IDENTIFIER = bytes(
    [0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A]
)
KTX_ENDIANNESS_LE = 0x04030201


def _key_value_block(pairs):
    """Serialize KTX key/value pairs, each padded to a 4-byte boundary."""
    out = b""
    for key, value in pairs:
        entry = key.encode("utf-8") + b"\0" + value.encode("utf-8") + b"\0"
        out += struct.pack("<I", len(entry)) + entry
        out += b"\0" * ((4 - (len(entry) % 4)) % 4)
    return out


def convert(source: Path, dest: Path, srgb: bool) -> None:
    image = Image.open(source)

    has_alpha = image.mode in ("RGBA", "LA", "PA") or "transparency" in image.info
    image = image.convert("RGBA" if has_alpha else "RGB")

    # GL's first pixel is lower-left; PNG's is upper-left.
    image = image.transpose(Image.Transpose.FLIP_TOP_BOTTOM)

    width, height = image.size
    channels = 4 if has_alpha else 3
    pixels = image.tobytes()

    gl_format = GL_RGBA if has_alpha else GL_RGB
    if srgb:
        internal_format = GL_SRGB8_ALPHA8 if has_alpha else GL_SRGB8
    else:
        internal_format = GL_RGBA8 if has_alpha else GL_RGB8

    # Pad each row up to GL_UNPACK_ALIGNMENT = 4.
    row_bytes = width * channels
    padded_row = (row_bytes + 3) & ~3
    if padded_row == row_bytes:
        level_data = pixels
    else:
        pad = b"\0" * (padded_row - row_bytes)
        level_data = b"".join(
            pixels[y * row_bytes : (y + 1) * row_bytes] + pad for y in range(height)
        )

    key_values = _key_value_block([("KTXorientation", "S=r,T=u")])

    header = KTX_IDENTIFIER + struct.pack(
        "<13I",
        KTX_ENDIANNESS_LE,
        GL_UNSIGNED_BYTE,  # glType
        1,  # glTypeSize
        gl_format,  # glFormat
        internal_format,  # glInternalFormat
        gl_format,  # glBaseInternalFormat
        width,
        height,
        0,  # pixelDepth: 0 for 2D
        0,  # numberOfArrayElements: 0 for non-array
        1,  # numberOfFaces: 1 for non-cubemap
        1,  # numberOfMipmapLevels: base level only
        len(key_values),
    )

    body = struct.pack("<I", len(level_data)) + level_data
    body += b"\0" * ((4 - (len(level_data) % 4)) % 4)  # mipPadding

    dest.write_bytes(header + key_values + body)

    print(
        f"{source.name} -> {dest.name}: {width}x{height}, "
        f"{channels} channels, internalFormat 0x{internal_format:04X}"
        f"{' (sRGB)' if srgb else ' (linear)'}, {len(level_data)} bytes of pixels"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="image file Pillow can read")
    parser.add_argument("-o", "--output", type=Path, help="defaults to <source>.ktx")
    parser.add_argument(
        "--srgb",
        action="store_true",
        help="tag the internal format as sRGB, so GL linearizes on fetch",
    )
    args = parser.parse_args()

    dest = args.output or args.source.with_suffix(".ktx")
    convert(args.source, dest, args.srgb)


if __name__ == "__main__":
    main()
