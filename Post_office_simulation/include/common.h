#ifndef COMMON_H
#define COMMON_H

/* * COMMON.H
 * Questo file agisce da "contratto" tra i vari processi (Direttore, Utente, Operatore, Erogatore)
 * Contiene:
 * 1. Le chiavi IPC per l'accesso alle risorse
 * 2. Le strutture dati condivise (Shared Memory e Messaggi)
 * 3. Funzioni helper (static inline) per semplificare le operazioni sui semafori
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <string.h>

// --- CHIAVI IPC ---
// Scelta progettuale: Chiavi Hardcoded (statiche)
// Alternativa: ftok(). Ho scelto chiavi fisse per facilitare il debug e la pulizia manuale
// (via ipcrm) in caso di crash, evitando ambiguità sui path dei file
#define KEY_SHM 12345
#define KEY_SEM 12346
#define KEY_MSG 12347

// --- COSTANTI DEL SISTEMA ---
#define NUM_SERVICES 6      // Numero di tipologie di servizio
#define MAX_SPORTELLI 10    // Numero massimo fisico di sportelli

// --- LAYOUT SEMAFORI ---
// Ho scelto di usare un UNICO array di semafori per gestire tutte le sincronizzazioni
// Questo riduce il numero di chiamate a semget/semctl
#define SEM_MUTEX 0         // Indice 0: Mutex binario per la SHM (Protezione dati)
#define SEM_START 1         // Indice 1: Barriera (Rendezvous) per lo start sincronizzato
#define SEM_QUEUE_BASE 2    // Indice 2+: Semafori contatori per le code dei servizi (Produttore/Consumatore)

// Macro utile per calcolare quanti semafori chiedere al sistema nel main
#define TOTAL_SEMS (SEM_QUEUE_BASE + NUM_SERVICES)

// Dati statici per la logica di simulazione
// (Static const permette di includerli in ogni file senza errori di link)
static const int SERVICE_TIMES_MINUTES[] = {10, 8, 6, 8, 20, 20};
static const char *SERVICE_NAMES[] = {
    "Spedizioni", "Posta", "Bancoposta", "Bollettini", "Prodotti Fin.", "Orologi"
};

// Struttura Configuration:
// Mantiene i parametri caricati da file e viene copiata in SHM per essere visibile a tutti
typedef struct {
    int sim_duration;       
    int nof_workers;        
    int nof_users;          
    int nano_secs_per_min;  // Time-scaling: nanosecondi reali per 1 minuto simulato
    int explode_threshold;  
    int nof_pause;          
    int p_serv_min;         
    int p_serv_max;
} Config;

// Struttura Statistiche:
// Raccoglie i dati richiesti. È duplicata in SHM: una istanza per il giorno corrente, una per i totali
typedef struct {
    int utenti_serviti;
    int servizi_erogati[NUM_SERVICES];
    int servizi_non_erogati; 
    long tempo_attesa_totale;   
    long tempo_servizio_totale; 
    int pause_effettuate;
    int operatori_attivi;
} Stats;

// --- MEMORIA CONDIVISA (SHM) ---
// Struttura Monolitica: Raggruppa TUTTO lo stato del sistema
// Vantaggio: Con un solo shmid ho accesso a flag, code, sportelli e statistiche
typedef struct {
    int ufficio_aperto;                 // Flag di Stato: 1=Aperto, 0=Chiuso
    int stop_simulation;                // Flag di Terminazione Globale
    
    // Code "Virtuali": contatori per sapere quanta gente c'è (per le statistiche e l'explode)
    // La sincronizzazione reale avviene sui semafori, questi sono dati di appoggio
    int utenti_in_attesa[NUM_SERVICES]; 
    
    // Mapping Sportelli:
    // mapping: ID servizio offerto dallo sportello i-esimo (-1 se chiuso)
    // occupati: PID dell'operatore seduto (0 se libero)
    int sportelli_mapping[MAX_SPORTELLI]; 
    int sportelli_occupati[MAX_SPORTELLI]; 
    
    Stats stats_giornaliere;            // Reset a inizio giornata
    Stats stats_totali;                 // Accumulatore persistente
    
    Config cfg;                         // Configurazione in sola lettura per i figli
} SharedData;

// Struttura Messaggio (Message Queue System V)
// Utilizzata per il protocollo di richiesta ticket (Utente -> Erogatore -> Utente)
typedef struct {
    long mtype;             // Routing: 1=Richiesta, PID=Risposta
    pid_t pid_richiedente;  // Chi chiede
    int servizio_richiesto; // Cosa chiede
    int numero_ticket;      // Risposta
} MsgTicket;

// --- HELPER FUNCTIONS SEMAFORI ---
// Definite 'static inline' per efficienza (evitano overhead chiamata funzione)
// e per includerle nell'header senza creare conflitti di simboli multipli

// Wrapper per semop BLOCCANTE (Standard P/V)
// Ritorna int per permettere il controllo errori
static inline int sem_op(int semid, int index, int op) {
    struct sembuf s = {index, op, 0};
    return semop(semid, &s, 1);
}

// Wrapper per semop NON BLOCCANTE (IPC_NOWAIT)
// Cruciale per l'operatore: permette di controllare la coda senza bloccarsi se vuota
// Ritorna -1 con errno=EAGAIN se la risorsa non è disponibile
static inline int sem_nowait(int semid, int index, int op) {
    struct sembuf s = {index, op, IPC_NOWAIT};
    return semop(semid, &s, 1);
}

// Macro per leggibilità (P=Wait/Down, V=Signal/Up)
#define P(id, idx) sem_op(id, idx, -1)
#define V(id, idx) sem_op(id, idx, 1)

#endif