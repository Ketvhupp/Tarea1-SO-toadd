#define _POSIX_C_SOURCE 200809L // esto lo explico en el comunicado3.txt 
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
#include <signal.h>

#include "comun.h"

/* funcion auxiliar: manda un string por RES_PIPE.
   la usamos en todos los comandos para no repetir el open/write/close */
void responder(const char *texto) {
    int fd_res = open(RES_PIPE, O_WRONLY);
    if (fd_res == -1) return;
    write(fd_res, texto, strlen(texto) + 1); /* +1 para incluir el \0 */
    close(fd_res);
}

/* funcion auxiliar para buscar un proceso por su IID.
   devuelve el índice en el arreglo o -1 si no lo encuentra. */
int buscar_proceso(int iid_buscado, proceso lista[], int total) {
    for (int i = 0; i < total; i++) {
        if (lista[i].iid == iid_buscado) {
            return i; // encontrado -> devuelve su posicion
        }
    }
    return -1; // no existe ese IID
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

           el problema que habia antes: cuando stop o kill marcaban el proceso como STOPPED,
           el waitpid igual lo encontraba muerto y lo sobreescribia a ZOMBIE. 
           
           fix: solo marcamos ZOMBIE si el proceso NO fue detenido a proposito.
           para saber eso, agregamos el campo "detenido" al struct proceso en comun.h.
           si detenido == 1, el proceso fue parado por stop o kill -> lo dejamos en STOPPED.
           si detenido == 0, murio solo -> lo marcamos ZOMBIE. */
        pid_t pid_muerto;
        int status;
        while ((pid_muerto = waitpid(-1, &status, WNOHANG)) > 0) {
            for (int i = 0; i < num_procesos; i++) {
                if (procesos[i].pid == pid_muerto && procesos[i].detenido == 0) {
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

        // 2. procesar comando START
        if (msg.comando == CMD_START) {
            pid_t pid = fork();

            if (pid == 0) {
                setpgid(0, 0); // el proceso se convierte en lider de su propio grupo (PGID = su PID)
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
                procesos[num_procesos].detenido = 0; /* recien creado, no fue detenido */
                strcpy(procesos[num_procesos].ruta, msg.ruta);
                num_procesos++;

                char respuesta[64];
                snprintf(respuesta, sizeof(respuesta), "IID: %d\n", iid);
                responder(respuesta);
            }
        }

        //comando PS: responder con la tabla de procesos
        if (msg.comando == CMD_PS) {
            char respuesta[MAX_RESP];
            char linea[512];

            snprintf(respuesta, sizeof(respuesta),
                     "%-6s %-8s %-10s %-10s %s\n",
                     "IID", "PID", "ESTADO", "UPTIME", "BINARIO");

            for (int i = 0; i < num_procesos; i++) {
                time_t seg = time(NULL) - procesos[i].t_inicio;
                int hh = (int)(seg / 3600);
                int mm = (int)((seg % 3600) / 60);
                int ss = (int)(seg % 60);
                char uptime[16];
                snprintf(uptime, sizeof(uptime), "%02d:%02d:%02d", hh, mm, ss);

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

        //comando STOP
        if (msg.comando == CMD_STOP) {
            int idx = buscar_proceso(msg.iid, procesos, num_procesos);

            if (idx == -1) {
                responder("Error: IID no encontrado\n");
                continue;
            }

            procesos[idx].detenido = 1; /* marcamos ANTES de matar, para que el waitpid
                                           del inicio del loop no lo sobreescriba a ZOMBIE */
            kill(procesos[idx].pid, SIGTERM);

            /* esperar a que el proceso muera de verdad antes de marcar STOPPED.
               usamos waitpid sin WNOHANG para este proceso especifico, asi cuando
               el toad-cli haga ps inmediatamente ya vera el estado correcto. */
            waitpid(procesos[idx].pid, NULL, 0);
            procesos[idx].estado = STOPPED;

            responder("Proceso detenido\n");
        }

        //comando STATUS
        if (msg.comando == CMD_STATUS) {
            int idx = buscar_proceso(msg.iid, procesos, num_procesos);
            if (idx != -1) {
                char respuesta[MAX_RESP];

                time_t seg = time(NULL) - procesos[idx].t_inicio;
                int hh = (int)(seg / 3600), mm = (int)((seg % 3600) / 60), ss = (int)(seg % 60);

                char *st;
                if      (procesos[idx].estado == RUNNING) st = "RUNNING";
                else if (procesos[idx].estado == STOPPED) st = "STOPPED";
                else if (procesos[idx].estado == ZOMBIE)  st = "ZOMBIE";
                else                                      st = "FAILED";

                snprintf(respuesta, sizeof(respuesta),
                        "IID: %d\nPID: %d\nRUTA: %s\nESTADO: %s\nUPTIME: %02d:%02d:%02d\n",
                        procesos[idx].iid, procesos[idx].pid, procesos[idx].ruta, st, hh, mm, ss);
                responder(respuesta);
            } else {
                responder("Error: No se encontro el IID\n");
            }
        }

        //comando KILL
        if (msg.comando == CMD_KILL) {
            int idx = buscar_proceso(msg.iid, procesos, num_procesos);
            if (idx != -1) {
                kill(-procesos[idx].pid, SIGKILL); // signo negativo = todo el grupo de procesos
                procesos[idx].estado   = STOPPED;
                procesos[idx].detenido = 1; /* igual que stop, marcamos que fue intencional */
                responder("Proceso y descendientes eliminados (SIGKILL).\n");
            } else {
                responder("Error: IID no encontrado.\n");
            }
        }

        //comando ZOMBIE: listar solo los procesos que murieron solos (sin stop ni kill)
        if (msg.comando == CMD_ZOMBIE) {
            char respuesta[MAX_RESP];
            char linea[512];

            snprintf(respuesta, sizeof(respuesta),
                     "%-6s %-8s %-10s %s\n",
                     "IID", "PID", "UPTIME", "BINARIO");

            int encontrados = 0;
            for (int i = 0; i < num_procesos; i++) {
                if (procesos[i].estado != ZOMBIE) continue; /* saltar los que no son zombie */

                encontrados++;
                time_t seg = time(NULL) - procesos[i].t_inicio;
                int hh = (int)(seg / 3600);
                int mm = (int)((seg % 3600) / 60);
                int ss = (int)(seg % 60);
                char uptime[16];
                snprintf(uptime, sizeof(uptime), "%02d:%02d:%02d", hh, mm, ss);

                snprintf(linea, sizeof(linea),
                         "%-6d %-8d %-10s %s\n",
                         procesos[i].iid, procesos[i].pid, uptime, procesos[i].ruta);

                if (strlen(respuesta) + strlen(linea) < MAX_RESP - 1)
                    strcat(respuesta, linea);
            }

            if (encontrados == 0)
                strcat(respuesta, "(sin procesos zombie)\n");

            responder(respuesta);
        }

    }

    return 0;
}

/*decidi hacer que los pipes abran y cierren en cada iteración del bucle, 
para evitar bloqueos de fifo y tener un modelo request/response más robusto y limpio*/