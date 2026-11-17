"""vim: nel Terminale si apre vim, si scrive un file, lo si salva e lo si riapre.

  1. dal Terminale "cd /tmp" e poi "vim prova.txt" (TERM=xterm-256color glielo da' la
     shell): la finestra cambia (vim disegna il file vuoto, a schermo intero). /tmp e non la
     Home: la shell del Terminale e' del super amministratore (uid 0), che oggi non ha una
     Home (/data/users/0/home non c'e', ne' nel disco ne' nella sandbox); /tmp e' in
     memoria e scrivibile da tutti, nella sandbox e in QEMU. La shell parte da "/", che e'
     in sola lettura;
  2. "i", due righe di testo, Esc: il testo compare; con le frecce il cursore torna
     all'inizio della prima riga (dove vim lo mette quando apre un file);
  3. ":wq" e Invio: vim scrive il file (e la sua swap accanto al file) ed esce, la
     shell torna;
  4. di nuovo "vim prova.txt": le righe del file sono le stesse di prima del salvataggio
     (si confrontano le foto della parte alta della finestra, senza la riga di stato in
     fondo, che dice cose diverse);
  5. ":w /nessuna/x": vim non puo' scrivere (la cartella non c'e') e lo dice in fondo, in
     bianco su rosso (ErrorMsg): si controlla che lo sfondo sia il rosso pastello e il testo
     il colore del testo, cioe' che si legga; ":q" per uscire.
La tastiera e' italiana: ":" e' Maiuscole + punto, "/" e' Maiuscole + 7. Le foto restano nella cartella della prova
(con --tieni).
"""

import time

import scrivania as s
from qemu import PUNTO_VUOTO, pixel_diversi

NOME = "vim"
ORDINE = 46
DESCRIZIONE = "vim nel Terminale: scrive un file, lo salva e lo riapre uguale"

FINESTRA = (140, 130, 140 + 760, 130 + 588)


def _dentro(foto):
    """La parte del pannello dove sta il testo (senza il vetro della finestra)."""
    return foto.crop((148, 180, 892, 710))


def _righe(foto):
    """Le prime righe del testo (senza la riga di stato di vim, in fondo)."""
    return foto.crop((148, 180, 892, 300))


# sistema/rosso/pastello e testo/primario, nei due temi (uikit/interno/tema.c). In QEMU il tema
# e' sempre il chiaro; nella sandbox lo dice --tema.
ROSSO_PASTELLO = {"chiaro": (0xFF, 0xD4, 0xD1), "scuro": (0x60, 0x28, 0x26)}
TESTO_PRIMARIO = {"chiaro": (0x41, 0x41, 0x46), "scuro": (0xEB, 0xEB, 0xEC)}


def _riga(foto, r):
    """La riga r del testo (da 0 a 24): il testo comincia a 24 dal bordo della finestra, sotto
    la barra del titolo alta 40, e le righe sono da 20."""
    y = 130 + 40 + 24 + 20 * r
    return foto.crop((164, y, 164 + 712, y + 20))


def _quanti_vicini(foto, colore, tolleranza=24):
    """Quanti pixel sono di quel colore (ogni canale entro la tolleranza)."""
    r0, g0, b0 = colore
    return sum(1 for r, g, b in foto.convert("RGB").getdata()
               if abs(r - r0) <= tolleranza and abs(g - g0) <= tolleranza and abs(b - b0) <= tolleranza)


def _scrivi(m, testo):
    """Lettere minuscole, spazi, punto, ":" e "/" (tastiera italiana)."""
    for c in testo:
        if c in ":/":
            m.tasto_giu("shift")
            m.premi({":": "dot", "/": "7"}[c])
            m.tasto_su("shift")
        else:
            m.premi({" ": "spc", ".": "dot"}.get(c, c))
        time.sleep(0.03)


def esegui(p):
    m = p.m
    m.clic(*s.APP_PULSANTE)                    # il vassoio
    time.sleep(0.4)
    m.clic(*s.VASSOIO["Terminale"])
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: pixel_diversi(_dentro(f), _dentro(p.scrivania), None, 40) > 300,
                  20.0, cosa="il testo della shell nel Terminale")
    shell = m.aspetta_schermo_fermo()

    # 1. cd /tmp, vim prova.txt
    _scrivi(m, "cd /tmp")
    m.premi("ret")
    time.sleep(0.3)
    shell = m.aspetta_schermo_fermo()
    _scrivi(m, "vim prova.txt")
    m.premi("ret")
    m.aspetta_che(lambda f: pixel_diversi(_dentro(f), _dentro(shell), None, 40) > 500,
                  10.0, cosa="vim nel Terminale")
    vuoto = m.aspetta_schermo_fermo()
    vuoto.crop(FINESTRA).save(p.cartella / "vim_vuoto.png")

    # 2. due righe, Esc, frecce fino all'inizio
    m.premi("i")
    _scrivi(m, "ciao da vim")
    m.premi("ret")
    _scrivi(m, "seconda riga")
    m.premi("esc")
    time.sleep(0.3)
    m.premi("up")
    for _ in range(12):
        m.premi("left")
    time.sleep(0.3)
    scritto = m.aspetta_schermo_fermo()
    scritto.crop(FINESTRA).save(p.cartella / "vim_testo.png")
    p.controlla(pixel_diversi(_righe(scritto), _righe(vuoto), None, 40) > 200,
                "il testo scritto non si vede nella finestra")

    # 3. :wq
    _scrivi(m, ":wq")
    m.premi("ret")
    time.sleep(0.5)
    uscito = m.aspetta_schermo_fermo()
    uscito.crop(FINESTRA).save(p.cartella / "vim_uscito.png")

    # 4. riaperto: le righe sono le stesse
    _scrivi(m, "vim prova.txt")
    m.premi("ret")
    m.aspetta_che(lambda f: pixel_diversi(_dentro(f), _dentro(uscito), None, 40) > 500,
                  10.0, cosa="vim riaperto")
    riaperto = m.aspetta_schermo_fermo()
    riaperto.crop(FINESTRA).save(p.cartella / "vim_riaperto.png")
    diversi = pixel_diversi(_righe(riaperto), _righe(scritto), None, 40)
    p.controlla(diversi < 20, f"il file riaperto non ha le righe scritte ({diversi} pixel diversi)")

    # 5. un messaggio di errore: ":w" in una cartella che non c'e'. vim scrive "E212: ..." in
    #    fondo con l'evidenziazione ErrorMsg, bianco (15) su rosso (1): nel Terminale il rosso
    #    di sfondo e' il pastello del rosso, e sopra il colore del testo (terminale.c, mappa_ansi)
    _scrivi(m, ":w /nessuna/x")
    m.premi("ret")
    time.sleep(0.5)
    errore = m.aspetta_schermo_fermo()
    errore.crop(FINESTRA).save(p.cartella / "vim_errore.png")
    #    la riga del messaggio e' quella con piu' rosso pastello (vim la mette in fondo, sopra
    #    "Press ENTER"): ci devono essere lo sfondo e, sopra, le lettere nel colore del testo
    tema = getattr(m, "tema", "chiaro")
    righe = [_riga(errore, r) for r in range(25)]
    rosso = [_quanti_vicini(r, ROSSO_PASTELLO[tema]) for r in righe]
    r = rosso.index(max(rosso))
    lettere = _quanti_vicini(righe[r], TESTO_PRIMARIO[tema])
    p.controlla(rosso[r] > 1500, f"il messaggio di errore di vim non ha lo sfondo rosso pastello ({rosso[r]} pixel)")
    p.controlla(lettere > 150, f"il messaggio di errore di vim non ha il testo nel colore del testo ({lettere} pixel)")
    _scrivi(m, ":q")
    m.premi("ret")
    time.sleep(0.5)
    m.aspetta_schermo_fermo().crop(FINESTRA).save(p.cartella / "vim_fine.png")
    p.nota("foto in " + str(p.cartella))


DISCO = "disco_app.img"   # la userbar con Esempio: le posizioni di scrivania.py valgono per questo disco
