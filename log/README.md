
同步/异步日志系统
===============
同步/异步日志系统主要涉及了两个模块，一个是日志模块，一个是阻塞队列模块,其中加入阻塞队列模块主要是解决异步写入日志做准备.
> * 自定义阻塞队列
> * 单例模式创建日志
> * 同步日志
> * 异步日志
> * 实现按天、超行分类

# 阻塞队列(Block Queue)实现分析

## 概述

这段代码实现了一个基于循环数组的阻塞队列(`block_queue`)，主要具有以下特点：

1. **循环数组结构**：通过取模运算，循环利用固定大小的内存空间
2. **线程安全**：使用互斥锁保证多线程环境下的操作安全
3. **阻塞机制**：通过条件变量实现队列为空时的阻塞等待
4. **模板设计**：支持任意数据类型

## 类定义与成员变量

```cpp
template <class T>
class block_queue
{
private:
    locker m_mutex;     // 互斥锁，保证线程安全
    cond m_cond;        // 条件变量，用于线程间通信
    
    T *m_array;         // 存储队列元素的动态数组
    int m_size;         // 当前队列中的元素数量
    int m_max_size;     // 队列的最大容量
    int m_front;        // 队首元素的索引(front指向第一个元素的前一个位置)
    int m_back;         // 队尾元素的索引
};
```

## 主要功能方法

### 构造与析构

```cpp
// 构造函数，初始化队列
block_queue(int max_size = 1000)

// 析构函数，释放内存
~block_queue()

// 清空队列
void clear()
```

### 状态查询

```cpp
// 判断队列是否已满
bool full() 

// 判断队列是否为空
bool empty() 

// 获取队首元素
bool front(T &value) 

// 获取队尾元素
bool back(T &value) 

// 获取当前队列大小
int size() 

// 获取队列最大容量
int max_size()
```

### 数据操作

```cpp
// 向队列添加元素(生产者)
bool push(const T &item)

// 从队列取出元素(消费者)，无超时版本
bool pop(T &item)

// 从队列取出元素(消费者)，带超时机制
bool pop(T &item, int ms_timeout)
```

## 循环数组实现原理

该队列使用循环数组实现，主要通过以下方式管理内存：

```cpp
// 入队操作 - 循环利用数组空间
m_back = (m_back + 1) % m_max_size;
m_array[m_back] = item;

// 出队操作 - 循环取出元素
m_front = (m_front + 1) % m_max_size;
item = m_array[m_front];
```

取模运算确保索引在有效范围内循环，当到达数组末尾时，自动回到数组开头，避免数组越界。

## 线程安全实现

每个公共方法在操作共享数据前都获取互斥锁，操作完成后释放锁：

```cpp
m_mutex.lock();
// 操作共享数据
m_mutex.unlock();
```

## 阻塞机制实现

### 生产者-消费者模型

1. **生产者(`push`)**：
   - 添加元素后广播通知所有等待的消费者
   - 队列已满时返回失败

2. **消费者(`pop`)**：
   - 队列为空时等待条件变量
   - 有元素时或被唤醒后取出元素

### 超时机制

`pop`方法的超时版本使用`timewait`设置等待超时：

```cpp
// 计算超时时间点
t.tv_sec = now.tv_sec + ms_timeout / 1000;
t.tv_nsec = (ms_timeout % 1000) * 1000;

// 等待直到有数据或超时
if (!m_cond.timewait(m_mutex.get(), t))
{
    m_mutex.unlock();
    return false;
}
```

## 使用场景

这种阻塞队列通常用于：

1. 多线程环境下的生产者-消费者模式
2. 异步日志系统（日志写入缓冲区）
3. 任务队列实现
4. 线程池中的任务分发

## 注意事项

1. 初始状态下`m_front = -1`，实际取出元素时先递增再访问
2. 内存管理需小心，析构函数中释放动态分配的数组
3. 条件变量等待必须与互斥锁配合使用
4. 队列满时的`broadcast`操作可能不必要，可以优化

## 依赖关系

代码依赖`../lock/locker.h`，其中包含互斥锁和条件变量的封装实现。



# 日志系统(Log System)实现分析

## 概述

这段代码实现了一个高效的日志系统，具有以下主要特点：

1. **单例模式**：使用静态实例确保全局只有一个日志对象
2. **异步日志**：支持异步写入，提高程序性能
3. **日志分级**：支持多级别日志（debug、info、warn、error）
4. **自动分割**：按日期和文件大小自动分割日志文件
5. **线程安全**：使用互斥锁保证多线程环境下的操作安全
6. **宏定义接口**：提供简洁的宏接口便于调用

## 类定义与成员变量

```cpp
class Log
{
private:
    // 文件相关
    char dir_name[128];          // 日志文件所在目录
    char log_name[128];          // 日志文件名
    FILE *m_fp;                  // 日志文件指针
    
    // 日志缓冲区
    char *m_buf;                 // 日志缓冲区
    int m_log_buf_size;          // 缓冲区大小
    
    // 日志分割相关
    int m_split_lines;           // 单个日志文件的最大行数
    long long m_count;           // 当前日志行数计数器
    int m_today;                 // 当前日期(天)，用于日期变更时创建新文件
    
    // 异步相关
    block_queue<string> *m_log_queue; // 阻塞队列，用于异步写入
    bool m_is_async;             // 是否启用异步写入模式
    
    // 线程安全与控制
    locker m_mutex;              // 互斥锁，保证线程安全
    int m_close_log;             // 是否关闭日志功能
};
```

## 主要功能方法

### 单例模式实现

```cpp
static Log *get_instance()
{
    static Log instance;
    return &instance;
}
```

这里使用C++11引入的线程安全的局部静态变量特性，实现了无锁的线程安全单例模式。

### 初始化

```cpp
bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);
```

初始化函数接受多个参数，用于配置日志系统：
- `file_name`：日志文件名
- `close_log`：是否关闭日志
- `log_buf_size`：日志缓冲区大小
- `split_lines`：单个日志文件的最大行数
- `max_queue_size`：异步模式下队列的大小（为0则使用同步模式）

### 日志写入

```cpp
void write_log(int level, const char *format, ...);
```

写入日志的核心函数，支持：
- 不同级别的日志（通过level参数区分）
- 格式化字符串（类似printf的格式）
- 自动添加时间戳和日志级别前缀

### 异步写入实现

```cpp
void *async_write_log()
{
    string single_log;
    while (m_log_queue->pop(single_log))
    {
        m_mutex.lock();
        fputs(single_log.c_str(), m_fp);
        m_mutex.unlock();
    }
}

static void *flush_log_thread(void *args)
{
    Log::get_instance()->async_write_log();
}
```

异步写入通过创建专门的写入线程，从阻塞队列中不断取出日志并写入文件。

### 宏定义接口

```cpp
#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}
```

为不同级别的日志提供了简洁的宏接口，使用者可以直接使用这些宏来写入日志。

## 关键实现细节

### 日志格式化与缓冲区管理

```cpp
int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                 my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                 my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, valst);
m_buf[n + m] = '\n';    // 在日志信息的末尾添加换行符
m_buf[n + m + 1] = '\0';
log_str = m_buf;
```

日志格式化分两步：
1. 先格式化时间戳和日志级别
2. 再格式化用户的日志内容
3. 最后添加换行符和字符串结束符

### 日志文件分割机制

```cpp
if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0)
{
    // 创建新的日志文件...
}
```

日志文件分割基于两个条件：
1. 日期变更（按天分割）
2. 当前日志文件行数达到设定的最大行数

新文件命名规则：
- 日期变更：`[目录名]/年_月_日_[日志名]`
- 行数达到上限：`[目录名]/年_月_日_[日志名].[计数器]`

### 同步与异步写入模式

```cpp
if (m_is_async && !m_log_queue->full())
{
    m_log_queue->push(log_str);
}
else
{
    m_mutex.lock();
    fputs(log_str.c_str(), m_fp);
    m_mutex.unlock();
}
```

日志系统支持两种写入模式：
1. **异步模式**：将日志消息放入队列，由专门的线程负责写入
2. **同步模式**：直接写入文件

即使在异步模式下，如果队列已满，也会自动切换到同步写入，确保日志不丢失。

## 性能优化设计

### 1. 异步写入

通过将日志写入操作与主业务逻辑解耦，减少I/O操作对主线程的影响。

### 2. 缓冲区管理

使用固定大小的缓冲区预先分配内存，避免频繁的内存分配和释放。

### 3. 批量刷新

日志不会立即写入磁盘，而是通过`flush`方法显式控制刷新时机，减少I/O操作次数。

### 4. 宏定义条件检查

日志宏在调用前检查`m_close_log`标志，避免在日志关闭时的不必要函数调用。

## 线程安全实现

所有涉及共享资源的操作都使用互斥锁保护：

```cpp
m_mutex.lock();
// 操作共享资源
m_mutex.unlock();
```

关键的共享资源包括：
1. 日志文件指针（m_fp）
2. 日志计数器（m_count）
3. 日志缓冲区（m_buf）

## 使用示例

```cpp
// 初始化日志系统
Log::get_instance()->init("./server.log", 0, 2000, 800000, 800);

// 写入不同级别的日志
LOG_DEBUG("%s", "This is a debug log");
LOG_INFO("User %d logged in", user_id);
LOG_WARN("Memory usage is high: %d%%", usage);
LOG_ERROR("Connection failed: %s", error_msg);
```

## 依赖关系

该日志系统依赖：
1. `block_queue.h`：提供阻塞队列实现，用于异步日志
2. POSIX线程库：用于创建异步写入线程
3. 标准库：文件操作、字符串处理等