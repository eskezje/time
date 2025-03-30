#include <stdio.h>
#include <stdint.h>
#include <Windows.h>
#include <intrin.h>
#include <math.h>
#include <stdbool.h>

#define SAMPLES 10
#define SLEEP_MS 1000

typedef struct {
    uint64_t frequency;
    unsigned int processor_id;
} SampleData;

uint64_t measure_frequency(unsigned int* processor_id) {
    uint64_t start_tsc, end_tsc;
    unsigned int aux;
    
    start_tsc = __rdtscp(&aux);
    *processor_id = aux & 0xFF;
    
    Sleep(SLEEP_MS);
    
    end_tsc = __rdtscp(&aux);
    return end_tsc - start_tsc;
}

void calculate_statistics(const SampleData* samples, int count, 
                         double* mean, double* std_dev, double* cv,
                         uint64_t* min_freq, uint64_t* max_freq) {
    *mean = 0.0;
    double variance = 0.0;
    *min_freq = UINT64_MAX;
    *max_freq = 0;

    // One-pass mean calculation
    for (int i = 0; i < count; i++) {
        *mean += (double)samples[i].frequency;
        if (samples[i].frequency < *min_freq) *min_freq = samples[i].frequency;
        if (samples[i].frequency > *max_freq) *max_freq = samples[i].frequency;
    }
    *mean /= count;

    // calculate variance
    for (int i = 0; i < count; i++) {
        double diff = (double)samples[i].frequency - *mean;
        variance += diff * diff;
    }
    variance /= count;

    *std_dev = sqrt(variance);
    *cv = (*std_dev / *mean) * 100.0;
}

void analyze_processor_distribution(const SampleData* samples, int count) {
    int cpu_counts[256] = {0};
    int unique_cpu_count = 0;

    for (int i = 0; i < count; i++) {
        if (cpu_counts[samples[i].processor_id] == 0) {
            unique_cpu_count++;
        }
        cpu_counts[samples[i].processor_id]++;
    }

    printf("\nProcessor Information:\n");
    printf("Unique processors used: %d\n", unique_cpu_count);
    printf("Processor distribution:\n");
    
    for (int i = 0; i < 256; i++) {
        if (cpu_counts[i] > 0) {
            printf("  Processor %d: %d samples (%.1f%%)\n", 
                  i, cpu_counts[i], (cpu_counts[i] * 100.0) / count);
        }
    }
}

int main() {
    SampleData samples[SAMPLES];
    double mean, std_dev, cv;
    uint64_t min_freq, max_freq;
    DWORD process_priority_class = GetPriorityClass(GetCurrentProcess());
    
    // boost process priority to reduce scheduling jitter
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    
    printf("Measuring TSC frequency stability over %d samples (%.1f seconds)...\n", 
           SAMPLES, (SAMPLES * SLEEP_MS) / 1000.0);

    // collect multiple samples
    for (int i = 0; i < SAMPLES; i++) {
        samples[i].frequency = measure_frequency(&samples[i].processor_id);
        printf("Sample %d: %llu Hz (Processor ID: %u)\n", 
               i + 1, samples[i].frequency, samples[i].processor_id);
    }

    // calculate statistics
    calculate_statistics(samples, SAMPLES, &mean, &std_dev, &cv, &min_freq, &max_freq);

    printf("\nStatistics:\n");
    printf("Mean frequency: %.2f Hz\n", mean);
    printf("Standard deviation: %.2f Hz\n", std_dev);
    printf("Coefficient of variation: %.6f%%\n", cv);
    printf("Min frequency: %llu Hz\n", min_freq);
    printf("Max frequency: %llu Hz\n", max_freq);
    printf("Range: %llu Hz (%.6f%% of mean)\n",
           max_freq - min_freq, (double)(max_freq - min_freq) * 100.0 / mean);

    // analyze processor distribution
    analyze_processor_distribution(samples, SAMPLES);
    
    // restore original priority
    SetPriorityClass(GetCurrentProcess(), process_priority_class);

    return 0;
}