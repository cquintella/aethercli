/*
 * Copyright (c) 2026 caq@intelliurb.com
 * PROPRIEDADE DA INTELLIURB - SOMENTE USO INTERNO
 * A cópia e o uso devem ser expressamente autorizados.
 */
#pragma once
#include <string>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>

namespace cli::log {

class Logger {
public:
    static void setLogPath(const std::string& path) {
        std::lock_guard<std::mutex> lock(log_mutex);
        log_path = path;
    }

    static void setEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(log_mutex);
        log_enabled = enabled;
    }

    // Linhas no formato syslog: "Mmm dd hh:mm:ss <facility> <severity>: mensagem".
    // Facility fixa "user" (aplicação de nível de usuário).
    static void info(const std::string& message) { write("info", message); }
    static void error(const std::string& message) { write("error", message); }

private:
    static void write(const char* severity, const std::string& message) {
        std::lock_guard<std::mutex> lock(log_mutex);

        if (!log_enabled || log_path.empty()) return;

        std::ofstream file(log_path, std::ios::app);
        if (!file.is_open()) return;

        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);

        file << std::put_time(std::localtime(&t), "%b %e %H:%M:%S")
             << " user " << severity << ": " << message << std::endl;
    }

    static inline std::string log_path = "";
    static inline bool log_enabled = false;
    static inline std::mutex log_mutex;
};

} // namespace cli::log
