#!/bin/bash

# Shader compilation script for bgfx
# This compiles .sc shader files into platform-specific bytecode

SHADERC="$1"
SHADER_SRC_DIR="$2"
OUTPUT_DIR="$3"
PLATFORM="$4"

# Create output directory
mkdir -p "${OUTPUT_DIR}"

# Determine platform-specific arguments
case $PLATFORM in
    "metal")
        PLATFORM_ARG="--platform osx -p metal"
        VS_SUFFIX="_mtl"
        FS_SUFFIX="_mtl"
        ;;
    "glsl")
        PLATFORM_ARG="--platform linux -p 120"
        VS_SUFFIX="_glsl"
        FS_SUFFIX="_glsl"
        ;;
    "spirv")
        PLATFORM_ARG="--platform linux -p spirv"
        VS_SUFFIX="_spv"
        FS_SUFFIX="_spv"
        ;;
    *)
        echo "Unknown platform: $PLATFORM"
        exit 1
        ;;
esac

# Compile vertex shader
"${SHADERC}" -f "${SHADER_SRC_DIR}/vs_lines.sc" -o "${OUTPUT_DIR}/vs_lines${VS_SUFFIX}.bin" \
    --type vertex ${PLATFORM_ARG} -i ./

# Compile fragment shader
"${SHADERC}" -f "${SHADER_SRC_DIR}/fs_lines.sc" -o "${OUTPUT_DIR}/fs_lines${FS_SUFFIX}.bin" \
    --type fragment ${PLATFORM_ARG} -i ./

echo "Shaders compiled for platform: $PLATFORM"
