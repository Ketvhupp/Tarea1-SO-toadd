/*Para que toadd y toad-cli hablen el mismo idioma y se entiendan*/

/* /tmp/ es el directorio temporal en donde todos los usuarios pueden
escribir. lo que se guarda luego muere cuando reinicias el pc.

solo para cambiar el nombre y no haya que escribir el largo tantas veces*/
#define REQ_PIPE "/tmp/toad_req"
#define RES_PIPE "/tmp/toad_res"

typedef struct { /*estructuras de datos que agrupa variables de diferentes tipos en una sola entidad. */
    int comando; // 1: start, 2: stop, 3: ps, 4: status, 5:kill, 6:zombie
    char ruta[256]; //para guardar el camino al binario. es el </bin/ls>
    int iid; //internal ID, el id del proceso al que se le quiere implemetar el comando.
} mensaje;

/* por qué usar stuct?
mandamos el formulario completo por el pipe de un solo viaje. viaja en bytes
si se manda la info suelta, el otro lado no sabe donde empieza el nombre
del programa y donde termina el ID*/