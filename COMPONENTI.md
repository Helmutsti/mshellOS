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
| network (servizio) | servizio | init | sistema (uid 0) | sì |
| catalog | servizio | init | persona (uid 1000) | no |
| shell | comando/servizio di prova | init (seriale) o launcher (Terminale) | sistema (uid 0) | sì |
| network (comando) | comando | la shell | chi lo lancia (oggi shell: uid 0) | no |
| curl | comando | la shell | chi lo lancia (oggi shell: uid 0) | no |
| Files | app | launcher | persona (uid 1000) | no |
| Attività | app | launcher | persona (uid 1000) | no |
| Impostazioni | app | launcher | persona (uid 1000) | no (ha "settings.system") |
| Store | app | launcher | persona (uid 1000) | no (ha "apps.manage") |
| Terminale | app | launcher | persona (uid 1000) | no |
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
- **Cosa fa**: PID 1. Apre schermo e porte ("compositore", "launcher", "catalog"), prepara il tubo degli eventi, avvia i servizi passandogli le risorse già aperte, li sorveglia e li fa ripartire se cadono. Avvia anche i programmi di prova in `/data/tests` se esiste.
- **Tipo**: servizio.
- **Lanciato da**: avvio.
- **Lancia**: catalog (come la persona, uid 1000), eventi, compositore, launcher, scrivania, network, shell (sulla seriale, solo se `/dev/ttyS0` si apre), programmi di `/data/tests`.
- **Gira come**: sistema (uid 0).
- **Poteri**: super sì. Non controlla permessi.

### eventi (`utente/programmi/eventi.c`)
- **Cosa fa**: legge tastiera e mouse da `/dev/input` (li prende per sé con EVIOCGRAB), li traduce (tastiera italiana, puntatore) e manda un messaggio per evento nel tubo verso il compositore.
- **Tipo**: servizio.
- **Lanciato da**: init (con il tubo al descrittore 0).
- **Lancia**: nessuno.
- **Gira come**: sistema (uid 0).
- **Poteri**: super sì (accesso ai dispositivi). Nessun controllo di permessi.

### compositore (`utente/programmi/compositore.c`)
- **Cosa fa**: padrone dello schermo. Compone finestre (con ombre e sfocatura), sfondo e pannelli, smista tastiera e mouse, sposta le finestre. Le app disegnano da sole in memoria condivisa. Riceve dal launcher la porta di ogni app (PORTA_APP, solo da uid 0) e lega a quell'app le finestre che arrivano dalla porta (`gkit/sistema/protocollo.h`, "LA PORTA DELL'APP"); i programmi del sistema si collegano ancora per nome.
- **Tipo**: servizio.
- **Lanciato da**: init (con schermo, tubo eventi e porta "compositore" già aperti).
- **Lancia**: nessuno.
- **Gira come**: sistema (uid 0).
- **Poteri**: super sì. Controlli: i ruoli speciali (sfondo, pannello) solo a chi ha uid 0 (controllo vero, via kernel). Il tema lo prende da catalog e ne osserva gli avvisi (per le ombre); non fa più da tramite per il cambio di tema.

### launcher (`utente/programmi/launcher.c`)
- **Cosa fa**: fa partire le applicazioni (come persona), la shell (come sistema) e i programmi; apre i file `.app` con lo Store; installa, aggiorna, toglie le app e svuota i loro dati; elenca le app.
- **Tipo**: servizio.
- **Lanciato da**: init (con la porta "launcher" già aperta).
- **Lancia**: le app (uid 1000, `avvia_app`: 0, 1 e 2 su /dev/null, al 3 la porta verso il compositore, al 4 quella verso catalog, al 5 quella verso il launcher stesso (il capo del servizio lo tiene lui) e al 6 quella verso network, solo se il manifesto ha `network.use` o `network.settings`; gli altri capi li manda ai servizi con il programma dell'app, e scrive nel registro una riga per app con le porte date: `SISTEMA.md`, "Le porte delle app"), lo Store per i file `.app` (allo stesso modo), i programmi di /system/bin (uid 1000), la shell (uid 0, `avvia_come_con_descrittori`).
- **Gira come**: sistema (uid 0): serve per far partire programmi con un altro uid e per scrivere in `/data/apps`.
- **Poteri**: super sì. Decide i permessi concessi a ogni app quando la fa partire (`permessi_concessi`: dichiarati nel manifesto, quelli di sistema solo alle app del sistema, sconosciuti tolti) e li passa ai servizi con le porte. Controlla con `ha_permesso`: "apps.open" (programmi), "system.shell" (shell), "apps.manage" (installare, aggiornare, togliere, svuotare dati).

### scrivania (`utente/programmi/scrivania.c`)
- **Cosa fa**: la "faccia" del sistema: sfondo, userbar (pulsante App e app di sistema, con segno per quelle aperte; in fondo foto profilo segnaposto e ora, aggiornata a ogni minuto con il timeout del poll) e vassoio delle app installate. Un clic su un'icona chiede al launcher di aprire l'app.
- **Tipo**: servizio (con superfici speciali, per il compositore è come un'app).
- **Lanciato da**: init.
- **Lancia**: nessun programma direttamente; chiede al launcher (`apri_applicazione`).
- **Gira come**: sistema (uid 0): serve per ottenere i ruoli sfondo e pannello dal compositore.
- **Poteri**: super sì. Non controlla permessi; legge i manifest delle app per le icone.

### catalog (`utente/programmi/catalog.c`)
- **Cosa fa**: possiede le impostazioni della persona (oggi tema e sfondo, in `/data/users/1000/preferences`): le dà a chi le chiede (`get`), avvisa chi lo chiede quando cambiano (`watch`, collegamento che resta aperto) e le cambia (`set`). Contratto in `corekit/catalog.h`, funzioni per chi chiede in `corekit/interno/catalog.c`.
- **Tipo**: servizio.
- **Lanciato da**: init (con la porta "catalog" già aperta).
- **Lancia**: nessuno.
- **Gira come**: la persona (uid 1000): tocca solo i suoi file, e quelli che scrive sono di lei.
- **Poteri**: nessuno speciale. `get` e `watch` a tutti; `set` chiede `ha_permesso` per "settings.system" (il sistema, e le app del sistema che lo dichiarano: Impostazioni). Riceve dal launcher la porta di ogni app (`attach`, solo da uid 0): chi si collega da una porta è l'app che il launcher ha detto. Lo usano le app (con UIKit), la scrivania e il compositore: chiedono tema e sfondo e osservano gli avvisi; Impostazioni li cambia. Nessun altro tocca i file delle preferenze.

### network (servizio) (`utente/programmi/network.c`)
- **Cosa fa**: all'avvio mette il firewall, accende la scheda, ottiene l'indirizzo (DHCP o fisso), la strada predefinita e i DNS; resta acceso per rinnovare il prestito. Ha una porta di controllo (stato, riavvia, ferma).
- **Tipo**: servizio.
- **Lanciato da**: init (lo fa ripartire se cade).
- **Lancia**: nessuno.
- **Gira come**: sistema (uid 0): servono CAP_NET_ADMIN e CAP_NET_RAW.
- **Poteri**: super sì. Permessi con `ha_permesso`: "network.use" per lo stato, "network.settings" per riavvia e ferma. Riceve dal launcher la porta delle app a cui ha concesso uno di quei due permessi (`attach`, solo da uid 0, con i permessi): chi si collega da una porta è l'app che il launcher ha detto. Il restart passa le porte al programma rilanciato (argomenti `<descrittore>=<programma>=<permessi>`), così le app non le trovano morte (da decidere in generale: `STATO.md`).
- Legge `/system/config/network/network.conf` e `firewall.conf` (vedi Configurazioni).

### shell (`utente/programmi/shell.c`)
- **Cosa fa**: shell semplice per le prove. Comandi interni (pwd, cd, ls, help, exit); ogni altra parola è un programma, cercato prima in `/system/bin` poi in `/data/users/<uid>/bin`. Niente variabili, tubi o redirezioni.
- **Tipo**: comando di prova (provvisorio); usata come programma del sistema.
- **Lanciato da**: init (sulla seriale) oppure il launcher (richiesta APRI_SHELL, dentro il Terminale).
- **Lancia**: i comandi digitati (con `fexecve`), quindi per esempio network e curl.
- **Gira come**: sistema (uid 0), in entrambi i casi. Non cambia uid ai comandi che lancia.
- **Poteri**: super sì (uid 0). Nessun controllo di permessi; il permesso "system.shell" è controllato dal launcher (il sistema e il Terminale).

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
- **Cosa fa**: task manager: processi (ad albero), memoria, disco. Può terminare e riavviare processi.
- **Tipo**: app.
- **Lanciato da**: launcher.
- **Lancia**: nessun programma direttamente; per "Riavvia" chiede al launcher di riaprire l'app (`apri_applicazione`).
- **Gira come**: persona (uid 1000), quindi può terminare solo le app, non i servizi (regola Unix).
- **Poteri**: super no. Manifesto: "processes.stop". Il permesso non è controllato da nessun servizio: il limite vero è l'uid 1000.

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
- **Cosa fa**: finestra con dentro la shell di prova, su un terminale finto (pty) con griglia di caratteri, colori e scorrimento. Provvisorio.
- **Tipo**: app.
- **Lanciato da**: launcher (solo dal vassoio: `nella_barra = false`).
- **Lancia**: la shell, tramite il launcher (richiesta APRI_SHELL), che la fa partire come sistema (uid 0) sul terminale finto del Terminale.
- **Gira come**: persona (uid 1000); la shell dentro gira come sistema (uid 0).
- **Poteri**: l'app non è super, ma di fatto dà una shell uid 0 a chi la usa. Manifesto: "system.shell" (dal 16 novembre 2026, quando il controllo è diventato vero). Il launcher lo controlla con `ha_permesso`.

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
- **Cosa fa**: base comune: file, processi, porte e messaggi, memoria condivisa, tempo, pacchetti (.app, manifesti), cliente del launcher e del servizio di rete. Il lato servizio (`chi_chiede`, `ha_permesso`) e i privilegi (`avvia_come`, `cambia_proprietario`) stanno nella parte `sistema/`.
- **Tipo**: libreria. **Lanciato da / Lancia**: non applicabile / nessuno. **Gira come**: quello del programma.
- **Poteri**: `ha_permesso` è vera (regola in `sistema/servizi.h`); `pacchetto_ha_permesso` e `pacchetto_puo_gestire` sono vere (leggono il manifesto di un'app in `/system/apps`). Le funzioni di `privilegi.h` riescono solo con uid 0 (controllo del kernel).

### gkit (`libreria/gkit/`)
- **Cosa fa**: base della grafica: apre la finestra, dà la tela, parla con il compositore, eventi di mouse e tastiera, strumenti di disegno. Il protocollo delle finestre sta in `sistema/protocollo.h`.
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
- Permessi dichiarati ma non controllati da nessun servizio: "files.write" (Files), "processes.stop" (Attività). Per "files.write" e "processes.stop" resta solo il limite Unix.
- Il commento in testa al compositore e al Terminale cita `utente/programmi/terminale.c`: oggi il Terminale è in `utente/applicazioni/Terminale/terminale.c`.
- Non ho letto i file lunghi per intero: compositore, network (servizio), files, store, uikit e il codice di Doom sono descritti solo dal commento in testa e da ricerche mirate.
