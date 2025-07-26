#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>


/*Per ogni nodo creiamo un thread che invoca la funzione read, la read permette al nodo di vedere se su una pipe ci sono dati che devono essere letti*/
/*Ogni nodo avrà una lista dei nodi vicini*/
/*Il nodo deve distribuire il messaggio a tutti i nodi vicini tranne a quello che glielo ha inviato*/
/*Gli archi tra i nodi saranno gestiti da due pipe*/

typedef struct {
    int snd_flag;     // 🔁 Flag che indica se il nodo deve inviare un messaggio
                      // Se vale 1, il nodo è pronto a inviare `data`
                      // Se vale 0, non deve inviare nulla

    int messaggio;         // 📦 Dato da inviare, valido solo se snd_flag è attivo

    int parent;       // 🌲 Nodo padre (cioè da chi ha ricevuto il messaggio)
                      // Utile per non rispedire il messaggio indietro
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



//il nodo accetta messaggi solo la prima volta
//se ha già un nodo parent ha già ricevuto un messaggio
NodeState stf(NodeState w, int *y, int num_vicini) {
    // Se abbiamo già un parent, ignoriamo i nuovi messaggi
    if (w.parent != -1) {
        return w;
    }

    // Scorriamo tutti i vicini per vedere chi ci ha scritto
    for (int i = 0; i < num_vicini; i++) {
        if (y[i] != -1) {
            // Primo vicino che ci manda un messaggio diventa il parent
            w.parent = i;              // i è l'indice nel vettore dei vicini
            w.messaggio = y[i];       //salviamo il messaggio
            w.snd_flag = 1;           //impostiamo il flag ad 1
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
        nodo.stato.messaggio=555; //messaggio da inviare
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
        break;
    case 6:
        nodo.id_nodo=id; 
        nodo.num_vicini=2;
        nodo.vicini=malloc(nodo.num_vicini*sizeof(int));
        nodo.vicini[0]=4;
        nodo.vicini[1]=5;
        nodo.stato.snd_flag=0;
        break;
    default:
        printf("ERRORE NELL'IDENTIFICATIVO DEL NODO");
        exit(1);
        break;
    }
    CreaFifo(nodo);
    return nodo;
}

void* funzioneThread(void*args){
    int id=*((int*)args);
    Nodo nodo=CreaNodo(id);
    //abbiamo creato i nodi e tutte le fifo per ogni nodo
    int numero_vicini=nodo.num_vicini;
    int y[numero_vicini]; //creiamo un array che possa contenere i messaggi inviati dai nodi vicini

    //dobbiamo controllare se i nodi vicini hanno inviato dati sulla fifo
    for(int i=0;i<numero_vicini;i++){
        //dobbiamo per ogni vicino aprire la fifo in lettura
        char percorso[50];
        sprintf(percorso,"fifo_%d_%d",nodo.id_nodo,nodo.vicini[i]);
        int fd = open(percorso, O_RDONLY | O_NONBLOCK); //apriamo la fifo in sola lettura  e in modalità non bloccante
        if (fd == -1) {
            perror("Errore apertura FIFO in lettura");
        }
        ssize_t n=read(fd,&y[i],sizeof(int));
        close(fd); 
        // la funzione read NON scrive nulla nel buffer &y[i]  se non c’è niente da leggere.
        //leggiamo il dato inviato dal vicino e lo salviamo sulla read
        //la read se non c'è alcun dato da leggere restituisce errno settato EAGAIN o EWOULDBLOCK
        //errno è una variabile che il sistema imposta quando una funzione di sistema fallisce
        if(n==-1){
            if((errno == EAGAIN) || (errno == EWOULDBLOCK)){
                //non abbiamo ricevuto nessun dato
                y[i] = -1; 
        }else{
            //altri tipi di errore
             perror("read");
             return 1;
            }
        }
    }
    //ora invochiamo stf per aggiornare lo stato (dobbiamo capire come passare il parent)
    nodo.stato=stf(nodo.stato,&y,numero_vicini);
    //stato aggiornato
    //fase di invio
    for(int i=0;i<numero_vicini;i++){
        if(nodo.stato.parent!=i){
            int messaggio_da_inviare=msg(nodo.stato,i);
            char percorso[50];
            sprintf(percorso,"fifo_%d_%d",nodo.id_nodo,nodo.vicini[i]);
            int fd=open(percorso,O_WRONLY); //apriamo la fifo in scrittura
            write(fd,&nodo.stato.messaggio,sizeof(int)); //inviamo il messaggio al nodo i sulla fifo
            close(fd);
        }

    }
    return;
}

int main(){
    int id_nodo[6];
    pthread_t th[6]; //alloco lo spazio in memoria per 6 thread
    for(int i=0;i<6;i++){
        id_nodo[i]=i+1;
        pthread_create(&th[i],NULL,funzioneThread,&id_nodo[i]);
    }
    for(int i=0;i<6;i++){
        pthread_join(th[i],NULL); 
    }
    return 0;
}