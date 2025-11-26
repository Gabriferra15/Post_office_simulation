PROGETTO DI SISTEMI OPERATIVI 2024/2025: SIMULAZIONE UFFICIO POSTALE

Nome: Ferrante Gabriele, Matricola: 1094504

1. Architettura del Sistema

Il progetto è stato realizzato adottando un’architettura multi-processo modulare in ambiente Linux. Il processo principale, Direttore, agisce da orchestratore: è responsabile dell'allocazione delle risorse IPC, del caricamento della configurazione e della generazione dei processi figli (erogatore, operatore, utente). In conformità con le richieste, la divisione in moduli è stata implementata tramite la chiamata di sistema execve(), garantendo che ogni entità esegua in uno spazio di indirizzamento separato e isolato.

2. Gestione delle Risorse e IPC (Inter-Process Communication)

Per la comunicazione e la condivisione dati ho scelto di utilizzare le primitive System V, preferendole per la loro capacità di gestire set di risorse atomiche (es. array di semafori).

    Memoria Condivisa (SHM): Utilizzata per mantenere lo stato globale del sistema (flag di apertura ufficio, mappatura sportelli, statistiche). Si è optato per un'unica struct unificata per ridurre l'overhead di gestione e centralizzare l'accesso ai dati.

    Code di Messaggi (Message Queue): Scelte per la gestione dell'erogazione dei ticket. Rispetto alle Pipe, le Message Queue offrono nativamente la conservazione dei message boundaries e permettono un routing efficiente: il campo mtype è stato sfruttato per indirizzare le risposte specificamente al PID del processo richiedente, simulando un canale privato virtuale.

    Semafori: Utilizzati per la sincronizzazione. È stato allocato un unico array di semafori per gestire:

        Mutua Esclusione (Mutex): Per proteggere l'accesso in scrittura alla SHM e prevenire Race Conditions.

        Sincronizzazione Code: Seguendo il modello Produttore-Consumatore, dove l'Utente produce richieste (Signal/V) e l'Operatore le consuma (Wait/P).

3. Scelte Implementative Rilevanti

3.1 Sincronizzazione all'Avvio (Barrier)

Per garantire che la simulazione inizi solo quando tutti i processi sono pronti, è stato implementato un meccanismo a Barriera (Turnstile). I figli si bloccano su un semaforo inizializzato a 0; il Direttore, dopo il setup, sblocca il primo processo, il quale a cascata sblocca il successivo. Questo evita race conditions in fase di inizializzazione.

3.2 Gestione Operatore e Prevenzione Deadlock

L'operatore applica una logica di polling intelligente.

    Ricerca Posto: Se non trova sportelli liberi, entra in un ciclo di attesa passiva (usleep) controllando periodicamente la disponibilità, soddisfacendo il requisito di attesa di pause altrui.

    Consumo Ticket: Per il prelievo dalla coda, si è utilizzata la flag IPC_NOWAIT invece di una wait bloccante. Questa scelta è critica per prevenire deadlock: se la coda è vuota e l'ufficio chiude, un'attesa bloccante impedirebbe all'operatore di terminare. Con NOWAIT, l'operatore riceve EAGAIN, controlla lo stato dell'ufficio e può terminare graziosamente.

3.3 Gestione Utente e Code

L'utente verifica la disponibilità del servizio accedendo alla SHM in lettura (protetta da Mutex per coerenza coi dati in scrittura del Direttore). La gestione dell'abbandono della coda a fine giornata è stata risolta implicitamente: l'utente attende nel sistema finché l'ufficio è aperto. Se alla chiusura non è stato servito, il processo termina e il Direttore calcola i servizi "non erogati" basandosi sul residuo della coda in memoria condivisa.

3.4 Efficienza (No Busy Waiting)

In tutto il progetto è stata rigorosamente evitata l'attesa attiva. Ogni ciclo di attesa (sia per l'arrivo utente, sia per il polling di stato) utilizza primitive di sospensione (usleep o blocchi su semafori/messaggi), garantendo un utilizzo della CPU minimo e massimizzando il grado di concorrenza.

4. Terminazione

La terminazione è gestita centralmente dal Direttore. Alla fine della simulazione (o su ricezione di SIGINT), viene eseguita una routine di cleanup che:

    Invia SIGTERM all'intero gruppo di processi (kill(0, ...)) per terminare i figli istantaneamente.

    Esegue il reap dei processi morti (wait) per evitare processi zombie.

    Rimuove tassativamente tutte le risorse IPC (IPC_RMID) per lasciare il sistema ospite pulito.