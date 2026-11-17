# Laboratorio Unidad 10 — Memorias del Computador
Sara Jaimes Castañeda — Arquitectura de Computadores

Este repositorio contiene el desarrollo de las dos partes del laboratorio guiado de memorias del computador, escrito en C y ejecutado sobre Linux. La Parte 1 mide la latencia de acceso a memoria con un benchmark propio para observar el efecto de la jerarquía de caché L1, L2, L3 y RAM, compara el acceso secuencial con el aleatorio y usa un acceso con stride fijo para exponer los conflictos de asociatividad. La Parte 2 examina la memoria virtual de un proceso Linux a través de /proc, observando el mapa de direcciones, la clasificación de páginas, los page faults y el costo de un page fault individual con mmap.

## Entorno utilizado

Ubuntu 24.04.5 LTS sobre WSL2 en Windows 11, con kernel 6.18.58.1-microsoft-standard-WSL2 y gcc 13.3.0. El procesador es un Intel Core i7-13620H y el tamaño de página del sistema es de 4096 bytes. Los programas se compilaron y ejecutaron directamente en la carpeta del repositorio para que el código y las capturas quedaran en el mismo lugar. Para la Parte 2 se usaron además pmap, vmstat y free de procps-ng 4.0.4 y el comando /usr/bin/time de GNU.

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

## Checkpoint 5 — Decisiones técnicas de la Parte 1
Capturas: capturas/p1_cp5_geometria_l1.png, capturas/p1_cp5_prefetch_objdump.png, capturas/p1_cp5_con_prefetch.png y capturas/p1_cp5_sin_prefetch.png

Para respaldar las dos decisiones se leyó la geometría de la L1d desde /sys y se calculó el stride de conflicto. Además, cache_bench.c se compiló con -O2 y cada flag de prefetch, se contaron con objdump las instrucciones de prefetch de cada binario y se ejecutaron los dos.

## Decisión Técnica — Stride que expone mejor los conflictos de asociatividad
El stride que garantiza que todos los accesos caigan en el mismo set es el tamaño de la caché dividido entre sus vías. Tanto en la L1 de la guía, 32 KB entre 8 vías, como en la L1d de este equipo, 48 KB entre 12 vías, el resultado es 4096 bytes. Con 64 sets y líneas de 64 bytes el índice de set lo forman los bits 6 a 11, y sumar 4096 no cambia ninguno.

Con un stride de 2048 bytes los accesos ya no caerían todos en el mismo set. Sumar 2048 cambia el bit 11, que es parte del índice, así que alternarían entre dos sets y cada uno recibiría la mitad de las líneas en conflicto.

A partir del noveno acceso en la L1 de la guía, o del decimotercero en la de este equipo, cada línea nueva desaloja a una de las que ya están en el set según la política de reemplazo, y el acceso se resuelve en L2 en lugar de L1. Por eso en el Checkpoint 4 el stride de 4096 costó 6.172 ns, mientras que en el Checkpoint 2 los arrays que caben en L1 se leyeron en unos 0.7 ns.

## Decisión Técnica — Prefetch por software del compilador vs prefetcher de hardware
El flag -fno-prefetch-loop-arrays no tiene ningún efecto sobre el prefetcher de hardware, solo decide si el compilador escribe instrucciones de prefetch en el binario. El prefetcher de hardware actúa dentro del procesador sin importar cómo se compiló el programa. Con objdump se encontraron 5 instrucciones prefetcht0 con -fprefetch-loop-arrays y ninguna con -fno-prefetch-loop-arrays.

En teoría el patrón más sensible es el secuencial, porque es el único que el compilador puede predecir, mientras que el aleatorio y el de stride de 4096 son casi insensibles porque dependen de la mezcla Fisher-Yates o de conflictos en un set. Sin embargo, aquí las 5 instrucciones quedaron en bench_rand y ninguna en bench_seq ni en bench_stride, porque el volatile impide que el compilador toque esos accesos. Por eso el secuencial dio casi igual con ambos flags, por ejemplo 1.339 y 1.343 ns en 4 MB.

La conclusión válida pero limitada es que comparar los dos flags solo mide el prefetch por software del compilador. El prefetcher de hardware siguió activo en ambas corridas y apagarlo requiere registros MSR o la BIOS. Además, el stride pasó de 0.592 y 1.126 ns a 1.450 y 1.883 ns sin tener ninguna instrucción de prefetch, así que esa diferencia es variación entre corridas.

# PARTE 2 — Análisis de Memoria Virtual en Linux

## Checkpoint 1 — Regiones del mapa de memoria
Capturas: capturas/p2_cp1_compilacion.png y capturas/p2_cp1_maps.png

En la primera prueba se usó la línea gcc -o mymaps mymaps.c && ./mymaps & tal como aparece en la guía, y el cat mostró el mapa de /usr/bin/bash en lugar del de mymaps. Esto pasó porque el & manda a segundo plano toda la línea, así que $! guardó el PID de la subshell que compilaba y no el del programa. Por lo tanto se compiló primero y luego se lanzó ./mymaps & por separado, sin tocar el código. Hecho esto, $! coincidió con el PID 3204 que imprimió el programa y el mapa mostró sus regiones mientras getchar() lo mantenía vivo.

| Región | Rango | Permisos | Propósito |
|---|---|---|---|
| Texto de mymaps | 5c3b985dd000-5c3b985de000 | r-xp | Código del programa, se lee y se ejecuta pero no se modifica |
| Solo lectura de mymaps | 5c3b985dc000-5c3b985dd000 y 5c3b985de000-5c3b985e0000 | r--p | Cabecera del ejecutable y constantes como la cadena "hola" |
| Datos de mymaps | 5c3b985e0000-5c3b985e1000 | rw-p | Variables globales como global_data (0x5c3b985e0010) |
| Heap | 5c3ba9c12000-5c3ba9c33000 | rw-p | Memoria dinámica pequeña que crece con brk |
| Anónima del malloc | 72c9996ff000-72c999800000 | rw-p | Bloque de 1 MB del malloc (0x72c9996ff010) |
| libc | 72c999828000-72c9999b1000 | r-xp | Código de la biblioteca estándar compartida |
| vDSO | 72c999a53000-72c999a55000 | r-xp | Funciones del kernel que se ejecutan sin llamada al sistema, como clock_gettime |
| ld-linux | 72c999a56000-72c999a81000 | r-xp | Cargador dinámico que enlaza las bibliotecas al iniciar |
| Pila | 7ffed5a76000-7ffed5a97000 | rw-p | Variables locales como heap (0x7ffed5a95800) |

La p final de los permisos indica que cada región es privada del proceso, y solo el texto, libc, el vDSO y ld-linux tienen permiso de ejecución. Además, el bloque de 1 MB no quedó dentro de [heap] sino en una región anónima propia, porque malloc atiende las reservas grandes con mmap en lugar de hacer crecer el heap.

## Checkpoint 2 — Clasificación de páginas en smaps
Capturas: capturas/p2_cp2_smaps.png, capturas/p2_cp2_smaps_regiones.png, capturas/p2_cp2_pmap.png y capturas/p2_cp2_vmstat_free.png

Con cat /proc/$$/smaps | head -60 se revisó la shell y se vio que cada región trae Size, Rss, Shared_Clean, Private_Dirty, Referenced y otros campos. Como las primeras 60 líneas solo cubren el binario de bash, se buscaron con grep las regiones del heap, la pila y el código de libc.

| Región | Rss | Shared_Clean | Private_Dirty |
|---|---|---|---|
| [heap] | 1428 kB | 0 kB | 1428 kB |
| [stack] | 112 kB | 0 kB | 112 kB |
| libc.so.6 r-xp | 1440 kB | 1184 kB | 0 kB |

El heap y la pila son Private_Dirty porque son páginas anónimas que la shell escribió y que no existen en ningún archivo, así que pertenecen solo a ese proceso y no se pueden compartir. En cambio, el código de libc es Shared_Clean porque se carga desde el archivo libc.so.6, nunca se modifica y las mismas páginas físicas las usan todos los procesos que tienen libc. Por eso su Pss de 605 kB es menor que su Rss, ya que el costo se reparte entre ellos. pmap -x mostró la misma shell con 5944 kB de RSS en total, y vmstat -s y free -h reportaron unos 31 GiB de RAM con 740 MiB usados y el swap sin uso.

## Checkpoint 3 — VmRSS y conteo de page faults
Capturas: capturas/p2_cp3_compilacion.png y capturas/p2_cp3_ejecucion.png

En la primera prueba el programa de la guía mostró VmRSS de 2080 kB antes y después del memset, sin ningún cambio. Esto pasó porque system() lanza un grep aparte, y para ese grep /proc/self es su propio proceso, así que se leía la memoria del grep y no la de pagefault_demo. Por lo tanto se ajustó el programa para armar la ruta /proc/PID/status con el getpid() del propio programa. Hecho esto, el VmRSS reflejó la memoria real del proceso.

| Momento | VmPeak | VmRSS |
|---|---|---|
| Antes del memset | 105132 kB | 1740 kB |
| Después del memset | 105132 kB | 104140 kB |

VmPeak ya incluye los 100 MB desde el inicio porque malloc reserva las direcciones virtuales, pero el VmRSS solo crece después del memset, exactamente 102400 kB, que son los 100 MB. Esto se debe a que malloc no genera page faults inmediatamente, ya que el kernel usa demand paging y solo asigna un marco físico cuando el proceso escribe por primera vez en cada página. Con /usr/bin/time -v se registraron 26008 minor faults, cerca de las 25 600 páginas de 4 KB que tienen 100 MB más las del arranque del programa. Un minor fault se resuelve en RAM sin leer el disco, en cambio un major fault necesita traer la página desde el disco, y en esta corrida solo hubo 1, junto con una sola entrada de sistema de archivos en el mismo reporte.

## Checkpoint 4 — Costo medido de un minor page fault
Capturas: capturas/p2_cp4_compilacion.png y capturas/p2_cp4_ejecucion.png

Se creó mmap_fault_cost.c, que reserva 200 páginas con mmap anónimo y mide con clock_gettime el primer y el segundo toque de cada una. El código de la guía funcionó sin cambios y se ejecutó tres veces seguidas.

| Corrida | Primer toque | Segundo toque | Overhead del fault |
|---|---|---|---|
| 1 | 3667.1 ns | 57.8 ns | 3609.3 ns |
| 2 | 2968.8 ns | 51.7 ns | 2917.1 ns |
| 3 | 3088.7 ns | 79.5 ns | 3009.2 ns |

En las tres corridas el primer toque fue entre 39 y 63 veces más caro que el segundo, porque al tocar por primera vez la página el procesador hace un trap al kernel, que asigna un marco físico y actualiza la tabla de páginas. El segundo toque ya encuentra la traducción resuelta y solo paga el acceso normal más la propia medición. El overhead atribuible al page fault quedó entre 2917 y 3609 ns, mientras que en la Parte 1 el acceso aleatorio a un array de 64 MB en RAM costó 35.807 ns. Por lo tanto un minor page fault cuesta entre 80 y 100 veces más que un acceso a RAM, es decir, unos dos órdenes de magnitud por encima.

## Checkpoint 5 — Decisiones técnicas de la Parte 2
Capturas: capturas/p2_cp5_costo_smaps_pmap.png y capturas/p2_cp5_map_populate.png

Para respaldar la primera decisión se comparó el tamaño de /proc/PID/maps y /proc/PID/smaps de la shell y se midió el tiempo de 1000 lecturas de maps, de smaps y de pmap -x. Para la segunda se hizo una copia de mmap_fault_cost.c en /tmp con MAP_POPULATE agregado mediante sed, y se ejecutó junto al programa original.

## Decisión Técnica — /proc/PID/smaps o pmap -x
pmap -x sí reporta cuánta RAM usa cada región, en su columna RSS, junto con Kbytes, Dirty y los permisos. Sin embargo, no separa Private_Dirty de Shared_Clean ni muestra Referenced o Pss, y tampoco nombra el heap, que en el Checkpoint 2 apareció solo como [ anon ] con 1428 kB.

La diferencia de costo importaría en un script que consulte la memoria de un proceso cada segundo. En la shell, smaps ocupó 32984 bytes contra 4238 de maps, y con cientos de regiones ese volumen crece mucho más. En ese caso conviene pmap -x porque entrega un resumen de 46 líneas con totales, fácil de procesar en cada consulta. Aun así, en 1000 lecturas pmap -x tardó 3.254 s frente a 1.129 s de smaps, porque es un programa aparte que arma su resumen con esa misma información, así que su ventaja está en la salida corta y no en leer menos.

Para una inspección rápida en la terminal se prefiere pmap -x porque muestra la tabla ya resumida sin filtrar a mano con grep o awk. En cambio, smaps es indispensable cuando hay que distinguir páginas compartidas, privadas, sucias o limpias región por región, como en el Checkpoint 2.

## Decisión Técnica — mmap con MAP_POPULATE o demand paging
Con MAP_POPULATE el primer toque bajaría casi al nivel del segundo, porque el kernel asigna los marcos y llena la tabla de páginas dentro de la propia llamada a mmap. En la prueba, el programa original midió 4658.1 ns en el primer toque, mientras que con MAP_POPULATE midió entre 90.0 y 114.6 ns frente a 45 y 48 ns del segundo. La diferencia que queda se atribuye a que la traducción aún no estaba en el TLB, y ya no a un page fault.

MAP_POPULATE conviene en un servidor que atiende peticiones y necesita una latencia baja y predecible, porque paga todos los faults al arrancar y ninguna petición sufre los 2.9 a 4.6 µs extra por página que se midieron. En cambio, el demand paging conviene a un programa de arranque único o que reserva memoria de forma generosa, ya que solo paga por lo que realmente usa.

Si un proceso reserva 100 MB y solo toca 10 MB, el demand paging evita asignar los 90 MB restantes, es decir, 23 040 marcos de 4 KB que nunca se usarían. Con MAP_POPULATE esos marcos se reservarían y se llenarían de ceros igual, gastando tiempo y RAM que otros procesos podrían necesitar.
