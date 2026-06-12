#pragma once

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>

/**
 * @brief C++11 标准实现的异步定时器，替代 QTimer
 */
class StdTimer {
public:
    StdTimer() : m_active(false) {}
    
    ~StdTimer() { 
        stop(); 
    }

    // 设置超时回调
    void setCallback(std::function<void()> callback) {
        m_callback = callback;
    }

    // 启动定时器 (毫秒)
    void start(int ms) {
        stop();
        m_intervalMs = ms;
        m_active = true;
        m_thread = std::thread([this]() {
            while (m_active) {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_cv.wait_for(lock, std::chrono::milliseconds(m_intervalMs), [this]() { return !m_active.load(); })) {
                    // m_active 为 false 时唤醒，表示被 stop()
                    break;
                }
                // 没被中断，触发回调
                if (m_active && m_callback) {
                    m_callback();
                }
            }
        });
    }

    // 停止定时器
    void stop() {
        if (m_active) {
            m_active = false;
            m_cv.notify_all();
            if (m_thread.joinable()) {
                m_thread.join();
            }
        }
    }

    // 是否正在运行
    bool isActive() const {
        return m_active;
    }

private:
    std::atomic<bool> m_active;
    int m_intervalMs{1000};
    std::function<void()> m_callback;
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};
