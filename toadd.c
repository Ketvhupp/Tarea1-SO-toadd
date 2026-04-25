/*toadd es un gestor de procesos que corre en background, independiente del terminal desde el que
fue iniciado. Su responsabilidad es ejecutar, monitorear y terminar otros procesos, manteniendo
registro de su estado durante toda su vida <- es Daemon */

/*ocuparemos dos pipes, para comunicarse de toadd a toad-cli y de toad-cli a toadd, pq los pipes
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
#include <time.h>

#include "comun.h"

/* funcion auxiliar: manda un string por RES_PIPE.
   la usamos en todos los comandos para no repetir el open/write/close */
void responder(const char *texto) {
    int fd_res = open(RES_PIPE, O_WRONLY);
    if (fd_res == -1) return;
    write(fd_res, texto, strlen(texto) + 1); /* +1 para incluir el \0 */
    close(fd_res);
}

//DAEMON
int main() {
    if (fork() != 0)
        exit(0);

    setsid();

    if (fork() != 0)
        exit(0);

    /* redirigir stdin/stdout/stderr a /dev/null en vez de solo cerrarlos.
       si solo los cerramos, el siguiente open() podria reutilizar esos fd (0,1,2)
       y causaria bugs raros donde un pipe queda conectado a stdout sin querer. */
    int dev_nulo = open("/dev/null", O_RDWR);
    dup2(dev_nulo, STDIN_FILENO);
    dup2(dev_nulo, STDOUT_FILENO);
    dup2(dev_nulo, STDERR_FILENO);
    close(dev_nulo);

    //PIPES
    mkfifo(REQ_PIPE, 0666);
    mkfifo(RES_PIPE, 0666);

    int iid_counter = 2; //el ID 1 se reserva para el proceso toadd.
    proceso procesos[100];
    int num_procesos = 0;

    while (1) {

        /* recolectar zombies: revisar si algun hijo termino sin que lo hayamos esperado.
           WNOHANG = no bloquear, solo revisar y seguir.
           si algun hijo termino, actualizamos su estado en la tabla. */
        pid_t pid_muerto;
        int status;
        while ((pid_muerto = waitpid(-1, &status, WNOHANG)) > 0) {
            for (int i = 0; i < num_procesos; i++) {
                if (procesos[i].pid == pid_muerto) {
                    procesos[i].estado = ZOMBIE;
                }
            }
        }

        mensaje msg;
        // 1. esperar comando del CLI
        int fd_req = open(REQ_PIPE, O_RDONLY); //abre el pipe para leer
        ssize_t n = read(fd_req, &msg, sizeof(mensaje)); //lee el mensaje que viene del pipe.
        close(fd_req); //cierra el pipe de lectura

        if (n <= 0) {
            continue; //si no se leyó nada, sigue esperando
        }

        // 2. procesar comando
        if (msg.comando == CMD_START) {
            pid_t pid = fork();

            if (pid == 0) {
                /* hijo: cerrar los pipes antes del exec.
                   si no los cerramos, el hijo tiene sus propias copias abiertas
                   de REQ_PIPE y RES_PIPE. eso confunde al toad-cli cuando intenta
                   leer la respuesta porque el pipe nunca queda sin escritores. */
                char *args[] = {msg.ruta, NULL};
                execvp(msg.ruta, args);
                exit(1); // si falla el exec

            } else {
                //padre: registrar y responder con el IID como texto
                int iid = iid_counter++;

                procesos[num_procesos].iid      = iid;
                procesos[num_procesos].pid      = pid;
                procesos[num_procesos].estado   = RUNNING;
                procesos[num_procesos].t_inicio = time(NULL);
                strcpy(procesos[num_procesos].ruta, msg.ruta);
                num_procesos++;

                /* ahora respondemos con texto igual que los otros comandos,
                   asi toad-cli puede leer siempre de la misma forma */
                char respuesta[64];
                snprintf(respuesta, sizeof(respuesta), "IID: %d\n", iid);
                responder(respuesta);
            }
        }

        if (msg.comando == CMD_PS) {
            /* armar el texto de respuesta con todos los procesos de la tabla */
            char respuesta[MAX_RESP];
            char linea[512];

            /* cabecera de la tabla */
            snprintf(respuesta, sizeof(respuesta),
                     "%-6s %-8s %-10s %-10s %s\n",
                     "IID", "PID", "ESTADO", "UPTIME", "BINARIO");

            for (int i = 0; i < num_procesos; i++) {
                /* calcular uptime: tiempo actual menos cuando se inicio */
                time_t seg = time(NULL) - procesos[i].t_inicio;
                int hh = (int)(seg / 3600);
                int mm = (int)((seg % 3600) / 60);
                int ss = (int)(seg % 60);
                char uptime[16];
                snprintf(uptime, sizeof(uptime), "%02d:%02d:%02d", hh, mm, ss);

                /* nombre del estado */
                char *nombre_estado;
                if      (procesos[i].estado == RUNNING) nombre_estado = "RUNNING";
                else if (procesos[i].estado == STOPPED) nombre_estado = "STOPPED";
                else if (procesos[i].estado == ZOMBIE)  nombre_estado = "ZOMBIE";
                else                                    nombre_estado = "FAILED";

                snprintf(linea, sizeof(linea),
                         "%-6d %-8d %-10s %-10s %s\n",
                         procesos[i].iid, procesos[i].pid,
                         nombre_estado, uptime, procesos[i].ruta);

                if (strlen(respuesta) + strlen(linea) < MAX_RESP - 1)
                    strcat(respuesta, linea);
            }

            if (num_procesos == 0)
                strcat(respuesta, "(sin procesos administrados)\n");

            responder(respuesta);
        }

        /* aqui iremos agregando los otros comandos (stop, kill, status, zombie) */
    }

    return 0;
}

/*decidi hacer que los pipes abran y cierren en cada iteración del bucle, 
para evitar bloqueos de fifo y tener un modelo request/response más robusto y limpio*/