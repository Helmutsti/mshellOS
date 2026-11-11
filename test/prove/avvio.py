"""avvio: Linux parte, il servizio eventi trova la tastiera e la tavoletta USB, la RAM e'
quella data, e la scrivania compare.

Si legge la seriale, dove il kernel Linux e i nostri programmi raccontano l'avvio. Si
cercano le righe:
    Linux version 6.18.63-0-virt ...                         (il kernel)
    efi: EFI v2.7 by EDK II                                   (solo PC: partito da UEFI)
    Memory: 1970416K/2090760K available ...                   (la RAM: quella che il
                                                               kernel vede / il totale)
    avvio: 26 moduli del kernel caricati                      (l'archivio di avvio)
    avvio: / montato (ext4 su /dev/vda)                       (il disco)
    eventi: tastiera "QEMU QEMU USB Keyboard" (/dev/input/event1)
    eventi: tavoletta "QEMU QEMU USB Tablet" (/dev/input/event2)
Il totale della RAM deve essere poco meno di quella data a QEMU (2048 MB sul PC se non si
dice --ram, 512 MB su ARM64): mancano i pezzi che il firmware tiene per se'.
Che non ci siano "Kernel panic" ed errori lo controlla il collaudo dopo ogni prova.
"""

import re

from qemu import isola_visibile

NOME = "avvio"
ORDINE = 10
DESCRIZIONE = "seriale dell'avvio: Linux, RAM, moduli, disco, tastiera e tavoletta USB; la scrivania compare"


def esegui(p):
    # la tastiera e la tavoletta USB arrivano mentre il sistema parte: si aspettano
    s = p.m.aspetta_seriale('eventi: tastiera "QEMU QEMU USB Keyboard"', 30)
    s = p.m.aspetta_seriale('eventi: tavoletta "QEMU QEMU USB Tablet"', 30)

    p.controlla(re.search(r"Linux version \d+\.\d+", s), "manca la riga 'Linux version ...'")
    if p.cpu == "x86_64":
        p.controlla(re.search(r"efi: EFI v\d", s), "il PC non e' partito da UEFI (manca 'efi: EFI v...')")
    moduli = re.search(r"avvio: (\d+) moduli del kernel caricati", s)
    p.controlla(moduli and int(moduli.group(1)) > 0, "avvio non ha caricato i moduli del kernel")
    p.controlla("avvio: / montato (ext4 su /dev/vd" in s, "il disco del sistema (ext4) non e' montato")

    memoria = re.search(r"Memory: (\d+)K/(\d+)K available", s)
    p.controlla(memoria, "manca la riga 'Memory: ...K/...K available'")
    totale_mb = int(memoria.group(2)) / 1024
    p.controlla(p.ram * 0.95 < totale_mb <= p.ram,
                f"RAM strana: Linux ne vede {totale_mb:.0f} MB su {p.ram}")
    p.nota(f"RAM: {totale_mb:.0f} MB su {p.ram}, {int(memoria.group(1)) / 1024:.0f} MB disponibili; "
           f"{moduli.group(1)} moduli")

    # la scrivania: l'isola c'e' (l'ha gia' aspettata il collaudo; qui lo si dice)
    p.controlla(isola_visibile(p.scrivania), "l'isola della scrivania non si vede")
