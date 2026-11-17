/* pagefault_demo.c
 * Reserva 100 MB con malloc y compara el VmRSS antes y despues
 * de escribir en todas las paginas con memset.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    size_t N = 100 * 1024 * 1024; /* 100 MB */
    char *buf = (char *)malloc(N);

    /* /proc/self dentro de system() seria el propio grep,
     * por eso se arma la ruta con el PID de este programa */
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "grep -E \"VmRSS|VmPeak\" /proc/%d/status", getpid());

    /* Antes de tocar la memoria: leer faults actuales */
    system(cmd);

    /* Forzar page faults escribiendo en cada pagina */
    memset(buf, 0xAA, N);

    /* Despues: RSS debe haber crecido ~100 MB */
    system(cmd);

    free(buf);
    return 0;
}
