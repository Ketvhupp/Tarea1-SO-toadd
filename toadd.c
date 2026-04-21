/*toadd es un gestor de procesos que corre en background, independiente del terminal desde el que
fue iniciado. Su responsabilidad es ejecutar, monitorear y terminar otros procesos, manteniendo
registro de su estado durante toda su vida <-- es Daemon, copié el código Daemon de la guía*/


/*ocuparemos dos pipes, para comuncarse de toadd a toad-cli y de toad-cli a toadd, pq los pipes
son unidireccionales. También, el pipe se hará con mkfifo, ya que la comunicación es entre 
procesos independientes, no son hijos del mismo padre.*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>

#include "comun.h"

//DAEMON
 int main () {
    if ( fork () != 0)
        exit (0);

    setsid ();

    if ( fork () != 0)
        exit (0);

    //por ahora comentados:
    close ( STDIN_FILENO );
    close ( STDOUT_FILENO );
    close ( STDERR_FILENO );

    //PIPES
    mkfifo(REQ_PIPE, 0666);
    mkfifo(RES_PIPE, 0666);

    int iid_counter = 2; //el ID 1 se reserva para el proceso toadd.
    proceso procesos[100]; 
    int num_procesos = 0;

    while (1) {

        /*ver después (no lo entiendo mucho)
        
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    for (int i = 0; i < num_procesos; i++) {
        if (procesos[i].pid == pid) {
            procesos[i].estado = ZOMBIE;
        }
    }
}
    */

        mensaje msg;
        // 1. esperar comando del CLI
        int fd_req = open(REQ_PIPE, O_RDONLY); //abre el pipe para leer
        ssize_t n = read(fd_req, &msg, sizeof(mensaje)); //lee el mensaje que viene del pipe. 
        close(fd_req); //cierra el pipe de lectura 

        if(n<=0) {
            continue; //si no se leyó nada, sigue esperando
        }


        /*para el start la tarea dice:
        2.1. start <bin_path>

            Solicita a toadd que ejecute el binario indicado por bin_path. El proceso resultante queda bajo
            administración de toadd. El comando debe imprimir el IID asignado al nuevo proceso. Los
            binarios son programas en C o C++ ya compilados previamente.
            $ toad-cli start /home/user/mi_programa
            IID: 2*/

        // 2. procesar comando
        if(msg.comando == 1) {  //start
            //ejecutar el binario que viene en msg.ruta
            pid_t pid = fork();

            if (pid == 0) { 
                //hijo
                char *args[] = {msg.ruta, NULL}; //argumentos para el programa. El primero siempre es la ruta del programa mismo, o sea, el binario.
                execvp(msg.ruta, args); //se va al programa y deja de ejecutarse el código de toadd. 
                exit(1); // Si falla el exec

            }else { // 3. responder
                //padre
                int iid = iid_counter++;

                //guardar proceso creado en el fork
                procesos[num_procesos].iid = iid;
                procesos[num_procesos].pid = pid;
                strcpy(procesos[num_procesos].ruta, msg.ruta);
                procesos[num_procesos].estado = RUNNING;

                num_procesos++;

                // 3. responder
                int fd_res = open(RES_PIPE, O_WRONLY); //abre el pipe para escribir
                
                write(fd_res, &iid, sizeof(int)); //envía el IID al pipe de respuesta para que toad-cli lo reciba
                close(fd_res); //cierra el pipe de escritura

            }
            
        }
        
    }

    return 0;

 }

 /*decidi hacer que los pipes abran y cierren en cada iteración del bucle, 
 para evitar bloqueos de fifo y tener un modelo request/response más robusto y limpio*/