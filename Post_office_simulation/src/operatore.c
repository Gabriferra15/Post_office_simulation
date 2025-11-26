#include "common.h"

/*
 * OPERATORE.C (Il "Consumatore")
 * * Questo processo simula il lavoratore allo sportello
 * * Punti Critici gestiti:
 * 1. Race Conditions sulla scelta del posto (risolto con Mutex)
 * 2. Prevenzione Deadlock in chiusura (risolto con IPC_NOWAIT).
 * 3. Attesa attiva su sportello occupato (Polling lento con usleep)
 */

int main(void) {
    // 1. Attach alle risorse IPC create dal Direttore
    int shm_id = shmget(KEY_SHM, sizeof(SharedData), 0666);
    SharedData *shm = (SharedData *)shmat(shm_id, NULL, 0);
    int sem_id = semget(KEY_SEM, 0, 0666);

    srand(getpid());
    
    int my_skill = rand() % NUM_SERVICES; // La specializzazione dell'operatore
    int pause_rimanenti = shm->cfg.nof_pause;
    
    // Sincronizzazione Start (Pattern Turnstile)
    P(sem_id, SEM_START); 
    V(sem_id, SEM_START);

    while (!shm->stop_simulation) {
        
        // --- FASE 1: RICERCA DELLO SPORTELLO ---
        int my_seat = -1;

        // Loop di attesa: finché l'ufficio è aperto e non ho la sedia, continuo a cercare
        // Questo soddisfa il requisito: resta in attesa che uno sportello si liberi
        while (shm->ufficio_aperto && !shm->stop_simulation) {
            
            P(sem_id, SEM_MUTEX); // Lock per leggere array condiviso
            for(int i=0; i<MAX_SPORTELLI; i++) {
                if(shm->sportelli_mapping[i] == my_skill && shm->sportelli_occupati[i] == 0) {
                    shm->sportelli_occupati[i] = getpid(); // Preso
                    my_seat = i;
                    shm->stats_giornaliere.operatori_attivi++;
                    break;
                }
            }
            V(sem_id, SEM_MUTEX);

            if (my_seat != -1) {
                break; // Ho trovato il posto, esco dal loop di ricerca
            } else {
                // Non ho trovato posto. Aspetto un po' (0.05s) e riprovo
                // Spero che un collega vada in pausa liberando lo sportello
                usleep(50000); 
            }
        }

        // Se ho trovato la sedia (e l'ufficio non ha chiuso nel frattempo) inizio a lavorare
        if (my_seat != -1) {
            
            // --- FASE 2: LOOP DI LAVORO (Consumatore) ---
            // Lavoro se l'ufficio è aperto OPPURE se c'è ancora coda da smaltire
            while (shm->ufficio_aperto || shm->utenti_in_attesa[my_skill] > 0) {
                
                // GESTIONE PAUSA (Opzionale)
                if (pause_rimanenti > 0 && (rand() % 100) < 5) { 
                    // Per andare in pausa DEVO liberare la risorsa (sedia).
                    P(sem_id, SEM_MUTEX);
                    shm->sportelli_occupati[my_seat] = 0; 
                    shm->stats_giornaliere.pause_effettuate++;
                    V(sem_id, SEM_MUTEX);
                    
                    usleep(shm->cfg.nano_secs_per_min * 10 / 1000); // Pausa caffè
                    pause_rimanenti--;
                    
                    // Al ritorno, devo ricompetere per la sedia
                    P(sem_id, SEM_MUTEX);
                    if(shm->sportelli_occupati[my_seat] == 0) {
                         shm->sportelli_occupati[my_seat] = getpid(); // Ripresa
                         V(sem_id, SEM_MUTEX);
                    } else {
                         V(sem_id, SEM_MUTEX);
                         // Posto perso (rubato da un collega)! Esco e aspetto domani
                         continue; 
                    }
                }

                // --- PRELIEVO CLIENTE (PUNTO CRITICO TECNICO) ---
                // Uso IPC_NOWAIT per evitare Deadlock se la coda è vuota e l'ufficio chiude
                struct sembuf s = {SEM_QUEUE_BASE + my_skill, -1, IPC_NOWAIT};
                
                if (semop(sem_id, &s, 1) != -1) {
                    // SUCCESSO: Preso cliente
                    struct timespec t_start, t_end;
                    clock_gettime(CLOCK_MONOTONIC, &t_start);

                    // Simulo servizio
                    int base = SERVICE_TIMES_MINUTES[my_skill];
                    int duration_min = base + (rand() % base) - (base/2);
                    if(duration_min < 1) duration_min = 1;
                    long duration_ns = (long)duration_min * shm->cfg.nano_secs_per_min;
                    usleep(duration_ns / 1000); 

                    clock_gettime(CLOCK_MONOTONIC, &t_end);
                    long elapsed = (t_end.tv_sec - t_start.tv_sec)*1e9 + (t_end.tv_nsec - t_start.tv_nsec);

                    // Aggiorno statistiche
                    P(sem_id, SEM_MUTEX);
                    shm->stats_giornaliere.utenti_serviti++;
                    shm->stats_giornaliere.servizi_erogati[my_skill]++;
                    shm->stats_giornaliere.tempo_servizio_totale += elapsed;
                    
                    long stima_attesa = elapsed * (10 + rand()%40) / 100; 
                    shm->stats_giornaliere.tempo_attesa_totale += stima_attesa;

                    shm->utenti_in_attesa[my_skill]--; 
                    V(sem_id, SEM_MUTEX);

                } else {
                     // FALLIMENTO: Coda vuota (EAGAIN)
                     if(errno == EAGAIN) {
                         usleep(1000); // Breve sleep no-busy-waiting
                         if(!shm->ufficio_aperto) break; // Se chiuso, fine turno
                     }
                }
            } // Fine While Lavoro

            // A fine turno, libero ufficialmente la sedia
            P(sem_id, SEM_MUTEX);
            if(shm->sportelli_occupati[my_seat] == getpid()) shm->sportelli_occupati[my_seat] = 0;
            V(sem_id, SEM_MUTEX);
        }
        
        // Attendo l'apertura del giorno successivo
        while(!shm->ufficio_aperto && !shm->stop_simulation) sleep(1);
    }
    
    shmdt(shm); 
    return 0;
}