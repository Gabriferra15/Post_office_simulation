#include "common.h"

int shm_id, sem_id, msg_id;

/* * FUNZIONE CLEANUP
 * Deve garantire che non rimangano risorse IPC appese
 * e che non ci siano processi zombie
 */
void cleanup() {
    // 1. Rimuovo le risorse IPC. Uso IPC_RMID per marcarle per la distruzione
    // Se non lo faccio, rimangono in /dev/shm o ipcs finché non riavvio la macchina
    shmctl(shm_id, IPC_RMID, NULL); 
    semctl(sem_id, 0, IPC_RMID);    
    msgctl(msg_id, IPC_RMID, NULL); 
    
    // 2. Strategia di chiusura processi:
    // - Ignoro SIGTERM per me stesso (altrimenti mi uccido da solo con kill(0))
    signal(SIGTERM, SIG_IGN);
    
    // - Invio SIGTERM a tutto il PROCESS GROUP (0)
    //   Questo uccide in un colpo solo Utenti, Operatori ed Erogatore senza dover tracciare i PID
    kill(0, SIGTERM); 
    
    // 3. Reap dei figli (Wait Loop)
    //   Fondamentale per evitare processi zombie nella tabella dei processi del sistema
    while(wait(NULL) > 0);

    printf("\n[Direttore] Pulizia completata. Bye!\n");
    exit(0);
}

// Gestore segnali: intercetto CTRL+C per non uscire brutalmente ma fare pulizia
void handle_sig(int sig) { (void)sig; cleanup(); }

// Parsing Configurazione: leggo il file .conf per settare i parametri dinamici
void load_config(const char *filename, Config *cfg) {
    FILE *f = fopen(filename, "r");
    if (!f) { perror("Errore apertura config"); exit(1); }
    
    char line[128], key[64];
    int val;
    
    // Valori di default (Fallback nel caso il file sia incompleto)
    cfg->sim_duration = 5; cfg->explode_threshold = 50; 
    cfg->nof_users = 20; cfg->nof_workers = 5; cfg->nano_secs_per_min = 100000;
    cfg->nof_pause = 3; cfg->p_serv_min = 10; cfg->p_serv_max = 90;

    while(fgets(line, sizeof(line), f)) {
        if(sscanf(line, "%[^=]=%d", key, &val) == 2) {
            // Mappo le stringhe del file nelle variabili della struct
            if(!strcmp(key, "SIM_DURATION")) cfg->sim_duration = val;
            else if(!strcmp(key, "EXPLODE_THRESHOLD")) cfg->explode_threshold = val;
            else if(!strcmp(key, "NOF_USERS")) cfg->nof_users = val;
            else if(!strcmp(key, "NOF_WORKERS")) cfg->nof_workers = val;
            else if(!strcmp(key, "NANO_SECS")) cfg->nano_secs_per_min = val;
            else if(!strcmp(key, "NOF_PAUSE")) cfg->nof_pause = val;
            else if(!strcmp(key, "P_SERV_MIN")) cfg->p_serv_min = val;
            else if(!strcmp(key, "P_SERV_MAX")) cfg->p_serv_max = val;
        }
    }
    fclose(f);
}

// Stampa reportistica: Legge dalla SHM
// Nota: Accede in lettura, ma va chiamata quando il sistema è stabile o protetto
void print_stats(SharedData *shm, int day, int simulation_end) {
    Stats *s = simulation_end ? &shm->stats_totali : &shm->stats_giornaliere;
    int div = simulation_end ? shm->cfg.sim_duration : 1; 
    
    printf("\n=== STATISTICHE %s ===\n", simulation_end ? "TOTALI" : "GIORNALIERE");
    printf("Utenti serviti: %d (Media: %.2f)\n", s->utenti_serviti, (float)s->utenti_serviti/div);
    printf("Servizi NON erogati: %d (Persi)\n", s->servizi_non_erogati);
    
    double avg_wait = s->utenti_serviti ? (double)s->tempo_attesa_totale / s->utenti_serviti : 0;
    
    printf("Tempo medio attesa (stimato): %.0f ns\n", avg_wait);
    printf("Pause effettuate: %d\n", s->pause_effettuate);

    printf("-- Dettaglio Servizi --\n");
    for(int i=0; i<NUM_SERVICES; i++) {
        printf("  %s: %d\n", SERVICE_NAMES[i], s->servizi_erogati[i]);
    }
    
    // Mapping visuale degli sportelli (Solo report giornaliero)
    if(!simulation_end) {
        printf("-- Stato Sportelli --\n");
        for(int i=0; i<MAX_SPORTELLI; i++) {
            if(shm->sportelli_mapping[i] != -1) {
                printf("  [%d] %s -> %s\n", i, SERVICE_NAMES[shm->sportelli_mapping[i]], 
                       shm->sportelli_occupati[i] ? "OCCUPATO" : "LIBERO");
            }
        }
    }
    printf("=========================\n");
}

int main(int argc, char *argv[]) {
    // Setup Signal Handler per uscita pulita su CTRL+C
    signal(SIGINT, handle_sig);

    // Inizializzo il generatore random per la configurazione degli sportelli
    srand(time(NULL));

    const char *conf_file = (argc > 1) ? argv[1] : "conf/config_timeout.conf";
    Config cfg_local;
    load_config(conf_file, &cfg_local);
    
    printf("[Direttore] Avvio simulazione: %d giorni, %d utenti, soglia %d\n", 
            cfg_local.sim_duration, cfg_local.nof_users, cfg_local.explode_threshold);

    // --- 1. FASE DI SETUP IPC ---
    // Creo le risorse con permessi 0666 (RW per tutti)
    shm_id = shmget(KEY_SHM, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id < 0) { perror("shmget"); exit(1); }

    // Creo array di semafori: Mutex + Start + 1 per ogni servizio (coda)
    sem_id = semget(KEY_SEM, 2 + NUM_SERVICES, IPC_CREAT | 0666);
    if (sem_id < 0) { perror("semget"); exit(1); }

    msg_id = msgget(KEY_MSG, IPC_CREAT | 0666);
    if (msg_id < 0) { perror("msgget"); exit(1); }
    
    // Attach e azzeramento memoria (fondamentale per pulire esecuzioni precedenti sporche)
    SharedData *shm = (SharedData *)shmat(shm_id, NULL, 0);
    if (shm == (void*)-1) { perror("shmat"); cleanup(); }
    memset(shm, 0, sizeof(SharedData)); 
    shm->cfg = cfg_local; // Pubblico la config in SHM per i figli

    // Inizializzazione Semafori (SETVAL)
    semctl(sem_id, SEM_MUTEX, SETVAL, 1); // MUTEX LIBERO (1) -> Binary Semaphore
    semctl(sem_id, SEM_START, SETVAL, 0); // BARRIERA CHIUSA (0) -> Nessuno parte finché non lo dico io
    for(int i=0; i<NUM_SERVICES; i++) semctl(sem_id, SEM_QUEUE_BASE+i, SETVAL, 0); // Code inizialmente vuote

    

    // --- 2. FASE DI FORKING ---
    // Uso il pattern fork() + exec() per rispettare la modularità richiesta.
    
    // Processo Erogatore Ticket
    if (fork() == 0) { 
        char *args[] = { "./bin/erogatore", NULL };
        execve("./bin/erogatore", args, NULL); 
        perror("Exec erogatore fallita"); exit(1); 
    }

    // Processi Operatori
    for(int i=0; i<cfg_local.nof_workers; i++) {
        if (fork() == 0) { 
            char *args[] = { "./bin/operatore", NULL };
            execve("./bin/operatore", args, NULL); 
            exit(1); 
        }
    }

    // Processi Utenti (passo la probabilità P come argomento stringa)
    for(int i=0; i<cfg_local.nof_users; i++) {
        if (fork() == 0) { 
            int p = cfg_local.p_serv_min + (rand() % (cfg_local.p_serv_max - cfg_local.p_serv_min + 1));
            char p_str[10]; sprintf(p_str, "%d", p);
            char *args[] = { "./bin/utente", p_str, NULL };
            execve("./bin/utente", args, NULL); 
            exit(1); 
        }
    }

    // Aspetto che il sistema operativo abbia creato le strutture dati dei processi
    sleep(1); 
    printf("[Direttore] Processi creati. Apro la barriera (Start)!\n");
    
    // Apro il tornello: Sblocco il primo processo che farà scattare la cascata
    struct sembuf start_op = {SEM_START, 1, 0};
    semop(sem_id, &start_op, 1); 

    // --- 3. LOOP DI SIMULAZIONE ---
    for(int day=1; day<=cfg_local.sim_duration; day++) {
        
        printf("\n--- Giorno %d Inizio ---\n", day);

        // SEZIONE CRITICA: Modifico lo stato dell'ufficio
        P(sem_id, SEM_MUTEX); 
        memset(&shm->stats_giornaliere, 0, sizeof(Stats)); // Reset stats di oggi
        
        // Assegno i servizi agli sportelli randomicamente
        for(int i=0; i<MAX_SPORTELLI; i++) {
            // 70% probabilità che uno sportello sia aperto
            shm->sportelli_mapping[i] = (rand() % 100 < 70) ? (rand() % NUM_SERVICES) : -1;
            shm->sportelli_occupati[i] = 0; // Resetto occupazione fisica
        }
        shm->ufficio_aperto = 1; // Flag "Aperto"
        V(sem_id, SEM_MUTEX);

        // La giornata lavorativa dura 2 secondi reali
        sleep(2); 

        // CHIUSURA UFFICIO
        P(sem_id, SEM_MUTEX);
        shm->ufficio_aperto = 0; // Segnalo chiusura (Utenti e Operatori se ne accorgono in polling)
        V(sem_id, SEM_MUTEX);

        printf("--- Giorno %d Fine (Ufficio Chiuso) ---\n", day);
        
        // Attesa per permettere agli operatori di finire l'ultimo servizio in corso
        usleep(500000); 

        // AGGIORNAMENTO STATISTICHE (Sezione Critica)
        P(sem_id, SEM_MUTEX);
        
        // Controllo code residue
        int rimasti_in_coda = 0;
        for(int i=0; i<NUM_SERVICES; i++) {
            rimasti_in_coda += shm->utenti_in_attesa[i];
            shm->stats_giornaliere.servizi_non_erogati += shm->utenti_in_attesa[i];
        }
        
        // Accumulo le statistiche giornaliere nel totale
        shm->stats_totali.utenti_serviti += shm->stats_giornaliere.utenti_serviti;
        shm->stats_totali.servizi_non_erogati += shm->stats_giornaliere.servizi_non_erogati;
        // ... (copia altri campi) ...
        shm->stats_totali.operatori_attivi += shm->stats_giornaliere.operatori_attivi;
        for(int i=0; i<NUM_SERVICES; i++) 
            shm->stats_totali.servizi_erogati[i] += shm->stats_giornaliere.servizi_erogati[i];
        
        V(sem_id, SEM_MUTEX); // Fine Sezione Critica

        print_stats(shm, day, 0); 

        // CHECK TERMINAZIONE ANTICIPATA (EXPLODE)
        if(rimasti_in_coda > cfg_local.explode_threshold) {
            printf("\n[CRITICAL] Troppi utenti in coda (%d > %d). Terminazione Explode!\n", 
                   rimasti_in_coda, cfg_local.explode_threshold);
            break;
        }
    }

    printf("\n--- FINE SIMULAZIONE ---\n");
    print_stats(shm, 0, 1); // Report finale
    
    shm->stop_simulation = 1; // Dico ai figli di uscire dai loro while
    sleep(1); // Do tempo ai figli di leggere il flag

    // Stacco la mia referenza alla SHM prima di distruggerla
    shmdt(shm); 
    
    cleanup(); // Chiamo la pulizia finale
    return 0;
}