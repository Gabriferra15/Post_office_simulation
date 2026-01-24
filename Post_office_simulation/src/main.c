#include "common.h"

int shm_id, sem_id, msg_id;


/* * FUNZIONE CLEANUP
 * Deve garantire che non rimangano risorse IPC appese
 * e che non ci siano processi zombie
 */ 
void cleanup() {
    // 1. Rimuovo le risorse IPC. Uso IPC_RMID per marcarle 
    // Se non lo faccio, rimangono in /dev/shm o ipcs finché non riavvio il pc 
    shmctl(shm_id, IPC_RMID, NULL); 
    semctl(sem_id, 0, IPC_RMID);    
    msgctl(msg_id, IPC_RMID, NULL); 
    // 2. Strategia di chiusura processi:   
    // - Ignoro SIGTERM per me stesso (altrimenti mi uccido da solo con kill(0)) 
    signal(SIGTERM, SIG_IGN);
    // - Invio SIGTERM a tutto il PROCESS GROUP (0)
    // Questo uccide in un colpo solo Utenti, Operatori ed Erogatore senza dover tracciare i PID 
    kill(0, SIGTERM); 
    // 3. Reap dei figli (Wait Loop)
    // Fondamentale per evitare processi zombie nella tabella dei processi del sistema 
    while(wait(NULL) > 0);

    printf("\n[Direttore] Pulizia completata. Bye!\n");
    exit(0);
}

void handle_sig(int sig) { (void)sig; cleanup(); }

/*
 * Carica la configurazione dal file .conf
 * Imposta valori di default se mancano chiavi
 */
void load_config(const char *filename, Config *cfg) {
    FILE *f = fopen(filename, "r");
    if (!f) { perror("Errore apertura config"); exit(1); }

    // Buffer di lettura
    char line[128], key[64];
    int val;

    // Valori di default
    cfg->sim_duration = 5; cfg->explode_threshold = 50; 
    cfg->nof_users = 20; cfg->nof_workers = 5; cfg->nano_secs_per_min = 100000;
    cfg->nof_pause = 3; cfg->p_serv_min = 10; cfg->p_serv_max = 90;

    // Parsing semplice chiave=valore
    while(fgets(line, sizeof(line), f)) {
        // Ignoro commenti e linee vuote
        if(sscanf(line, "%[^=]=%d", key, &val) == 2) {
            // Assegno il valore alla chiave corrispondente
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

// Stampa le statistiche giornaliere o totali
// simulation_end = 1 per totali, 0 per giornaliere
void print_stats(SharedData *shm, int day, int simulation_end) {
    // Se giornaliere, stampo il giorno
    Stats *s = simulation_end ? &shm->stats_totali : &shm->stats_giornaliere;
    int div = simulation_end ? shm->cfg.sim_duration : 1; 

    printf("\n=== STATISTICHE %s ===\n", simulation_end ? "TOTALI" : "GIORNALIERE");
    printf("Utenti serviti: %d (Media: %.2f)\n", s->utenti_serviti, (float)s->utenti_serviti/div);
    printf("Servizi NON erogati: %d (Persi)\n", s->servizi_non_erogati);

    // Tempo medio di attesa stimato
    double avg_wait = s->utenti_serviti ? (double)s->tempo_attesa_totale / s->utenti_serviti : 0;
    printf("Tempo medio attesa (stimato): %.0f ns\n", avg_wait);
    printf("Pause effettuate: %d\n", s->pause_effettuate);
    printf("-- Dettaglio Servizi --\n");

    // Dettaglio servizi erogati
    for(int i=0; i<NUM_SERVICES; i++) {
        printf("  %s: %d\n", SERVICE_NAMES[i], s->servizi_erogati[i]);
    }

    // Stato sportelli (solo se giornaliere)
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
    // Setup Gestione Segnali
    signal(SIGINT, handle_sig);

    // Inizializzazioni IPC
    srand(time(NULL));
    // Caricamento Configurazione
    const char *conf_file = (argc > 1) ? argv[1] : "conf/config_timeout.conf";
    Config cfg_local;
    load_config(conf_file, &cfg_local);

    // Creazione Risorse IPC
    shm_id = shmget(KEY_SHM, sizeof(SharedData), IPC_CREAT | 0666);
    sem_id = semget(KEY_SEM, 2 + NUM_SERVICES, IPC_CREAT | 0666);
    msg_id = msgget(KEY_MSG, IPC_CREAT | 0666);

    SharedData *shm = (SharedData *)shmat(shm_id, NULL, 0);
    memset(shm, 0, sizeof(SharedData)); 

    // Inizializzazione SharedData
    shm->cfg = cfg_local;

    // Inizializzazione Semafori 
    semctl(sem_id, SEM_MUTEX, SETVAL, 1);
    semctl(sem_id, SEM_START, SETVAL, 0);

    // Code "Virtuali" inizializzati a 0
    for(int i=0; i<NUM_SERVICES; i++) semctl(sem_id, SEM_QUEUE_BASE+i, SETVAL, 0);

    // Creazione Processi Figli
    if (fork() == 0) { 
        char *args[] = { "./bin/erogatore", NULL };
        execve("./bin/erogatore", args, NULL); exit(1); 
    }
    for(int i=0; i<cfg_local.nof_workers; i++) {
        if (fork() == 0) { 
            char *args[] = { "./bin/operatore", NULL };
            execve("./bin/operatore", args, NULL); exit(1); 
        }
    }
    for(int i=0; i<cfg_local.nof_users; i++) {
        if (fork() == 0) { 
            // Calcolo probabilità di richiedere un servizio
            int p = cfg_local.p_serv_min + (rand() % (cfg_local.p_serv_max - cfg_local.p_serv_min + 1));
            char p_str[10]; sprintf(p_str, "%d", p);
            char *args[] = { "./bin/utente", p_str, NULL };
            execve("./bin/utente", args, NULL); exit(1); 
        }
    }

    // Avvio Simulazione
    sleep(1); 
    // Sblocco il primo utente
    struct sembuf start_op = {SEM_START, 1, 0};
    semop(sem_id, &start_op, 1); 

    // Ciclo sui giorni di simulazione
    for(int day = 1; day <= cfg_local.sim_duration; day++) {
        printf("\n--- Giorno %d Inizio ---\n", day);
        // Apertura Ufficio: resetto dati giornalieri e mapping sportelli
        P(sem_id, SEM_MUTEX); 
        memset(&shm->stats_giornaliere, 0, sizeof(Stats)); 
        // Mapping randomico sportelli (80% di probabilità di essere aperto)
        for(int i = 0; i < MAX_SPORTELLI; i++) {
            shm->sportelli_mapping[i] = (rand() % 100 < 80) ? (rand() % NUM_SERVICES) : -1;
            shm->sportelli_occupati[i] = 0; 
        }
        // Apertura ufficio
        shm->ufficio_aperto = 1; 
        // Sblocco tutti gli utenti in attesa
        V(sem_id, SEM_MUTEX);

        // Simulo la giornata lavorativa
        sleep(2); 

        // Chiusura Ufficio
        shm->ufficio_aperto = 0; 
        printf("--- Giorno %d Fine (Ufficio Chiuso) ---\n", day);

        // Attendo un po' per permettere agli utenti di reagire alla chiusura
        usleep(500000); 

        // Raccolta Statistiche Giornaliere
        P(sem_id, SEM_MUTEX);
        // Conta quanti utenti sono rimasti in coda
        int rimasti = 0;
        // Pulizia code virtuali
        for(int i = 0; i < NUM_SERVICES; i++) {
            rimasti += shm->utenti_in_attesa[i];
            shm->utenti_in_attesa[i] = 0; 
        }
        //Accumulo i rimasti in coda ai non erogati del giorno
        shm->stats_giornaliere.servizi_non_erogati += rimasti;
        
        shm->stats_totali.utenti_serviti += shm->stats_giornaliere.utenti_serviti;
        shm->stats_totali.servizi_non_erogati += shm->stats_giornaliere.servizi_non_erogati;
        shm->stats_totali.tempo_attesa_totale += shm->stats_giornaliere.tempo_attesa_totale;
        shm->stats_totali.tempo_servizio_totale += shm->stats_giornaliere.tempo_servizio_totale;
        shm->stats_totali.pause_effettuate += shm->stats_giornaliere.pause_effettuate;
        shm->stats_totali.operatori_attivi += shm->stats_giornaliere.operatori_attivi;

        // Accumulo servizi erogati
        for(int i = 0; i < NUM_SERVICES; i++) 
            shm->stats_totali.servizi_erogati[i] += shm->stats_giornaliere.servizi_erogati[i];
        V(sem_id, SEM_MUTEX);

        print_stats(shm, day, 0); 
        if(rimasti > cfg_local.explode_threshold) break;
    }

    printf("\n--- FINE SIMULAZIONE ---\n");
    // Stampa statistiche finali
    print_stats(shm, 0, 1);
    // Segnalo a tutti i processi di terminare
    shm->stop_simulation = 1; 
    sleep(1); 
    
    // Stacco la memoria condivisa
    shmdt(shm); 

    cleanup(); 

    return 0;
}