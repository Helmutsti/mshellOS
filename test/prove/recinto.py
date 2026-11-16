"""recinto: un programma chiuso in un recinto non vede disco, internet, servizi e altri programmi.

Si accende il disco disco_recinto.img (vedi build.sh), con la scheda di rete di QEMU (RETE =
True, per la controprova di internet): in /data/tests c'e' il programma recinto
(utente/prove/recinto.c), che init fa partire come sistema. Lui prova le stesse cose chiedendo
direttamente al kernel, senza CoreKit: prima come uid 1000 senza recinto (la controprova: Home,
launcher e internet ci arrivano), poi dentro un recinto (namespaces, radice nuova con solo il
suo programma, una cartella dati e le risorse del sistema, uid 1000, NO_NEW_PRIVS, seccomp con un elenco di chiamate vietate), dove tutto
deve rispondere "non esiste" o "rete irraggiungibile"; poi un secondo recinto, con la rete del
computer (permesso network.use): internet arriva, ma Home, launcher e disco restano invisibili. Qui si aspetta "recinto: tutto bene".
"""

import time

from qemu import ErroreProva

NOME = "recinto"
ORDINE = 92
DESCRIZIONE = "recinto delle app: dentro niente disco, internet, servizi e altri programmi (controprova fuori)"
DISCO = "disco_recinto.img"
RETE = True
SOLO_QEMU = "il recinto chiede al kernel i namespaces, che Docker senza permessi in piu' non da'"


def esegui(p):
    m = p.m
    fine = time.time() + 120
    seriale = m.testo_seriale()
    while "recinto: tutto bene" not in seriale and "recinto: fallita" not in seriale:
        if time.time() > fine:
            raise ErroreProva("dopo 120 s il programma recinto non ha finito:\n" +
                              "\n".join(r for r in seriale.splitlines() if r.startswith("recinto:")))
        time.sleep(0.5)
        seriale = m.testo_seriale()
    righe = [r for r in seriale.splitlines() if r.startswith("recinto:")]
    print("\n".join("            " + r for r in righe))
    sbagliati = [r for r in righe if "FALLITA" in r]
    p.controlla(not sbagliati, "passi falliti: " + "; ".join(sbagliati))
    p.controlla("recinto: tutto bene" in seriale, "il programma recinto non dice 'tutto bene'")
