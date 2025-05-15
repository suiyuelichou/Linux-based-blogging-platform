#!/bin/bash

# 设置变量
SERVER_EXEC="./server"
PID_FILE="./server.pid"

# 函数：杀死进程并确认其终止
kill_process() {
    local pid=$1
    local proc_name=$2
    
    echo "正在关闭$proc_name (PID: $pid)..."
    kill "$pid"
    
    # 等待进程终止
    local count=0
    while ps -p "$pid" > /dev/null && [ "$count" -lt 10 ]; do
        echo "等待进程关闭...$count/10"
        sleep 1
        count=$((count+1))
    done
    
    # 如果进程仍然存在，强制终止
    if ps -p "$pid" > /dev/null; then
        echo "进程未响应，强制终止..."
        kill -9 "$pid"
        sleep 1
    fi
    
    # 最终确认
    if ps -p "$pid" > /dev/null; then
        echo "警告：无法终止进程 $pid"
        return 1
    else
        echo "进程已成功关闭"
        return 0
    fi
}

# 尝试从pid文件获取进程ID
if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if ps -p "$PID" > /dev/null; then
        if kill_process "$PID" "服务器"; then
            rm -f "$PID_FILE"
            echo "PID文件已清理"
        fi
    else
        echo "PID文件存在但进程不存在，清理PID文件..."
        rm -f "$PID_FILE"
    fi
else
    echo "未找到PID文件，尝试查找服务器进程..."
    
    # 使用更精确的方式查找服务器进程
    SERVER_PIDS=$(pgrep -f "$(basename $SERVER_EXEC)$")
    
    if [ -n "$SERVER_PIDS" ]; then
        SUCCESS=true
        
        for pid in $SERVER_PIDS; do
            if ! kill_process "$pid" "服务器进程"; then
                SUCCESS=false
            fi
        done
        
        if $SUCCESS; then
            echo "所有服务器进程已关闭"
        else
            echo "警告：部分服务器进程可能未成功关闭"
            exit 1
        fi
    else
        echo "未找到运行中的服务器进程"
    fi
fi

# 最终确认没有服务器进程在运行
FINAL_CHECK=$(pgrep -f "$(basename $SERVER_EXEC)$")
if [ -n "$FINAL_CHECK" ]; then
    echo "警告：仍有服务器进程在运行，PID: $FINAL_CHECK"
    exit 1
else
    echo "确认：没有服务器进程在运行"
fi 