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
    // 1. Validar que el usuario escribió algo (ej: ./toad-cli start /bin/ls)
    if (argc < 3) {
        printf("Uso: %s <comando> <ruta/IID>\n", argv[0]);
        return 1;
    }

    mensaje msg;
    memset(&msg, 0, sizeof(mensaje)); // Limpiar la caja por seguridad

    // 2. Identificar el comando
    if (strcmp(argv[1], "start") == 0) {
        msg.comando = 1; // Nuestro código para START
        strncpy(msg.ruta, argv[2], 255);
    } else {
        printf("Comando no reconocido.\n");
        return 1;
    }

    // 3. ENVIAR la petición al Manager
    int fd_req = open(REQ_PIPE, O_WRONLY);
    if (fd_req == -1) {
        perror("Error al conectar con toadd (¿está corriendo?)");
        return 1;
    }
    write(fd_req, &msg, sizeof(mensaje));
    close(fd_req);

    // 4. RECIBIR la respuesta del Manager
    int iid_recibido;
    int fd_res = open(RES_PIPE, O_RDONLY);
    read(fd_res, &iid_recibido, sizeof(int));
    close(fd_res);

    // 5. Mostrar el resultado final
    printf("IID: %d\n", iid_recibido);

    return 0;
}