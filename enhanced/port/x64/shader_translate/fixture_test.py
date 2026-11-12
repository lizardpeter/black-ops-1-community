import pathlib
import struct
import subprocess
import sys
import tempfile

# Synthetic SM3 pair with position/UV inputs and one sampled texture.
vertex = [
    0xfffe0300,
    0x0200001f, 0x80000000, 0x900f0000,  # dcl_position v0
    0x0200001f, 0x80000005, 0x900f0001,  # dcl_texcoord v1
    0x0200001f, 0x80000000, 0xe00f0000,  # dcl_position o0
    0x0200001f, 0x80000005, 0xe00f0001,  # dcl_texcoord o1
    0x02000001, 0xe00f0000, 0x90e40000,  # mov o0, v0
    0x02000001, 0xe00f0001, 0x90e40001,  # mov o1, v1
    0x0000ffff,
]
pixel = [
    0xffff0300,
    0x0200001f, 0x80000005, 0x900f0000,  # dcl_texcoord v0
    0x0200001f, 0x90000000, 0xa00f0800,  # dcl_2d s0
    0x03000042, 0x800f0000, 0x90e40000, 0xa0e40800,  # texld r0, v0, s0
    0x02000001, 0x800f0800, 0x80e40000,  # mov oC0, r0
    0x0000ffff,
]

with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    for name, tokens in (("vertex", vertex), ("pixel", pixel)):
        (root / f"{name}.bin").write_bytes(struct.pack(f"<{len(tokens)}I", *tokens))
    vertex_path = root / "vertex.bin"
    pixel_path = root / "pixel.bin"
    prefix = root / "test"
    subprocess.run([sys.argv[1], vertex_path, pixel_path, prefix], check=True)
    for suffix in (".vert.spv", ".frag.spv"):
        data = prefix.with_suffix(suffix).read_bytes()
        words = struct.unpack(f"<{len(data) // 4}I", data)
        assert words[0] == 0x07230203
        cursor = 5
        while cursor < len(words):
            count = words[cursor] >> 16
            assert count and cursor + count <= len(words)
            cursor += count
    assert subprocess.run([sys.argv[1], pixel_path, vertex_path, prefix]).returncode != 0
