/*
 * Copyright (c) 2026 caq@intelliurb.com
 */
#pragma once

// Depende de /proc: só faz sentido em Linux. Em outras plataformas as
// funções retornam 0 (reservado para a futura status bar).
#include <string>

#if defined(__linux__)
#include <fstream>
#include <sstream>
#endif

namespace cli::system {

class SystemStats {
public:
#if defined(__linux__)
    static float getCPUUsage() {
        static long double lastSum = 0, lastIdle = 0;
        std::ifstream file("/proc/stat");
        std::string line;
        if (!std::getline(file, line)) return 0.0f;

        std::string cpu;
        long double user = 0, nice = 0, system = 0, idle = 0, iowait = 0,
                    irq = 0, softirq = 0, steal = 0;
        std::stringstream ss(line);
        if (!(ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal)) {
            return 0.0f;
        }

        long double Idle = idle + iowait;
        long double NonIdle = user + nice + system + irq + softirq + steal;
        long double Total = Idle + NonIdle;

        long double totalDiff = Total - lastSum;
        long double idleDiff = Idle - lastIdle;

        lastSum = Total;
        lastIdle = Idle;

        if (totalDiff == 0) return 0.0f;
        return (float)((totalDiff - idleDiff) / totalDiff * 100.0L);
    }

    static float getMemoryUsage() {
        std::ifstream file("/proc/meminfo");
        std::string label, unit;
        long total = 0, free_mem = 0, available = 0;

        if (!(file >> label >> total >> unit)) return 0.0f;     // MemTotal
        if (!(file >> label >> free_mem >> unit)) return 0.0f;  // MemFree
        if (!(file >> label >> available >> unit)) return 0.0f; // MemAvailable

        if (total == 0) return 0.0f;
        return (float)(total - available) / (float)total * 100.0f;
    }
#else
    static float getCPUUsage() { return 0.0f; }
    static float getMemoryUsage() { return 0.0f; }
#endif
};

} // namespace cli::system
