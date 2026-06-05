#!/bin/bash

# ============================================
# Moment - CUMT校园论坛 快速启动脚本
# ============================================

set -e

echo "🎓 Moment - CUMT校园论坛"
echo "========================"
echo ""

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 检查依赖
check_dependencies() {
    echo -e "${BLUE}检查依赖...${NC}"
    
    if ! command -v node &> /dev/null; then
        echo -e "${RED}❌ Node.js 未安装${NC}"
        exit 1
    fi
    
    if ! command -v cmake &> /dev/null; then
        echo -e "${RED}❌ CMake 未安装${NC}"
        exit 1
    fi
    
    if ! command -v g++ &> /dev/null; then
        echo -e "${RED}❌ G++ 编译器未安装${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}✅ 依赖检查通过${NC}"
    echo ""
}

# 编译后端
build_backend() {
    echo -e "${BLUE}编译后端服务...${NC}"
    cd echo-server
    
    if [ ! -d "build" ]; then
        mkdir build
    fi
    
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1
    make -j$(nproc) > /dev/null 2>&1
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ 后端编译成功${NC}"
    else
        echo -e "${RED}❌ 后端编译失败${NC}"
        exit 1
    fi
    
    cd ../..
    echo ""
}

# 启动后端
start_backend() {
    echo -e "${BLUE}启动后端服务...${NC}"
    cd echo-server/build
    ./echo_server &
    BACKEND_PID=$!
    cd ../..
    
    sleep 2
    
    if curl -s http://127.0.0.1:8080 > /dev/null 2>&1; then
        echo -e "${GREEN}✅ 后端服务运行在 http://127.0.0.1:8080${NC}"
    else
        echo -e "${YELLOW}⚠️  后端服务可能启动失败，请检查日志${NC}"
    fi
    
    echo ""
}

# 启动AI服务（可选）
start_ai_service() {
    read -p "是否启动AI服务？(y/n): " choice
    if [[ $choice == "y" || $choice == "Y" ]]; then
        echo -e "${BLUE}启动AI服务...${NC}"
        cd echo-server
        bash scripts/start_ai.sh &
        AI_PID=$!
        cd ..
        
        sleep 5
        
        if curl -s http://127.0.0.1:18080 > /dev/null 2>&1; then
            echo -e "${GREEN}✅ AI服务运行在 http://127.0.0.1:18080${NC}"
        else
            echo -e "${YELLOW}⚠️  AI服务可能启动失败${NC}"
        fi
        
        echo ""
    fi
}

# 启动前端
start_frontend() {
    echo -e "${BLUE}启动前端开发服务器...${NC}"
    cd echo-client
    
    if [ ! -d "node_modules" ]; then
        echo -e "${YELLOW}安装前端依赖...${NC}"
        npm install
    fi
    
    npm run dev &
    FRONTEND_PID=$!
    cd ..
    
    sleep 3
    echo -e "${GREEN}✅ 前端服务运行在 http://localhost:5173${NC}"
    echo ""
}

# 清理函数
cleanup() {
    echo ""
    echo -e "${YELLOW}正在停止所有服务...${NC}"
    kill $BACKEND_PID 2>/dev/null
    kill $FRONTEND_PID 2>/dev/null
    kill $AI_PID 2>/dev/null
    echo -e "${GREEN}所有服务已停止${NC}"
    exit 0
}

# 注册清理函数
trap cleanup SIGINT SIGTERM

# 主流程
main() {
    check_dependencies
    build_backend
    start_backend
    start_ai_service
    start_frontend
    
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}🎉 Moment 已成功启动！${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo -e "${BLUE}前端: http://localhost:5173${NC}"
    echo -e "${BLUE}后端: http://127.0.0.1:8080${NC}"
    echo -e "${BLUE}AI:   http://127.0.0.1:18080 (如果已启动)${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo -e "${YELLOW}按 Ctrl+C 停止所有服务${NC}"
    echo ""
    
    # 等待用户中断
    wait
}

# 显示帮助
if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    echo "用法: ./start.sh [选项]"
    echo ""
    echo "选项:"
    echo "  -h, --help     显示帮助信息"
    echo ""
    echo "说明:"
    echo "  此脚本会自动编译并启动Moment的所有服务"
    echo "  包括后端API、前端开发服务器和可选的AI服务"
    exit 0
fi

# 执行主流程
main
