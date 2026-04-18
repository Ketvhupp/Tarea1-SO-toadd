/*Qué va a hacer toad-cli:

1- recibe comandos del usuario, o sea el comando de la terminal (como start)
2- llena el struct mensaje para poder enviarlo por pipe
3- lo mete en el pipe REQ_PIPE
4- se queda esperando a que toadd le mande la respuesta por el pipe RES_PIPE

*/