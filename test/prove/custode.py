"""custode: init custodisce le porte delle app (corekit/sistema/servizi.h).

Si accende il disco disco_custode.img (vedi build.sh), con la scheda di rete di QEMU
(RETE = True): in /data/tests c'e' il programma custode (utente/prove/custode.c), che init fa
partire come sistema. Lui fa da "launcher finto": crea tre porte di app finte (due verso catalog,
una verso network), le prova, uccide catalog e network con SIGKILL e riprova le STESSE porte,
senza rifarle: init le ha tenute e le riconsegna ai servizi quando li fa ripartire. Ogni passo lo
scrive nella seriale ("custode: ... -> si'"); qui si aspetta "custode: tutto bene", nessun passo
FALLITA, e nel registro le righe di init "riconsegnate ... porte a ..." per catalog e network.
"""

import time

from qemu import ErroreProva

NOME = "custode"
ORDINE = 94
DESCRIZIONE = "init custodisce le porte delle app: catalog e network uccisi, le stesse porte funzionano ancora"
DISCO = "disco_custode.img"
RETE = True
SOLO_QEMU = "network (scheda, DHCP, firewall) nella sandbox Docker non lo gestisce il servizio"


def esegui(p):
    m = p.m
    fine = time.time() + 150
    seriale = m.testo_seriale()
    while "custode: tutto bene" not in seriale and "custode: fallita" not in seriale:
        if time.time() > fine:
            raise ErroreProva("dopo 150 s il programma custode non ha finito:\n" +
                              "\n".join(r for r in seriale.splitlines() if r.startswith(("custode:", "init:"))))
        time.sleep(0.5)
        seriale = m.testo_seriale()
    sbagliati = [r for r in seriale.splitlines() if r.startswith("custode:") and "FALLITA" in r]
    p.controlla(not sbagliati, "passi falliti: " + "; ".join(sbagliati))
    p.controlla("custode: tutto bene" in seriale, "il programma custode non dice 'tutto bene'")
    righe = seriale.splitlines()
    # catalog ha avuto due porte (finta1, finta2), network una (finta3)
    for servizio, quante in (("catalog", 2), ("network", 1)):
        riga = f"init: riconsegnate {quante} porte a /system/services/{servizio}/{servizio}"
        p.controlla(any(r.startswith(riga) for r in righe), f"manca la riga di init: '{riga}'")
