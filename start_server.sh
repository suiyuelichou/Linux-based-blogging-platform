#!/bin/bash

# 设置变量
SERVER_EXEC="./server"
PID_FILE="./server.pid"
STDOUT_LOG="./server_stdout.log"
STDERR_LOG="./server_stderr.log"

# 改进服务器运行检测机制
if [ -f "$PID_FILE" ]; then
    OLD_PID=$(cat "$PID_FILE")
    if ps -p "$OLD_PID" > /dev/null; then
        echo "服务器已经在运行中 (PID: $OLD_PID)"
        exit 1
    else
        echo "发现过期的PID文件，正在清理..."
        rm -f "$PID_FILE"
    fi
fi

# 再次检查是否有同名进程在运行
SERVER_PROCS=$(pgrep -f "$(basename $SERVER_EXEC)$" | wc -l)
if [ "$SERVER_PROCS" -gt 0 ]; then
    echo "检测到服务器进程可能在运行中，请先运行 ./stop_server.sh 确保停止"
    exit 1
fi

# 检查可执行文件是否存在
if [ ! -f "$SERVER_EXEC" ]; then
    echo "服务器可执行文件不存在，尝试编译..."
    make clean && make
    if [ ! -f "$SERVER_EXEC" ]; then
        echo "编译失败，请检查源代码"
        exit 1
    fi
    echo "编译成功"
fi

# 启动服务器
echo "正在启动服务器..."
nohup "$SERVER_EXEC" > "$STDOUT_LOG" 2> "$STDERR_LOG" &

# 保存进程ID并验证服务器是否成功启动
PID=$!
sleep 1  # 等待短暂时间确保进程稳定

if ps -p $PID > /dev/null; then
    echo $PID > "$PID_FILE"
    echo "服务器已成功启动，进程ID: $PID"
    echo "日志文件位于: $STDOUT_LOG 和 $STDERR_LOG"
else
    echo "服务器启动失败，请检查日志文件"
    exit 1
fi