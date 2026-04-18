/*toadd es un gestor de procesos que corre en background, independiente del terminal desde el que
fue iniciado. Su responsabilidad es ejecutar, monitorear y terminar otros procesos, manteniendo
registro de su estado durante toda su vida <-- es Daemon, copié el código Daemon de la guía*/


/*ocuparemos dos pipes, para comuncarse de toadd a toad-cli y de toad-cli a toadd, pq los pipes
son unidireccionales. También, el pipe se hará con mkfifo, ya que la comunicación es entre 
procesos independientes, no son hijos del mismo padre.*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

 int main () {
    if ( fork () != 0)
        exit (0);

    setsid ();

    if ( fork () != 0)
        exit (0);

    close ( STDIN_FILENO );
    close ( STDOUT_FILENO );
    close ( STDERR_FILENO );

    while (1)
        sleep (1);

    return 0;
 }