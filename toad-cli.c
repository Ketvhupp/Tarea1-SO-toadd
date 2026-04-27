/* toad-cli: interfaz de linea de comandos para interactuar con toadd.
   
   lo que hace:
   1. recibe el comando del usuario desde la terminal
   2. llena el struct mensaje y lo manda por REQ_PIPE
   3. espera la respuesta de toadd por RES_PIPE
   4. imprime el resultado */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "comun.h"

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Uso: %s <comando> [ruta|IID]\n", argv[0]);
        printf("Comandos: start, stop, kill, ps, status, zombie\n");
        return 1;
    }

    mensaje msg;
    memset(&msg, 0, sizeof(mensaje)); /* limpiar la estructura por seguridad */

    /* identificar el comando y armar el mensaje.
       validamos argc por separado en cada caso porque ps y zombie
       no necesitan argumentos extra, pero start, stop, kill y status si. */
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
        printf("Comando no reconocido: %s\n", argv[1]);
        return 1;
    }

    /* enviar el mensaje a toadd por REQ_PIPE */
    int fd_req = open(REQ_PIPE, O_WRONLY);
    if (fd_req == -1) {
        perror("Error al conectar con toadd (esta corriendo?)");
        return 1;
    }
    write(fd_req, &msg, sizeof(mensaje));
    close(fd_req);

    /* recibir y mostrar la respuesta.
       todos los comandos responden con texto, asi que siempre
       leemos de la misma forma sin importar cual fue el comando. */
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