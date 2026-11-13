"""messaggi: lo standard dei messaggi tra servizi e clienti (utente/libreria/messaggi.c).

Si accende il disco della prova dei messaggi (disco_messaggi.img, vedi build.sh): in
/data/tests c'e' il programma messaggi (utente/prove/messaggi.c), che init fa partire come
sistema. Lui apre una porta, fa da servizio finto e da cliente, e prova: messaggi sbagliati
rifiutati mentre si costruiscono, una richiesta echo con chiedi (chiave ripetuta, numero,
foglio allegato, una risposta con un altro id da buttare, chi chiede), un pacchetto fuori
standard (risposta invalid senza id) e una richiesta sconosciuta (invalid con l'id).
Ogni passo lo scrive nella seriale ("messaggi: ... -> si'"); qui si aspetta "messaggi:
tutto bene" e che nessun passo sia FALLITA.
"""

import time

from qemu import ErroreProva

NOME = "messaggi"
ORDINE = 90
DESCRIZIONE = "messaggi chiave=valore: costruire, rifiutare, chiedi con id, allegato, invalid"
DISCO = "disco_messaggi.img"


def esegui(p):
    m = p.m
    fine = time.time() + 60
    seriale = m.testo_seriale()
    while "messaggi: tutto bene" not in seriale and "messaggi: fallita" not in seriale:
        if time.time() > fine:
            raise ErroreProva("dopo 60 s il programma messaggi non ha finito")
        time.sleep(0.5)
        seriale = m.testo_seriale()
    sbagliati = [r for r in seriale.splitlines() if r.startswith("messaggi:") and "FALLITA" in r]
    p.controlla(not sbagliati, "passi falliti: " + "; ".join(sbagliati))
    p.controlla("messaggi: tutto bene" in seriale, "il programma messaggi non dice 'tutto bene'")
