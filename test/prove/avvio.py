"""avvio: il kernel parte, vede la tastiera e il mouse USB, conta la RAM, e la scrivania
compare.

Si legge la seriale, dove il kernel racconta l'avvio. Si cercano le righe:
    mkernel: caricatore UEFI                                          (solo PC: il caricatore)
    mkernel: schermo 1280x800                                         (solo PC)
    USB     : presa 5, 0627:0001, 480 Mbit/s -- tastiera
    USB     : presa 6, 0627:0001, 480 Mbit/s -- mouse (assoluto)     (la tavoletta)
    Pagine  : 521835 libere = 2038 MB                                 (le pagine libere;
                                                                       ARM64 le dice in KB)
    RAM     : da 0x40000000, 512 MB                                   (solo ARM64)
    VFS     : / montato (ext4 su ..., sola lettura)                  (il disco)
Le pagine libere devono essere poco meno della RAM data a QEMU (2048 MB sul PC se non si
dice --ram, 512 MB su ARM64): il resto e' il kernel, le sue tabelle e il firmware.
Che non ci siano PANICO ed errori lo controlla il collaudo dopo ogni prova.
"""

import re

from qemu import isola_visibile

NOME = "avvio"
ORDINE = 10
DESCRIZIONE = "seriale dell'avvio: USB, RAM, disco; la scrivania compare"


def esegui(p):
    s = p.m.testo_seriale()
    p.controlla(re.search(r"USB\s+:.*-- .*tastiera", s), "la tastiera USB non e' stata vista")
    p.controlla(re.search(r"USB\s+:.*mouse \(assoluto\)", s),
                "la tavoletta USB (mouse assoluto) non e' stata vista")
    p.controlla("VFS     : / montato" in s, "il disco del sistema (ext4) non e' montato")

    pagine = re.search(r"Pagine\s+:\s+(\d+) libere = (\d+) (KB|MB)", s)
    p.controlla(pagine, "manca la riga 'Pagine : ... libere = ...'")
    libere_mb = int(pagine.group(2)) / (1024 if pagine.group(3) == "KB" else 1)
    p.controlla(abs(int(pagine.group(1)) * 4 / 1024 - libere_mb) < 2,
                f"pagine libere: {pagine.group(1)} pagine da 4 KB non fanno {libere_mb:.0f} MB")
    if p.cpu == "arm64":
        ram = re.search(r"RAM\s+: da 0x[0-9a-f]+, (\d+) MB", s)
        p.controlla(ram, "manca la riga 'RAM : da ..., N MB'")
        p.controlla(int(ram.group(1)) == p.ram, f"RAM: {ram.group(1)} MB invece di {p.ram}")
    else:
        p.controlla("mkernel: caricatore UEFI" in s, "il caricatore UEFI non si e' presentato")
        p.controlla("mkernel: schermo 1280x800" in s,
                    "il caricatore non ha avuto lo schermo 1280x800 dal firmware")
    # liberi ne restano poco meno della RAM (il kernel, le tabelle, il firmware)
    p.controlla(p.ram * 0.9 < libere_mb <= p.ram,
                f"pagine libere strane: {libere_mb:.0f} MB su {p.ram}")

    # la scrivania: l'isola c'e' (l'ha gia' aspettata il collaudo; qui lo si dice)
    p.controlla(isola_visibile(p.scrivania), "l'isola della scrivania non si vede")
