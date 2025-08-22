#!/usr/bin/env python3
"""
Convert compiled bgfx shader binaries to C++ embedded bytecode headers.
Usage: python3 embed_shaders.py <input_dir> <output_header>
"""

import sys
import os
from pathlib import Path

def bytes_to_cpp_array(data, var_name):
    """Convert binary data to C++ uint8_t array initialization."""
    result = f"static const uint8_t {var_name}[] = {{\n"

    # Format bytes as hex, 16 per line
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_bytes = ', '.join(f'0x{b:02x}' for b in chunk)
        result += f"    {hex_bytes}"
        if i + 16 < len(data):
            result += ","
        result += "\n"

    result += "};\n"
    result += f"static const size_t {var_name}_size = sizeof({var_name});\n\n"
    return result

def embed_shaders(input_dir, output_header):
    """Generate embedded shader header from compiled binaries."""
    input_path = Path(input_dir)

    # Find shader binaries
    shader_files = {
        'vs_lines': None,
        'fs_lines': None
    }

    for shader_file in input_path.glob("*.bin"):
        name = shader_file.stem
        # Extract base name (remove platform suffix)
        base_name = name.split('_')[0] + '_' + name.split('_')[1]  # vs_lines or fs_lines
        if base_name in shader_files:
            shader_files[base_name] = shader_file

    # Generate header content
    header_content = f"""#pragma once

// Auto-generated embedded shaders for high-performance GPU rendering
// Generated from compiled bgfx shader binaries

#include <cstdint>
#include <cstddef>

namespace oscil::shaders {{

"""

    # Embed each shader
    for shader_name, shader_path in shader_files.items():
        if shader_path and shader_path.exists():
            with open(shader_path, 'rb') as f:
                shader_data = f.read()

            var_name = f"embedded_{shader_name}"
            header_content += f"// {shader_name} shader ({len(shader_data)} bytes)\n"
            header_content += bytes_to_cpp_array(shader_data, var_name)
        else:
            # Create empty placeholder
            var_name = f"embedded_{shader_name}"
            header_content += f"// {shader_name} shader (placeholder - not compiled)\n"
            header_content += f"static const uint8_t {var_name}[] = {{0x00}};\n"
            header_content += f"static const size_t {var_name}_size = 1;\n\n"

    header_content += "} // namespace oscil::shaders\n"

    # Write header file
    with open(output_header, 'w') as f:
        f.write(header_content)

    print(f"Generated embedded shader header: {output_header}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 embed_shaders.py <input_dir> <output_header>")
        sys.exit(1)

    embed_shaders(sys.argv[1], sys.argv[2])
