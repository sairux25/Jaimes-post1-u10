/* cache_bench.c
 * Mide la latencia de acceso a memoria para arrays de distintos tamanos
 * y asi observa el efecto de la jerarquia de cache (L1, L2, L3 y RAM).
 * Cache detectada en cpu0: L1d 48K, L2 1280K, L3 24576K, linea de 64 B.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define REPEAT 100
#define LINE 64                       /* tamano de linea de cache */
#define MAX_BYTES (64*1024*1024)      /* array mas grande del benchmark */

/* Medir tiempo de acceso secuencial a array de N bytes.
 * Se lee un byte por linea de cache y cada tamano repite el recorrido
 * hasta sumar los mismos accesos que el array mas grande. */
double bench_seq(size_t n_bytes) {
    volatile char *arr = (volatile char *)malloc(n_bytes);
    if (!arr) return -1.0;
    memset((void*)arr, 1, n_bytes);

    size_t reps = REPEAT * (MAX_BYTES / n_bytes);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (size_t r = 0; r < reps; r++)
        for (size_t i = 0; i < n_bytes; i += LINE)
            (void)arr[i]; /* acceso de lectura */

    clock_gettime(CLOCK_MONOTONIC, &t1);
    free((void*)arr);

    double ns = (t1.tv_sec - t0.tv_sec)*1e9
              + (t1.tv_nsec - t0.tv_nsec);

    return ns / (reps * (double)(n_bytes / LINE)); /* ns/acceso */
}

int main(void) {
    size_t sizes[] = {
        4*1024, 8*1024, 16*1024, 32*1024,  /* region L1 (L1d de 48K) */
        64*1024, 128*1024, 256*1024,       /* region L2 (L2 de 1280K) */
        512*1024, 1024*1024,               /* todavia caben en L2 */
        4*1024*1024, 16*1024*1024,         /* region L3 (L3 de 24576K) */
        64*1024*1024                       /* region RAM */
    };
    int n = sizeof(sizes)/sizeof(sizes[0]);

    printf("%-12s %10s\n","Array (KB)","ns/acceso");
    for (int i = 0; i < n; i++) {
        double lat = bench_seq(sizes[i]);
        printf("%-12zu %10.3f\n", sizes[i]/1024, lat);
    }
    return 0;
}
