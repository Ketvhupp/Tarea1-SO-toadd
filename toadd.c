#define _POSIX_C_SOURCE 200809L /* activa funciones POSIX como kill() y waitpid(),
                                   que no son parte del estandar C17 puro.
                                   sin esto el compilador tira warnings de "implicit declaration". */

/* toadd: gestor de procesos que corre en background, independiente del terminal.
   se encarga de ejecutar, monitorear y terminar otros procesos,
   manteniendo una tabla interna con su estado durante toda su vida. */

/* la comunicacion con toad-cli se hace con dos pipes con nombre (FIFO),
   uno para recibir comandos y otro para mandar respuestas.
   usamos dos porque los pipes son unidireccionales. */

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

/* manda un string de texto por RES_PIPE.
   la usan todos los comandos para no repetir el open/write/close en cada uno. */
void responder(const char *texto) {
    int fd_res = open(RES_PIPE, O_WRONLY);
    if (fd_res == -1) return;
    write(fd_res, texto, strlen(texto) + 1); /* +1 para incluir el \0 al final */
    close(fd_res);
}

/* busca un proceso en la tabla por su IID.
   devuelve el indice en el arreglo, o -1 si no lo encuentra.
   la usan stop, kill y status porque todos trabajan con un IID especifico. */
int buscar_proceso(int iid_buscado, proceso lista[], int total) {
    for (int i = 0; i < total; i++) {
        if (lista[i].iid == iid_buscado) {
            return i;
        }
    }
    return -1;
}

int main() {

    /* daemonizacion: separarse del terminal con doble fork + setsid.
       - primer fork: el padre (el proceso que lanzaste desde la terminal) sale,
         el hijo sigue corriendo.
       - setsid: crea una nueva sesion sin terminal de control.
       - segundo fork: garantiza que el proceso nunca pueda volver a adquirir
         un terminal aunque lo pida. */
    if (fork() != 0)
        exit(0);

    setsid();

    if (fork() != 0)
        exit(0);

    /* redirigir stdin/stdout/stderr a /dev/null en vez de solo cerrarlos.
       si solo los cerramos, el siguiente open() podria reutilizar esos fd (0,1,2)
       y causaria bugs donde un pipe queda conectado a stdout sin querer. */
    int dev_nulo = open("/dev/null", O_RDWR);
    dup2(dev_nulo, STDIN_FILENO);
    dup2(dev_nulo, STDOUT_FILENO);
    dup2(dev_nulo, STDERR_FILENO);
    close(dev_nulo);

    /* crear los pipes si no existen todavia */
    mkfifo(REQ_PIPE, 0666);
    mkfifo(RES_PIPE, 0666);

    int iid_counter = 2;     /* el IID 1 esta reservado para toadd mismo */
    proceso procesos[100];
    int num_procesos = 0;

    while (1) {

        /* recolectar procesos que terminaron solos.
           WNOHANG = no bloquear, solo revisar si hay alguno muerto y seguir.
           
           solo marcamos ZOMBIE si detenido == 0, o sea si el proceso murio
           por su cuenta. si detenido == 1 significa que lo paramos nosotros
           con stop o kill, y ya lo dejamos en STOPPED, asi que no lo tocamos. */
        pid_t pid_muerto;
        int status;
        while ((pid_muerto = waitpid(-1, &status, WNOHANG)) > 0) {
            for (int i = 0; i < num_procesos; i++) {
                if (procesos[i].pid == pid_muerto && procesos[i].detenido == 0) {
                    procesos[i].estado = ZOMBIE;
                }
            }
        }

        /* esperar el siguiente comando del toad-cli.
           open() bloquea hasta que alguien abra el otro extremo del pipe para escribir,
           lo que nos sirve de sincronizacion natural con toad-cli. */
        mensaje msg;
        int fd_req = open(REQ_PIPE, O_RDONLY);
        ssize_t n = read(fd_req, &msg, sizeof(mensaje));
        close(fd_req);

        if (n <= 0) {
            continue;
        }

        /* START: ejecutar un nuevo binario y registrarlo en la tabla */
        if (msg.comando == CMD_START) {
            pid_t pid = fork();

            if (pid == 0) {
                /* hijo: se convierte en lider de su propio grupo de procesos.
                   esto es necesario para que kill pueda matar al proceso
                   y todos sus descendientes de un solo golpe usando el PGID. */
                setpgid(0, 0);
                char *args[] = {msg.ruta, NULL};
                execvp(msg.ruta, args);
                exit(1); /* si execvp falla, el hijo sale */

            } else {
                /* padre: registrar el proceso en la tabla y responder con su IID */
                int iid = iid_counter++;

                procesos[num_procesos].iid      = iid;
                procesos[num_procesos].pid      = pid;
                procesos[num_procesos].estado   = RUNNING;
                procesos[num_procesos].t_inicio = time(NULL);
                procesos[num_procesos].detenido = 0;
                strcpy(procesos[num_procesos].ruta, msg.ruta);
                num_procesos++;

                char respuesta[64];
                snprintf(respuesta, sizeof(respuesta), "IID: %d\n", iid);
                responder(respuesta);
            }
        }

        /* PS: listar todos los procesos de la tabla con su info completa */
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

        /* STOP: mandar SIGTERM al proceso para que termine educadamente */
        if (msg.comando == CMD_STOP) {
            int idx = buscar_proceso(msg.iid, procesos, num_procesos);

            if (idx == -1) {
                responder("Error: IID no encontrado\n");
                continue;
            }

            /* marcamos detenido ANTES de mandar la senal, para que el waitpid
               del inicio del loop no lo pise y lo marque como ZOMBIE. */
            procesos[idx].detenido = 1;
            kill(procesos[idx].pid, SIGTERM);

            /* esperamos a que el proceso muera de verdad antes de responder.
               sin esto, si toad-cli hace ps inmediatamente despues del stop,
               podria ver el estado incorrecto. */
            waitpid(procesos[idx].pid, NULL, 0);
            procesos[idx].estado = STOPPED;

            responder("Proceso detenido\n");
        }

        /* STATUS: mostrar la info detallada de un proceso especifico */
        if (msg.comando == CMD_STATUS) {
            int idx = buscar_proceso(msg.iid, procesos, num_procesos);

            if (idx == -1) {
                responder("Error: No se encontro el IID\n");
            } else {
                char respuesta[MAX_RESP];

                time_t seg = time(NULL) - procesos[idx].t_inicio;
                int hh = (int)(seg / 3600);
                int mm = (int)((seg % 3600) / 60);
                int ss = (int)(seg % 60);

                char *st;
                if      (procesos[idx].estado == RUNNING) st = "RUNNING";
                else if (procesos[idx].estado == STOPPED) st = "STOPPED";
                else if (procesos[idx].estado == ZOMBIE)  st = "ZOMBIE";
                else                                      st = "FAILED";

                snprintf(respuesta, sizeof(respuesta),
                        "IID: %d\nPID: %d\nRUTA: %s\nESTADO: %s\nUPTIME: %02d:%02d:%02d\n",
                        procesos[idx].iid, procesos[idx].pid,
                        procesos[idx].ruta, st, hh, mm, ss);
                responder(respuesta);
            }
        }

        /* KILL: mandar SIGKILL al proceso y a todos sus descendientes.
           usamos el PGID negativo para que la senal llegue a todo el grupo,
           esto funciona porque en start hicimos setpgid(0,0) en el hijo. */
        if (msg.comando == CMD_KILL) {
            int idx = buscar_proceso(msg.iid, procesos, num_procesos);

            if (idx == -1) {
                responder("Error: IID no encontrado.\n");
            } else {
                procesos[idx].detenido = 1; /* igual que stop, marcamos que fue intencional */
                kill(-procesos[idx].pid, SIGKILL);
                procesos[idx].estado = STOPPED;
                responder("Proceso y descendientes eliminados (SIGKILL).\n");
            }
        }

        /* ZOMBIE: listar solo los procesos que murieron solos, sin stop ni kill */
        if (msg.comando == CMD_ZOMBIE) {
            char respuesta[MAX_RESP];
            char linea[512];

            snprintf(respuesta, sizeof(respuesta),
                     "%-6s %-8s %-10s %s\n",
                     "IID", "PID", "UPTIME", "BINARIO");

            int encontrados = 0;
            for (int i = 0; i < num_procesos; i++) {
                if (procesos[i].estado != ZOMBIE) continue;

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