#include "logger.h"

#include <iostream>
#include <filesystem>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace thread_pool_improved {

// 获取单例实例
Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

// 析构
Logger::~Logger() {
    Shutdown();
}

// 初始化日志系统
bool Logger::Initialize(const LoggerConfig& config) {
    if (initialized_) {
        std::cerr << "警告: 日志系统已经初始化，忽略重复初始化" << std::endl;
        return true;
    }

    config_ = config;

    try {
        // 创建sinks容器
        std::vector<spdlog::sink_ptr> sinks;

        // 添加控制台 sink
        if (config_.enable_console) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(ToSpdlogLevel(config_.level));
            console_sink->set_pattern(config_.pattern);
            sinks.push_back(console_sink);
        }

        // 添加文件 sink
        if (config_.enable_file) {
            // 创建日志目录
            if (!CreateLogDirectory(config_.file_path)) {
                std::cerr << "错误: 无法创建日志目录: " << config_.file_path << std::endl;
                return false;
            }
            // 使用旋转文件 sink
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                config_.file_path,
                config_.max_file_size,
                config_.max_files
            );
            file_sink->set_level(ToSpdlogLevel(config_.level));
            file_sink->set_pattern(config_.pattern);
            sinks.push_back(file_sink);
        }

        // 检查至少有一个 sink
        if (sinks.empty()) {
            std::cerr << "错误: 至少需要启用一个输出方式（控制台或文件）" << std::endl;
            return false;
        }

        // 创建日志器
        logger_ = std::make_shared<spdlog::logger>(config_.name, sinks.begin(), sinks.end());

        // 设置日志级别
        logger_->set_level(ToSpdlogLevel(config_.level));

        // 设置是否自动刷新
        if (config_.auto_flush) {
            logger_->flush_on(spdlog::level::info);
        }

        // 注册到 spdlog
        spdlog::register_logger(logger_);

        // 设置为默认日志器
        spdlog::set_default_logger(logger_);

        initialized_ = true;

        // 记录初始化成功日志
        LOG_INFO("日志系统初始化成功 - 名称: {}, 级别: {}, 控制台: {}, 文件: {}", 
                config_.name, 
                static_cast<int>(config_.level),
                config_.enable_console ? "启用" : "禁用",
                config_.enable_file ? config_.file_path : "禁用");

        return true;

    } catch (const std::exception& e) {
        std::cerr << "错误: 初始化日志系统失败: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "错误: 初始化日志系统失败: 未知错误" << std::endl;
        return false;
    }
}

// 设置日志级别
void Logger::SetLevel(LogLevel level) {
    if (!initialized_ || !logger_) {
        std::cerr << "警告: 日志系统未初始化，无法设置日志级别" << std::endl;
        return;
    }
    
    config_.level = level;
    logger_->set_level(ToSpdlogLevel(level));
    
    // 同时设置所有 sink 的级别
    for (auto& sink : logger_->sinks()) {
        sink->set_level(ToSpdlogLevel(level));
    }
    
    LOG_INFO("日志级别已更新为: {}", static_cast<int>(level));
}

// 刷新日志缓冲区
void Logger::Flush() {
    if (logger_) {
        logger_->flush();
    }
}

// 关闭日志系统
void Logger::Shutdown() {
    if (!initialized_) {
        return;
    }
    
    try {
        if (logger_) {
            LOG_INFO("日志系统正在关闭...");
            logger_->flush();
            spdlog::drop(config_.name);
            logger_.reset();
        }
        
        // 关闭 spdlog
        spdlog::shutdown();
        
        initialized_ = false;
        
    } catch (const std::exception& e) {
        std::cerr << "警告: 关闭日志系统时发生错误: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "警告: 关闭日志系统时发生错误: 未知错误" << std::endl;
    }
}

// 转换日志级别（从自定义枚举转为spdlog枚举）
spdlog::level::level_enum Logger::ToSpdlogLevel(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:    return spdlog::level::trace;
        case LogLevel::DEBUG:    return spdlog::level::debug;
        case LogLevel::INFO:     return spdlog::level::info;
        case LogLevel::WARN:     return spdlog::level::warn;
        case LogLevel::ERROR:    return spdlog::level::err;
        case LogLevel::CRITICAL: return spdlog::level::critical;
        case LogLevel::OFF:      return spdlog::level::off;
        default:                 return spdlog::level::info;
    }
}

// 创建日志目录
bool Logger::CreateLogDirectory(const std::string& file_path) {
    try {
        std::filesystem::path path(file_path);
        std::filesystem::path dir = path.parent_path();
        
        if (!dir.empty() && !std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: 创建日志目录失败: " << e.what() << std::endl;
        return false;
    }
}

}