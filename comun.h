/*Para que toadd y toad-cli hablen el mismo idioma y se entiendan*/

/* /tmp/ es el directorio temporal en donde todos los usuarios pueden
escribir. lo que se guarda luego muere cuando reinicias el pc.

solo para cambiar el nombre y no haya que escribir el largo tantas veces*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

#define REQ_PIPE "/tmp/toad_req"
#define RES_PIPE "/tmp/toad_res"

#define RUNNING 1
#define STOPPED 2
#define ZOMBIE  3
#define FAILED  4

/* mismos numeros que antes, solo los deje
   nombrados aqui para que sea mas facil de leer en el codigo */
#define CMD_START  1
#define CMD_STOP   2
#define CMD_PS     3
#define CMD_STATUS 4
#define CMD_KILL   5
#define CMD_ZOMBIE 6

/* tamaño maximo del texto de respuesta que toadd manda por el pipe.
   4096 bytes es suficiente para listar hasta 100 procesos con sus datos, claude me dio el calculop */
#define MAX_RESP 4096

/* separe los struct entre mensaje y proceso para no mezclar logica interna
   con comunicacion. no es necesario mandar pid y estado por el pipe. */

/* solo para el pipe, comunicacion */
typedef struct {
    int  comando;        /* que quiere hacer: CMD_START, CMD_STOP, etc. */
    char ruta[256];  /* ruta al binario (solo se usa en start) */
    int  iid;        /* a que proceso apunta (stop, kill, status) */
} mensaje;


/* para guardar procesos. gestion interna de toadd */
typedef struct {
    int    iid;
    pid_t  pid;
    char   ruta[256];
    int    estado;
    time_t t_inicio;  /* agregado: momento en que se inicio el proceso.
                         necesario para calcular el uptime en ps y status */
} proceso;

/* por que usar struct?
   mandamos el formulario completo por el pipe de un solo viaje. viaja en bytes.
   si se manda la info suelta, el otro lado no sabe donde empieza el nombre
   del programa y donde termina el ID */