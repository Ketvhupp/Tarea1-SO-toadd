# Funcionamiento de toadd: Gestor de procesos

La tarea consta de dos programas principales, los cuales están denominados toadd.c y toad-cli.c dentro del código.

- toadd es un gestor administrador de procesos Daemon, corre en segundo plano e independiente de la terminal. Permite ejecutar, 
monitorear y terminar otros procesos, manteniendo registro de su estado durante toda su vida.

- toad-cli es la herramienta de línea de comandos con la que el usuario interactúa con toadd. También corre independientemente.

Además, se incluyeron 4 programas ejecutables (dentro de la carpeta pruebas) de prueba para iniciar y gestionar.


## Arquitectura del sistema 

El proyecto se basa en tres puntos importantes de Sistemas Operativos:

1. Daemonización: toadd se desprende de la terminal con un doble ```fork()``` y ```setsid()```, así, puede seguir corriendo aunque
   se cierre la sesión. Además, redirige su salida a ```/dev/null``` para no ensuciar la consola.

3. Comunicación Inter-Procesos (IPC): Se usan Pipes con nombre (FIFO) localizados en ```/tmp/```. Esto ya que se necesita una
   comuniación entre procesos no relacionados.

   - ```toad_req```: toad-cli envía las peticiones (```mensaje```).

   - ```toad_res```: toadd envía las respuestas de texto.

5. Gestión de Estados: toadd mantiene una tabla de procesos. Usa ```waitpid``` con la bandera ```WNOHANG``` para recolectar procesos
   que terminan naturalmente.

## Guía de uso 

1. Compilaciones:
   Se deben de compilar escencialmente:
   - toadd: ```gcc toadd.c -o toadd -Wall -Wextra -std=c17```
   - toad-cli: ```gcc toad-cli.c -o toad-cli -Wall -Wextra -std=c17```

   Para testar los programas de prueba:
   - haii: ```gcc haii.c -o haii -Wall -Wextra -std=c17```
   - infinito: ```gcc infinito.c -o infinito -Wall -Wextra -std=c17```
   - muere_rapido: ```gcc muere_rapido.c -o muere_rapido -Wall -Wextra -std=c17```
   - familia: ```gcc familia.c -o familia -Wall -Wextra -std=c17```



2. Comandos Disponibles:

   - START: ```./toad-cli start ./haii``` -> Retorna un IID (Internal ID). Puede ser cualquier programa ejecutable.

   - PS: ```./toad-cli ps``` -> Muestra IID, PID, Estado, Uptime y Ruta.

   - STATUS: ```./toad-cli status <IID>``` -> Muestra solo la tabla del proceso con el iid seleccionado.

   - STOP (SIGTERM): ```./toad-cli stop <IID>``` -> Termina el proceso padre amablemente, con el iid seleccionado.

   - KILL (SIGKILL): ```./toad-cli kill <IID>``` -> Elimina al proceso con el iid seleccionado y a todos sus descendientes usando el PGID.

   - ZOMBIE: ```./toad-cli zombie``` -> Lista procesos que terminaron por su cuenta. 

---
#### Detalles de cada programa ejecutable

- haii: Solo vive por 10 segundos. Sirve para probar procesos ```ZOMBIE```.
- infinito: Vive para siempre, sirve para probar los comandos ```STOP``` y ```KILL```, ya que el proceso nunca terminará por si solo.
- muere_rapido: vive solo por 2 segundos. Más eficiente para probar el comando y estado ```ZOMBIE```.
- familia: Crea más procesos con ```fork()```, sirve para probar que el comando ```KILL``` termine con el proceso y todos sus descendientes, dejándolo sin huérfanos.
