#!/bin/bash

# 测试工具执行脚本
# 交互式菜单选择执行 build/test 目录下的测试工具

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
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

print_menu() {
    echo -e "${CYAN}请选择要执行的测试：${NC}"
    echo ""
    echo -e "${YELLOW}1. CircularQueue 测试${NC}"
    echo -e "${YELLOW}2. ThreadPool 测试${NC}"
    echo -e "${YELLOW}3. ThreadPool Config 测试${NC}"
    echo -e "${YELLOW}4. 执行所有测试${NC}"
    echo -e "${YELLOW}5. 退出${NC}"
    echo ""
    echo -n -e "${CYAN}请输入选项 (1-5): ${NC}"
}

# 检查 build/test 目录是否存在
BUILD_TEST_DIR="../build/test"
if [ ! -d "$BUILD_TEST_DIR" ]; then
    print_error "找不到 $BUILD_TEST_DIR 目录"
    echo "请先构建项目: mkdir -p build && cd build && cmake .. && make"
    exit 1
fi

# 检查测试工具是否存在
CIRCULAR_QUEUE_TEST="$BUILD_TEST_DIR/circular_queue_test"
THREAD_POOL_TEST="$BUILD_TEST_DIR/thread_pool_test"
THREAD_POOL_CONFIG_TEST="$BUILD_TEST_DIR/thread_pool_config_test"

# 显示菜单并获取用户选择
while true; do
    clear
    print_header "测试工具执行脚本"
    
    # 检查文件存在性并显示状态
    echo "🔍 检查测试工具状态："
    if [ -f "$CIRCULAR_QUEUE_TEST" ]; then
        print_success "CircularQueue 测试工具可用"
    else
        print_warning "CircularQueue 测试工具不存在"
    fi
    
    if [ -f "$THREAD_POOL_TEST" ]; then
        print_success "ThreadPool 测试工具可用"
    else
        print_warning "ThreadPool 测试工具不存在"
    fi
    
    if [ -f "$THREAD_POOL_CONFIG_TEST" ]; then
        print_success "ThreadPool Config 测试工具可用"
    else
        print_warning "ThreadPool Config 测试工具不存在"
    fi
    
    echo ""
    print_menu
    read -r choice
    
    case $choice in
        1)
            if [ ! -f "$CIRCULAR_QUEUE_TEST" ]; then
                print_error "circular_queue_test 不存在，请先构建项目"
                echo "按任意键继续..."
                read -n 1
                continue
            fi
            
            echo ""
            print_header "执行 CircularQueue 测试"
            "$CIRCULAR_QUEUE_TEST"
            TEST_RESULT=$?
            
            echo ""
            if [ $TEST_RESULT -eq 0 ]; then
                print_success "🎉 CircularQueue 测试通过！"
            else
                print_error "CircularQueue 测试失败"
            fi
            echo "按任意键继续..."
            read -n 1
            ;;
            
        2)
            if [ ! -f "$THREAD_POOL_TEST" ]; then
                print_error "thread_pool_test 不存在，请先构建项目"
                echo "按任意键继续..."
                read -n 1
                continue
            fi
            
            echo ""
            print_header "执行 ThreadPool 测试"
            "$THREAD_POOL_TEST"
            TEST_RESULT=$?
            
            echo ""
            if [ $TEST_RESULT -eq 0 ]; then
                print_success "🎉 ThreadPool 测试通过！"
            else
                print_error "ThreadPool 测试失败"
            fi
            echo "按任意键继续..."
            read -n 1
            ;;
            
        3)
            if [ ! -f "$THREAD_POOL_CONFIG_TEST" ]; then
                print_error "thread_pool_config_test 不存在，请先构建项目"
                echo "按任意键继续..."
                read -n 1
                continue
            fi
            
            echo ""
            print_header "执行 ThreadPool Config 测试"
            "$THREAD_POOL_CONFIG_TEST"
            TEST_RESULT=$?
            
            echo ""
            if [ $TEST_RESULT -eq 0 ]; then
                print_success "🎉 ThreadPool Config 测试通过！"
            else
                print_error "ThreadPool Config 测试失败"
            fi
            echo "按任意键继续..."
            read -n 1
            ;;
            
        4)
            echo ""
            print_header "执行所有测试"
            EXIT_CODE=0
            
            # 执行循环队列测试
            if [ -f "$CIRCULAR_QUEUE_TEST" ]; then
                echo "🔍 执行 CircularQueue 测试..."
                "$CIRCULAR_QUEUE_TEST"
                CIRCULAR_RESULT=$?
                
                if [ $CIRCULAR_RESULT -eq 0 ]; then
                    print_success "CircularQueue 测试通过！"
                else
                    print_error "CircularQueue 测试失败"
                    EXIT_CODE=1
                fi
                echo ""
            else
                print_warning "跳过 CircularQueue 测试（文件不存在）"
            fi
            
            # 执行线程池测试
            if [ -f "$THREAD_POOL_TEST" ]; then
                echo "🔍 执行 ThreadPool 测试..."
                "$THREAD_POOL_TEST"
                THREAD_POOL_RESULT=$?
                
                if [ $THREAD_POOL_RESULT -eq 0 ]; then
                    print_success "ThreadPool 测试通过！"
                else
                    print_error "ThreadPool 测试失败"
                    EXIT_CODE=1
                fi
            else
                print_warning "跳过 ThreadPool 测试（文件不存在）"
            fi
            
            # 执行配置测试
            if [ -f "$THREAD_POOL_CONFIG_TEST" ]; then
                echo "🔍 执行 ThreadPool Config 测试..."
                "$THREAD_POOL_CONFIG_TEST"
                CONFIG_RESULT=$?
                
                if [ $CONFIG_RESULT -eq 0 ]; then
                    print_success "ThreadPool Config 测试通过！"
                else
                    print_error "ThreadPool Config 测试失败"
                    EXIT_CODE=1
                fi
            else
                print_warning "跳过 ThreadPool Config 测试（文件不存在）"
            fi
            
            echo ""
            if [ $EXIT_CODE -eq 0 ]; then
                print_success "所有测试执行完成！"
            else
                print_error "部分测试失败"
            fi
            echo "按任意键继续..."
            read -n 1
            ;;
            
        5)
            echo ""
            print_success "退出测试脚本"
            exit 0
            ;;
            
        *)
            print_error "无效选项，请输入 1-5"
            echo "按任意键继续..."
            read -n 1
            ;;
    esac
done 