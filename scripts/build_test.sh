#!/bin/bash

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}======================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}======================================${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

# 检查是否在正确的目录
if [ ! -f "../test/CMakeLists.txt" ]; then
    print_error "请在脚本所在目录下，运行此脚本"
    exit 1
fi

print_header "构建并测试线程池"

# 删除对应的目录
rm -rf ../build

# 创建构建目录
mkdir -p ../build
cd ../build

# CMake配置
echo "正在配置项目..."
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

if [ $? -ne 0 ]; then
    print_error "CMake 配置失败"
    exit 1
fi

# 并行编译
echo "正在编译项目..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    print_error "编译失败"
    exit 1
fi

print_success "编译成功"