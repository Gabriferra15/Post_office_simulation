#include "common.h"

/*
 * UTENTE.C (Il Cliente / Produttore di Lavoro)
 * * Implementazione Requisiti:
 * 1. Probabilità P_SERV personalizzata (passata via exec)
 * 2. Scelta casuale del servizio e dell'orario di arrivo
 * 3. Controllo disponibilità sportelli (Lettura protetta)
 * 4. Logica di "Abbandono": Se l'ufficio chiude e l'utente è ancora in coda,
 *   smette semplicemente di aspettare. Il conteggio "Non Erogati"
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
    
    // --- 2. SINCRONIZZAZIONE START ---
    // Aspetto il via del Direttore e sblocco subito il prossimo utente
    P(sem_id, SEM_START); 
    V(sem_id, SEM_START); 

    while (!shm->stop_simulation) {
        //Polling sull'apertura dell'ufficio
        while(!shm->ufficio_aperto && !shm->stop_simulation) sleep(1);
        if(shm->stop_simulation) break;

        usleep((rand() % 100) * 1000); //Ritardo casuale

        // --- RICHIESTA SERVIZIO ---
        // Decido se richiedere un servizio in base a P_SERV
        if ((rand() % 100) < P_SERV && shm->ufficio_aperto) {
            // Scelgo un servizio a caso
            int servizio = rand() % NUM_SERVICES; 
            int servizio_disponibile = 0;

            // Controllo se il servizio è offerto da qualche sportello
            P(sem_id, SEM_MUTEX);
            for(int i=0; i<MAX_SPORTELLI; i++) {
                // Se uno sportello offre il servizio, lo segno come disponibile
                if(shm->sportelli_mapping[i] == servizio) { 
                    servizio_disponibile = 1; 
                    break; 
                }
            }
            V(sem_id, SEM_MUTEX);

            if(servizio_disponibile) {
                // Invio la richiesta di ticket
                MsgTicket m = {1, getpid(), servizio, 0};
                msgsnd(msg_id, &m, sizeof(MsgTicket)-sizeof(long), 0);
                // Attendo la risposta (numero di ticket)
                msgrcv(msg_id, &m, sizeof(MsgTicket)-sizeof(long), getpid(), 0);

                P(sem_id, SEM_MUTEX);
                // Incremento il contatore virtuale della coda del servizio richiesto
                shm->utenti_in_attesa[servizio]++;
                V(sem_id, SEM_MUTEX);

                // Attendo di essere servito (decremento del contatore virtuale
                // fatto dall'operatore al momento del servizio)
                V(sem_id, SEM_QUEUE_BASE + servizio);
            } else {
                // MODIFICA: Se il servizio non esiste, conta come non erogato subito
                P(sem_id, SEM_MUTEX);
                shm->stats_giornaliere.servizi_non_erogati++;
                V(sem_id, SEM_MUTEX);
            }
        }
        
        // --- ATTESA PASSIVA & ABBANDONO ---
        // Qui gestisco sia l'attesa del servizio sia l'eventuale abbandono
        
        // 1. Finché l'ufficio è aperto, aspetto (simulo di essere in fila o servito)
        //    Uso polling lento (0.1s) per non sprecare CPU
        while(shm->ufficio_aperto && !shm->stop_simulation) usleep(1000); 
        
        // 2. L'ufficio ha chiuso
        //    Se ero in coda e non sono stato servito, il contatore `utenti_in_attesa`
        //    non è stato decrementato dall'operatore
        //    "Abbandono" semplicemente uscendo da questo loop e tornando a casa
        //    Il Direttore conterà i residui come servizi non erogati
        
        // 3. Aspetto a casa che l'ufficio riapra il giorno dopo
        while(!shm->ufficio_aperto && !shm->stop_simulation) usleep(1000); 

    }
    
    shmdt(shm); // Stacco la memoria condivisa
    return 0;
}