#!/bin/bash

# 移动至脚本文件的上一级目录（确保后续操作正确）
cd "$(dirname "$0")/.."

# 清理旧构建
rm -rf build
mkdir -p build
cd build

# CMake配置
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# 并行编译
make -j$(nproc)