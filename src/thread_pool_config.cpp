#include "thread_pool_config.h"
#include "logger.h"

#include <fstream>

namespace thread_pool_improved {
// 从JSON文件加载配置
ThreadPoolStruct ThreadPoolConfig::LoadFromFile(const std::string& config_path) {
    LOG_INFO("开始从文件加载线程池配置: {}", config_path);

    // 检查文件是否存在
    if (!std::filesystem::exists(config_path)) {
        LOG_ERROR("配置文件不存在: {}", config_path);
        throw std::runtime_error("配置文件不存在: " + config_path);
    }

    // 读取文件内容
    std::ifstream file(config_path);
    if (!file.is_open()) {
        LOG_ERROR("无法打开配置文件: {}", config_path);
        throw std::runtime_error("无法打开配置文件: " + config_path);
    }

    std::string json_content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();
    
    LOG_DEBUG("配置文件内容长度: {} 字节", json_content.length());
    
    auto config = LoadFromString(json_content);
    LOG_INFO("成功加载线程池配置 - 核心线程: {}, 最大线程: {}, 队列大小: {}", 
             config.core_threads, config.max_threads, config.max_queue_size);
    
    return config;
}

// 从JSON字符串加载配置
ThreadPoolStruct ThreadPoolConfig::LoadFromString(const std::string& json_str) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_str);     // 字符串解析为json格式
        ThreadPoolStruct config = j.get<ThreadPoolStruct>();    // json转为对应类型
        ValidateConfig(config);
        return config;
    } catch (const nlohmann::json::parse_error& e) {
        throw std::invalid_argument("JSON解析错误: " + std::string(e.what()));
    } catch (const nlohmann::json::exception& e) {
        throw std::invalid_argument("JSON配置错误: " + std::string(e.what()));
    } catch (...) {
        throw std::invalid_argument("JSON配置错误: 未知原因");
    }
}

// 验证配置参数的有效性
void ThreadPoolConfig::ValidateConfig(const ThreadPoolStruct& config) {
    if (config.core_threads == 0) {
        throw std::invalid_argument("核心线程数必须大于0");
    }
    
    if (config.max_threads < config.core_threads) {
        throw std::invalid_argument("最大线程数不能小于核心线程数");
    }
    
    // 推荐的最大线程数限制（避免过多线程）
    const size_t recommended_max = std::thread::hardware_concurrency() * 4;
    if (config.max_threads > recommended_max) {
        LOG_WARN("警告: 最大线程数: {} 超过推荐值: {} , 可能影响性能", config.max_threads, recommended_max);
    }
    
    // 推荐的队列大小限制
    if (config.max_queue_size > 100000) {
        LOG_WARN("警告: 队列大小: {} 过大，可能占用过多内存", config.max_queue_size);
    }
}

// 将字符串转为QueueFullPolicy枚举
QueueFullPolicy ThreadPoolConfig::StringToQueueFullPolicy(const std::string& policy_str) {
    if (policy_str == "BLOCK" || policy_str == "block") {
        return QueueFullPolicy::BLOCK;
    } else if (policy_str == "OVERWRITE" || policy_str == "overwrite") {
        return QueueFullPolicy::OVERWRITE;
    } else if (policy_str == "DISCARD" || policy_str == "discard") {
        return QueueFullPolicy::DISCARD;
    } else {
        throw std::invalid_argument("无效的队列满处理策略: " + policy_str + 
                                   " (有效值: BLOCK, OVERWRITE, DISCARD)");
    }
}

// 将QueueFullPolicy枚举转为字符串
std::string ThreadPoolConfig::QueueFullPolicyToString(QueueFullPolicy policy) {
    switch (policy) {
        case QueueFullPolicy::BLOCK:
            return "BLOCK";
        case QueueFullPolicy::OVERWRITE:
            return "OVERWRITE";
        case QueueFullPolicy::DISCARD:
            return "DISCARD";
        default:
            throw std::invalid_argument("未知的队列满处理策略");
    }
}

}