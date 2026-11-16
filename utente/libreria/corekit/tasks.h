/* CoreKit, pubblico: tasks, il servizio dei processi, della memoria e del disco (SISTEMA.md,
 * sezione 1). Dice quali programmi girano, quanta RAM e quanto disco si usano, e ferma un
 * programma. E' il "task manager" del sistema: lo usa Attivita'.
 *
 * Perche' un servizio: un'app nel recinto non vede /proc, ne' le cartelle del sistema. Tasks gira
 * come SISTEMA (uid 0), vede tutto, e da' queste informazioni (e il potere di fermare) solo a chi
 * ha il permesso da task manager. Ascolta sul nome "tasks", che gli presta init; per le app c'e'
 * la porta D_PORTA_TASKS (messaggi.h, "le porte delle app"), data dal launcher solo a chi ha
 * processes.view o processes.stop nel manifesto.
 *
 * I messaggi seguono lo STANDARD di SISTEMA.md (sezione 3; messaggi.h). Qui il CONTRATTO; le
 * funzioni per chi chiede sono quelle di corekit.h (elenco_processi, stato_memoria, stato_disco,
 * dimensione_cartella, termina_processo) e lo nascondono: stanno in interno/tasks.c. Sullo stesso
 * collegamento si possono fare piu' richieste.
 *
 * LE RICHIESTE (il valore di request):
 *   processes     nessun dato. Risposta ok con count (quanti) e il foglio allegato: un
 *                 struct elenco_processi (qui sotto), con i programmi in ordine di numero
 *                 (non i lavori del kernel). Il nome di un'app e' quello della sua cartella
 *                 (es. attivita), non "code". Permesso processes.view.
 *   memory        nessun dato. Risposta ok con total, free, shared (byte). Permesso processes.view.
 *   disk          path (/system o /data). Risposta ok con total e free (byte) della partizione;
 *                 invalid se il percorso e' un altro. Permesso processes.view.
 *   folder_size   path (/system, /system/apps, /data/apps o /data/users). Risposta ok con size:
 *                 i byte di tutti i file della cartella e delle cartelle dentro (fino a
 *                 PROFONDITA_TASKS livelli); invalid se il percorso e' un altro. Puo' volerci un
 *                 po': tasks serve una richiesta alla volta. Permesso processes.view.
 *   stop          pid. Ferma il processo (SIGKILL). Permesso processes.stop. LA REGOLA, anche se
 *                 tasks e' il sistema: si ferma SOLO un processo della persona (uid 1000), mai uno
 *                 del sistema (uid 0) e mai init; tasks guarda l'uid del processo prima di
 *                 fermarlo. Risposte: ok; invalid (pid non valido); denied (manca il permesso, o il
 *                 processo non e' della persona: allora c'e' reason=not_yours); error con
 *                 reason=not_found se il processo non c'e' piu'.
 * Una richiesta sconosciuta: invalid.
 *
 * I PERMESSI (ha_permesso, sistema/servizi.h: la regola; un rifiuto si scrive nel registro):
 *     processes, memory, disk, folder_size   processes.view, o processes.stop (che lo comprende)
 *     stop                                   processes.stop
 * Sono permessi di sistema: li hanno il sistema e le app del sistema che li dichiarano; un
 * programma della persona fuori dal launcher non li ha. */
#ifndef TASKS_H
#define TASKS_H

#include "corekit.h"
#include "chiamate.h"

#define NOME_TASKS "tasks"

/* LA PORTA (messaggi.h, "le porte delle app"): a questo descrittore un'app con processes.view o
 * processes.stop trova la sua porta verso tasks. Il 7 e' di picker. */
#define D_PORTA_TASKS 8

/* le richieste (il valore di request) */
#define TASKS_RICHIESTA_PROCESSI "processes"
#define TASKS_RICHIESTA_MEMORIA  "memory"
#define TASKS_RICHIESTA_DISCO    "disk"
#define TASKS_RICHIESTA_CARTELLA "folder_size"
#define TASKS_RICHIESTA_FERMA    "stop"

/* le chiavi (oltre alla busta) */
#define TASKS_CHIAVE_QUANTI  "count"
#define TASKS_CHIAVE_PID     "pid"
#define TASKS_CHIAVE_PERCORSO "path"
#define TASKS_CHIAVE_TOTALE  "total"
#define TASKS_CHIAVE_LIBERA  "free"
#define TASKS_CHIAVE_CONDIVISA "shared"
#define TASKS_CHIAVE_PESO    "size"
#define TASKS_CHIAVE_MOTIVO  "reason"

/* i perche' (il valore di reason) */
#define TASKS_MOTIVO_NON_SUO    "not_yours"
#define TASKS_MOTIVO_NON_TROVATO "not_found"

/* i permessi che tasks chiede */
#define PERMESSO_VEDERE_PROCESSI "processes.view"
#define PERMESSO_FERMARE_PROCESSI "processes.stop"

/* Il foglio della risposta a processes: i programmi in esecuzione. */
#define MAX_PROCESSI_TASKS 1024
struct elenco_processi {
    int quanti;
    struct info_processo p[MAX_PROCESSI_TASKS];
};

/* Il risultato "manca il permesso" delle funzioni di corekit.h che chiedono a tasks (negativo come
 * -1, "non ha risposto": chi controlla solo < 0 non deve distinguerli). */
#define TASKS_NEGATO (-2)

/* Quante cartelle una dentro l'altra folder_size scende, al piu'. */
#define PROFONDITA_TASKS 8

#endif
