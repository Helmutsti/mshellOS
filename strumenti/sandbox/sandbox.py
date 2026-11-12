# /// script
# requires-python = ">=3.10"
# dependencies = ["pillow"]
# ///
"""sandbox.py: mshell vero in un container Docker, senza QEMU, per prove che durano secondi.

    uv run strumenti/sandbox/sandbox.py compila                 compila (solo cio' che e' cambiato)
    uv run strumenti/sandbox/sandbox.py foto                    la scrivania in un PNG
    uv run strumenti/sandbox/sandbox.py foto files              apre Files e lo fotografa
    uv run strumenti/sandbox/sandbox.py foto attivita --tema scuro --uscita a.png
    uv run strumenti/sandbox/sandbox.py prova files mouse       prove di test/prove/ nella sandbox

COM'E' FATTA. Nel container girano i programmi VERI di mshell (init, compositore, launcher,
scrivania, le app), compilati con il compilatore e gli argomenti di build.sh. Cambiano
solo i capi "di hardware" (strumenti/sandbox/sandbox.h): lo schermo e' un file in memoria
(schermo_sandbox.c al posto del DRM) e eventi riceve tastiera e mouse da una presa TCP
(eventi_sandbox.c, con il codice di eventi.c). Il container ha "/" in sola lettura (il
sistema), /data scrivibile (un volume che sparisce alla fine), /run e /tmp in memoria.

LE IMMAGINI DOCKER
    mshell-sandbox-attrezzi   alpine con clang, lld, make, zip e la radice di musl (come
                              build.sh, con la stessa cartella di pacchetti build/scarico):
                              si fa una volta sola
    mshell-sandbox            il sistema: FROM scratch, con dentro solo l'albero di
                              build/sandbox/radice.tar (fatto da compila.sh)
    mshell-sandbox-oggetti    un volume con gli oggetti compilati (per ricompilare poco)

PER LE PROVE. La classe Sandbox ha gli stessi metodi di Macchina in test/qemu.py
(accendi, aspetta_scrivania, foto, aspetta_schermo_fermo, aspetta_che, muovi, pulsante,
rotella, clic, trascina, cerchio, tasto_giu, tasto_su, premi, testo_seriale,
aspetta_seriale, spegni): le prove di test/prove/ girano uguali (comando "prova").

    sb = Sandbox(tema="chiaro")
    sb.accendi()
    sb.aspetta_scrivania()
    sb.apri("files")
    sb.foto().save("files.png")
    sb.spegni()
"""

import argparse
import importlib.util
import math
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from PIL import Image

QUI = Path(__file__).resolve().parent            # strumenti/sandbox
REPO = QUI.parent.parent
TEST = REPO / "test"
sys.path.insert(0, str(TEST))                    # qemu.py e scrivania.py: le stesse misure

import qemu                                      # noqa: E402
from qemu import ErroreProva, PUNTO_VUOTO, pixel_diversi  # noqa: E402

LARGHEZZA, ALTEZZA = 1280, 800
IMMAGINE = "mshell-sandbox"
ATTREZZI = "mshell-sandbox-attrezzi"
OGGETTI = __import__("os").environ.get("SANDBOX_OGGETTI", "mshell-sandbox-oggetti")
PORTA = 7000                                     # SANDBOX_PORTA in sandbox.h
COSTRUITO = REPO / "build" / "sandbox"
# i dischi delle prove di build.sh, e cosa vuol dire per la sandbox (avvio_sandbox.c)
DISCHI = {"disco.img": "nessuna", "disco_prove.img": "riempitivi", "disco_pacchetti.img": "pacchetti"}

# I codici dei tasti di Linux (KEY_*, linux/input-event-codes.h) per i nomi dei tasti di
# QEMU (i "qcode" che usano le prove: "a", "ret", "shift"...).
TASTI = {"esc": 1, "minus": 12, "equal": 13, "backspace": 14, "tab": 15,
         "bracket_left": 26, "bracket_right": 27, "ret": 28, "ctrl": 29, "semicolon": 39,
         "apostrophe": 40, "grave_accent": 41, "shift": 42, "backslash": 43, "comma": 51,
         "dot": 52, "slash": 53, "shift_r": 54, "kp_multiply": 55, "alt": 56, "spc": 57,
         "caps_lock": 58, "less": 86, "ctrl_r": 97, "alt_r": 100, "home": 102, "up": 103,
         "pgup": 104, "left": 105, "right": 106, "end": 107, "down": 108, "pgdn": 109,
         "insert": 110, "delete": 111, "meta_l": 125, "meta_r": 126, "f11": 87, "f12": 88}
for _i, _c in enumerate("1234567890"):
    TASTI[_c] = 2 + _i
for _riga, _inizio in (("qwertyuiop", 16), ("asdfghjkl", 30), ("zxcvbnm", 44)):
    for _i, _c in enumerate(_riga):
        TASTI[_c] = _inizio + _i
for _i in range(10):
    TASTI[f"f{_i + 1}"] = 59 + _i

EV_SYN, EV_KEY, EV_REL, EV_ABS = 0, 1, 2, 3
ABS_X, ABS_Y, REL_WHEEL, SYN_REPORT = 0, 1, 8, 0
PULSANTI = {"left": 0x110, "right": 0x111, "middle": 0x112}
TASTIERA, TAVOLETTA = 0, 1

# le applicazioni della userbar: id (come nei manifesti) -> nome in test/scrivania.py
APP = {"attivita": "Attivita", "doom": "Doom", "esempio": "Esempio", "files": "Files",
       "impostazioni": "Impostazioni", "store": "Store"}


def docker(*argomenti, controlla=True, **altro):
    r = subprocess.run(["docker", *argomenti], capture_output=True, text=True, **altro)
    if controlla and r.returncode:
        raise ErroreProva(f"docker {argomenti[0]}: {r.stderr.strip() or r.stdout.strip()}")
    return r


def c_e_immagine(nome):
    return docker("image", "inspect", nome, controlla=False).returncode == 0


# --- compilare ---------------------------------------------------------------------------

def prepara_attrezzi():
    """L'immagine con il compilatore, fatta una volta: gli stessi pacchetti di build.sh
    (passo 0), presi dalla stessa cartella build/scarico."""
    if c_e_immagine(ATTREZZI):
        return
    print("sandbox: preparo gli attrezzi (una volta sola) ...", flush=True)
    nome = f"{ATTREZZI}-{os.getpid()}"
    comando = ("apk add --cache-dir /w/build/scarico/host clang lld make zip tar >/dev/null && "
               "apk --arch x86_64 --root /radice --initdb --no-scripts "
               "--repositories-file /etc/apk/repositories --keys-dir /usr/share/apk/keys/x86_64 "
               "--cache-dir /w/build/scarico/x86_64 add musl-dev linux-headers compiler-rt >/dev/null")
    (REPO / "build" / "scarico" / "host").mkdir(parents=True, exist_ok=True)
    (REPO / "build" / "scarico" / "x86_64").mkdir(parents=True, exist_ok=True)
    r = subprocess.run(["docker", "run", "--name", nome, "--platform", "linux/amd64",
                        "-v", f"{REPO}:/w", "alpine:3.24", "sh", "-c", comando])
    try:
        if r.returncode:
            raise ErroreProva("non riesco a preparare gli attrezzi (serve la rete la prima volta)")
        docker("commit", nome, ATTREZZI)
    finally:
        docker("rm", "-f", nome, controlla=False)


def compila():
    """Compila cio' che e' cambiato (compila.sh, nel container degli attrezzi) e rifa'
    l'immagine del sistema. Torna i secondi."""
    inizio = time.time()
    prepara_attrezzi()
    r = subprocess.run(["docker", "run", "--rm", "--platform", "linux/amd64",
                        "-v", f"{REPO}:/w", "-v", f"{OGGETTI}:/o", ATTREZZI,
                        "sh", "/w/strumenti/sandbox/compila.sh"])
    if r.returncode:
        raise ErroreProva("la compilazione della sandbox non e' riuscita")
    docker("build", "-q", "-t", IMMAGINE, str(COSTRUITO))
    return time.time() - inizio


def da_ricompilare():
    """Vero se un sorgente (utente/, file/, strumenti/sandbox/) e' piu' nuovo del sistema
    compilato, o se l'immagine non c'e'."""
    tar = COSTRUITO / "radice.tar"
    if not tar.exists() or not c_e_immagine(IMMAGINE):
        return True
    fatto = tar.stat().st_mtime
    for cartella in (REPO / "utente", REPO / "file", QUI):
        for radice, _, file in os.walk(cartella):
            for f in file:
                if os.path.getmtime(os.path.join(radice, f)) > fatto:
                    return True
    return False


# --- la sandbox accesa -------------------------------------------------------------------

class Sandbox:
    """Un mshell acceso in un container. Gli stessi metodi di qemu.Macchina."""

    def __init__(self, tema="chiaro", disco="disco.img", lavoro=None, pausa=0.05):
        if disco not in DISCHI:
            raise ErroreProva(f"disco sconosciuto: {disco}")
        self.tema = tema
        self.disco = disco
        self.lavoro = Path(lavoro) if lavoro else Path(tempfile.mkdtemp(prefix="mshell-sandbox-"))
        self.pausa = pausa            # tra muovere e premere (in QEMU 0,15 s: qui basta poco)
        self.nome = f"mshell-sandbox-{os.getpid()}-{int(time.time() * 1000) % 100000}"
        self.processo = None
        self.presa = None
        self.seriale = self.lavoro / "seriale.txt"
        self.foto_fatte = 0
        self.ultimo_puntatore = (0, 0)
        self.ram = 0                  # la RAM e' quella del computer: le prove sulla memoria non valgono
        self.cpu = "x86_64"

    # -- accendere e spegnere --

    def accendi(self, attesa=20.0):
        if not c_e_immagine(IMMAGINE):
            raise ErroreProva(f"manca l'immagine {IMMAGINE}: prima 'sandbox.py compila'")
        self.lavoro.mkdir(parents=True, exist_ok=True)
        # SYS_PTRACE: il compositore legge /proc/<numero>/exe delle app (programma_di, per
        # sapere di chi e' una finestra); per un processo di un altro uid Linux lo permette
        # solo con questo permesso, che il root vero ha e Docker toglie.
        comando = ["docker", "run", "--rm", "--name", self.nome, "--platform", "linux/amd64",
                   "--cap-add", "SYS_PTRACE", "--read-only", "--tmpfs", "/run:rw,nosuid,nodev,mode=755",
                   "--tmpfs", "/tmp:rw,nosuid,nodev,mode=1777", "-v", "/data",
                   "-p", f"127.0.0.1::{PORTA}", IMMAGINE, self.tema, DISCHI[self.disco]]
        self.processo = subprocess.Popen(comando, stdout=open(self.seriale, "wb"),
                                         stderr=subprocess.STDOUT)
        fine = time.time() + attesa
        porta = None
        while porta is None:
            if self.processo.poll() is not None:
                raise ErroreProva("il container si e' chiuso subito:\n" + self.testo_seriale())
            r = docker("port", self.nome, str(PORTA), controlla=False)
            if r.returncode == 0 and r.stdout.strip():
                porta = int(r.stdout.split()[0].rsplit(":", 1)[1])
            elif time.time() > fine:
                raise ErroreProva("il container non risponde")
            else:
                time.sleep(0.05)
        while True:
            try:
                self.presa = socket.create_connection(("127.0.0.1", porta), timeout=30)
                self.presa.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                self.sincronizza()
                return
            except (OSError, ErroreProva):
                if self.presa:
                    self.presa.close()
                    self.presa = None
                if time.time() > fine or self.processo.poll() is not None:
                    self.spegni()
                    raise ErroreProva("eventi della sandbox non risponde:\n" + self.testo_seriale())
                time.sleep(0.05)

    def spegni(self):
        if self.presa:
            try:
                self.presa.close()
            except OSError:
                pass
            self.presa = None
        if self.processo:
            docker("rm", "-f", self.nome, controlla=False)
            try:
                self.processo.wait(10)
            except subprocess.TimeoutExpired:
                self.processo.kill()
            self.processo = None

    def viva(self):
        return self.processo is not None and self.processo.poll() is None

    def __enter__(self):
        self.accendi()
        return self

    def __exit__(self, *_):
        self.spegni()

    # -- la presa --

    def _ricevi(self, byte):
        dati = bytearray()
        while len(dati) < byte:
            pezzo = self.presa.recv(min(byte - len(dati), 1 << 20))
            if not pezzo:
                raise ErroreProva("eventi della sandbox ha chiuso la presa")
            dati += pezzo
        return bytes(dati)

    def _eventi(self, quale, eventi):
        """Manda eventi di Linux (tipo, codice, valore) al dispositivo, con SYN_REPORT in fondo."""
        dati = b"".join(struct.pack("<cBHHi", b"e", quale, t, c, v)
                        for t, c, v in [*eventi, (EV_SYN, SYN_REPORT, 0)])
        self.presa.sendall(dati)

    def sincronizza(self):
        """Aspetta che tutto quello che si e' mandato sia arrivato al compositore."""
        self.presa.sendall(b"p")
        if self._ricevi(1) != b"P":
            raise ErroreProva("eventi della sandbox risponde male")

    # -- la seriale (quello che i programmi scrivono nel registro) --

    def testo_seriale(self):
        try:
            return self.seriale.read_text(encoding="latin-1", errors="replace")
        except FileNotFoundError:
            return ""

    def aspetta_seriale(self, testo, attesa=20.0):
        fine = time.time() + attesa
        while time.time() < fine:
            s = self.testo_seriale()
            if testo in s:
                return s
            if not self.viva():
                raise ErroreProva("il container si e' chiuso da solo")
            time.sleep(0.05)
        raise ErroreProva(f"dopo {attesa:.0f} s il registro non dice ancora '{testo}'")

    # -- lo schermo --

    def foto(self, nome=None):
        """La foto dello schermo (RGB). Con un nome resta anche nella cartella di lavoro."""
        self.presa.sendall(b"f")
        testa = self._ricevi(9)
        larghezza, altezza = struct.unpack("<II", testa[1:])
        if not larghezza:
            immagine = Image.new("RGB", (LARGHEZZA, ALTEZZA))
        else:
            pixel = self._ricevi(larghezza * altezza * 4)
            immagine = Image.frombuffer("RGB", (larghezza, altezza), pixel, "raw", "BGRX", 0, 1)
        self.foto_fatte += 1
        if nome:
            immagine.save(self.lavoro / f"{self.foto_fatte:03d}_{nome}.png")
        return immagine

    def aspetta_schermo_fermo(self, attesa=20.0, pausa=0.25, maschera=None):
        fine = time.time() + attesa
        prima = self.foto()
        while time.time() < fine:
            time.sleep(pausa)
            ora = self.foto()
            if pixel_diversi(prima, ora, maschera) == 0:
                return ora
            prima = ora
        raise ErroreProva(f"dopo {attesa:.0f} s lo schermo cambia ancora")

    def aspetta_che(self, condizione, attesa=10.0, pausa=0.1, cosa="la condizione"):
        fine = time.time() + attesa
        while True:
            f = self.foto()
            if condizione(f):
                return f
            if time.time() > fine:
                raise ErroreProva(f"dopo {attesa:.0f} s non si vede {cosa}")
            time.sleep(pausa)

    def aspetta_scrivania(self, attesa=20.0):
        self.aspetta_seriale("eventi: tavoletta", attesa)
        self.muovi(*PUNTO_VUOTO)
        from scrivania import userbar_visibile
        self.aspetta_che(userbar_visibile, attesa, 0.1, "la userbar della scrivania")
        return self.aspetta_schermo_fermo()

    def apri(self, app, attesa=15.0):
        """Apre un'applicazione dalla userbar (per id: "files", "attivita"...) e aspetta che
        lo schermo si fermi. Torna la foto."""
        import scrivania
        nome = APP.get(app.lower(), app)
        if nome not in scrivania.APP:
            raise ErroreProva(f"'{app}' non e' nella userbar ({', '.join(APP)})")
        return scrivania.apri(self, nome, attesa)

    # -- il mouse (la tavoletta: posizioni da 0 a 32767, come la usb-tablet di QEMU) --

    def muovi(self, x, y):
        x = min(max(int(round(x)), 0), LARGHEZZA - 1)
        y = min(max(int(round(y)), 0), ALTEZZA - 1)
        self._eventi(TAVOLETTA, [(EV_ABS, ABS_X, x * 32767 // (LARGHEZZA - 1)),
                                 (EV_ABS, ABS_Y, y * 32767 // (ALTEZZA - 1))])
        self.ultimo_puntatore = (x, y)

    def pulsante(self, giu, quale="left"):
        """Come in qemu.py: "left", "right", "middle"; "wheel-up" e "wheel-down" sono uno
        scatto della rotella quando si "premono"."""
        if quale in ("wheel-up", "wheel-down"):
            if giu:
                self._eventi(TAVOLETTA, [(EV_REL, REL_WHEEL, 1 if quale == "wheel-up" else -1)])
            return
        self._eventi(TAVOLETTA, [(EV_KEY, PULSANTI[quale], 1 if giu else 0)])

    def rotella(self, scatti, pausa=0.05):
        quale = "wheel-down" if scatti > 0 else "wheel-up"
        for _ in range(abs(scatti)):
            self.pulsante(True, quale)
            time.sleep(pausa)
            self.pulsante(False, quale)
            time.sleep(pausa)

    def clic(self, x, y, pausa=None):
        pausa = self.pausa if pausa is None else pausa
        self.muovi(x, y)
        time.sleep(pausa)
        self.pulsante(True)
        time.sleep(pausa)
        self.pulsante(False)
        time.sleep(pausa)

    def trascina(self, x1, y1, x2, y2, passi=25, pausa=0.03):
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

    # -- la tastiera (i nomi dei tasti di QEMU) --

    def _tasto(self, tasto, giu):
        if tasto not in TASTI:
            raise ErroreProva(f"tasto sconosciuto: {tasto}")
        self._eventi(TASTIERA, [(EV_KEY, TASTI[tasto], 1 if giu else 0)])

    def tasto_giu(self, tasto):
        self._tasto(tasto, True)

    def tasto_su(self, tasto):
        self._tasto(tasto, False)

    def premi(self, tasto, tieni=0.08):
        self.tasto_giu(tasto)
        time.sleep(tieni)
        self.tasto_su(tasto)

    def hmp(self, comando):
        raise ErroreProva("la sandbox non ha il monitor di QEMU")


# --- i comandi ------------------------------------------------------------------------------

def comando_compila(args):
    print(f"sandbox: compilata in {compila():.1f} s")


def assicura_compilata(args):
    if args.senza_compilare:
        return
    if da_ricompilare():
        print(f"sandbox: compilata in {compila():.1f} s", flush=True)


def comando_foto(args):
    assicura_compilata(args)
    inizio = time.time()
    with Sandbox(tema=args.tema) as sb:
        sb.aspetta_scrivania()
        if args.app:
            sb.apri(args.app)
        foto = sb.aspetta_schermo_fermo()
    uscita = Path(args.uscita or f"sandbox_{args.app or 'scrivania'}_{args.tema}.png")
    foto.save(uscita)
    print(f"sandbox: {uscita} in {time.time() - inizio:.1f} s")


def comando_prova(args):
    """Le prove di test/prove/, con la sandbox al posto di QEMU (stesso Prova di collaudo.py)."""
    assicura_compilata(args)
    import collaudo
    prove = {p.NOME: p for p in collaudo.carica_prove()}
    sconosciute = [n for n in args.prove if n not in prove]
    if sconosciute:
        sys.exit(f"prove sconosciute: {', '.join(sconosciute)} (ci sono: {', '.join(prove)})")
    fallite = 0
    for nome in args.prove:
        modulo = prove[nome]
        cartella = collaudo.RISULTATI / "sandbox" / nome
        shutil.rmtree(cartella, ignore_errors=True)
        cartella.mkdir(parents=True)
        sb = Sandbox(tema=args.tema, disco=getattr(modulo, "DISCO", "disco.img"), lavoro=cartella)
        p = collaudo.Prova(nome, "x86_64", sb, cartella, False)
        inizio = time.time()
        esito = "OK"
        try:
            sb.accendi()
            p.scrivania = sb.aspetta_scrivania()
            modulo.esegui(p)
            for v in collaudo.VIETATE:
                if v in sb.testo_seriale():
                    raise ErroreProva(f"nel registro c'e' '{v}'")
        except Exception as e:      # noqa: BLE001 (ogni errore fa fallire la prova)
            esito = f"FALLITA: {e}"
            fallite += 1
            try:
                sb.foto("al_fallimento")
            except Exception:       # noqa: BLE001
                pass
        finally:
            sb.spegni()
        print(f"  [sandbox] {nome:14} {esito}  {time.time() - inizio:5.1f} s", flush=True)
        if esito == "OK" and not args.tieni:
            shutil.rmtree(cartella, ignore_errors=True)
        elif esito != "OK":
            print(f"            foto e registro in {cartella.relative_to(REPO)}")
    return 1 if fallite else 0


def main():
    a = argparse.ArgumentParser(description="mshell in un container Docker (senza QEMU)")
    sotto = a.add_subparsers(dest="comando", required=True)
    sotto.add_parser("compila", help="compila (solo cio' che e' cambiato) e rifa' l'immagine")
    f = sotto.add_parser("foto", help="accende, apre un'app (se si dice) e salva un PNG")
    f.add_argument("app", nargs="?", help=f"una app della userbar: {', '.join(APP)}")
    f.add_argument("--uscita", help="il file PNG (se no sandbox_<app>_<tema>.png)")
    p = sotto.add_parser("prova", help="esegue prove di test/prove/ nella sandbox")
    p.add_argument("prove", nargs="+")
    p.add_argument("--tieni", action="store_true", help="tiene le foto anche se riesce")
    for s in (f, p):
        s.add_argument("--tema", choices=["chiaro", "scuro"], default="chiaro")
        s.add_argument("--senza-compilare", action="store_true",
                       help="non controlla se i sorgenti sono cambiati")
    args = a.parse_args()
    try:
        r = {"compila": comando_compila, "foto": comando_foto, "prova": comando_prova}[args.comando](args)
    except ErroreProva as e:
        sys.exit(f"sandbox: {e}")
    return r or 0


if __name__ == "__main__":
    sys.exit(main())
