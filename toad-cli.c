/*Qué va a hacer toad-cli:

1- recibe comandos del usuario, o sea el comando de la terminal (como start)
2- llena el struct mensaje para poder enviarlo por pipe
3- lo mete en el pipe REQ_PIPE
4- se queda esperando a que toadd le mande la respuesta por el pipe RES_PIPE

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "comun.h"

int main(int argc, char *argv[]) {

    /* validar que escribieron al menos un comando.
       antes era argc < 3 siempre, pero eso rompia comandos sin argumentos
       como ps y zombie. ahora validamos por comando. */
    if (argc < 2) {
        printf("Uso: %s <comando> [ruta|IID]\n", argv[0]);
        printf("Comandos: start, stop, kill, ps, status, zombie\n");
        return 1;
    }

    mensaje msg;
    memset(&msg, 0, sizeof(mensaje)); // Limpiar la caja por seguridad

    // 2. Identificar el comando
    if (strcmp(argv[1], "start") == 0) {
        if (argc < 3) {
            printf("Uso: %s start <ruta_del_binario>\n", argv[0]);
            return 1;
        }
        msg.comando = CMD_START;
        strncpy(msg.ruta, argv[2], 255);

    } else if (strcmp(argv[1], "stop") == 0) {
        if (argc < 3) {
            printf("Uso: %s stop <IID>\n", argv[0]);
            return 1;
        }
        msg.comando = CMD_STOP;
        msg.iid = atoi(argv[2]);

    } else if (strcmp(argv[1], "kill") == 0) {
        if (argc < 3) {
            printf("Uso: %s kill <IID>\n", argv[0]);
            return 1;
        }
        msg.comando = CMD_KILL;
        msg.iid = atoi(argv[2]);

    } else if (strcmp(argv[1], "ps") == 0) {
        msg.comando = CMD_PS;

    } else if (strcmp(argv[1], "status") == 0) {
        if (argc < 3) {
            printf("Uso: %s status <IID>\n", argv[0]);
            return 1;
        }
        msg.comando = CMD_STATUS;
        msg.iid = atoi(argv[2]);

    } else if (strcmp(argv[1], "zombie") == 0) {
        msg.comando = CMD_ZOMBIE;

    } else {
        printf("Comando no reconocido: %s\n", argv[1]); /*ahi deberian estar toditos paulap, ademas del caso donde el profe no sepa escribir*/
        return 1;
    }

    // 3. ENVIAR la petición al Manager
    int fd_req = open(REQ_PIPE, O_WRONLY);
    if (fd_req == -1) {
        perror("Error al conectar con toadd (esta corriendo?)");
        return 1;
    }
    write(fd_req, &msg, sizeof(mensaje));
    close(fd_req);

// 4. RECIBIR y mostrar la respuesta
    /* todos los comandos ahora responden con texto, asi que leemos
       siempre de la misma forma sin importar cual fue el comando */
    char respuesta[MAX_RESP];
    int fd_res = open(RES_PIPE, O_RDONLY);
    if (fd_res == -1) {
        perror("Error al leer respuesta de toadd");
        return 1;
    }
    ssize_t n = read(fd_res, respuesta, sizeof(respuesta) - 1);
    close(fd_res);
 
    if (n > 0) {
        respuesta[n] = '\0';
        printf("%s", respuesta);
    }
 
    return 0;
}