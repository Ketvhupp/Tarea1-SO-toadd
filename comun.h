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

#define REQ_PIPE "/tmp/toad_req"
#define RES_PIPE "/tmp/toad_res"

#define RUNNING 1
#define STOPPED 2
#define ZOMBIE  3
#define FAILED  4

//separé los struct entre mensaje y proceso para no mezclar logica interna con comunicacion.
//no es necesario mandar pid y estado por el pipe.

//solo para el pipe, comunicacion
typedef struct {
    int comando; // 1: start, 2: stop, 3: ps, 4: status, 5:kill, 6:zombie
    char ruta[256]; //para guardar el camino al binario. es el </bin/ls>
    int iid; //internal ID, el id del proceso al que se le quiere implemetar el comando.
} mensaje;


//para guardar procesos. gestion interna
typedef struct {
    int iid; 
    pid_t pid; //el pid del proceso que se va a ejecutar.
    char ruta[256]; //para guardar el camino al binario. es el </bin/ls>
    int estado; //para el comando status, el estado del proceso. 1: running, 2: stopped, 3: zombie
} proceso;

/* por qué usar stuct?
mandamos el formulario completo por el pipe de un solo viaje. viaja en bytes
si se manda la info suelta, el otro lado no sabe donde empieza el nombre
del programa y donde termina el ID*/