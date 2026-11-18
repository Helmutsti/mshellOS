"""blocconote_modifiche: le modifiche non salvate del Blocco note (utente/applicazioni/BloccoNote).

Senza il picker (i file veri sono nella prova blocconote_file, solo in QEMU). Si guarda la finestra:
  1. documento nuovo: il titolo non ha l'asterisco;
  2. si scrive: il titolo prende l'asterisco ("Senza titolo* - Blocco note");
  3. Nuovo con modifiche non salvate: compare l'avviso "Salvare le modifiche?";
  4. Esc lo chiude e il testo resta (Esc e' Annulla, non Non salvare);
  5. Nuovo e poi Annulla: il testo resta, l'asterisco anche;
  6. Nuovo e poi Non salvare: il documento e' vuoto (di nuovo il segnaposto) e l'asterisco e' sparito.
Le foto restano nella cartella della prova (con --tieni).
"""

import scrivania as s
from qemu import pixel_diversi

NOME = "blocconote_modifiche"
ORDINE = 47
DESCRIZIONE = "il Blocco note: asterisco nel titolo, avviso Salvare le modifiche? (Esc, Annulla, Non salvare)"

FINESTRA = (140, 130, 140 + 760, 130 + 588)
PANNELLO = (148, 218, 148 + 744, 218 + 492)          # l'area di testo (il pannello)


def scrivi(m, testo):
    """Scrive 'testo' (solo lettere minuscole e spazi) un tasto alla volta."""
    for c in testo:
        m.premi("spc" if c == " " else c, 0.03)


TITOLO = (FINESTRA[0] + 200, FINESTRA[1] + 4, FINESTRA[0] + 560, FINESTRA[1] + 28)   # il testo del titolo
CENTRO = (FINESTRA[0] + FINESTRA[2]) // 2, (FINESTRA[1] + FINESTRA[3]) // 2
NUOVO = (164, 194)                                    # il primo pulsante della barra (32 x 32, gap 8)
# l'avviso a tre scelte (480 x 160) e' al centro della finestra; i pulsanti stanno a 24 dai bordi
AVVISO = (CENTRO[0] - 240, CENTRO[1] - 80, CENTRO[0] + 240, CENTRO[1] + 80)
NON_SALVARE = (AVVISO[0] + 200, AVVISO[3] - 44)
ANNULLA = (AVVISO[0] + 303, AVVISO[3] - 44)


def esegui(p):
    m = p.m
    s.apri(m, "BloccoNote")
    m.muovi(1270, 790)

    def scatta(nome):
        f = m.aspetta_schermo_fermo()
        f.crop(FINESTRA).save(p.cartella / f"blocconote_{nome}.png")
        return f

    nuovo = scatta("nuovo")
    scrivi(m, "ciao")
    scritto = scatta("scritto")
    p.controlla(pixel_diversi(scritto.crop(TITOLO), nuovo.crop(TITOLO), None, 40) > 5,
                "scrivendo il titolo non e' cambiato (manca l'asterisco)")

    m.clic(*NUOVO)
    m.muovi(1270, 790)
    avviso = scatta("avviso")
    p.controlla(pixel_diversi(avviso.crop(AVVISO), scritto.crop(AVVISO), None, 40) > 2000,
                "Nuovo con modifiche non salvate non ha mostrato l'avviso")

    m.premi("esc")
    m.muovi(1270, 790)
    dopo_esc = scatta("dopo_esc")
    p.controlla(pixel_diversi(dopo_esc.crop(PANNELLO), scritto.crop(PANNELLO), None, 40) < 50,
                "Esc sull'avviso ha cambiato il testo")
    p.controlla(pixel_diversi(dopo_esc.crop(AVVISO), scritto.crop(AVVISO), None, 40) < 300,
                "Esc non ha chiuso l'avviso")

    m.clic(*NUOVO)
    m.clic(*ANNULLA)
    m.muovi(1270, 790)
    dopo_annulla = scatta("dopo_annulla")
    p.controlla(pixel_diversi(dopo_annulla, scritto, None, 40) < 300, "Annulla non ha lasciato tutto com'era")

    m.clic(*NUOVO)
    m.clic(*NON_SALVARE)
    m.muovi(1270, 790)
    vuoto = scatta("dopo_non_salvare")
    p.controlla(pixel_diversi(vuoto, nuovo, None, 40) < 300,
                "Non salvare non ha dato un documento vuoto e senza asterisco")
