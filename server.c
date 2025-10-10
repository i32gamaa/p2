#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include<signal.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>


#define MSG_SIZE 250 //
#define MAX_CLIENTS 20 //ESTABLECE EL NÚMERO DE CLIENTES
#define port 1865

struct jugadores {

    char login[50];

    char contraseña[50];

};

// Gestión dinámica de cuentas válidas (persistidas en usuarios.txt)
struct jugadores *validos = NULL;
int n_validos = 0;
int capacity_validos = 0;

// Añade un usuario a la lista en memoria (sin persistir)
void add_user_to_memory(const char *user, const char *pass){
    if(n_validos + 1 > capacity_validos){
        int newcap = capacity_validos == 0 ? 8 : capacity_validos * 2;
        struct jugadores *tmp = realloc(validos, newcap * sizeof(struct jugadores));
        if(!tmp) return; // falló realloc, no añadimos
        validos = tmp;
        capacity_validos = newcap;
    }
    strncpy(validos[n_validos].login, user, sizeof(validos[n_validos].login)-1);
    validos[n_validos].login[sizeof(validos[n_validos].login)-1] = '\0';
    strncpy(validos[n_validos].contraseña, pass, sizeof(validos[n_validos].contraseña)-1);
    validos[n_validos].contraseña[sizeof(validos[n_validos].contraseña)-1] = '\0';
    n_validos++;
}

// Devuelve índice en validos[] o -1 si no existe
int find_user_index(const char *user){
    for(int i=0;i<n_validos;i++){
        if(strcmp(validos[i].login, user) == 0) return i;
    }
    return -1;
}

// Comprueba contraseña por índice
int check_password(int idx, const char *pass){
    if(idx < 0 || idx >= n_validos) return 0;
    return strcmp(validos[idx].contraseña, pass) == 0;

}

// Carga usuarios desde el fichero "usuarios.txt" (formato: usuario contraseña por línea)
void load_users_from_file(const char *path){
    FILE *f = fopen(path, "r");
    if(!f) return; // no existe aún
    char user[50], pass[50];
    while(fscanf(f, "%49s %49s", user, pass) == 2){
        if(find_user_index(user) == -1){
            add_user_to_memory(user, pass);
        }
    }
    fclose(f);
}

// Persiste un usuario al fichero (append)
int save_user_to_file(const char *path, const char *user, const char *pass){
    FILE *f = fopen(path, "a");
    if(!f) return 0;
    fprintf(f, "%s %s\n", user, pass);
    fclose(f);
    return 1;
}


/*
 * El servidor ofrece el servicio de un chat
 */

void manejador(int signum);
void salirCliente(int socket, fd_set * readfds, int * numClientes, int arrayClientes[]);



int main ( )
{
  
	/*---------------------------------------------------- 
		Descriptor del socket y buffer de datos                
	-----------------------------------------------------*/
	int sd, new_sd;
	struct sockaddr_in sockname, from;
	char buffer[MSG_SIZE];
	socklen_t from_len;
    	fd_set readfds, auxfds;
   	 int salida;
    int arrayClientes[MAX_CLIENTS];
    int numClientes = 0;
    int autenticado[MAX_CLIENTS];
    char usernames[MAX_CLIENTS][50];
    int user_index_by_client[MAX_CLIENTS];
    int in_game[MAX_CLIENTS];
    int game_target[MAX_CLIENTS];
    int partner_socket[MAX_CLIENTS];
    int score[MAX_CLIENTS];
    int last_roll_num[MAX_CLIENTS];
    int last_roll_values[MAX_CLIENTS][2];
    int passes_used[MAX_CLIENTS];
    int turn_owner[MAX_CLIENTS];
    int planted[MAX_CLIENTS];
    int waiting_socket = -1; // socket descriptor del jugador en espera
   	 //contadores
    	int i,j,k;
	int recibidos;
   	 char identificador[MSG_SIZE];
    
    	int on, ret;

    struct jugadores *vector_jugadores;


    
	/* --------------------------------------------------
		Se abre el socket 
	---------------------------------------------------*/
  	sd = socket (AF_INET, SOCK_STREAM, 0);
	if (sd == -1)
	{
		perror("No se puede abrir el socket cliente\n");
    		exit (1);	
	}
    
    	// Activaremos una propiedad del socket para permitir· que otros
    	// sockets puedan reutilizar cualquier puerto al que nos enlacemos.
    	// Esto permite· en protocolos como el TCP, poder ejecutar un
    	// mismo programa varias veces seguidas y enlazarlo siempre al
   	 // mismo puerto. De lo contrario habrÌa que esperar a que el puerto
    	// quedase disponible (TIME_WAIT en el caso de TCP)
    	on=1;
    	ret = setsockopt( sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));



	sockname.sin_family = AF_INET;
	sockname.sin_port = htons(2000);
	sockname.sin_addr.s_addr =  INADDR_ANY;

	if (bind (sd, (struct sockaddr *) &sockname, sizeof (sockname)) == -1)
	{
		perror("Error en la operación bind");
		exit(1);
	}
	

   	/*---------------------------------------------------------------------
		Del las peticiones que vamos a aceptar sólo necesitamos el 
		tamaño de su estructura, el resto de información (familia, puerto, 
		ip), nos la proporcionará el método que recibe las peticiones.
   	----------------------------------------------------------------------*/
	from_len = sizeof (from);


	if(listen(sd,1) == -1){
		perror("Error en la operación de listen");
		exit(1);
	}

	
    	printf("El servidor está esperando conexiones...\n");	//Inicializar los conjuntos fd_set
    	
    	FD_ZERO(&readfds);
    	FD_ZERO(&auxfds);
    	FD_SET(sd,&readfds);
    	FD_SET(0,&readfds);

    	// Inicializar arrays de estado de autenticación
    	for(i = 0; i < MAX_CLIENTS; i++){
    		autenticado[i] = 0;
    		usernames[i][0] = '\0';
    		arrayClientes[i] = -1;
        user_index_by_client[i] = -1;
        in_game[i] = 0;
        game_target[i] = 0;
                partner_socket[i] = -1;
                score[i] = 0;
                last_roll_num[i] = 0;
                last_roll_values[i][0] = last_roll_values[i][1] = 0;
                passes_used[i] = 0;
                turn_owner[i] = 0;
                planted[i] = 0;
    	}

    // Cargar usuarios desde fichero (si existe)
    load_users_from_file("usuarios.txt");

    // Semilla para números aleatorios
    srand(time(NULL));
    
   	
    	//Capturamos la señal SIGINT (Ctrl+c)
    	signal(SIGINT,manejador);
    
	/*-----------------------------------------------------------------------
		El servidor acepta una petición
	------------------------------------------------------------------------ */
	while(1){
            
            //Esperamos recibir mensajes de los clientes (nuevas conexiones o mensajes de los clientes ya conectados)
            
            auxfds = readfds;
            
            salida = select(FD_SETSIZE,&auxfds,NULL,NULL,NULL);
            
            if(salida > 0){
                
                
                for(i=0; i<FD_SETSIZE; i++){
                    
                    //Buscamos el socket por el que se ha establecido la comunicación
                    if(FD_ISSET(i, &auxfds)) {
                        
                        if( i == sd){
                            
                            if((new_sd = accept(sd, (struct sockaddr *)&from, &from_len)) == -1){
                                perror("Error aceptando peticiones");
                            }
                            else
                            {
                                if(numClientes < MAX_CLIENTS){
                                    arrayClientes[numClientes] = new_sd;
                                    // Inicializar estado para este cliente
                                    autenticado[numClientes] = 0;
                                    usernames[numClientes][0] = '\0';
                                    user_index_by_client[numClientes] = -1;
                                    in_game[numClientes] = 0;
                                    game_target[numClientes] = 0;
                                    FD_SET(new_sd,&readfds);
                                    numClientes++;

                                    strcpy(buffer, "Regístrate o inicie sesión\n");

                                    send(new_sd,buffer,strlen(buffer),0);


                                }
                                else
                                {
                                    bzero(buffer,sizeof(buffer));
                                    strcpy(buffer,"Demasiados clientes conectados\n");
                                    send(new_sd,buffer,strlen(buffer),0);
                                    close(new_sd);
                                }
                                
                            }
                            
                            
                        }
                        else if (i == 0){
                            //Se ha introducido información de teclado
                            bzero(buffer, sizeof(buffer));
                            fgets(buffer, sizeof(buffer),stdin);
                            
                            //Controlar si se ha introducido "SALIR", cerrando todos los sockets y finalmente saliendo del servidor. (implementar)
                            if(strcmp(buffer,"SALIR\n") == 0){
                             
                                    for (j = 0; j < numClientes; j++){
				   bzero(buffer, sizeof(buffer));
				   strcpy(buffer,"Desconexión servidor\n"); 
                                    send(arrayClientes[j],buffer , strlen(buffer),0);
                                    close(arrayClientes[j]);
                                    FD_CLR(arrayClientes[j],&readfds);
                                }
                                    close(sd);
                                    exit(-1);
                                
                                
                            }
                            //Mensajes que se quieran mandar a los clientes (implementar)
                            
                        } 
                        else{
                            bzero(buffer,sizeof(buffer));
                            
                            recibidos = recv(i,buffer,sizeof(buffer),0);
                            
                            if(recibidos > 0){

                                // Encontrar índice del cliente en arrayClientes
                                int idx = -1;
                                for(j = 0; j < numClientes; j++){
                                    if(arrayClientes[j] == i){ idx = j; break; }
                                }

                                // Normalizar buffer (asegurar terminador y quitar \r\n)
                                if(recibidos >= MSG_SIZE) recibidos = MSG_SIZE-1;
                                buffer[recibidos] = '\0';
                                int L = strlen(buffer);
                                while(L>0 && (buffer[L-1]=='\n' || buffer[L-1]=='\r')){ buffer[L-1] = '\0'; L--; }

                                if(strcmp(buffer,"SALIR") == 0){
                                    // Eliminar cliente
                                    int __idx_tmp = idx;
                                    salirCliente(i,&readfds,&numClientes,arrayClientes);
                                    if(__idx_tmp != -1){
                                        int __j;
                    for(__j = __idx_tmp; __j < numClientes; __j++){
                        autenticado[__j] = autenticado[__j + 1];
                        strncpy(usernames[__j], usernames[__j + 1], sizeof(usernames[0]));
                        user_index_by_client[__j] = user_index_by_client[__j + 1];
                        in_game[__j] = in_game[__j + 1];
                        game_target[__j] = game_target[__j + 1];
                    }
                    autenticado[numClientes] = 0;
                    usernames[numClientes][0] = '\0';
                    user_index_by_client[numClientes] = -1;
                    in_game[numClientes] = 0;
                    game_target[numClientes] = 0;
                    // Si el socket eliminado era el que estaba esperando, limpiarlo
                    if(waiting_socket == i) waiting_socket = -1;
                                    }

                                }
                                else {
                                    // Si no se encontró índice, ignorar
                                    if(idx == -1) continue;

                                    // Si no autenticado, procesar USUARIO/PASSWORD
                                    if(autenticado[idx] == 0){
                                        if(strncmp(buffer, "USUARIO ", 8) == 0){
                                            char *user = buffer + 8;
                                            int found = find_user_index(user);
                                            if(found != -1){
                                                bzero(buffer,sizeof(buffer));
                                                strcpy(buffer, "+Ok. Usuario correcto\n");
                                                send(i, buffer, strlen(buffer), 0);
                                                // guardar índice del usuario para este cliente
                                                user_index_by_client[idx] = found;
                                                strncpy(usernames[idx], user, sizeof(usernames[idx])-1);
                                                usernames[idx][sizeof(usernames[idx])-1] = '\0';
                                            } else {
                                                bzero(buffer,sizeof(buffer));
                                                strcpy(buffer, "-Err. Usuario incorrecto\n");
                                                send(i, buffer, strlen(buffer), 0);
                                            }
                                        }
                                        else if(strncmp(buffer, "PASSWORD ", 9) == 0){
                                            char *pass = buffer + 9;
                                            if(user_index_by_client[idx] == -1){
                                                bzero(buffer,sizeof(buffer));
                                                strcpy(buffer, "-Err. Error en la validación");
                                                send(i, buffer, strlen(buffer), 0);
                                            } else {
                                                if(check_password(user_index_by_client[idx], pass)){
                                                    bzero(buffer,sizeof(buffer));
                                                    strcpy(buffer, "+Ok. Usuario validado\n");
                                                    send(i, buffer, strlen(buffer), 0);
                                                    autenticado[idx] = 1;
                                                } else {
                                                    bzero(buffer,sizeof(buffer));
                                                    strcpy(buffer, "-Err. Error en la validación");
                                                    send(i, buffer, strlen(buffer), 0);
                                                }
                                            }
                                        }
                                        else if(strncmp(buffer, "REGISTRO ", 9) == 0){
                                            // Formato esperado: REGISTRO -u usuario -p password
                                            char usertmp[50], passtmp[50];
                                            usertmp[0] = '\0'; passtmp[0] = '\0';
                                            // intentar parsear
                                            // buscamos -u y -p
                                            char *p = buffer + 9;
                                            // parse simple
                                            char opt[3];
                                            while(*p){
                                                while(*p == ' ') p++;
                                                if(sscanf(p, "%2s", opt) != 1) break;
                                                if(strcmp(opt, "-u") == 0){
                                                    // avanzar el puntero hasta el usuario
                                                    char tmp[100];
                                                    if(sscanf(p+2, "%99s", tmp) == 1){
                                                        strncpy(usertmp, tmp, sizeof(usertmp)-1);
                                                        usertmp[sizeof(usertmp)-1] = '\0';
                                                    }
                                                } else if(strcmp(opt, "-p") == 0){
                                                    char tmp[100];
                                                    if(sscanf(p+2, "%99s", tmp) == 1){
                                                        strncpy(passtmp, tmp, sizeof(passtmp)-1);
                                                        passtmp[sizeof(passtmp)-1] = '\0';
                                                    }
                                                }
                                                // avanzar p hasta siguiente espacio después de la opción leída
                                                while(*p && *p != ' ') p++;
                                            }

                                            if(usertmp[0] == '\0' || passtmp[0] == '\0'){
                                                bzero(buffer,sizeof(buffer));
                                                strcpy(buffer, "-Err. Formato registro incorrecto\n");
                                                send(i, buffer, strlen(buffer), 0);
                                            } else {
                                                if(find_user_index(usertmp) != -1){
                                                    bzero(buffer,sizeof(buffer));
                                                    strcpy(buffer, "-Err. Usuario ya existente\n");
                                                    send(i, buffer, strlen(buffer), 0);
                                                } else {
                                                    // añadir y persistir
                                                    add_user_to_memory(usertmp, passtmp);
                                                    if(save_user_to_file("usuarios.txt", usertmp, passtmp)){
                                                        bzero(buffer,sizeof(buffer));
                                                        strcpy(buffer, "+Ok. Registro completado\n");
                                                        send(i, buffer, strlen(buffer), 0);
                                                    } else {
                                                        bzero(buffer,sizeof(buffer));
                                                        strcpy(buffer, "-Err. No se pudo guardar usuario\n");
                                                        send(i, buffer, strlen(buffer), 0);
                                                    }
                                                }
                                            }
                                        }
                                        else{
                                            // Mensaje no válido hasta autenticación
                                            bzero(buffer,sizeof(buffer));
                                            strcpy(buffer, "-Err. Debe autenticarse");
                                            send(i, buffer, strlen(buffer), 0);
                                        }
                                    }
                                    else{
                                        // Cliente autenticado: reenviar mensaje al resto
                                        // Comprobar comando iniciar partida
                                        if(strcmp(buffer, "INICIAR-PARTIDA") == 0){
                                            // índice del cliente
                                            int idx_cliente = idx;
                                            // Si ya está en juego, ignorar
                                            if(in_game[idx_cliente]){
                                                bzero(buffer,sizeof(buffer));
                                                strcpy(buffer, "-Err. Ya en partida");
                                                send(i, buffer, strlen(buffer), 0);
                                            } else {
                                                if(waiting_socket == -1){
                                                    // No hay nadie esperando: ponerlo en espera
                                                    waiting_socket = i;
                                                    bzero(buffer,sizeof(buffer));
                                                    strcpy(buffer, "+Ok. Esperando otro jugador\n");
                                                    send(i, buffer, strlen(buffer), 0);
                                                } else {
                                                    // Hay alguien esperando: formar pareja si no es el mismo socket
                                                    if(waiting_socket == i){
                                                        bzero(buffer,sizeof(buffer));
                                                        strcpy(buffer, "-Err. Ya estás esperando otro jugador");
                                                        send(i, buffer, strlen(buffer), 0);
                                                    } else {
                                                        int other = waiting_socket;
                                                        // localizar índices internos para marcar in_game
                                                        int idx_other = -1;
                                                        for(j=0;j<numClientes;j++) if(arrayClientes[j] == other){ idx_other = j; break; }
                                                        int idx_this = idx_cliente;
                                                        int objetivo = rand() % 100 + 1; // número objetivo 1..100
                                                        in_game[idx_other] = 1;
                                                        in_game[idx_this] = 1;
                                                        game_target[idx_other] = objetivo;
                                                        game_target[idx_this] = objetivo;
                                                        // Inicializar estado de partida para ambos
                                                        partner_socket[idx_other] = i;
                                                        partner_socket[idx_this] = other;
                                                        score[idx_other] = 0;
                                                        score[idx_this] = 0;
                                                        last_roll_num[idx_other] = 0;
                                                        last_roll_num[idx_this] = 0;
                                                        last_roll_values[idx_other][0] = last_roll_values[idx_other][1] = 0;
                                                        last_roll_values[idx_this][0] = last_roll_values[idx_this][1] = 0;
                                                        passes_used[idx_other] = 0;
                                                        passes_used[idx_this] = 0;
                                                        planted[idx_other] = 0;
                                                        planted[idx_this] = 0;
                                                        // decidir primer turno: el que inició queda como owner
                                                        turn_owner[idx_this] = 1;
                                                        turn_owner[idx_other] = 0;
                                                        // enviar a ambos
                                                        char msgpart[MSG_SIZE];
                                                        snprintf(msgpart, sizeof(msgpart), "+Ok. Empieza la partida. NÚMERO OBJETIVO: [%d]\n", objetivo);
                                                        send(other, msgpart, strlen(msgpart), 0);
                                                        send(i, msgpart, strlen(msgpart), 0);
                                                        // liberar waiting
                                                        waiting_socket = -1;
                                                    }
                                                }
                                            }
                                        } else {
                                            // Si está en partida, procesar comandos de juego
                                            if(in_game[idx]){
                                                // solo el dueño del turno puede tirar o plantar; no tirar si ya plantado
                                                if(strncmp(buffer, "TIRAR-DADOS", 11) == 0){
                                                    // formato: TIRAR-DADOS n (n=1 o 2)
                                                    int n = 0;
                                                    if(sscanf(buffer+11, "%d", &n) != 1){
                                                        bzero(buffer,sizeof(buffer));
                                                        strcpy(buffer, "-Err Número de dados no válido\n");
                                                        send(i, buffer, strlen(buffer), 0);
                                                    } else if(n < 1 || n > 2){
                                                        bzero(buffer,sizeof(buffer));
                                                        strcpy(buffer, "-Err Número de dados no válido\n");
                                                        send(i, buffer, strlen(buffer), 0);
                                                    } else {
                                                        // comprobar dueño del turno
                                                        if(!turn_owner[idx]){
                                                            bzero(buffer,sizeof(buffer));
                                                            strcpy(buffer, "-Err. No es tu turno\n");
                                                            send(i, buffer, strlen(buffer), 0);
                                                        } else if(planted[idx]){
                                                            bzero(buffer,sizeof(buffer));
                                                            strcpy(buffer, "-Err. Ya te has plantado\n");
                                                            send(i, buffer, strlen(buffer), 0);
                                                        } else {
                                                            // tirar n dados
                                                            int valores[2] = {0,0};
                                                            int suma = 0;
                                                            for(int d=0; d<n; d++){
                                                                int v = (rand() % 6) + 1; // 1..6
                                                                valores[d] = v;
                                                                suma += v;
                                                            }
                                                            int nueva_puntuacion = score[idx] + suma;
                                                            if(score[idx] >= game_target[idx]){
                                                                // ya alcanzó el objetivo, no puede tirar más
                                                                bzero(buffer,sizeof(buffer));
                                                                strcpy(buffer, "-Err. Ha alcanzado el número objetivo, debe enviar PLANTARME\n");
                                                                send(i, buffer, strlen(buffer), 0);
                                                            } else if(nueva_puntuacion > game_target[idx]){
                                                                bzero(buffer,sizeof(buffer));
                                                                snprintf(buffer, sizeof(buffer), "-Err. Excedido el valor de %d\n", game_target[idx]);
                                                                send(i, buffer, strlen(buffer), 0);
                                                            } else {
                                                                // actualizar estado
                                                                score[idx] = nueva_puntuacion;
                                                                last_roll_num[idx] = n;
                                                                last_roll_values[idx][0] = valores[0];
                                                                last_roll_values[idx][1] = valores[1];
                                                                // enviar al jugador su tirada
                                                                if(n == 1){
                                                                    bzero(buffer,sizeof(buffer));
                                                                    snprintf(buffer, sizeof(buffer), "+Ok.[<DADO 1>,%d]\n", valores[0]);
                                                                    send(i, buffer, strlen(buffer), 0);
                                                                } else {
                                                                    bzero(buffer,sizeof(buffer));
                                                                    snprintf(buffer, sizeof(buffer), "+Ok.[<DADO 1>,%d; <DADO 2>,%d]\n", valores[0], valores[1]);
                                                                    send(i, buffer, strlen(buffer), 0);
                                                                }
                                                                // enviar también la puntuación total y la tirada previa del oponente
                                                                int partner = partner_socket[idx];
                                                                int idx_partner = -1;
                                                                for(j=0;j<numClientes;j++) if(arrayClientes[j] == partner){ idx_partner = j; break; }
                                                                if(idx_partner != -1){
                                                                    // informar al jugador de la puntuación total y la última tirada del oponente
                                                                    char info[MSG_SIZE];
                                                                    int opp_last_num = last_roll_num[idx_partner];
                                                                    if(opp_last_num == 0){
                                                                        snprintf(info, sizeof(info), "PUNTUACION-TOTAL: %d. Oponente no ha tirado aún.\n", score[idx]);
                                                                    } else if(opp_last_num == 1){
                                                                        snprintf(info, sizeof(info), "PUNTUACION-TOTAL: %d. Oponente ultima tirada: [<DADO 1>,%d]\n", score[idx], last_roll_values[idx_partner][0]);
                                                                    } else {
                                                                        snprintf(info, sizeof(info), "PUNTUACION-TOTAL: %d. Oponente ultima tirada: [<DADO 1>,%d; <DADO 2>,%d]\n", score[idx], last_roll_values[idx_partner][0], last_roll_values[idx_partner][1]);
                                                                    }
                                                                    send(i, info, strlen(info), 0);
                                                                    // tras una tirada válida, pasar el turno al partner
                                                                    if(idx_partner != -1){
                                                                        turn_owner[idx] = 0;
                                                                        turn_owner[idx_partner] = 1;
                                                                        bzero(buffer,sizeof(buffer));
                                                                        strcpy(buffer, "+Ok. Es tu turno\n");
                                                                        send(partner, buffer, strlen(buffer), 0);
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                } else if(strncmp(buffer, "NOTIRAR-DADOS", 13) == 0 || strncmp(buffer, "NO-TIRAR-DADOS", 13) == 0){
                                                    // pasar turno, hasta 3 veces por jugador
                                                    if(passes_used[idx] >= 3){
                                                        bzero(buffer,sizeof(buffer));
                                                        strcpy(buffer, "-Err. No puedes pasar más veces\n");
                                                        send(i, buffer, strlen(buffer), 0);
                                                    } else {
                                                        passes_used[idx]++;
                                                        bzero(buffer,sizeof(buffer));
                                                        strcpy(buffer, "+Ok. Pasas el turno\n");
                                                        send(i, buffer, strlen(buffer), 0);
                                                        // cambiar turno al otro
                                                        int partner = partner_socket[idx];
                                                        int idx_partner = -1;
                                                        for(j=0;j<numClientes;j++) if(arrayClientes[j] == partner){ idx_partner = j; break; }
                                                        if(idx_partner != -1){
                                                            turn_owner[idx] = 0;
                                                            turn_owner[idx_partner] = 1;
                                                            // notificar al partner que es su turno (opcional)
                                                            bzero(buffer,sizeof(buffer));
                                                            strcpy(buffer, "+Ok. Es tu turno\n");
                                                            send(partner, buffer, strlen(buffer), 0);
                                                        }
                                                    }
                                                } else if(strcmp(buffer, "PLANTARME") == 0){
                                                    // plantarme: no puede tirar más
                                                    planted[idx] = 1;
                                                    bzero(buffer,sizeof(buffer));
                                                    strcpy(buffer, "+Ok. Te has plantado\n");
                                                    send(i, buffer, strlen(buffer), 0);
                                                    // cambiar turno al partner
                                                    int partner = partner_socket[idx];
                                                    int idx_partner = -1;
                                                    for(j=0;j<numClientes;j++) if(arrayClientes[j] == partner){ idx_partner = j; break; }
                                                    if(idx_partner != -1){
                                                        turn_owner[idx] = 0;
                                                        turn_owner[idx_partner] = 1;
                                                        bzero(buffer,sizeof(buffer));
                                                        strcpy(buffer, "+Ok. Es tu turno\n");
                                                        send(partner, buffer, strlen(buffer), 0);
                                                    }
                                                } else {
                                                    // comando desconocido dentro de partida
                                                    bzero(buffer,sizeof(buffer));
                                                    strcpy(buffer, "Comando incorrecto\n");
                                                    send(i, buffer, strlen(buffer), 0);
                                                }
                                            } else {
                                                // reenviar mensaje normal
                                                char identificador_local[MSG_SIZE];
                                                snprintf(identificador_local, sizeof(identificador_local), "<%d>: %s", i, buffer);
                                                bzero(buffer,sizeof(buffer));
                                                strcpy(buffer, identificador_local);
                                                printf("%s\n", buffer);
                                                for(j=0; j<numClientes; j++)
                                                    if(arrayClientes[j] != i)
                                                        send(arrayClientes[j],buffer,strlen(buffer),0);
                                            }
                                        }
                                    }
                                }

                            }
                            //Si el cliente introdujo ctrl+c
                            if(recibidos== 0)
                            {
                                printf("El socket %d, ha introducido ctrl+c\n", i);
                                //Eliminar ese socket y reordenar estados
                                int __idx_tmp2 = -1;
                                for(__idx_tmp2 = 0; __idx_tmp2 < numClientes; __idx_tmp2++){
                                    if(arrayClientes[__idx_tmp2] == i) break;
                                }
                                salirCliente(i,&readfds,&numClientes,arrayClientes);
                                if(__idx_tmp2 != -1){
                                    int __j2;
                    for(__j2 = __idx_tmp2; __j2 < numClientes; __j2++){
                        autenticado[__j2] = autenticado[__j2 + 1];
                        strncpy(usernames[__j2], usernames[__j2 + 1], sizeof(usernames[0]));
                        user_index_by_client[__j2] = user_index_by_client[__j2 + 1];
                        in_game[__j2] = in_game[__j2 + 1];
                        game_target[__j2] = game_target[__j2 + 1];
                    }
                    autenticado[numClientes] = 0;
                    usernames[numClientes][0] = '\0';
                    user_index_by_client[numClientes] = -1;
                    in_game[numClientes] = 0;
                    game_target[numClientes] = 0;
                    if(waiting_socket == i) waiting_socket = -1;
                                }
                            }
                        }
                    }
                }
            }
		}

	close(sd);
	return 0;
	
}

void salirCliente(int socket, fd_set * readfds, int * numClientes, int arrayClientes[]){
  
    char buffer[250];
    int j;
    
    close(socket);
    FD_CLR(socket,readfds);
    
    //Re-estructurar el array de clientes
    for (j = 0; j < (*numClientes) - 1; j++)
        if (arrayClientes[j] == socket)
            break;
    for (; j < (*numClientes) - 1; j++)
        (arrayClientes[j] = arrayClientes[j+1]);
    
    (*numClientes)--;
    
    bzero(buffer,sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "Desconexión del cliente <%d>\n", socket);
    
    for(j=0; j<(*numClientes); j++)
        if(arrayClientes[j] != socket)
            send(arrayClientes[j],buffer,strlen(buffer),0);


}


void manejador (int signum){
    printf("\nSe ha recibido la señal sigint\n");
    signal(SIGINT,manejador);
    
    //Implementar lo que se desee realizar cuando ocurra la excepción de ctrl+c en el servidor
}
