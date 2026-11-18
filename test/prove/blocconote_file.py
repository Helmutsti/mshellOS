"""blocconote_file: i file del Blocco note, con il picker vero (la finestra Apri/Salva di UIKit).

Solo in QEMU: il picker vero (init lo avvia, il compositore mostra la sua finestra sopra l'app).
  1. si scrive "ciao mondo" e si preme Salva con nome: la finestra Salva propone "Senza titolo.txt";
     si preme Salva: il titolo diventa "Senza titolo.txt - Blocco note", senza asterisco;
  2. Nuovo: documento vuoto; poi Apri, si sceglie Senza titolo.txt: il testo "ciao mondo" torna.
"""

import time

import scrivania as s
from qemu import pixel_diversi

NOME = "blocconote_file"
ORDINE = 48
DESCRIZIONE = "il Blocco note con il picker vero: Salva con nome, Nuovo, Apri"
DISCO = "disco_app.img"   # la userbar con Esempio, come la conoscono le prove (scrivania.py)
SOLO_QEMU = "il picker vero (la finestra Apri/Salva) gira solo in QEMU"

FINESTRA = (140, 130, 140 + 760, 130 + 588)
PANNELLO = (148, 218, 148 + 744, 218 + 492)
TITOLO = (FINESTRA[0] + 200, FINESTRA[1] + 4, FINESTRA[0] + 560, FINESTRA[1] + 28)


def scrivi(m, testo):
    for c in testo:
        m.premi("spc" if c == " " else c, 0.03)


def doppio_clic(m, x, y):
    m.muovi(x, y)
    time.sleep(0.15)
    for _ in range(2):
        m.pulsante(True)
        time.sleep(0.05)
        m.pulsante(False)
        time.sleep(0.05)


def esegui(p):
    m = p.m
    s.apri(m, "BloccoNote")
    m.muovi(1270, 790)

    def scatta(nome):
        f = m.aspetta_schermo_fermo()
        f.save(p.cartella / f"blocconote_file_{nome}.png")
        return f

    scritto = scatta("0_nuovo")
    scrivi(m, "ciao mondo")
    scritto = scatta("1_scritto")
    m.clic(284, 194)                         # Salva con nome
    time.sleep(1.0)
    scatta("2_salva")
    doppio_clic(m, 380, 435)                 # entra in Documenti
    time.sleep(0.8)
    scatta("3_documenti")
    m.clic(715, 627)                         # Salva
    m.muovi(1270, 790)
    time.sleep(1.0)
    salvato = scatta("4_salvato")
    p.controlla(pixel_diversi(salvato.crop(TITOLO), scritto.crop(TITOLO), None, 40) > 5,
                "dopo Salva con nome il titolo e' lo stesso di prima (con l'asterisco)")
    m.clic(164, 194)                         # Nuovo (niente modifiche: nessuna domanda)
    m.muovi(1270, 790)
    time.sleep(0.5)
    scatta("5_nuovo")
    m.clic(204, 194)                         # Apri
    time.sleep(1.0)
    scatta("6_apri")
    doppio_clic(m, 364, 412)                 # Documenti
    time.sleep(0.8)
    scatta("7_apri_documenti")
    m.clic(380, 499)                         # Senza titolo.txt (dopo "una cartella...", appunti, elenco)
    time.sleep(0.3)
    m.clic(720, 604)                         # Apri
    m.muovi(1270, 790)
    time.sleep(1.0)
    aperto = scatta("8_aperto")
    p.controlla(pixel_diversi(aperto.crop(PANNELLO), scritto.crop(PANNELLO), None, 40) < 150,    # il cursore e' all'inizio, non in fondo
                "il testo aperto non e' quello salvato (ciao mondo)")
    p.controlla(pixel_diversi(aperto.crop(TITOLO), salvato.crop(TITOLO), None, 40) < 5,
                "dopo Apri il titolo non e' quello del file, senza asterisco")

    # modifiche non salvate su un documento NUOVO: Nuovo, Salva dall'avviso apre la finestra Salva
    m.clic(164, 194)                         # Nuovo: il fuoco torna all'area, si scrive subito
    time.sleep(0.5)
    scrivi(m, "altro")
    prima = scatta("8b_altro")
    m.clic(164, 194)                         # Nuovo con modifiche: avviso
    m.muovi(1270, 790)
    time.sleep(0.5)
    scatta("9_avviso")
    m.clic(702, 460)                         # Salva dell'avviso
    time.sleep(1.0)
    finestra_salva = scatta("10_salva_da_avviso")
    m.clic(645, 627)                         # Annulla del picker: tutto com'era
    m.muovi(1270, 790)
    time.sleep(0.8)
    annullato = scatta("11_annullato")
    p.controlla(pixel_diversi(annullato.crop(PANNELLO), prima.crop(PANNELLO), None, 40) < 150,
                "Annulla nella finestra Salva ha cambiato il testo")
    p.controlla(pixel_diversi(annullato.crop(TITOLO), prima.crop(TITOLO), None, 40) < 5,
                "Annulla nella finestra Salva ha cambiato il titolo")
