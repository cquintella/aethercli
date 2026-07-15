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
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#if defined(__APPLE__)
#include <mach/mach_init.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>
#include <mach/vm_map.h>
#endif
#if defined(__FreeBSD__)
#include <sys/vmmeter.h>
#include <vm/vm_param.h>
#endif
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
#elif defined(__APPLE__) || defined(__FreeBSD__)
    static float getCPUUsage() {
        // Dummy CPU stats fallback for macOS/FreeBSD to avoid complex Mach/sysctl CPU queries
        // for simple unix like support if needed, or implement sysctl logic
        // But for CPU usage on Apple, host_processor_info is typical.
#if defined(__APPLE__)
        mach_msg_type_number_t count;
        processor_cpu_load_info_data_t *cpuLoad;
        mach_port_t mach_port = mach_host_self();
        natural_t cpuCount;
        static unsigned long long last_user = 0, last_system = 0, last_idle = 0, last_nice = 0;

        if (host_processor_info(mach_port, PROCESSOR_CPU_LOAD_INFO, &cpuCount, (processor_info_array_t *)&cpuLoad, &count) == KERN_SUCCESS) {
            unsigned long long totalUser = 0, totalSystem = 0, totalIdle = 0, totalNice = 0;
            for (natural_t i = 0; i < cpuCount; i++) {
                totalUser += cpuLoad[i].cpu_ticks[CPU_STATE_USER];
                totalSystem += cpuLoad[i].cpu_ticks[CPU_STATE_SYSTEM];
                totalIdle += cpuLoad[i].cpu_ticks[CPU_STATE_IDLE];
                totalNice += cpuLoad[i].cpu_ticks[CPU_STATE_NICE];
            }
            vm_deallocate(mach_task_self(), (vm_address_t)cpuLoad, count * sizeof(*cpuLoad));
            
            unsigned long long totalDiff = (totalUser - last_user) + (totalSystem - last_system) + (totalIdle - last_idle) + (totalNice - last_nice);
            unsigned long long idleDiff = totalIdle - last_idle;
            
            last_user = totalUser;
            last_system = totalSystem;
            last_idle = totalIdle;
            last_nice = totalNice;

            if (totalDiff == 0) return 0.0f;
            return (float)((totalDiff - idleDiff) * 100.0 / totalDiff);
        }
#elif defined(__FreeBSD__)
        // Simple FreeBSD CPU load via sysctl kern.cp_time
        long cp_time[CPUSTATES];
        size_t len = sizeof(cp_time);
        static long last_cp_time[CPUSTATES] = {0};
        
        if (sysctlbyname("kern.cp_time", &cp_time, &len, NULL, 0) == 0) {
            long totalDiff = 0;
            long idleDiff = cp_time[CP_IDLE] - last_cp_time[CP_IDLE];
            for (int i = 0; i < CPUSTATES; i++) {
                totalDiff += cp_time[i] - last_cp_time[i];
                last_cp_time[i] = cp_time[i];
            }
            if (totalDiff == 0) return 0.0f;
            return (float)((totalDiff - idleDiff) * 100.0 / totalDiff);
        }
#endif
        return 0.0f;
    }

    static float getMemoryUsage() {
#if defined(__APPLE__)
        vm_size_t page_size;
        mach_port_t mach_port = mach_host_self();
        vm_statistics64_data_t vm_stats;
        mach_msg_type_number_t count = sizeof(vm_stats) / sizeof(natural_t);
        if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
            KERN_SUCCESS == host_statistics64(mach_port, HOST_VM_INFO64, (host_info64_t)&vm_stats, &count)) {
            long long free_memory = (int64_t)vm_stats.free_count * (int64_t)page_size;
            int64_t mem_physical = 0;
            size_t size = sizeof(mem_physical);
            sysctlbyname("hw.memsize", &mem_physical, &size, NULL, 0);

            if (mem_physical == 0) return 0.0f;
            return (float)(mem_physical - free_memory) / (float)mem_physical * 100.0f;
        }
#elif defined(__FreeBSD__)
        unsigned long mem_total = 0, mem_free = 0;
        size_t len_total = sizeof(mem_total);
        size_t len_free = sizeof(mem_free);
        unsigned int page_size = 0;
        size_t len_page = sizeof(page_size);

        if (sysctlbyname("hw.physmem", &mem_total, &len_total, NULL, 0) == 0 &&
            sysctlbyname("vm.stats.vm.v_free_count", &mem_free, &len_free, NULL, 0) == 0 &&
            sysctlbyname("hw.pagesize", &page_size, &len_page, NULL, 0) == 0) {
            
            unsigned long free_bytes = mem_free * page_size;
            if (mem_total == 0) return 0.0f;
            return (float)(mem_total - free_bytes) / (float)mem_total * 100.0f;
        }
#endif
        return 0.0f;
    }
#else
    static float getCPUUsage() { return 0.0f; }
    static float getMemoryUsage() { return 0.0f; }
#endif
};

} // namespace cli::system
