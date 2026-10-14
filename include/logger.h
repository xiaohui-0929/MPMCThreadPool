#pragma once
/*
日志类
*/

#include <spdlog/spdlog.h>

namespace thread_pool_improved {

// 日志级别枚举
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5,
    OFF = 6
};

// 日志配置
struct LoggerConfig {
    std::string name = "thread_pool";                               // 日志器名称
    std::string file_path = "../logs/thread_pool.log";              // 日志文件路径
    std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v";    // 日志格式
    LogLevel level = LogLevel::INFO;                                // 日志级别
    bool enable_console = true;                                     // 是否启用控制台输出
    bool enable_file = true;                                        // 是否启用文件输出
    size_t max_file_size = 10 * 1024 * 1024;                        // 最大文件大小 (10MB)
    size_t max_files = 5;                                           // 最大文件数量
    bool auto_flush = true;                                         // 是否自动刷新
};

// 日志单例管理
class Logger {
public:
    // 获取单例实例
    static Logger& GetInstance();

    // 初始化日志系统
    bool Initialize(const LoggerConfig& config = LoggerConfig{});

    // 设置日志级别
    void SetLevel(LogLevel level);

    // 获取当前日志级别
    LogLevel GetLevel() const { return config_.level; }

    // 刷新日志缓冲区
    void Flush();

    // 关闭日志系统
    void Shutdown();

    // 检查是否已经初始化
    bool IsInitialized() const { return initialized_; }

    // 记录不同类型日志

    template<typename... Args>
    void Trace(const std::string& fmt, Args&&... args) {
        if (logger_ && logger_->should_log(spdlog::level::trace)) {
            logger_->trace(fmt, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void Debug(const std::string& fmt, Args&&... args) {
        if (logger_ && logger_->should_log(spdlog::level::debug)) {
            logger_->debug(fmt, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void Info(const std::string& fmt, Args&&... args) {
        if (logger_ && logger_->should_log(spdlog::level::info)) {
            logger_->info(fmt, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void Warn(const std::string& fmt, Args&&... args) {
        if (logger_ && logger_->should_log(spdlog::level::warn)) {
            logger_->warn(fmt, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void Error(const std::string& fmt, Args&&... args) {
        if (logger_ && logger_->should_log(spdlog::level::err)) {
            logger_->error(fmt, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void Critical(const std::string& fmt, Args&&... args) {
        if (logger_ && logger_->should_log(spdlog::level::critical)) {
            logger_->critical(fmt, std::forward<Args>(args)...);
        }
    }

private:
    // 限制构造函数访问
    Logger() = default;

    ~Logger();

    // 禁止拷贝构造
    Logger(const Logger&) = delete;

    // 禁止拷贝赋值
    Logger& operator=(const Logger&) = delete;

    // 转换日志级别（从自定义枚举转为spdlog枚举）
    static spdlog::level::level_enum ToSpdlogLevel(LogLevel level);

    // 创建日志目录
    bool CreateLogDirectory(const std::string& file_path);

private:
    std::shared_ptr<spdlog::logger> logger_;    // spdlog 日志器
    bool initialized_ = false;                  // 是否初始化
    LoggerConfig config_;                       // 日志配置
};

// 便利宏定义
#define LOG_TRACE(fmt, ...)     thread_pool_improved::Logger::GetInstance().Trace(fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)     thread_pool_improved::Logger::GetInstance().Debug(fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)      thread_pool_improved::Logger::GetInstance().Info(fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)      thread_pool_improved::Logger::GetInstance().Warn(fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)     thread_pool_improved::Logger::GetInstance().Error(fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...)  thread_pool_improved::Logger::GetInstance().Critical(fmt, ##__VA_ARGS__)

// 条件日志宏
#define LOG_IF(condition, level, fmt, ...) \
    do { \
        if (condition) { \
            LOG_##level(fmt, ##__VA_ARGS__); \
        } \
    } while(0)

// 函数进入/退出跟踪宏（仅在 DEBUG 模式下有效）
#ifdef DEBUG
    #define LOG_FUNC_ENTER() LOG_TRACE("进入函数: {}", __FUNCTION__)
    #define LOG_FUNC_EXIT()     LOG_TRACE("退出函数: {}", __FUNCTION__)
#else
    #define LOG_FUNC_ENTER()    do {} while(0)
    #define LOG_FUNC_EXIT()     do {} while(0)
#endif

// 性能测量宏
#define LOG_PERF_START(name) \
    auto __perf_start_##name = std::chrono::high_resolution_clock::now()

#define LOG_PERF_END(name) \
    do { \
        auto __perf_end = std::chrono::high_resolution_clock::now(); \
        auto __perf_duration = std::chrono::duration_cast<std::chrono::microseconds>(__perf_end - __perf_start_##name); \
        LOG_DEBUG("性能测量 [{}]: {} 微秒", #name, __perf_duration.count()); \
    } while(0)
}