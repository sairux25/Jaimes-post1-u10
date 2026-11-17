# Laboratorio Unidad 10 — Memorias del Computador
Sara Jaimes Castañeda — Arquitectura de Computadores

Este repositorio contiene el desarrollo de las dos partes del laboratorio guiado de memorias del computador, escrito en C y ejecutado sobre Linux. La Parte 1 mide la latencia de acceso a memoria con un benchmark propio para observar el efecto de la jerarquía de caché L1, L2, L3 y RAM, compara el acceso secuencial con el aleatorio y usa un acceso con stride fijo para exponer los conflictos de asociatividad. La Parte 2 examina la memoria virtual de un proceso Linux a través de /proc, observando el mapa de direcciones, la clasificación de páginas, los page faults y el costo de un page fault individual con mmap.

## Entorno utilizado

Ubuntu 24.04.5 LTS sobre WSL2 en Windows 11, con kernel 6.18.58.1-microsoft-standard-WSL2 y gcc 13.3.0. El procesador es un Intel Core i7-13620H y el tamaño de página del sistema es de 4096 bytes. Los programas se compilaron y ejecutaron directamente en la carpeta del repositorio para que el código y las capturas quedaran en el mismo lugar.

# PARTE 1 — Benchmark de Jerarquía de Caché

## Checkpoint 1 — Detección de niveles de caché
Capturas: capturas/p1_cp1_niveles_cache.png y capturas/p1_cp1_lscpu.png

El ciclo que lee /sys/devices/system/cpu/cpu0/cache devolvió 48K, 32K, 1280K y 24576K. Como el ciclo imprime el número de índice y no el nivel, se consultó también lscpu -C, que muestra que el índice 0 es la L1 de datos, el 1 la L1 de instrucciones, el 2 la L2 y el 3 la L3. Por lo tanto el procesador tiene una L1d de 48 KB con 12 vías y 64 sets, una L2 de 1280 KB con 10 vías y una L3 de 24 MB compartida por todos los núcleos, todas con líneas de 64 bytes. Estos tamaños son los que marcan dónde se esperan los saltos del benchmark.

## Checkpoint 2 — Saltos de latencia por nivel de caché
Capturas: capturas/p1_cp2_compilacion.png y capturas/p1_cp2_ejecucion.png

En la primera prueba el código de la guía, que lee el array byte por byte, dio una latencia casi plana de unos 0.7 ns/byte desde 64 KB hasta 64 MB, sin ningún salto visible. Esto pasó porque solo una de cada 64 lecturas necesita traer una línea nueva, así que el costo del propio ciclo compilado con -O0 tapa el tiempo de la memoria, y además el prefetcher del procesador adelanta las líneas en un recorrido secuencial. Por lo tanto se ajustó bench_seq para leer un byte por línea de caché y para que cada tamaño repita el recorrido hasta sumar los mismos accesos que el array de 64 MB, así los arrays pequeños no se miden en tiempos demasiado cortos. Hecho esto, la tabla se reporta en ns por acceso y muestra tres saltos claros.

| Array (KB) | ns/acceso | Nivel |
|---|---|---|
| 4 | 2.005 | L1 |
| 8 | 0.780 | L1 |
| 16 | 0.729 | L1 |
| 32 | 0.682 | L1 |
| 64 | 1.042 | L2 ← salto L1 a L2 |
| 128 | 1.032 | L2 |
| 256 | 0.998 | L2 |
| 512 | 1.014 | L2 |
| 1024 | 1.116 | L2 |
| 4096 | 1.621 | L3 ← salto L2 a L3 |
| 16384 | 2.083 | L3 |
| 65536 | 11.540 | RAM ← salto L3 a RAM |

Los saltos aparecen justo al superar los 48 KB de la L1d, los 1280 KB de la L2 y los 24 MB de la L3, lo que confirma que corresponden a los tamaños detectados en el Checkpoint 1. El valor de 4 KB salió más alto que el de 8 KB en todas las corridas, pero no se toma como un cambio de nivel porque 8, 16 y 32 KB vuelven a quedar cerca de 0.7 ns.

## Checkpoint 3 — Comparativa secuencial vs aleatorio
Capturas: capturas/p1_cp3_compilacion.png y capturas/p1_cp3_ejecucion.png

Se agregó bench_rand, que mezcla un array de índices con Fisher-Yates y luego lee los enteros en ese orden aleatorio, y main() se modificó para medir los dos patrones con los mismos tamaños. El código de la guía funcionó sin cambios y las dos columnas quedaron en ns por acceso.

| Array (KB) | Secuencial | Aleatorio |
|---|---|---|
| 4 | 1.139 | 2.402 |
| 8 | 0.776 | 1.913 |
| 16 | 0.727 | 2.565 |
| 32 | 0.719 | 1.826 |
| 64 | 1.041 | 1.300 |
| 128 | 1.007 | 1.520 |
| 256 | 1.053 | 1.349 |
| 512 | 1.072 | 1.563 |
| 1024 | 1.042 | 1.698 |
| 4096 | 1.547 | 2.226 |
| 16384 | 1.977 | 14.627 |
| 65536 | 7.738 | 35.807 |

Hasta 4 MB el acceso aleatorio se mantiene entre 1.3 y 2.6 ns, porque los datos todavía se encuentran en L1, L2 o L3 y cada fallo de L1 se resuelve en un nivel de caché cercano. En 16 MB, en cambio, el aleatorio sube a 14.627 ns mientras el secuencial queda en 1.977 ns. Ese salto se atribuye sobre todo al TLB miss, ya que 16 MB son 4096 páginas de 4 KB y cada acceso salta a una página distinta que el TLB no alcanza a cubrir. A esto se suma el cache miss en L3, porque el array de índices ocupa el doble que el de datos y entre los dos superan los 24 MB de la L3. En 64 MB el array ya no cabe en ninguna caché, por lo tanto cada lectura aleatoria paga el cache miss hasta RAM y además el TLB miss, y llega a 35.807 ns, unas 4.6 veces el valor secuencial.

## Checkpoint 4 — Conflictos de asociatividad con stride
Capturas: capturas/p1_cp4_compilacion.png y capturas/p1_cp4_ejecucion.png

En la primera prueba con el código de la guía, el stride de 64 bytes y el de 4096 bytes sobre el array de 1 MB dieron casi lo mismo en tres corridas, entre 4.6 y 5.1 ns por acceso en ambos casos. Esto pasó porque cada acceso calculaba (i * stride) % n_bytes, una división lenta que tapaba la diferencia, y porque con 4096 bytes cada vuelta hace solo 256 accesos, así que la medición era demasiado corta. Por lo tanto se quitó el %, que no cambia ninguna dirección leída porque i * stride nunca llega a n_bytes, y se hizo que los dos strides repitan el recorrido hasta sumar los mismos 1 638 400 accesos. Hecho esto, el stride de 4096 bytes costó 6.172 ns contra 4.818 ns del de 64 bytes, un 28 % más, y en todas las corridas hechas con el ajuste el de 4096 bytes salió más lento.

| Stride | ns/acceso |
|---|---|
| 64 B | 4.818 |
| 4096 B | 6.172 |

Los dos experimentos trabajan sobre el mismo array de 1 MB, que cabe completo en la L2 de 1280 KB, así que la diferencia no se debe a un tamaño de trabajo distinto. La L1d tiene 64 sets con líneas de 64 bytes, por lo que el set lo deciden los bits 6 a 11 de la dirección. Con el stride de 64 bytes los accesos recorren los 64 sets en orden, en cambio con 4096 bytes esos bits nunca cambian y las 256 direcciones caen en el mismo set, que solo guarda 12 líneas. Por eso cada línea nueva desaloja a otra y casi ningún acceso se resuelve en L1, lo que confirma que la penalización viene de los conflictos de asociatividad.
