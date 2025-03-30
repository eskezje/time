#include <windows.h>
#include <stdio.h>
#include <intrin.h>
#include <stdlib.h>

// compare function for qsort (double)
int compare_double(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da < db) ? -1 : (da > db) ? 1 : 0;
}

// calibrate CPU frequency by performing multiple busy-wait runs.
// delay_us: the busy-wait duration in microseconds for each calibration run.
// runs: number of calibration runs to perform.
// returns the median frequency (cycles per second) as measured by __rdtsc.
double calibrate_cpu_frequency(int delay_us, int runs) {
    LARGE_INTEGER freq, qpc_start, qpc_end;
    QueryPerformanceFrequency(&freq);

    double* frequencies = malloc(runs * sizeof(double));
    if (!frequencies) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    for (int i = 0; i < runs; i++) {
        unsigned __int64 tsc_start, tsc_end;
        QueryPerformanceCounter(&qpc_start);
        tsc_start = __rdtsc();

        // busy-wait using QPC for delay_us microseconds.
        double ticks_per_us = (double)freq.QuadPart / 1000000.0;
        LONGLONG target = qpc_start.QuadPart + (LONGLONG)(delay_us * ticks_per_us);
        LARGE_INTEGER current;
        do {
            QueryPerformanceCounter(&current);
        } while (current.QuadPart < target);

        tsc_end = __rdtsc();
        // measure elapsed time using QPC (using current instead of qpc_end for consistency).
        double elapsed_sec = (double)(current.QuadPart - qpc_start.QuadPart) / freq.QuadPart;
        if (elapsed_sec <= 0.0)
            frequencies[i] = 0.0;
        else
            frequencies[i] = (double)(tsc_end - tsc_start) / elapsed_sec;
    }

    qsort(frequencies, runs, sizeof(double), compare_double);
    double median = frequencies[runs / 2];
    free(frequencies);
    return median;
}

// busy-wait using __rdtsc for a given number of microseconds using a known CPU frequency.
void busy_wait_rdtsc_us(int microseconds, double cpu_freq) {
    // calculate cycles required for the given microseconds.
    unsigned __int64 cycles_to_wait = (unsigned __int64)(microseconds * (cpu_freq / 1000000.0));
    unsigned __int64 start = __rdtsc();
    while ((__rdtsc() - start) < cycles_to_wait) {
        // busy wait.
    }
}

int main(void) {
    LARGE_INTEGER qpc_start, qpc_end, qpc_freq;
    QueryPerformanceFrequency(&qpc_freq);

    // calibrate CPU frequency using multiple runs for better accuracy.
    const int cal_runs = 30;
    const int calibration_delay_us = 100000; // 100,000 µs = 100 ms per calibration run.
    double cpu_freq = calibrate_cpu_frequency(calibration_delay_us, cal_runs);
    double cpu_freq_mhz = cpu_freq / 1000000.0;
    printf("Calibrated CPU Frequency: %.2f MHz\n", cpu_freq_mhz);

    // Target busy-wait delay (in microseconds).
    const int target_delay_us = 1000;

    // main loop: use the RDTSC busy-wait to "sleep" for 1 ms and compare QPC measurements.
    while (1) {
        QueryPerformanceCounter(&qpc_start);
        unsigned __int64 tsc_start = __rdtsc();

        // busy wait for the target delay using rdtsc.
        busy_wait_rdtsc_us(target_delay_us, cpu_freq);

        unsigned __int64 tsc_end = __rdtsc();
        QueryPerformanceCounter(&qpc_end);

        // calculate elapsed time in milliseconds using QPC.
        double elapsed_qpc_ms = (double)(qpc_end.QuadPart - qpc_start.QuadPart) * 1000.0 / qpc_freq.QuadPart;
        // calculate elapsed time in milliseconds from rdtsc cycles.
        unsigned __int64 elapsed_cycles = tsc_end - tsc_start;
        double elapsed_rdtsc_ms = (double)elapsed_cycles * 1000.0 / cpu_freq;

        printf("Target: 1.00 ms | QPC: %.2f ms | RDTSC: %.2f ms (cycles: %llu)\n",
            elapsed_qpc_ms, elapsed_rdtsc_ms, elapsed_cycles);

        Sleep(250);
    }

    return 0;
}
