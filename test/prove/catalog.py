"""catalog: il servizio delle impostazioni della persona (corekit/catalog.h).

Si accende il disco disco_catalog.img (vedi build.sh; nella sandbox, avvio con "catalog"): in
/data/tests c'e' il programma catalog (utente/prove/catalog.c), che init fa partire come
sistema. Lui legge le impostazioni (tema chiaro e sfondo predefinito all'inizio), si fa avvisare,
cambia tema e sfondo (il file del tema cambia, gli avvisi arrivano), prova i valori sbagliati e
una richiesta sconosciuta (invalid), e rimette il tema chiaro. Ogni passo lo scrive nella seriale
("prova catalog: ... -> si'"); qui si aspetta "prova catalog: tutto bene" e che nessun passo sia
FALLITA. Il programma e' del sistema (uid 0): ha settings.system, e nel registro non c'e' nessun
rifiuto. (Il rifiuto a un'app senza il permesso lo prova applicazioni, con Esempio.)
"""

import time

from qemu import ErroreProva

NOME = "catalog"
ORDINE = 93
DESCRIZIONE = "servizio catalog: get, watch con gli avvisi, set di tema e sfondo, valori sbagliati"
DISCO = "disco_catalog.img"


def esegui(p):
    m = p.m
    fine = time.time() + 60
    seriale = m.testo_seriale()
    while "prova catalog: tutto bene" not in seriale and "prova catalog: fallita" not in seriale:
        if time.time() > fine:
            raise ErroreProva("dopo 60 s il programma catalog non ha finito:\n" +
                              "\n".join(r for r in seriale.splitlines() if "catalog" in r))
        time.sleep(0.5)
        seriale = m.testo_seriale()
    sbagliati = [r for r in seriale.splitlines() if r.startswith("prova catalog:") and "FALLITA" in r]
    p.controlla(not sbagliati, "passi falliti: " + "; ".join(sbagliati))
    p.controlla("prova catalog: tutto bene" in seriale, "il programma catalog non dice 'tutto bene'")
    p.controlla(not any(r.startswith("permesso ") and "(uid 0): negato" in r for r in seriale.splitlines()),
                "al sistema (uid 0) e' stato negato un permesso")
