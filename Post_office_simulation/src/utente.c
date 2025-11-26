#include "common.h"

/*
 * UTENTE.C (Il Cliente / Produttore di Lavoro)
 * * Implementazione Requisiti:
 * 1. Probabilità P_SERV personalizzata (passata via exec)
 * 2. Scelta casuale del servizio e dell'orario di arrivo
 * 3. Controllo disponibilità sportelli (Lettura protetta)
 * 4. Logica di "Abbandono": Se l'ufficio chiude e l'utente è ancora in coda,
 *   smette semplicemente di aspettare. Il conteggio dei "Non Erogati"
 *   è delegato al Direttore che legge la coda residua.
 */

int main(int argc, char *argv[]) {
    // Controllo argomenti (la probabilità P_SERV arriva dal main)
    if(argc < 2) return 1;
    int P_SERV = atoi(argv[1]); 

    // --- 1. ATTACH RISORSE IPC ---
    int shm_id = shmget(KEY_SHM, sizeof(SharedData), 0666);
    SharedData *shm = (SharedData *)shmat(shm_id, NULL, 0);
    int sem_id = semget(KEY_SEM, 0, 0666);
    int msg_id = msgget(KEY_MSG, 0666);

    // Seed random unico per processo (PID * Time) per evitare che
    // tutti gli utenti facciano le stesse scelte nello stesso istante
    srand(getpid() * time(NULL)); 
    
    // --- 2. SINCRONIZZAZIONE START (Pattern Turnstile) ---
    // Aspetto il via del Direttore e sblocco subito il prossimo utente
    P(sem_id, SEM_START); 
    V(sem_id, SEM_START); 

    while (!shm->stop_simulation) {
        
        // Stabilisce un orario -> Simulo ritardo arrivo random
        usleep((rand() % 30) * shm->cfg.nano_secs_per_min / 1000);

        // "Decide se recarsi... secondo probabilità"
        int r = rand() % 100;
        
        // Controllo anche che l'ufficio non abbia chiuso durante il mio "viaggio" (sleep)
        if (r < P_SERV && shm->ufficio_aperto) {
            
            // "Stabilisce il servizio"
            int servizio = rand() % NUM_SERVICES; 
            
            // --- CHECK DISPONIBILITÀ (Lettore) ---
            // Verifico se OGGI quel servizio è attivo
            // Uso il MUTEX in lettura per evitare Race Conditions se il Direttore 
            // sta ancora configurando gli sportelli
            int servizio_disponibile = 0;
            P(sem_id, SEM_MUTEX);
            for(int i=0; i<MAX_SPORTELLI; i++) {
                if(shm->sportelli_mapping[i] == servizio) { 
                    servizio_disponibile = 1; 
                    break; 
                }
            }
            V(sem_id, SEM_MUTEX);

            if(servizio_disponibile) {
                // --- FASE 1: PRENDERE IL TICKET ---
                // Richiesta sincrona via Message Queue
                MsgTicket m = {1, getpid(), servizio, 0};
                msgsnd(msg_id, &m, sizeof(MsgTicket)-sizeof(long), 0);
                
                // Attendo risposta sul mio canale privato (mtype = mio PID)
                msgrcv(msg_id, &m, sizeof(MsgTicket)-sizeof(long), getpid(), 0);

                // --- FASE 2: IN CODA (Ruolo: Produttore) ---
                
                // Aggiorno contatore visuale (Shared Memory)
                P(sem_id, SEM_MUTEX);
                shm->utenti_in_attesa[servizio]++;
                V(sem_id, SEM_MUTEX);

                // Segnalo all'Operatore che c'è lavoro
                // Faccio V() (Signal) perché sto "producendo" un cliente in coda
                // L'operatore farà P() (Wait) per servirmi
                V(sem_id, SEM_QUEUE_BASE + servizio);

                // N.B.:
                // A questo punto sono logicamente in coda. Non mi blocco su un semaforo
                // (perché non ho un canale di ritorno 1-a-1 per la fine servizio),
                // ma entro nel loop di attesa passiva qui sotto
            }
        }
        
        // --- ATTESA PASSIVA & ABBANDONO ---
        // Qui gestisco sia l'attesa del servizio sia l'eventuale abbandono
        
        // 1. Finché l'ufficio è aperto, aspetto (simulo di essere in fila o servito)
        //    Uso polling lento (0.1s) per non sprecare CPU
        while(shm->ufficio_aperto && !shm->stop_simulation) usleep(100000); 
        
        // 2. L'ufficio ha chiuso
        //    Se ero in coda e non sono stato servito, il contatore `utenti_in_attesa`
        //    non è stato decrementato dall'operatore
        //    Io "abbandono" semplicemente uscendo da questo loop e tornando a casa
        //    Il Direttore conterà i residui come "Servizi Non Erogati"
        
        // 3. Aspetto a casa che l'ufficio riapra il giorno dopo
        while(!shm->ufficio_aperto && !shm->stop_simulation) usleep(100000); 

    }
    
    shmdt(shm); // Stacco la memoria condivisa
    return 0;
}