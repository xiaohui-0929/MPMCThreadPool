#pragma once

#include "thread_pool_improved.h"

#include <nlohmann/json.hpp>

namespace thread_pool_improved {
// JSON配置管理器
class ThreadPoolConfig {
public:
    // 从JSON文件加载配置
    static ThreadPoolStruct LoadFromFile(const std::string& config_path);

    // 从JSON字符串加载配置
    static ThreadPoolStruct LoadFromString(const std::string& json_str);

    // 将字符串转为QueueFullPolicy枚举
    static QueueFullPolicy StringToQueueFullPolicy(const std::string& policy_str);

    // 将QueueFullPolicy枚举转为字符串
    static std::string QueueFullPolicyToString(QueueFullPolicy policy);
private:
    // 验证配置参数的有效性
    static void ValidateConfig(const ThreadPoolStruct& config);
};
}

// JSON序列化支持
namespace nlohmann {
    // 模板特化提供自定义类型的序列化和反序列化
    template<>
    struct adl_serializer<thread_pool_improved::QueueFullPolicy> {
        static void to_json(json& j, const thread_pool_improved::QueueFullPolicy& policy) {
            j = thread_pool_improved::ThreadPoolConfig::QueueFullPolicyToString(policy);
        }

        static void from_json(const json& j, thread_pool_improved::QueueFullPolicy& policy) {
            policy = thread_pool_improved::ThreadPoolConfig::StringToQueueFullPolicy(j.get<std::string>());
        }
    };

    template<>
    struct adl_serializer<thread_pool_improved::ThreadPoolStruct> {
        static void to_json(json& j, const thread_pool_improved::ThreadPoolStruct& config) {
            j = json{
                {"core_threads", config.core_threads},
                {"max_threads", config.max_threads},
                {"max_queue_size", config.max_queue_size},
                {"queue_full_policy", config.queue_full_policy},
                // 动态线程管理配置
                {"enable_dynamic_threads", config.enable_dynamic_threads},
                {"thread_creation_threshold", config.thread_creation_threshold},
                {"thread_idle_timeout", config.thread_idle_timeout.count()},
                {"load_check_interval", config.load_check_interval.count()},
                {"scale_up_threshold", config.scale_up_threshold},
                {"scale_down_threshold", config.scale_down_threshold},
                {"min_idle_time_for_removal", config.min_idle_time_for_removal.count()},
                {"max_consecutive_idle_checks", config.max_consecutive_idle_checks}
            };
        }

        static void from_json(const json& j, thread_pool_improved::ThreadPoolStruct& config) {
            if (j.contains("core_threads")) {
                config.core_threads = j.at("core_threads").get<size_t>();
            }
            if (j.contains("max_threads")) {
                config.max_threads = j.at("max_threads").get<size_t>();
            }
            if (j.contains("max_queue_size")) {
                config.max_queue_size = j.at("max_queue_size").get<size_t>();
            }
            if (j.contains("queue_full_policy")) {
                config.queue_full_policy = j.at("queue_full_policy").get<thread_pool_improved::QueueFullPolicy>();
            }
            
            // 动态线程管理配置
            if (j.contains("enable_dynamic_threads")) {
                config.enable_dynamic_threads = j.at("enable_dynamic_threads").get<bool>();
            }
            if (j.contains("thread_creation_threshold")) {
                config.thread_creation_threshold = j.at("thread_creation_threshold").get<size_t>();
            }
            if (j.contains("thread_idle_timeout_ms")) {
                config.thread_idle_timeout = std::chrono::milliseconds(j.at("thread_idle_timeout_ms").get<int64_t>());
            }
            if (j.contains("load_check_interval_ms")) {
                config.load_check_interval = std::chrono::milliseconds(j.at("load_check_interval_ms").get<int64_t>());
            }
            if (j.contains("scale_up_threshold")) {
                config.scale_up_threshold = j.at("scale_up_threshold").get<double>();
            }
            if (j.contains("scale_down_threshold")) {
                config.scale_down_threshold = j.at("scale_down_threshold").get<double>();
            }
            if (j.contains("min_idle_time_for_removal_ms")) {
                config.min_idle_time_for_removal = std::chrono::milliseconds(j.at("min_idle_time_for_removal_ms").get<int64_t>());
            }
            if (j.contains("max_consecutive_idle_checks")) {
                config.max_consecutive_idle_checks = j.at("max_consecutive_idle_checks").get<size_t>();
            }
            
            // 只有在max_threads未显式设置时才自动调整为至少等于core_threads
            if (!j.contains("max_threads") && config.max_threads < config.core_threads) {
                config.max_threads = config.core_threads;
            }
        }
    };
}