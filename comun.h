/* para que toadd y toad-cli hablen el mismo idioma y se entiendan */

/* /tmp/ es el directorio temporal en donde todos los usuarios pueden
   escribir. lo que se guarda luego muere cuando reinicias el pc. */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <signal.h>

/* rutas de los pipes en /tmp/ */
#define REQ_PIPE "/tmp/toad_req"
#define RES_PIPE "/tmp/toad_res"

/* estados posibles de un proceso */
#define RUNNING 1
#define STOPPED 2
#define ZOMBIE  3
#define FAILED  4

/* codigos de comando, un numero por cada accion que puede pedir toad-cli */
#define CMD_START  1
#define CMD_STOP   2
#define CMD_PS     3
#define CMD_STATUS 4
#define CMD_KILL   5
#define CMD_ZOMBIE 6

/* tamaño maximo del texto de respuesta que toadd manda por el pipe.
   4096 bytes alcanza para listar hasta 100 procesos con todos sus datos */
#define MAX_RESP 4096

/* struct para la comunicacion por pipe (toad-cli -> toadd).
   mandamos todo en un solo viaje en bytes, asi el otro lado sabe
   exactamente donde empieza y termina cada campo. */
typedef struct {
    int  comando;    /* que accion ejecutar: CMD_START, CMD_STOP, etc. */
    char ruta[256];  /* ruta al binario, solo se usa en start */
    int  iid;        /* a que proceso apunta, se usa en stop, kill y status */
} mensaje;

/* struct para guardar la info de cada proceso. solo lo usa toadd internamente,
   no viaja por el pipe. */
typedef struct {
    int    iid;
    pid_t  pid;
    char   ruta[256];
    int    estado;
    time_t t_inicio;  /* cuando se inicio, para calcular el uptime */
    int    detenido;  /* 1 si lo paramos nosotros con stop o kill,
                         0 si esta corriendo o murio solo.
                         sirve para que waitpid no marque como ZOMBIE
                         un proceso que ya dejamos en STOPPED a proposito. */
} proceso;