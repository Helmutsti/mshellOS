"""stato_rete: la porta di controllo del servizio network (corekit/sistema/network.h).

Si accende il disco disco_stato_rete.img (vedi build.sh), con la scheda di rete di QEMU
(RETE = True: -nic user): in /data/tests c'e' il programma stato_rete
(utente/prove/stato_rete.c), che init fa partire come sistema. Lui chiede lo stato alla
rete accesa dal DHCP, prova una richiesta sconosciuta (invalid), spegne la rete con stop
(il firewall deve restare), la riaccende con restart. Ogni passo lo scrive nella seriale
("stato_rete: ... -> si'"); qui si aspetta "stato_rete: tutto bene" e che nessun passo sia
FALLITA, e si controlla nel registro la riga dei permessi provvisori.
"""

import time

from qemu import ErroreProva

NOME = "stato_rete"
ORDINE = 91
DESCRIZIONE = "porta del servizio network: status, richiesta sconosciuta, stop, restart; il comando network"
DISCO = "disco_stato_rete.img"
RETE = True
SOLO_QEMU = "la rete (scheda, DHCP, firewall) nella sandbox Docker non la gestisce il servizio"


def esegui(p):
    m = p.m
    fine = time.time() + 150
    seriale = m.testo_seriale()
    while "stato_rete: tutto bene" not in seriale and "stato_rete: fallita" not in seriale:
        if time.time() > fine:
            raise ErroreProva("dopo 150 s il programma stato_rete non ha finito:\n" +
                              "\n".join(r for r in seriale.splitlines() if r.startswith(("stato_rete:", "network:"))))
        time.sleep(0.5)
        seriale = m.testo_seriale()
    sbagliati = [r for r in seriale.splitlines() if r.startswith("stato_rete:") and "FALLITA" in r]
    p.controlla(not sbagliati, "passi falliti: " + "; ".join(sbagliati))
    p.controlla("stato_rete: tutto bene" in seriale, "il programma stato_rete non dice 'tutto bene'")
    for permesso in ("network.use", "network.settings"):
        p.controlla(f"permesso {permesso} a /data/tests/stato_rete (uid 0): concesso (provvisorio)" in seriale,
                    f"manca nel registro la riga del permesso provvisorio {permesso}")
