#include "common.h"

/*
 * EROGATORE.C (Il Server di Ticket)
 * * Ruolo: Server Iterativo
 * * Architettura:
 * - Riceve richieste su un canale noto a tutti (TYPE = 1)
 * - Invia risposte su canali privati (TYPE = PID Utente)
 * * Scelta di design IPC:
 * Ho scelto le Message Queue perché permettono lo scambio di dati strutturati (struct)
 * e garantiscono nativamente l'ordinamento FIFO delle richieste, senza dover implementare
 * un mutex esplicito per gestire l'accesso concorrente al contatore dei ticket.
 */

int main(void) {
    // 1. Collegamento alla Coda
    // Non uso IPC_CREAT perché la coda deve essere stata creata dal Direttore
    // Se non esiste, è un errore fatale
    int msg_id = msgget(KEY_MSG, 0666);
    if (msg_id == -1) exit(1);

    MsgTicket msg;
    int global_ticket_counter = 1;

    // 2. Loop Infinito (Server Loop)
    // Non c'è una condizione di uscita esplicita basata su variabili (es. stop_simulation)
    // perché questo processo è "guidato dagli eventi" (messaggi)
    while (1) {
        
        // --- RICEZIONE ---
        // Attendo un messaggio di TIPO 1 (richiesta ticket)
        // msgrcv è BLOCCANTE: se la coda è vuota, il processo va in sleep
        // Questo evita il Busy Waiting (CPU al 0% mentre attende)
        // 
        // Nota sulla dimensione: passo "sizeof(MsgTicket) - sizeof(long)" 
        // perché il campo mtype (long) non viene contato nel payload del messaggio.
        ssize_t bytes = msgrcv(msg_id, &msg, sizeof(MsgTicket) - sizeof(long), 1, 0);

        if (bytes == -1) {
            // GESTIONE TERMINAZIONE "ELEGANTE"
            // Se msgrcv fallisce con EIDRM (Identifier Removed), significa che 
            // il Direttore ha eseguito msgctl(IPC_RMID)
            // La coda è stata distrutta "sotto i miei piedi"
            if (errno == EIDRM) {
                // È il segnale che la simulazione è finita ed esco pulito
                break; 
            }
            // Altri errori (es. interrupt di segnale) vengono ignorati per riprovare
            continue;
        }

        // --- ELABORAZIONE ---
        // Preparo la risposta riutilizzando la stessa struct
        // Assegno il numero progressivo.
        msg.numero_ticket = global_ticket_counter++;

        // --- ROUTING DELLA RISPOSTA ---
        // Il punto cruciale: Cambio l'mtype
        // Invece di rispondere su un canale generico, uso il PID del richiedente
        // In questo modo, solo LUI riceverà questo specifico messaggio
        msg.mtype = msg.pid_richiedente; 

        // Invio (non bloccante di default, a meno che la coda non sia piena)
        msgsnd(msg_id, &msg, sizeof(MsgTicket) - sizeof(long), 0);
    }
    
    // shmdt non serve qui perché non ho fatto shmat
    return 0;
}