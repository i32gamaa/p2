#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>


int main ( )
{
  
	/*---------------------------------------------------- 
		Descriptor del socket y buffer de datos                
	-----------------------------------------------------*/
	int sd; // descriptor del socket
	struct sockaddr_in sockname;
	char buffer[250];
	socklen_t len_sockname;
    	fd_set readfds, auxfds;
    	int salida;
    	int fin = 0;
	int autenticado = 0; // 0 = no autenticado, 1 = autenticado
  	sd = socket (AF_INET, SOCK_STREAM, 0);
	if (sd == -1)
	{
		perror("No se puede abrir el socket cliente\n");
    		exit (1);	
	}

   
    
	/* ------------------------------------------------------------------
		Se rellenan los campos de la estructura con la IP del 
				int n = recv(sd, buffer, sizeof(buffer)-1, 0);
				if(n > 0){
					buffer[n] = '\0';
					// recortar posibles \r y \n del final
					int LB = strlen(buffer);
					while(LB>0 && (buffer[LB-1]=='\n' || buffer[LB-1]=='\r')){ buffer[LB-1] = '\0'; LB--; }
					printf("\n%s\n", buffer);

					// Actualizar estado de autenticación local si el servidor lo confirma (buscar substring)
					if(strstr(buffer, "+Ok. Usuario validado") != NULL) autenticado = 1;
					if(strstr(buffer, "-Err. Usuario incorrecto") != NULL) autenticado = 0;
					if(strstr(buffer, "-Err. Error en la validación") != NULL) autenticado = 0;

					if(strcmp(buffer,"Demasiados clientes conectados") == 0)
						fin =1;

					if(strcmp(buffer,"Desconexión servidor") == 0)
						fin =1;
				} else if(n == 0) {
					// servidor cerró
					printf("Servidor desconectado\n");
					fin = 1;
				}
	-------------------------------------------------------------------*/
	sockname.sin_family = AF_INET;
	sockname.sin_port = htons(2000);
	sockname.sin_addr.s_addr =  inet_addr("127.0.0.1");

	/* ------------------------------------------------------------------
		Se solicita la conexión con el servidor
	-------------------------------------------------------------------*/
	len_sockname = sizeof(sockname);
	
	if (connect(sd, (struct sockaddr *)&sockname, len_sockname) == -1)
	{
		perror ("Error de conexión");
		exit(1);
	}
    
    //Inicializamos las estructuras
    FD_ZERO(&auxfds);
    FD_ZERO(&readfds);
    
    FD_SET(0,&readfds);
    FD_SET(sd,&readfds);

    
	/* ------------------------------------------------------------------
		Se transmite la información
	-------------------------------------------------------------------*/
	do
	{
		auxfds = readfds;
		salida = select(sd+1,&auxfds,NULL,NULL,NULL);

		//Tengo mensaje desde el servidor
		if(FD_ISSET(sd, &auxfds)){

			bzero(buffer,sizeof(buffer));
			int n = recv(sd, buffer, sizeof(buffer)-1, 0);
			if(n > 0){
				buffer[n] = '\0';
				printf("\n%s\n", buffer);

				// Actualizar estado de autenticación local si el servidor lo confirma
				if(strcmp(buffer, "+Ok. Usuario validado") == 0) autenticado = 1;
				if(strcmp(buffer, "-Err. Usuario incorrecto") == 0) autenticado = 0;
				if(strcmp(buffer, "-Err. Error en la validación") == 0) autenticado = 0;

				if(strcmp(buffer,"Demasiados clientes conectados\n") == 0)
					fin =1;

				if(strcmp(buffer,"Desconexión servidor\n") == 0)
					fin =1;
			} else if(n == 0) {
				// servidor cerró
				printf("Servidor desconectado\n");
				fin = 1;
			}

		}
		else
		{

			//He introducido información por teclado
			if(FD_ISSET(0,&auxfds)){
				bzero(buffer,sizeof(buffer));

				if(fgets(buffer,sizeof(buffer),stdin) == NULL){
					continue;
				}

				// Recortar \n final
				int L = strlen(buffer);
				if(L>0 && buffer[L-1]=='\n') buffer[L-1] = '\0';

				// Si no está autenticado, permitir USUARIO, PASSWORD, REGISTRO o INICIAR-PARTIDA (el servidor validará)
				if(!autenticado){
					if(strncmp(buffer, "USUARIO ", 8) != 0 && strncmp(buffer, "PASSWORD ", 9) != 0 
					   && strncmp(buffer, "REGISTRO ", 9) != 0 && strcmp(buffer, "SALIR") != 0
					   && strcmp(buffer, "INICIAR-PARTIDA") != 0){
						printf("Debe autenticarse primero con 'USUARIO usuario' y 'PASSWORD password' (o usar 'REGISTRO -u usuario -p password')\n");
						continue;
					}
				}

				// Si el usuario escribe SALIR (sin newline) cerramos
				if(strcmp(buffer,"SALIR") == 0){
					fin = 1;
				}

				size_t len = strlen(buffer);
				if(len > 0){
					// enviar con el salto de linea original para mantener compatibilidad con servidor
					char tosend[250];
					snprintf(tosend, sizeof(tosend), "%s\n", buffer);
					send(sd, tosend, strlen(tosend), 0);
				}

			}


		}


	}while(fin == 0);
		
    close(sd);

    return 0;
		
}




















