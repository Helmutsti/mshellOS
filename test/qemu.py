"""qemu.py: accendere mshell in QEMU senza finestra e "toccarlo" da fuori.

E' il pezzo su cui stanno tutte le prove. Fa tre cose:

  1. ACCENDE QEMU come run.sh / run.ps1 (stessi dispositivi: USB con tastiera e tavoletta,
     il disco, la scheda video virtio-gpu), ma con "-display none": lo schermo non si
     vede, si fotografa.
     Il disco (uno solo: la tabella GPT con le partizioni avvio, system e data) si COPIA
     prima in una cartella di lavoro: il sistema ci scrive (in data), e le prove non
     devono cambiare build/ (ne' litigare con un QEMU aperto che lo tiene).
     Sul PC (x86_64) parte prima il firmware UEFI (OVMF, il file edk2-x86_64-code.fd che
     viene con QEMU), che lancia la UKI (kernel Linux + archivio di avvio) dalla
     partizione avvio del disco. Su ARM64 QEMU carica da solo il kernel e l'archivio di
     avvio (-kernel, -initrd).

  2. PARLA CON QEMU attraverso QMP, il "monitor per i programmi" di QEMU: una presa TCP
     dove si mandano comandi in JSON, una riga per comando, e QEMU risponde con una riga.
         {"execute": "screendump", "arguments": {"filename": "foto.png", "format": "png"}}
         {"return": {}}
     Con QMP si fotografa lo schermo (screendump), si muove il mouse e si premono i tasti
     (input-send-event, come se arrivassero dalla tavoletta e dalla tastiera USB) e si
     manda qualsiasi comando del monitor "umano" (HMP, human-monitor-command), per
     esempio "info usb". Si apre anche il monitor HMP su una seconda porta: non lo usa
     nessuna prova, serve a chi vuole guardare dentro QEMU mentre una prova gira.

  3. LEGGE LA SERIALE: quello che il kernel Linux e i nostri programmi scrivono sulla
     seriale va in un file. Le prove ci cercano le righe dell'avvio ("eventi: tastiera
     ...") e controllano che non ci sia un "Kernel panic". Per sapere quando il sistema e'
     pronto si ASPETTA UNA CONDIZIONE (una riga nella seriale, lo schermo fermo) invece di
     dormire un tempo fisso: su un computer lento si aspetta di piu', su uno veloce di meno.

Lo schermo di mshell e' 1280x800. La tavoletta USB dice DOVE e' il puntatore con due
numeri da 0 a 32767 (tutto a sinistra .. tutto a destra): per andare al pixel x si manda
x * 32767 / 1280.
"""

import json
import math
import os
import platform
import shutil
import socket
import subprocess
import time
from pathlib import Path

from PIL import Image, ImageChops

LARGHEZZA, ALTEZZA = 1280, 800


class ErroreProva(Exception):
    """Una prova e' fallita: il messaggio dice perche'."""


# --- trovare QEMU -------------------------------------------------------------------

def trova_qemu(nome):
    """'qemu-system-i386' -> il percorso del programma. Prima il PATH (Mac, Linux, e
    Windows se QEMU e' nel PATH), poi la cartella dove lo mette l'installatore Windows.
    La variabile QEMU_DIR, se c'e', vince su tutto."""
    cartelle = []
    if os.environ.get("QEMU_DIR"):
        cartelle.append(Path(os.environ["QEMU_DIR"]))
    for c in cartelle:
        for n in (nome, nome + ".exe"):
            if (c / n).exists():
                return str(c / n)
    trovato = shutil.which(nome)
    if trovato:
        return trovato
    windows = Path(r"C:\Program Files\qemu") / (nome + ".exe")
    if windows.exists():
        return str(windows)
    raise ErroreProva(f"non trovo {nome}: installa QEMU o imposta QEMU_DIR")


def trova_firmware(qemu):
    """Il firmware UEFI del PC (OVMF): edk2-x86_64-code.fd, che viene con QEMU.
    La variabile OVMF (il percorso del file), se c'e', vince su tutto. Poi, vicino al
    programma di QEMU: Windows lo mette in <qemu>/share/, Homebrew e Linux in
    <prefisso>/share/qemu/ (con qemu in <prefisso>/bin/). Poi `brew --prefix qemu`
    e /usr/share/qemu/, come run.sh."""
    nome = "edk2-x86_64-code.fd"
    if os.environ.get("OVMF"):
        f = Path(os.environ["OVMF"])
        if f.exists():
            return str(f)
        raise ErroreProva(f"OVMF={f}: il file non c'e'")
    cartella = Path(qemu).resolve().parent
    candidati = [cartella / "share" / nome, cartella.parent / "share" / "qemu" / nome]
    try:
        brew = subprocess.run(["brew", "--prefix", "qemu"], capture_output=True, text=True,
                              timeout=10).stdout.strip()
        if brew:
            candidati.append(Path(brew) / "share" / "qemu" / nome)
    except (OSError, subprocess.TimeoutExpired):
        pass
    candidati.append(Path("/usr/share/qemu") / nome)
    for f in candidati:
        if f.exists():
            return str(f)
    raise ErroreProva(f"non trovo il firmware UEFI {nome} (cercato in "
                      f"{', '.join(str(c.parent) for c in candidati)}): imposta OVMF")


def porta_libera(porta):
    """Vero se su 127.0.0.1:porta non ascolta nessuno (un altro QEMU, un'altra prova)."""
    with socket.socket() as s:
        s.settimeout(0.3)
        return s.connect_ex(("127.0.0.1", porta)) != 0


# --- QMP ----------------------------------------------------------------------------

class Qmp:
    """Il collegamento QMP: una riga JSON per comando, una riga di risposta. In mezzo
    QEMU puo' mandare degli "eventi" (righe con "event"): si saltano."""

    def __init__(self, porta, attesa=20.0):
        fine = time.time() + attesa
        while True:
            try:
                self.presa = socket.create_connection(("127.0.0.1", porta), timeout=30)
                break
            except OSError:
                if time.time() > fine:
                    raise ErroreProva(f"QMP: nessuno risponde sulla porta {porta}")
                time.sleep(0.1)
        self.righe = self.presa.makefile("r", encoding="utf-8")
        self.righe.readline()                      # il saluto di QEMU
        self.comando("qmp_capabilities")          # "si comincia"

    def comando(self, nome, **argomenti):
        richiesta = {"execute": nome}
        if argomenti:
            richiesta["arguments"] = argomenti
        self.presa.sendall((json.dumps(richiesta) + "\n").encode())
        while True:
            riga = self.righe.readline()
            if not riga:
                raise ErroreProva(f"QMP: QEMU ha chiuso durante '{nome}'")
            risposta = json.loads(riga)
            if "event" in risposta:
                continue
            if "error" in risposta:
                raise ErroreProva(f"QMP '{nome}': {risposta['error'].get('desc')}")
            return risposta.get("return")

    def chiudi(self):
        try:
            self.presa.close()
        except OSError:
            pass


# --- la macchina ----------------------------------------------------------------------

class Macchina:
    """Un mshell acceso in QEMU, per una prova.

        m = Macchina("x86_64", build, lavoro, porta=4465, ram=2048)
        m.accendi()
        m.aspetta_scrivania()
        m.clic(510, 38)
        foto = m.foto("dopo_il_clic")
        m.spegni()
    """

    # La RAM di ARM64 e' fissa (512 MB, come run.sh); quella del PC si sceglie (--ram).
    RAM_ARM64 = 512

    def __init__(self, cpu, build, lavoro, porta, ram=2048, disco="disco.img"):
        self.cpu = cpu
        self.ram = self.RAM_ARM64 if cpu == "arm64" else ram     # in MB
        self.build = Path(build) / cpu      # build.sh compila in build/<cpu>/
        self.lavoro = Path(lavoro)          # qui vanno seriale, disco copiato, foto
        # il disco da copiare: disco.img, o disco_prove.img (con i programmi delle prove
        # in /data/tests, che init fa partire: vedi build.sh) se la prova lo chiede
        self.disco = disco
        self.porta_qmp = porta
        self.porta_hmp = porta + 1
        self.processo = None
        self.qmp = None
        self.seriale = self.lavoro / "seriale.txt"
        self.foto_fatte = 0
        self.ultimo_puntatore = (0, 0)

    # -- accendere e spegnere --

    def comando_qemu(self):
        """La riga di comando di QEMU, come in run.sh, ma senza finestra."""
        disco = self.lavoro / "disco.img"
        if self.cpu == "arm64":
            veloce = (platform.system() == "Darwin" and platform.machine() == "arm64")
            cpu_qemu = ["-accel", "hvf", "-cpu", "host"] if veloce else ["-cpu", "cortex-a72"]
            riga = (self.build / "riga_kernel.txt").read_text().strip()
            argomenti = [trova_qemu("qemu-system-aarch64"), "-M", "virt", *cpu_qemu,
                         "-m", str(self.ram), "-smp", "4",
                         "-kernel", str(self.build / "linux"),
                         "-initrd", str(self.build / "avvio.img"),
                         "-append", riga,
                         "-device", "virtio-gpu-pci,xres=1280,yres=800"]
        else:
            programma = trova_qemu("qemu-system-x86_64")
            firmware = trova_firmware(programma)
            argomenti = [programma, "-machine", "pc", "-m", str(self.ram), "-smp", "4",
                         "-drive", f"if=pflash,format=raw,readonly=on,file={firmware}",
                         "-vga", "none", "-device", "virtio-vga,xres=1280,yres=800"]
        # bootindex=1: il firmware UEFI del PC parte da questo disco (su ARM non conta)
        argomenti += ["-drive", f"if=none,id=disco,file={disco},format=raw",
                      "-device", "virtio-blk-pci,drive=disco,bootindex=1"]
        # Un'ora fissa (clock=vm: corre solo quando corre la macchina): le date dei file
        # e l'ora che le app mostrano sono sempre piu' o meno le stesse, prova dopo prova.
        return argomenti + [
            "-rtc", "base=2026-01-01T10:00:00,clock=vm",
            "-device", "qemu-xhci", "-device", "usb-kbd", "-device", "usb-tablet",
            "-display", "none",
            "-serial", f"file:{self.seriale}",
            "-qmp", f"tcp:127.0.0.1:{self.porta_qmp},server,nowait",
            "-monitor", f"tcp:127.0.0.1:{self.porta_hmp},server,nowait",
        ]

    def accendi(self):
        self.lavoro.mkdir(parents=True, exist_ok=True)
        origine = self.build / self.disco
        avvio = ["linux", "avvio.img", "riga_kernel.txt"] if self.cpu == "arm64" else []
        for f in [origine] + [self.build / n for n in avvio]:
            if not f.exists():
                raise ErroreProva(f"manca {f}: compila prima (--compila)")
        for p in (self.porta_qmp, self.porta_hmp):
            if not porta_libera(p):
                raise ErroreProva(f"la porta {p} e' gia' usata: scegline altre con --porte")
        shutil.copyfile(origine, self.lavoro / "disco.img")
        if self.seriale.exists():
            self.seriale.unlink()
        self.processo = subprocess.Popen(self.comando_qemu(), stdout=subprocess.DEVNULL,
                                         stderr=open(self.lavoro / "qemu_errori.txt", "w"))
        try:
            self.qmp = Qmp(self.porta_qmp)
        except ErroreProva:
            self.spegni()
            raise

    def spegni(self):
        """Spegne SOLO il QEMU di questa macchina: prima chiedendo (quit), poi, se non
        ascolta, chiudendo il processo che abbiamo aperto noi."""
        if self.qmp:
            try:
                self.qmp.comando("quit")
            except (ErroreProva, OSError):
                pass
            self.qmp.chiudi()
            self.qmp = None
        if self.processo:
            try:
                self.processo.wait(10)
            except subprocess.TimeoutExpired:
                self.processo.kill()
                self.processo.wait(5)
            self.processo = None

    def viva(self):
        return self.processo is not None and self.processo.poll() is None

    # -- la seriale --

    def testo_seriale(self):
        try:
            return self.seriale.read_text(encoding="latin-1", errors="replace")
        except FileNotFoundError:
            return ""

    def aspetta_seriale(self, testo, attesa=60.0):
        """Aspetta che 'testo' compaia nella seriale. Torna tutta la seriale."""
        fine = time.time() + attesa
        while time.time() < fine:
            s = self.testo_seriale()
            if testo in s:
                return s
            for panico in ("Kernel panic", "PANICO"):
                if panico in s:
                    raise ErroreProva(f"{panico} nella seriale:\n" + s[s.index(panico):][:400])
            if not self.viva():
                raise ErroreProva("QEMU si e' chiuso da solo")
            time.sleep(0.2)
        raise ErroreProva(f"dopo {attesa:.0f} s la seriale non dice ancora '{testo}'")

    # -- lo schermo --

    def foto(self, nome=None):
        """Fotografa lo schermo (screendump) e la torna come immagine RGB. Con un nome, la
        foto resta nella cartella di lavoro (per guardarla se la prova fallisce)."""
        self.foto_fatte += 1
        file = self.lavoro / (f"{self.foto_fatte:03d}_{nome}.png" if nome else "_ultima.png")
        self.qmp.comando("screendump", filename=str(file), format="png")
        with Image.open(file) as im:
            return im.convert("RGB")

    def aspetta_schermo_fermo(self, attesa=20.0, pausa=0.4, maschera=None):
        """Aspetta che due foto di fila siano uguali (fuori dall'orologio): le animazioni
        sono finite, le finestre disegnate. Torna l'ultima foto."""
        fine = time.time() + attesa
        prima = self.foto()
        while time.time() < fine:
            time.sleep(pausa)
            ora = self.foto()
            if pixel_diversi(prima, ora, maschera) == 0:
                return ora
            prima = ora
        raise ErroreProva(f"dopo {attesa:.0f} s lo schermo cambia ancora")

    def aspetta_che(self, condizione, attesa=10.0, pausa=0.25, cosa="la condizione"):
        """Fotografa finche' condizione(foto) e' vera. Torna la foto."""
        fine = time.time() + attesa
        while True:
            f = self.foto()
            if condizione(f):
                return f
            if time.time() > fine:
                raise ErroreProva(f"dopo {attesa:.0f} s non si vede {cosa}")
            time.sleep(pausa)

    def aspetta_scrivania(self, attesa=120.0):
        """Aspetta il desktop: le partizioni montate e la tavoletta USB presa dal servizio
        eventi (le righe nella seriale), poi la userbar disegnata a sinistra e lo schermo fermo.
        Il puntatore va in un angolo, dove non copre niente. Sul PC l'attesa comprende il
        firmware UEFI, che parte prima del kernel.
        La tavoletta si aspetta perche' Linux prepara l'USB mentre il sistema parte: un
        movimento mandato prima che la tavoletta sia pronta si perde (QEMU svuota la sua
        coda quando Linux la "resetta", come fa con ogni dispositivo USB nuovo)."""
        self.aspetta_seriale("avvio: /data montato", attesa)
        self.aspetta_seriale('eventi: tavoletta', attesa)
        self.muovi(*PUNTO_VUOTO)
        from scrivania import userbar_visibile
        self.aspetta_che(userbar_visibile, attesa, 0.5, "la userbar della scrivania")
        return self.aspetta_schermo_fermo()

    # -- il mouse (la tavoletta USB: posizioni assolute) --

    def _eventi(self, eventi):
        self.qmp.comando("input-send-event", events=eventi)

    def muovi(self, x, y):
        x = min(max(int(round(x)), 0), LARGHEZZA - 1)
        y = min(max(int(round(y)), 0), ALTEZZA - 1)
        self._eventi([
            {"type": "abs", "data": {"axis": "x", "value": x * 32767 // (LARGHEZZA - 1)}},
            {"type": "abs", "data": {"axis": "y", "value": y * 32767 // (ALTEZZA - 1)}}])
        self.ultimo_puntatore = (x, y)

    def pulsante(self, giu, quale="left"):
        self._eventi([{"type": "btn", "data": {"down": giu, "button": quale}}])

    def rotella(self, scatti, pausa=0.05):
        """Gira la rotella di 'scatti' (positivi: in basso, verso di se'), uno alla volta.
        Per QEMU la rotella e' fatta di due "pulsanti", wheel-down e wheel-up: premuto e
        lasciato e' uno scatto. La tavoletta USB lo manda nel campo Wheel del rapporto."""
        quale = "wheel-down" if scatti > 0 else "wheel-up"
        for _ in range(abs(scatti)):
            self.pulsante(True, quale)
            time.sleep(pausa)
            self.pulsante(False, quale)
            time.sleep(pausa)

    def clic(self, x, y, pausa=0.15):
        """Va in (x, y), preme e lascia. Le pause lasciano al kernel il tempo di vedere
        il movimento e il clic come due rapporti USB separati."""
        self.muovi(x, y)
        time.sleep(pausa)
        self.pulsante(True)
        time.sleep(pausa)
        self.pulsante(False)
        time.sleep(pausa)

    def trascina(self, x1, y1, x2, y2, passi=25, pausa=0.03):
        """Preme in (x1, y1), si sposta a (x2, y2) in tanti piccoli passi, lascia."""
        self.muovi(x1, y1)
        time.sleep(0.2)
        self.pulsante(True)
        time.sleep(0.2)
        for i in range(1, passi + 1):
            self.muovi(x1 + (x2 - x1) * i / passi, y1 + (y2 - y1) * i / passi)
            time.sleep(pausa)
        time.sleep(0.2)
        self.pulsante(False)
        time.sleep(0.2)

    def cerchio(self, cx, cy, raggio, secondi, giro=2.0, ogni=1 / 120, ogni_tanto=None):
        """Fa girare il puntatore in tondo per 'secondi' (un giro ogni 'giro' secondi),
        mandando una posizione ogni 'ogni' secondi. Se c'e' ogni_tanto=(intervallo, f),
        chiama f() ogni 'intervallo' secondi (per fotografare durante il giro).
        Torna quante posizioni ha mandato."""
        inizio = time.time()
        prossima = inizio + (ogni_tanto[0] if ogni_tanto else 1e9)
        mandate = 0
        while (t := time.time() - inizio) < secondi:
            a = t * 2 * math.pi / giro
            self.muovi(cx + raggio * math.cos(a), cy + raggio * math.sin(a))
            mandate += 1
            if time.time() >= prossima:
                ogni_tanto[1]()
                prossima = time.time() + ogni_tanto[0]
            time.sleep(ogni)
        return mandate

    # -- la tastiera (USB: i tasti si chiamano con i "qcode" di QEMU: "a", "ret", "esc") --

    def tasto_giu(self, tasto):
        self._eventi([{"type": "key", "data": {"down": True, "key": {"type": "qcode", "data": tasto}}}])

    def tasto_su(self, tasto):
        self._eventi([{"type": "key", "data": {"down": False, "key": {"type": "qcode", "data": tasto}}}])

    def premi(self, tasto, tieni=0.08):
        self.tasto_giu(tasto)
        time.sleep(tieni)
        self.tasto_su(tasto)

    # -- il monitor umano (HMP) attraverso QMP --

    def hmp(self, comando):
        return self.qmp.comando("human-monitor-command", **{"command-line": comando})


# --- guardare le foto ---------------------------------------------------------------

# Un punto dove il puntatore non copre niente: in basso a destra, sullo sfondo.
PUNTO_VUOTO = (1270, 790)


def pixel_diversi(a, b, maschera=None, soglia=0):
    """Quanti pixel cambiano fra due foto (fuori dalla maschera, un rettangolo o una
    lista di rettangoli). 'soglia' ignora le differenze piccole (sfumature)."""
    d = ImageChops.difference(a, b).convert("L")
    if maschera:
        for r in (maschera if isinstance(maschera[0], tuple) else [maschera]):
            d.paste(0, r)
    if soglia:
        d = d.point(lambda v: 255 if v > soglia else 0)
    else:
        d = d.point(lambda v: 255 if v else 0)
    return d.histogram()[255]


def riquadro_diverso(a, b, maschera=None, soglia=0):
    """Il rettangolo (x1, y1, x2, y2) che contiene tutti i pixel cambiati, o None."""
    d = ImageChops.difference(a, b).convert("L")
    if maschera:
        for r in (maschera if isinstance(maschera[0], tuple) else [maschera]):
            d.paste(0, r)
    d = d.point(lambda v: 255 if v > soglia else 0)
    return d.getbbox()


def riquadro_colore(foto, colore, area=None, tolleranza=0):
    """Il rettangolo che contiene tutti i pixel di quel colore (0xRRGGBB), o None."""
    if area:
        ritaglio = foto.crop(area)
        dx, dy = area[0], area[1]
    else:
        ritaglio, dx, dy = foto, 0, 0
    r, g, b = (colore >> 16) & 255, (colore >> 8) & 255, colore & 255
    canali = ritaglio.split()
    maschere = [c.point(lambda v, k=k: 255 if abs(v - k) <= tolleranza else 0)
                for c, k in zip(canali, (r, g, b))]
    m = ImageChops.multiply(ImageChops.multiply(maschere[0], maschere[1]), maschere[2])
    box = m.getbbox()
    if not box:
        return None
    return (box[0] + dx, box[1] + dy, box[2] + dx, box[3] + dy)


def colore_in(foto, x, y):
    r, g, b = foto.getpixel((x, y))
    return (r << 16) | (g << 8) | b
