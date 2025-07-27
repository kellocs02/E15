#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>


/*Per ogni nodo creiamo un thread che invoca la funzione read, la read permette al nodo di vedere se su una pipe ci sono dati che devono essere letti*/
/*Ogni nodo avrà una lista dei nodi vicini*/
/*Il nodo deve distribuire il messaggio a tutti i nodi vicini tranne a quello che glielo ha inviato*/
/*Gli archi tra i nodi saranno gestiti da due pipe*/

typedef struct {
    int snd_flag;     
                      
    int messaggio;       

    int parent;       
                      
} NodeState;


typedef struct{
    int id_nodo;
    int *vicini;
    int num_vicini;
    NodeState stato;
}Nodo;




// Funzione msg: restituisce il messaggio da inviare al vicino i
int msg(NodeState w, int i) {
  if (w.snd_flag) {
        return w.messaggio; //se il flag è a 1 ritorna il messaggio da inviare
    } else {
        return -1; // -1 rappresenta 'null', cioè nessun messaggio da inviare
    }
}

char* ApriFile(int id_nodo, int id_nodo_vicino) {
    char* percorso = malloc(50);
    if (!percorso) {
        perror("malloc");
        exit(1);
    }

    if(id_nodo > id_nodo_vicino){
        sprintf(percorso, "fifo_%d_%d", id_nodo_vicino, id_nodo);
    } else {
        sprintf(percorso, "fifo_%d_%d", id_nodo, id_nodo_vicino);
    }

    return percorso;
}


//il nodo accetta messaggi solo la prima volta
//se ha già un nodo parent ha già ricevuto un messaggio
NodeState stf(NodeState w, int *y, int num_vicini, int *vicini) {
    if (w.parent != -1) {
        return w;
    }

    for (int i = 0; i < num_vicini; i++) {
        if (y[i] != -1) {
            w.parent = vicini[i];     
            w.messaggio = y[i];
            w.snd_flag = 1;
            break;
        }
    }

    return w;
}

//creiamo le fifo per ogni nodo con i suoi vicini
void CreaFifo(Nodo nodo){
    for (int i=0; i<nodo.num_vicini; i++){
        if (nodo.id_nodo < nodo.vicini[i]) {  // solo se il nodo ha id minore
            char percorso[50];
            sprintf(percorso, "fifo_%d_%d", nodo.id_nodo, nodo.vicini[i]);
            if (mkfifo(percorso, 0666) == -1) {
                perror("errore creazione fifo");
            }
        }
    }
}



Nodo CreaNodo(int id){
    Nodo nodo; //alloco lo spazio in memoria per il nodo
    switch (id)
    {
    case 1:
        nodo.id_nodo=id; 
        nodo.num_vicini=4;
        nodo.vicini=malloc(nodo.num_vicini*sizeof(int));
        nodo.vicini[0]=2;
        nodo.vicini[1]=3;
        nodo.vicini[2]=4;
        nodo.vicini[3]=5;
        nodo.stato.snd_flag=1; //indica che il nodo è pronto ad inviare il messaggio
        nodo.stato.messaggio=55; //messaggio da inviare
        break;
    case 2:
        nodo.id_nodo=id; 
        nodo.num_vicini=4;
        nodo.vicini=malloc(nodo.num_vicini*sizeof(int));
        nodo.vicini[0]=1;
        nodo.vicini[1]=3;
        nodo.vicini[2]=4;
        nodo.vicini[3]=5;
        nodo.stato.snd_flag=0;
        nodo.stato.parent=-1;
        break;
    case 3:
        nodo.id_nodo=id; 
        nodo.num_vicini=4;
        nodo.vicini=malloc(nodo.num_vicini*sizeof(int));
        nodo.vicini[0]=1;
        nodo.vicini[1]=2;
        nodo.vicini[2]=4;
        nodo.vicini[3]=5;
        nodo.stato.snd_flag=0;
        nodo.stato.parent=-1;
        break;
    case 4: 
        nodo.id_nodo=id; 
        nodo.num_vicini=4;
        nodo.vicini=malloc(nodo.num_vicini*sizeof(int));
        nodo.vicini[0]=1;
        nodo.vicini[1]=2;
        nodo.vicini[2]=3;
        nodo.vicini[3]=6;
        nodo.stato.snd_flag=0;
        nodo.stato.parent=-1;
        break;
    case 5:
        nodo.id_nodo=id; 
        nodo.num_vicini=4;
        nodo.vicini=malloc(nodo.num_vicini*sizeof(int));
        nodo.vicini[0]=1;
        nodo.vicini[1]=2;
        nodo.vicini[2]=3;
        nodo.vicini[3]=6;
        nodo.stato.snd_flag=0;
        nodo.stato.parent=-1;
        break;
    case 6:
        nodo.id_nodo=id; 
        nodo.num_vicini=2;
        nodo.vicini=malloc(nodo.num_vicini*sizeof(int));
        nodo.vicini[0]=4;
        nodo.vicini[1]=5;
        nodo.stato.snd_flag=0;
        nodo.stato.parent=-1;
        break;
    default:
        printf("ERRORE NELL'IDENTIFICATIVO DEL NODO");
        exit(1);
        break;
    }
    CreaFifo(nodo);
    printf("Nodo e fifo create\n");
    return nodo;
}




void* funzioneThread(void* args) {
    int id = *((int*)args);
    Nodo nodo = CreaNodo(id);
    
    printf("È stato creato il nodo: %d\n", id);

    int numero_vicini = nodo.num_vicini;
    int y[numero_vicini];
    for (int i = 0; i < numero_vicini; i++) {
        y[i] = -1; // inizializza array dei messaggi ricevuti
    }

    int messaggio_inviato = 0;

    // Apriamo tutte le FIFO una volta e le teniamo aperte
    int fd_read[numero_vicini];
    int fd_write[numero_vicini];

    for (int i = 0; i < numero_vicini; i++) {
        char* percorso = ApriFile(nodo.id_nodo, nodo.vicini[i]);

        // FIFO aperta in lettura NON BLOCCANTE
        fd_read[i] = open(percorso, O_RDONLY | O_NONBLOCK);
        if (fd_read[i] == -1) {
            perror("Errore apertura FIFO in lettura");
            // puoi decidere se terminare o tentare ancora dopo
        }

        // FIFO aperta in scrittura NON BLOCCANTE
        fd_write[i] = open(percorso, O_WRONLY | O_NONBLOCK);
        if (fd_write[i] == -1) {
            if (errno == ENXIO) {
                // Nessun lettore, potrebbe succedere all'inizio
                printf("[Nodo %d] Nessun reader per FIFO %s (ENXIO)\n", nodo.id_nodo, percorso);
                // puoi chiudere o lasciare così
            } else {
                perror("Errore apertura FIFO in scrittura");
            }
        }

        free(percorso);
    }

    while (1) {
        if (nodo.stato.snd_flag == 0) {
            for (int i = 0; i < numero_vicini; i++) {
                if (fd_read[i] != -1) {
                    ssize_t n = read(fd_read[i], &y[i], sizeof(int));
                    if (n == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            printf("[Nodo %d] Nessun dato disponibile su FIFO con nodo %d\n",
                                   nodo.id_nodo, nodo.vicini[i]);
                        } else {
                            perror("Errore lettura FIFO");
                        }
                        y[i] = -1;
                    } else if (n == 0) {
                        printf("[Nodo %d] Writer ha chiuso la FIFO con nodo %d\n",
                               nodo.id_nodo, nodo.vicini[i]);
                        y[i] = -1;
                    } else {
                        printf("[Nodo %d] Ricevuto %d da nodo %d\n",
                               nodo.id_nodo, y[i], nodo.vicini[i]);
                    }
                } else {
                    y[i] = -1;
                }
            }
        }

        nodo.stato = stf(nodo.stato, y, numero_vicini, nodo.vicini);

        if (messaggio_inviato == 0 && nodo.stato.snd_flag == 1) {
            for (int i = 0; i < numero_vicini; i++) {
                if (nodo.vicini[i] != nodo.stato.parent) {
                    int messaggio_da_inviare = msg(nodo.stato, i);
                    if (fd_write[i] != -1) {
                        if (write(fd_write[i], &messaggio_da_inviare, sizeof(int)) == -1) {
                            perror("Errore scrittura messaggio");
                        } else {
                            printf("[Nodo %d] Inviato %d a nodo %d\n",
                                   nodo.id_nodo, messaggio_da_inviare, nodo.vicini[i]);
                        }
                    }
                }
            }
            messaggio_inviato = 1;
        }

        usleep(500000);
    }

    // chiudi fd alla fine (qui mai raggiunto però)
    for (int i = 0; i < numero_vicini; i++) {
        if (fd_read[i] != -1) close(fd_read[i]);
        if (fd_write[i] != -1) close(fd_write[i]);
    }

    return NULL;
}




int main(){
    int id_nodo[6];
    pthread_t th[6]; //alloco lo spazio in memoria per 6 thread
    for(int i=0;i<6;i++){
        id_nodo[i]=i+1;
        pthread_create(&th[i],NULL,funzioneThread,&id_nodo[i]);
        sleep(2);
    }
    for(int i=0;i<6;i++){
        pthread_join(th[i],NULL); 
    }
    return 0;
}
