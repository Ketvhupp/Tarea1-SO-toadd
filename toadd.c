/*toadd es un gestor de procesos que corre en background, independiente del terminal desde el que
fue iniciado. Su responsabilidad es ejecutar, monitorear y terminar otros procesos, manteniendo
registro de su estado durante toda su vida <-- es Daemon, copié el código Daemon de la guía*/


/*ocuparemos dos pipes, para comuncarse de toadd a toad-cli y de toad-cli a toadd, pq los pipes
son unidireccionales. También, el pipe se hará con mkfifo, ya que la comunicación es entre 
procesos independientes, no son hijos del mismo padre.*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

//DAEMON
 int main () {
    if ( fork () != 0)
        exit (0);

    setsid ();

    if ( fork () != 0)
        exit (0);

    close ( STDIN_FILENO );
    close ( STDOUT_FILENO );
    close ( STDERR_FILENO );

    //PIPES
    mkfifo(REQ_PIPE, 0666);
    mkfifo(RES_PIPE, 0666);

    int iid_counter = 2; //el ID 1 se reserva para el proceso toadd.


    while (1)
        mensaje msg;


        /*para el start la tarea dice:
        2.1. start <bin_path>

            Solicita a toadd que ejecute el binario indicado por bin_path. El proceso resultante queda bajo
            administración de toadd. El comando debe imprimir el IID asignado al nuevo proceso. Los
            binarios son programas en C o C++ ya compilados previamente.
            $ toad-cli start /home/user/mi_programa
            IID: 2*/

        if(msg.comando == 1) {  //start
            //ejecutar el binario que viene en msg.ruta
            pid_t pid = fork();
            if (pid == 0) { //hijo
                char *args[] = {msg.ruta, NULL}; //argumentos para el programa. El primero siempre es la ruta del programa mismo, o sea, el binario.
                execvp(msg.ruta, args); //se va al programa y deja de ejecutarse el código de toadd. 
                exit(1); // Si falla el exec

            }else { //padre
                
                printf("IID: %d\n", iid_counter);
                iid_counter++;
            }
            
        }
        
    

    return 0;
 }