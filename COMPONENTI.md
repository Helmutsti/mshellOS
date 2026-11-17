# Componenti userland di mshell

Fonti: commenti in testa ai file di `utente/`, manifest.toml delle app, SISTEMA.md, APPLICAZIONI.md.

Nota sui permessi: `ha_permesso` (corekit, `interno/messaggi.c`; la regola in `sistema/servizi.h`) è vera dal 16 novembre 2026: il sistema (uid 0) ha tutto; un'app, dalla sua porta, ha i permessi che il launcher le concede (dichiarati nel manifesto, quelli di sistema solo alle app del sistema) e passa ai servizi con `attach`; un altro programma della persona ha i permessi normali. I rifiuti vanno nel registro. Sono veri anche il controllo dell'uid (SO_PEERCRED), `pacchetto_ha_permesso` (lo usa ancora il compositore per CAMBIA_TEMA) e le regole Unix (uid 1000 non può terminare processi di uid 0).

## Tabella riassuntiva

| Nome | Tipo | Lanciato da | Gira come | Super |
|---|---|---|---|---|
| avvio | avvio | kernel (da initramfs, /init) | sistema (uid 0) | sì |
| init | servizio | avvio (exec, resta PID 1) | sistema (uid 0) | sì |
| eventi | servizio | init | sistema (uid 0) | sì |
| compositore | servizio | init | sistema (uid 0) | sì |
| launcher | servizio | init | sistema (uid 0) | sì |
| scrivania | servizio | init | sistema (uid 0) | sì |
| picker | servizio | init | persona (uid 1000) | no |
| network (servizio) | servizio | init | sistema (uid 0) | sì |
| catalog | servizio | init | persona (uid 1000) | no |
| tasks | servizio | init | sistema (uid 0) | sì |
| shell | comando/servizio di prova | init (seriale) o il Terminale | sistema (uid 0) | sì |
| network (comando) | comando | la shell | chi lo lancia (oggi shell: uid 0) | no |
| curl | comando | la shell | chi lo lancia (oggi shell: uid 0) | no |
| vim | comando | la shell | chi lo lancia (oggi shell: uid 0) | no |
| Files | app | launcher | persona (uid 1000) | no |
| Attività | app | launcher | persona (uid 1000) | no |
| Impostazioni | app | launcher | persona (uid 1000) | no (ha "settings.system") |
| Store | app | launcher | persona (uid 1000) | no (ha "apps.manage") |
| Terminale | app | launcher | sistema (uid 0, provvisorio: l'unica app) | sì |
| Doom | app | launcher | persona (uid 1000) | no |
| network.conf, firewall.conf | configurazione | letti dal servizio network | non applicabile | non applicabile |
| corekit | libreria | collegata dentro ogni programma | quello del programma | non applicabile |
| gkit | libreria | idem | quello del programma | non applicabile |
| uikit | libreria | idem | quello del programma | non applicabile |
| esterni (miniz, stb, tomlc99) | libreria | idem | quello del programma | non applicabile |

---

## Avvio

### avvio (`utente/avvio/avvio.c`)
- **Cosa fa**: primo programma dell'archivio di avvio (initramfs). Monta /dev, /proc, /sys, carica i moduli del kernel, trova le partizioni per nome, monta system come `/` in sola lettura e data in `/data`, poi passa a `/system/init`.
- **Tipo**: avvio.
- **Lanciato da**: kernel (come `/init`, processo 1).
- **Lancia**: init (con execve, prende il suo posto).
- **Gira come**: sistema (uid 0).
- **Poteri**: super sì (monta, carica moduli). Nessun permesso da manifesto, nessun controllo.

---

## Servizi

### init (`utente/programmi/init.c`)
- **Cosa fa**: PID 1. Apre schermo e porte ("compositore", "launcher", "catalog", "tasks", "picker"), prepara il tubo degli eventi, avvia i servizi passandogli le risorse già aperte, li sorveglia e li fa ripartire se cadono. Custodisce le porte delle app di catalog, launcher, network, tasks e picker (canale per servizio, `keep` e `restore`, senza mai leggere dalle porte: le butta quando l'app le chiude) e le riconsegna a ogni partenza del servizio (riga `init: riconsegnate <n> porte a <percorso>`). Avvia anche i programmi di prova in `/data/tests` se esiste.
- **Tipo**: servizio.
- **Lanciato da**: avvio.
- **Lancia**: catalog e picker (come la persona, uid 1000), tasks (come sistema), eventi, compositore, launcher, scrivania, network, shell (sulla seriale, solo se `/dev/ttyS0` si apre), programmi di `/data/tests`.
- **Gira come**: sistema (uid 0).
- **Poteri**: super sì. Non controlla permessi.

### eventi (`utente/programmi/eventi.c`)
- **Cosa fa**: legge tastiera e mouse da `/dev/input` (li prende per sé con EVIOCGRAB), li traduce (tastiera italiana: i caratteri sono numeri Unicode, anche le lettere accentate e l'euro con AltGr+E; puntatore) e manda un messaggio per evento nel tubo verso il compositore. La ripetizione dei tasti (dopo circa 500 ms, circa 30 al secondo) la fa il kernel (EVIOCSREP); eventi la passa uguale per tutti i tasti, fisici e caratteri, tranne i modificatori.
- **Tipo**: servizio.
- **Lanciato da**: init (con il tubo al descrittore 0).
- **Lancia**: nessuno.
- **Gira come**: sistema (uid 0).
- **Poteri**: super sì (accesso ai dispositivi). Nessun controllo di permessi.

### compositore (`utente/programmi/compositore.c`)
- **Cosa fa**: padrone dello schermo. Compone finestre (con ombre e sfocatura), sfondo e pannelli, smista tastiera e mouse, sposta le finestre. Le app disegnano da sole in memoria condivisa. Riceve dal launcher la porta di ogni app (PORTA_APP, solo da uid 0) e lega a quell'app le finestre che arrivano dalla porta (`gkit/sistema/protocollo.h`, "LA PORTA DELL'APP"); i programmi del sistema si collegano ancora per nome.
  **La finestra di scelta** (`RUOLO_SCELTA`, `gkit/sistema/protocollo.h`, "LA FINESTRA DI SCELTA"): sta in uno strato proprio, sopra tutte le finestre e sotto i pannelli; si mette centrata sopra la finestra più in primo piano dell'app `sopra` (o al centro dello schermo); prende il fuoco quando compare e non è annunciata alla scrivania. Finché è aperta ogni finestra di quell'app è velata (il velo `SUPERFICIE_VELO`, disegnato con `velo_finestra_disegna` di UIKit sulla forma della finestra) e non riceve puntatore, rotella né tasti: un clic su una velata porta davanti la scelta. Quando si chiude, velo via e fuoco all'app. Accettata solo dal collegamento per nome il cui programma (lo dice il kernel) è `/system/services/picker/picker`.
- **Tipo**: servizio.
- **Lanciato da**: init (con schermo, tubo eventi e porta "compositore" già aperti).
- **Lancia**: nessuno.
- **Gira come**: sistema (uid 0).
- **Poteri**: super sì. Controlli: i ruoli speciali (sfondo, pannello) solo a chi ha uid 0 (controllo vero, via kernel). Il tema lo prende da catalog e ne osserva gli avvisi (per le ombre); non fa più da tramite per il cambio di tema.

### launcher (`utente/programmi/launcher.c`)
- **Cosa fa**: fa partire le applicazioni (come persona; il Terminale come sistema) e i programmi; apre i file `.app` con lo Store; installa, aggiorna, toglie le app e svuota i loro dati; elenca le app.
- **Tipo**: servizio.
- **Lanciato da**: init (con la porta "launcher" già aperta, e il canale del custode al descrittore 1: ogni porta che tiene gli manda una copia, e al riavvio init gliele riconsegna).
- **Lancia**: le app (uid 1000, tranne il Terminale che parte come sistema, uid 0: unico caso, provvisorio, decisione della persona del 17/11/2026, `uid_dell_app`; `avvia_app`: 0, 1 e 2 su /dev/null, al 3 la porta verso il compositore, al 4 quella verso catalog, al 5 quella verso il launcher stesso (il capo del servizio lo tiene lui) e al 6 quella verso network, solo se il manifesto ha `network.use` o `network.settings`, al 7 quella verso picker (a tutte) e all'8 quella verso tasks, solo se il manifesto ha `processes.view` o `processes.stop`; gli altri capi li manda ai servizi con il programma dell'app, e scrive nel registro una riga per app con le porte date: `SISTEMA.md`, "Le porte delle app"), lo Store per i file `.app` (allo stesso modo), i programmi di /system/bin (uid 1000).
- **Gira come**: sistema (uid 0): serve per far partire programmi con un altro uid e per scrivere in `/data/apps`.
- **Poteri**: super sì. Decide i permessi concessi a ogni app quando la fa partire (`permessi_concessi`: dichiarati nel manifesto, quelli di sistema solo alle app del sistema, sconosciuti tolti) e li passa ai servizi con le porte. Controlla con `ha_permesso`: "apps.open" (programmi), "apps.manage" (installare, aggiornare, togliere, svuotare dati).

### scrivania (`utente/programmi/scrivania.c`)
- **Cosa fa**: la "faccia" del sistema: sfondo, userbar (pulsante App e app di sistema, con segno per quelle aperte; in fondo foto profilo segnaposto e ora, aggiornata a ogni minuto con il timeout del poll) e vassoio delle app installate. Un clic su un'icona chiede al launcher di aprire l'app.
- **Tipo**: servizio (con superfici speciali, per il compositore è come un'app).
- **Lanciato da**: init.
- **Lancia**: nessun programma direttamente; chiede al launcher (`apri_applicazione`).
- **Gira come**: sistema (uid 0): serve per ottenere i ruoli sfondo e pannello dal compositore.
- **Poteri**: super sì. Non controlla permessi; legge i manifest delle app per le icone.

### picker (`utente/programmi/picker.c`, `picker_nome.c`, `picker_finestra.c`)
- **Cosa fa**: fa scegliere alla persona un file da aprire (`open`) o dove salvare (`save`) e lo consegna già aperto all'app che l'ha chiesto, allegato alla risposta. Mostra la finestra "Apri/Salva" (`picker_finestra.c`, UIKit, `disegni/ApriSalva/`) sopra l'app; una richiesta alla volta. Il servizio apre la Home e, dentro, cartelle e file con `openat2` (`RESOLVE_BENEATH`, `RESOLVE_NO_SYMLINKS`, `RESOLVE_NO_MAGICLINKS`); alla finestra dà solo l'elenco (file e cartelle, niente collegamenti) e "esiste già?". Contratto in `corekit/picker.h`; prova `picker` (con la finestra finta `utente/prove/picker_finestra_finta.c`, solo nel disco `disco_picker.img`).
- **Tipo**: servizio.
- **Lanciato da**: init (con la porta "picker" già aperta, e il canale del custode al descrittore 1).
- **Lancia**: nessuno.
- **Gira come**: persona (uid 1000): apre solo i file di lei, e quelli che crea (0644) sono suoi. Si collega al compositore per nome, come un programma che non è di sistema: il compositore gli concede `RUOLO_SCELTA` perché il suo programma è `/system/services/picker/picker`.
- **Poteri**: nessuno speciale. Nessun permesso da controllare (è di base per ogni app). Rifiuta (`invalid`) un nome proposto con "/", "." o "..", e non consegna mai niente fuori dalla Home né che non sia un file normale. Riceve dal launcher la porta di ogni app (`attach`, solo da uid 0): la finestra va sopra l'app della porta.

### catalog (`utente/programmi/catalog.c`)
- **Cosa fa**: possiede le impostazioni della persona (oggi tema e sfondo, in `/data/users/1000/preferences`): le dà a chi le chiede (`get`), avvisa chi lo chiede quando cambiano (`watch`, collegamento che resta aperto) e le cambia (`set`). Contratto in `corekit/catalog.h`, funzioni per chi chiede in `corekit/interno/catalog.c`.
- **Tipo**: servizio.
- **Lanciato da**: init (con la porta "catalog" già aperta, e il canale del custode al descrittore 1: ogni `attach` ne manda una copia a init, e al riavvio init riconsegna le porte).
- **Lancia**: nessuno.
- **Gira come**: la persona (uid 1000): tocca solo i suoi file, e quelli che scrive sono di lei.
- **Poteri**: nessuno speciale. `get` e `watch` a tutti; `set` chiede `ha_permesso` per "settings.system" (il sistema, e le app del sistema che lo dichiarano: Impostazioni). Riceve dal launcher la porta di ogni app (`attach`, solo da uid 0): chi si collega da una porta è l'app che il launcher ha detto. Lo usano le app (con UIKit), la scrivania e il compositore: chiedono tema e sfondo e osservano gli avvisi; Impostazioni li cambia. Nessun altro tocca i file delle preferenze.

### tasks (`utente/programmi/tasks.c`)
- **Cosa fa**: il task manager del sistema: dà l'elenco dei programmi in esecuzione (`processes`, da `/proc`, in un foglio allegato), la memoria (`memory`, `/proc/meminfo`), lo spazio delle partizioni `/system` e `/data` (`disk`, `statvfs`) e quanto pesano `/system`, `/system/apps`, `/data/apps` e `/data/users` (`folder_size`, camminando nelle cartelle); ferma un programma (`stop`, SIGKILL via pidfd). Contratto in `corekit/tasks.h`; funzioni per chi chiede (`elenco_processi`, `stato_memoria`, `stato_disco`, `dimensione_cartella`, `termina_processo`) in `corekit/interno/tasks.c`. Il codice che leggeva `/proc` stava in CoreKit (corekit.c, file.c): ora sta solo qui.
- **Tipo**: servizio.
- **Lanciato da**: init (con la porta "tasks" già aperta, e il canale del custode al descrittore 1).
- **Lancia**: nessuno.
- **Gira come**: sistema (uid 0): deve vedere tutti i processi.
- **Poteri**: super sì (vede ogni processo e potrebbe fermarli tutti), ma non li usa: `stop` ferma solo processi con uid 1000, mai uid 0 né init (controlla l'uid del processo, aperto con pidfd, prima del segnale; risposta `denied`, `reason=not_yours`). Permessi con `ha_permesso`: `processes.view` (o `processes.stop`, che lo comprende) per vedere, `processes.stop` per fermare. I percorsi di `disk` e `folder_size` sono solo quelli dell'elenco (gli altri `invalid`). Un programma della persona fuori dal launcher (senza porta) non ha permessi di sistema: `denied`. Riceve dal launcher la porta di ogni app (`attach`, solo da uid 0).

### network (servizio) (`utente/programmi/network.c`)
- **Cosa fa**: all'avvio mette il firewall, accende la scheda, ottiene l'indirizzo (DHCP o fisso), la strada predefinita e i DNS; resta acceso per rinnovare il prestito. Ha una porta di controllo (stato, riavvia, ferma).
- **Tipo**: servizio.
- **Lanciato da**: init (lo fa ripartire se cade; gli presta il canale del custode al descrittore 0: init tiene le porte delle app e gliele riconsegna a ogni partenza). Il `restart` risponde ed esce: init lo fa ripartire.
- **Lancia**: nessuno.
- **Gira come**: sistema (uid 0): servono CAP_NET_ADMIN e CAP_NET_RAW.
- **Poteri**: super sì. Permessi con `ha_permesso`: "network.use" per lo stato, "network.settings" per riavvia e ferma. Riceve dal launcher la porta delle app a cui ha concesso uno di quei due permessi (`attach`, solo da uid 0, con i permessi): chi si collega da una porta è l'app che il launcher ha detto. Il restart passa le porte al programma rilanciato (argomenti `<descrittore>=<programma>=<permessi>`), così le app non le trovano morte (da decidere in generale: `STATO.md`).
- Legge `/system/config/network/network.conf` e `firewall.conf` (vedi Configurazioni).

### shell (`utente/programmi/shell.c`)
- **Cosa fa**: shell semplice per le prove. Comandi interni (pwd, cd, ls, help, exit); ogni altra parola è un programma, cercato prima in `/system/bin` poi in `/data/users/<uid>/bin`. Niente variabili, tubi o redirezioni.
- **Tipo**: comando di prova (provvisorio); usata come programma del sistema.
- **Lanciato da**: init (sulla seriale) oppure il Terminale (sul suo terminale finto, `avvia_con_variabile`). Tutti e due le danno `TERM=xterm-256color` (decisione della persona, 17/11/2026: il Terminale e la seriale di QEMU capiscono xterm).
- **Lancia**: i comandi digitati (con `fexecve`), quindi per esempio network, curl e vim. Ai comandi dà un ambiente minimo: `PWD`, `HOME=/home` e il `TERM` che ha ricevuto (se l'ha ricevuto).
- **Gira come**: sistema (uid 0), in entrambi i casi: sulla seriale perché init è sistema, nel Terminale perché ne eredita i poteri (il Terminale oggi è sistema). Non cambia uid ai comandi che lancia.
- **Poteri**: super sì (uid 0). Nessun controllo di permessi.

---

## Comandi

### network (comando) (`utente/comandi/network.c`)
- **Cosa fa**: chiede al servizio di rete lo stato (`network [status]`), e per chi ha il permesso lo riavvia (`restart`) o spegne la scheda (`stop`).
- **Tipo**: comando.
- **Lanciato da**: la shell.
- **Lancia**: nessuno.
- **Gira come**: chi lo lancia; oggi, dalla shell, uid 0. Non imposta uid.
- **Poteri**: no. Il controllo è nel servizio: "network.use" (stato), "network.settings" (riavvia, ferma). Un comando lanciato dalla persona ha i permessi normali (quindi lo stato sì, riavviare e fermare no); oggi la shell gira come sistema, e allora può tutto.

### curl (`strumenti/compilazione/curl.sh`)
- **Cosa fa**: il curl di sempre (non è nostro: sorgenti ufficiali, versione fissata, compilato statico da build.sh). Scarica dal web, HTTP e HTTPS con mbedTLS; controlla i certificati con quelli di Alpine in `/etc/ssl/certs/ca-certificates.crt`.
- **Tipo**: comando.
- **Lanciato da**: la shell.
- **Lancia**: nessuno.
- **Gira come**: chi lo lancia; oggi, dalla shell, uid 0. Non imposta uid.
- **Poteri**: no. Non controlla permessi. Non chiaro se in futuro debba dichiarare "network.use": nel codice non c'è alcun controllo.

### vim (`strumenti/compilazione/vim.sh`)
- **Cosa fa**: l'editor vim nella versione più piccola ("tiny": niente sintassi colorata, niente linguaggio di vim, niente aiuto). Non è nostro: sorgenti ufficiali, la stessa versione del pacchetto di Alpine 3.24, compilato statico con ncurses di Alpine legata dentro. Legge le descrizioni dei terminali da `/etc/terminfo` (xterm-256color, xterm, vt100, linux) e le sue impostazioni di fabbrica da `/system/share/vim/vim92/defaults.vim`; nessun vimrc di sistema.
- **Tipo**: comando.
- **Lanciato da**: la shell (che gli passa il `TERM` ricevuto dal Terminale o da init: `xterm-256color`).
- **Lancia**: nessuno (`:!` lancerebbe una shell `sh`, che nel sistema non c'è).
- **Gira come**: chi lo lancia; oggi, dalla shell, uid 0. Non imposta uid.
- **Poteri**: no. Scrive i file con i permessi di chi lo lancia; il file di scambio (`.swp`) accanto al file, oppure in `/tmp`.

---

## App (`utente/applicazioni/`)

Tutte partono dal launcher come persona (uid 1000, `UID_APPLICAZIONI`), cioè senza poteri di sistema. Il manifesto è in `manifest.toml` (campo `permessi`). Chi dichiara un permesso lo dichiara solo nel manifesto: nel codice dell'app non c'è un controllo; i controlli stanno nei servizi, che per ora rispondono sempre sì (salvo i controlli veri indicati).

### Files (`applicazioni/Files/files.c`)
- **Cosa fa**: gestore dei file (come il Finder): sfoglia cartelle, griglia o tabella, filtri, rinomina e organizza.
- **Tipo**: app.
- **Lanciato da**: launcher (su richiesta della scrivania o di altre app).
- **Lancia**: nessun programma direttamente; per aprire un file chiede al launcher (`apri_file`).
- **Gira come**: persona (uid 1000).
- **Poteri**: super no. Manifesto: "files.write"; funzioni "file.apri_cartella", "file.mostra". Nel codice non c'è un controllo di "files.write": i file sono protetti dai permessi Unix.

### Attività (`applicazioni/Attivita/attivita.c`)
- **Cosa fa**: task manager: processi (ad albero), memoria, disco. Può terminare e riavviare processi. Non legge niente da sola: chiede tutto a tasks con le funzioni di CoreKit (`elenco_processi`, `stato_memoria`, `stato_disco`, `dimensione_cartella`, `termina_processo`).
- **Tipo**: app.
- **Lanciato da**: launcher.
- **Lancia**: nessun programma direttamente; per "Riavvia" chiede al launcher di riaprire l'app (`apri_applicazione`).
- **Gira come**: persona (uid 1000). La regola "solo i processi della persona" la applica tasks.
- **Poteri**: super no. Manifesto: "processes.stop" (permesso di sistema, concesso perché è un'app del sistema): con questo il launcher le dà la porta verso tasks, che le dà processi, memoria e disco e le ferma i programmi della persona (mai quelli del sistema).

### Impostazioni (`applicazioni/Impostazioni/impostazioni.c`)
- **Cosa fa**: impostazioni del sistema. Oggi solo "Aspetto": tema chiaro/scuro e sfondo della scrivania (anteprima e "Cambia": elenco Predefinito + foto di `/data/users/<uid>/wallpapers`, scelta salvata in `preferences/sfondo`); le altre sezioni sono vuote.
- **Tipo**: app.
- **Lanciato da**: launcher (anche su richiesta dello Store).
- **Lancia**: nessuno.
- **Gira come**: persona (uid 1000).
- **Poteri**: super no. Manifesto: "settings.system" (permesso di sistema, concesso perché è un'app del sistema): con questo catalog le lascia cambiare tema e sfondo.

### Store (`applicazioni/Store/store.c`)
- **Cosa fa**: elenca le app installate con il dettaglio (permessi, azioni "Svuota dati" e "Togli app"); con un percorso `.app` mostra la richiesta di installazione o aggiornamento.
- **Tipo**: app.
- **Lanciato da**: launcher (userbar, o apertura di un file `.app`).
- **Lancia**: nessun programma direttamente; chiede al launcher di aprire app (`apri_applicazione`, anche Impostazioni) e di installare, aggiornare, descrivere pacchetti.
- **Gira come**: persona (uid 1000).
- **Poteri**: super no, ma ha un permesso di sistema: manifesto "apps.manage", concesso perché è un'app del sistema; il launcher lo controlla con `ha_permesso` sulla porta dello Store.

### Terminale (`applicazioni/Terminale/terminale.c`)
- **Cosa fa**: finestra con dentro la shell di prova, su un terminale finto (pty). Il lavoro del terminale (griglia, colori, cursore, schermo alternativo, tasti) lo fa libvterm (sorgenti ufficiali 0.3.3, scaricati e compilati da `strumenti/compilazione/libvterm.sh`, legata solo qui); il Terminale la disegna con il suo stile. Provvisorio.
- **Tipo**: app.
- **Lanciato da**: launcher (solo dal vassoio: `nella_barra = false`).
- **Lancia**: la shell, da sé (`avvia_con_variabile`, fork ed exec): lo schiavo del terminale finto come 0, 1 e 2 e `TERM=xterm-256color` nell'ambiente; la sessione e il terminale di controllo li prende la shell (setsid, TIOCSCTTY). Chiudendo la finestra la shell riceve SIGHUP.
- **Gira come**: sistema (uid 0): provvisorio, decisione della persona del 17/11/2026; è l'unica app a cui il launcher lo fa (`uid_dell_app`), e per farla girare come la persona basta togliere quel caso. La shell dentro ne eredita i poteri. Le sue porte verso i servizi restano quelle di un'app (i permessi del manifesto: nessuno). Conseguenza: Attività non può terminarla (tasks ferma solo i processi della persona).
- **Poteri**: super sì (uid 0), e dà una shell uid 0 a chi la usa. Manifesto: nessun permesso. Senza recinto, come le altre app oggi.

### Doom (`applicazioni/Doom/`, adattamento `doomgeneric_mshell.c`)
- **Cosa fa**: il gioco Doom (shareware, doomgeneric), disegnato con solo GKit.
- **Tipo**: app.
- **Lanciato da**: launcher.
- **Lancia**: nessuno.
- **Gira come**: persona (uid 1000).
- **Poteri**: no. Manifesto: nessun permesso (`[]`).

---

## Configurazioni (`utente/config/network/`)

### network.conf e firewall.conf
- **Cosa fa**: `network.conf` dice la scheda (eth0) e il modo (dhcp o static con address, gateway, dns). `firewall.conf` dice entrata, uscita, inoltro e ping (oggi: entrata blocca, uscita permetti, inoltro blocca, ping permetti).
- **Tipo**: configurazione (non è un programma).
- **Lanciato da**: letti dal servizio network all'avvio; `build.sh` li mette in `/system/config/network/`.
- **Lancia**: nessuno.
- **Gira come**: non applicabile. **Poteri**: non applicabile (sola lettura, nessuno li cambia a runtime).

---

## Librerie (`utente/libreria/`)

Collegate dentro ogni programma (statico): non girano da sole e non hanno uid. Ogni libreria ha una parte pubblica, `sistema/` (solo per i programmi del sistema) e `interno/`.

### corekit (`libreria/corekit/`)
- **Cosa fa**: base comune: file, processi, porte e messaggi, memoria condivisa, tempo, pacchetti (.app, manifesti), cliente del launcher, del servizio di rete e di tasks (processi, memoria, disco). Il lato servizio (`chi_chiede`, `ha_permesso`) e i privilegi (`avvia_come`, `cambia_proprietario`) stanno nella parte `sistema/`.
- **Tipo**: libreria. **Lanciato da / Lancia**: non applicabile / nessuno. **Gira come**: quello del programma.
- **Poteri**: `ha_permesso` è vera (regola in `sistema/servizi.h`); `pacchetto_ha_permesso` e `pacchetto_puo_gestire` sono vere (leggono il manifesto di un'app in `/system/apps`). Le funzioni di `privilegi.h` riescono solo con uid 0 (controllo del kernel).

### gkit (`libreria/gkit/`)
- **Cosa fa**: base della grafica: apre la finestra, dà la tela, parla con il compositore, eventi di mouse e tastiera, strumenti di disegno. Il testo è UTF-8: i font (`.font`, versione 2) hanno i caratteri per numero Unicode e un segno sostitutivo per quelli che mancano. Il protocollo delle finestre sta in `sistema/protocollo.h`.
- **Tipo**: libreria. **Lanciato da / Lancia**: non applicabile / nessuno. **Gira come**: quello del programma. **Poteri**: no.

### uikit (`libreria/uikit/`)
- **Cosa fa**: interfaccia: stile e temi (dal Figma), widget (pulsanti, tabelle, schede, barra laterale, pannello, avvisi...), finestra a aree e ciclo degli eventi. Include userbar e vassoio (usati dalla scrivania).
- **Tipo**: libreria. **Lanciato da / Lancia**: non applicabile / nessuno. **Gira come**: quello del programma. **Poteri**: no.

### esterni (`libreria/esterni/`)
- **miniz**: leggere e scrivere archivi zip (per i pacchetti `.app`).
- **stb**: `stb_image`, per leggere immagini PNG.
- **tomlc99**: leggere i file TOML (i manifesti).
- Tipo: libreria. Gira come quello del programma. Poteri: no.

---

## Programmi di prova (`utente/prove/`)

Programmi di prova, solo per i test automatici, non fanno parte del sistema: `argomenti`, `firewall`, `messaggi`, `pacchetti`, `rete`, `riempitivo`, `stato_rete` (messi in `/data/tests`, avviati da init come sistema, uid 0, quando esiste quella cartella) e le app di prova `applicazioni/Contatore` e `applicazioni/Esempio` (Esempio ha `network.use` nel manifesto: all'avvio chiede il tema a catalog e lo stato a network dalle sue porte, per provarle).

---

## Cose non chiare

- Se i comandi (network, curl) avranno un manifesto con permessi: oggi non ne hanno; curl non controlla alcun permesso.
- Con quali permessi gira un comando lanciato dalla shell: oggi la shell è uid 0 e non cambia uid ai comandi (SISTEMA.md lo lascia tra le domande aperte).
- Permessi dichiarati ma non controllati da nessun servizio: "files.write" (Files). "processes.stop" (Attività) lo controlla tasks.
- Il commento in testa al compositore e al Terminale cita `utente/programmi/terminale.c`: oggi il Terminale è in `utente/applicazioni/Terminale/terminale.c`.
- Non ho letto i file lunghi per intero: compositore, network (servizio), files, store, uikit e il codice di Doom sono descritti solo dal commento in testa e da ricerche mirate.
