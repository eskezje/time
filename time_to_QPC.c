#include <windows.h>
#include <stdio.h>
#include <intrin.h>
#include <stdlib.h>

// Function to compare unsigned __int64 values for qsort
int compare_uint64(const void* a, const void* b) {
    unsigned __int64 val_a = *(const unsigned __int64*)a;
    unsigned __int64 val_b = *(const unsigned __int64*)b;
    
    if (val_a < val_b) return -1;
    if (val_a > val_b) return 1;
    return 0;
}

// function to compare double values for qsort
int compare_double(const void* a, const void* b) {
    double val_a = *(const double*)a;
    double val_b = *(const double*)b;
    
    if (val_a < val_b) return -1;
    if (val_a > val_b) return 1;
    return 0;
}

// busy-wait function for precise timing
void busy_wait_us(int microseconds) {
    LARGE_INTEGER freq, start, current;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    
    // calculate how many QPC ticks for the requested microseconds
    double ticks_per_us = (double)freq.QuadPart / 1000000.0;
    LONGLONG target_count = start.QuadPart + (LONGLONG)(microseconds * ticks_per_us);
    
    do {
        QueryPerformanceCounter(&current);
    } while (current.QuadPart < target_count);
}

int main(void) {
    LARGE_INTEGER qpc, qpc_start, qpc_end, freq;
    const int iterations = 1000000;
    const int cal_runs = 30;
    const int discard_percent = 10; // discard top and bottom 10%
    unsigned __int64 tsc_start, tsc_end;
    unsigned __int64 *measurements = malloc(iterations * sizeof(unsigned __int64));
    
    if (measurements == NULL) {
        printf("Memory allocation failed\n");
        return 1;
    }
    
    double cpu_freq_mhz = 0.0;
    int i, j;
    
    QueryPerformanceFrequency(&freq);
    
    for (i = 0; i < 1000; i++) {
        QueryPerformanceCounter(&qpc);
        __rdtsc();
    }
    
    double* cpu_freqs = malloc(cal_runs * sizeof(double));
    if (cpu_freqs == NULL) {
        printf("Memory allocation failed\n");
        free(measurements);
        return 1;
    }
    
    // calibrate CPU frequency
    for (j = 0; j < cal_runs; j++) {
        QueryPerformanceCounter(&qpc_start);
        tsc_start = __rdtsc();
        
        busy_wait_us(100000);
        
        tsc_end = __rdtsc();
        QueryPerformanceCounter(&qpc_end);
        
        double elapsed_sec = (qpc_end.QuadPart - qpc_start.QuadPart) / (double)freq.QuadPart;
        if (elapsed_sec > 0) {
            cpu_freqs[j] = (tsc_end - tsc_start) / elapsed_sec;
        } else {
            cpu_freqs[j] = 0;
            printf("Warning: Zero elapsed time in calibration run %d\n", j);
        }
    }
    
    qsort(cpu_freqs, cal_runs, sizeof(double), compare_double);
    cpu_freq_mhz = cpu_freqs[cal_runs / 2] / 1000000.0;
    free(cpu_freqs);
    
    // measure QueryPerformanceCounter overhead using RDTSC
    for (i = 0; i < iterations; i++) {
        tsc_start = __rdtsc();
        QueryPerformanceCounter(&qpc);
        tsc_end = __rdtsc();
        
        measurements[i] = tsc_end - tsc_start;
    }
    
    // Sort measurements
    qsort(measurements, iterations, sizeof(unsigned __int64), compare_uint64);
    
    // calculate statistics (discard outliers)
    int discard = iterations * discard_percent / 100;
    unsigned __int64 total_cycles = 0;
    for (i = discard; i < iterations - discard; i++) {
        total_cycles += measurements[i];
    }
    
    int valid_samples = iterations - 2 * discard;
    double avg_cycles = (double)total_cycles / valid_samples;
    double avg_time_ns = (avg_cycles / (cpu_freq_mhz * 1000000.0)) * 1e9;
    
    printf("CPU Frequency: %.2f MHz\n", cpu_freq_mhz);
    printf("Min cycles: %llu\n", measurements[discard]);
    printf("Max cycles: %llu\n", measurements[iterations - discard - 1]);
    printf("Median cycles: %llu\n", measurements[iterations / 2]);
    printf("Average cycles for QueryPerformanceCounter: %.2f cycles\n", avg_cycles);
    printf("Average time for QueryPerformanceCounter: %.2f ns\n", avg_time_ns);
    
    free(measurements);

    printf("\nPress Enter to exit...");
    getchar();

    return 0;
}
