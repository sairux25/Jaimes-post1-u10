/* mymaps.c
 * Programa minimo que muestra direcciones de stack, heap y datos
 * y se queda esperando para poder leer su /proc/PID/maps.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int global_data = 42;      /* segmento .data */
const char *msg = "hola";  /* segmento .rodata */

int main(void) {
    void *heap = malloc(1024 * 1024); /* 1 MB en heap */

    printf("PID: %d\n", getpid());
    printf("stack var addr: %p\n", (void*)&heap);
    printf("heap addr:      %p\n", heap);
    printf("global addr: %p\n", (void*)&global_data);
    printf("Presione Enter para continuar...\n");

    getchar(); /* mantiene vivo el proceso mientras se lee su mapa */

    free(heap);
    return 0;
}
