# prova-veloce.py: LA PROVA VELOCE DELLA SCRIVANIA, con un QEMU sempre acceso (decisione della
# persona, 19/11/2026). Gira su Windows, dalla cartella del progetto:
#
#   uv run --with pillow python -I strumenti/prova-veloce.py <copione>     (un file)
#   uv run --with pillow python -I strumenti/prova-veloce.py -c "tasti meta_l-tab; foto disposizioni"
#   uv run --with pillow python -I strumenti/prova-veloce.py --spegni      (spegne il QEMU di prova)
#
# Cosa fa (guida: strumenti/LEGGIMI-prove.md):
#   1. compila i programmi di mshell nel container degli attrezzi (strumenti/compila.sh), in
#      build/prova-<posto>/radice;
#   2. se il QEMU di prova non e' acceso, lo accende (finestra nascosta) su una copia del disco
#      (build/prova-<posto>/disco.img, rifatta quando build/disco-qemu-pc.img e' piu' nuovo), con -snapshot:
#      nel disco non si scrive mai;
#   3. dalla seriale (la shell dell'amministratore) copia dentro i programmi nuovi in /run/prova e
#      li "sovrappone" a quelli del sistema (mount --bind), poi riavvia solo la scrivania (greetd);
#   4. esegue il copione: tasti, attese, foto dello schermo (in build/prova-<posto>/foto/<nome>.png);
#   5. dice quanto ci ha messo.
#
# IL COPIONE: una istruzione per riga (o separate da ";"), "#" per i commenti.
#   tasti <combinazione>   tasti premuti insieme e lasciati, con i nomi di QEMU: meta_l-tab,
#                          meta_l-spc, meta_l-up, ret, esc, a, shift-a ... (meta_l = Super/Start)
#   tieni <tasto>          tiene premuto un tasto (es. meta_l) finche' non arriva "lascia"
#   lascia <tasto>         lo lascia
#   scrivi <testo>         scrive lettere, cifre e spazi, un tasto alla volta
#   aspetta <ms>           aspetta i millisecondi indicati
#   foto <nome>            fotografa lo schermo in build/prova-<posto>/foto/<nome>.png
#   clic <x> <y>           clic con il tasto sinistro sul pixel x, y dello schermo
#   app                    apre l'app di prova (come utente1, nel desktop attuale)
#   seriale <comando>      esegue un comando nella shell dell'amministratore e ne stampa l'uscita
#
# Porte (solo sul PC, 127.0.0.1): monitor 4450, seriale 4451, QMP 4452, piu' 10 per ogni posto
# (--posto 1: 4460-4462). Cartella: build/prova-<posto>. Non tocca altri QEMU.
import argparse
import base64
import hashlib
import json
import os
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path

QUI = Path(__file__).resolve().parent.parent
# Un QEMU di prova per ogni "posto" (--posto N, predefinito 0): agenti che lavorano insieme usano
# posti diversi, cioe' porte e cartelle diverse (build/prova-N).
POSTO = 0
for i, x in enumerate(sys.argv):
    if x == "--posto" and i + 1 < len(sys.argv):
        POSTO = int(sys.argv[i + 1])
PROVA = QUI / "build" / f"prova-{POSTO}"
RADICE = PROVA / "radice"
FOTO = PROVA / "foto"
# Il disco della costruzione completa: nella cartella del progetto; gli agenti lavorano in una copia
# (.claude/worktrees/<nome>) dove non c'e', e usano quello della cartella principale.
DISCO_BASE = QUI / "build" / "disco-qemu-pc.img"
if not DISCO_BASE.exists() and ".claude" in QUI.parts:
    DISCO_BASE = Path(*QUI.parts[:QUI.parts.index(".claude")]) / "build" / "disco-qemu-pc.img"
DISCO = PROVA / "disco.img"
QEMU = r"C:\Program Files\qemu\qemu-system-x86_64.exe"
FIRMWARE = r"C:\Program Files\qemu\share\edk2-x86_64-code.fd"
P_MONITOR, P_SERIALE, P_QMP = 4450 + 10 * POSTO, 4451 + 10 * POSTO, 4452 + 10 * POSTO
ATTREZZI = "mshell-debian-attrezzi"

inizio = time.monotonic()


def scrivi(testo):
    print(f"[{time.monotonic() - inizio:5.1f}s] {testo}", flush=True)


def porta_aperta(porta):
    try:
        socket.create_connection(("127.0.0.1", porta), timeout=0.5).close()
        return True
    except OSError:
        return False


# --- 1. compilare ---

def compila():
    if RADICE.exists():
        shutil.rmtree(RADICE)
    PROVA.mkdir(parents=True, exist_ok=True)
    r = subprocess.run(["docker", "run", "--rm", "-v", f"{QUI}:/w", ATTREZZI, "sh", "/w/strumenti/compila.sh",
                        f"/w/build/prova-{POSTO}/radice"], capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout + r.stderr)
        sys.exit("FALLITA: la compilazione non e' andata")
    scrivi("compilato")


# --- 2. il QEMU di prova ---

def accendi():
    if not DISCO_BASE.exists():
        sys.exit("FALLITA: manca build/disco-qemu-pc.img: serve una costruzione completa (agente principale)")
    if not DISCO.exists() or DISCO.stat().st_mtime < DISCO_BASE.stat().st_mtime:
        shutil.copyfile(DISCO_BASE, DISCO)
        scrivi("copiato il disco di prova")
    argomenti = [QEMU, "-machine", "q35", "-accel", "whpx", "-smp", "4", "-nodefaults", "-m", "2048",
                 "-drive", f"if=pflash,format=raw,readonly=on,file={FIRMWARE}",
                 "-drive", f"if=none,id=disco,file={DISCO},format=raw", "-snapshot",
                 "-device", "virtio-blk-pci,drive=disco,bootindex=1",
                 "-vga", "none", "-device", "virtio-vga,xres=1280,yres=800",
                 "-device", "qemu-xhci", "-device", "usb-tablet", "-display", "none",
                 "-monitor", f"tcp:127.0.0.1:{P_MONITOR},server=on,wait=off",
                 "-serial", f"tcp:127.0.0.1:{P_SERIALE},server=on,wait=off",
                 "-qmp", f"tcp:127.0.0.1:{P_QMP},server=on,wait=off"]
    nascosta = 0x08000000 | 0x00000008   # CREATE_NO_WINDOW | DETACHED_PROCESS: niente terminale che compare
    q = subprocess.Popen(argomenti, creationflags=nascosta, stdin=subprocess.DEVNULL,
                         stdout=open(PROVA / "qemu.log", "w"), stderr=subprocess.STDOUT)
    (PROVA / "qemu.pid").write_text(str(q.pid))
    for _ in range(100):
        if porta_aperta(P_MONITOR):
            break
        time.sleep(0.1)
    scrivi("QEMU di prova acceso")


class Seriale:
    """La shell dell'amministratore sulla seriale. Ogni comando finisce con un segno
    (FATTO<n>, scritto come conto perche' l'eco del comando non lo contenga gia')."""

    def __init__(self):
        self.s = socket.create_connection(("127.0.0.1", P_SERIALE), timeout=1)
        self.n = 0

    def leggi_fino(self, segno, attesa):
        dati, fine = b"", time.monotonic() + attesa
        while time.monotonic() < fine:
            try:
                pezzo = self.s.recv(65536)
                if not pezzo:
                    break
                dati += pezzo
                if segno in dati:
                    return dati.decode(errors="replace")
            except socket.timeout:
                pass
        return None

    def comando(self, testo, attesa=30):
        self.n += 1
        segno = f"FATTO{self.n}"
        self.s.sendall(f"{testo}; echo FATTO$(({self.n}+0))\n".encode())
        uscita = self.leggi_fino(segno.encode(), attesa)
        if uscita is None:
            sys.exit(f"FALLITA: la seriale non risponde a: {testo[:60]}")
        return uscita.split(segno)[0]

    def pronta(self, attesa):
        fine = time.monotonic() + attesa
        while time.monotonic() < fine:
            self.s.sendall(b"\n echo PRO$((0+1))NTA\n")
            if self.leggi_fino(b"PRO1NTA", 2):
                self.comando("stty -echo")
                return
        sys.exit("FALLITA: la shell della seriale non arriva (sistema acceso?)")


def copia_dentro(ser):
    """I file nuovi in /run/prova, sovrapposti a quelli del sistema (il sistema e' in sola lettura)."""
    nuovi = [p for p in RADICE.rglob("*") if p.is_file()]
    # prima si spegne la scrivania: i programmi della prova di prima (compositore, barra) sono
    # ancora accesi dai file sovrapposti, e un file in uso non si riscrive (ETXTBSY) e non si
    # smonta (EBUSY): il sistema sarebbe rimasto con i programmi vecchi, senza errori visibili
    ser.comando("systemctl stop greetd; i=0; c=1; while [ $c = 1 ] && [ $i -lt 50 ]; do c=0; "
                "for f in /proc/[0-9]*/comm; do read n < $f 2>/dev/null; case $n in mshell-*) c=1;; esac; "
                "done; sleep 0.1; i=$((i+1)); done")
    for p in nuovi:
        dove = "/" + p.relative_to(RADICE).as_posix()
        if "ESISTE" not in ser.comando(f"[ -e {dove} ] && echo ESI''STE"):
            scrivi(f"ATTENZIONE: {dove} non c'e' nel sistema: serve l'aggiornamento del disco")
            continue
        righe = base64.encodebytes(p.read_bytes()).decode()
        ser.comando(f"mkdir -p /run/prova$(dirname {dove}) && base64 -d > /run/prova{dove}.nuovo <<'FINE'\n{righe}FINE\n"
                    # si scrive accanto e si sostituisce solo dopo aver tolto la sovrapposizione:
                    # finche' il file e' sovrapposto non si puo' ne' riscrivere (se e' un programma
                    # acceso) ne' sostituire, e la prova userebbe in silenzio quello vecchio
                    # (si sganciano TUTTI gli strati, anche quelli lasciati dalle prove prima)
                    f"while umount -l {dove} 2>/dev/null; do :; done; "
                    f"while umount -l /run/prova{dove} 2>/dev/null; do :; done; mv -f /run/prova{dove}.nuovo /run/prova{dove}; "
                    f"chmod --reference={dove} /run/prova{dove}; "
                    f"mount --bind /run/prova{dove} {dove}", attesa=60)
        # il controllo: il file che il sistema vede deve essere proprio quello appena compilato
        atteso = hashlib.md5(p.read_bytes()).hexdigest()
        if atteso not in ser.comando(f"md5sum {dove}"):
            sys.exit(f"FALLITA: {dove} nel QEMU di prova non e' quello appena compilato")
    scrivi(f"copiati {len(nuovi)} file nel QEMU di prova")


def riavvia_scrivania(ser):
    # greetd apre la sessione automatica (initial_session) una volta sola per accensione, e se lo
    # ricorda nel file /run/greetd.run: togliendolo, al riavvio la riapre
    ser.comando("rm -f /run/greetd.run; systemctl restart greetd")
    # si aspetta che la barra sia accesa (il suo processo), poi che abbia disegnato
    ser.comando("c=0; i=0; while [ $c = 0 ] && [ $i -lt 100 ]; do for f in /proc/[0-9]*/comm; do "
                "read n < $f 2>/dev/null; [ \"$n\" = mshell-barra ] && c=1; done; sleep 0.1; i=$((i+1)); done")
    time.sleep(1.5)
    scrivi("scrivania riavviata")


# --- 3. il copione ---

class Monitor:
    def __init__(self):
        self.s = socket.create_connection(("127.0.0.1", P_MONITOR), timeout=2)
        self.attendi()

    def attendi(self):
        dati = b""
        while not dati.endswith(b"(qemu) "):
            dati += self.s.recv(65536)
        return dati.decode(errors="replace")

    def comando(self, testo):
        self.s.sendall((testo + "\n").encode())
        return self.attendi()


class Qmp:
    def __init__(self):
        self.s = socket.create_connection(("127.0.0.1", P_QMP), timeout=2)
        self.f = self.s.makefile("rb")
        self.f.readline()
        self.invia({"execute": "qmp_capabilities"})

    def invia(self, msg):
        self.s.sendall(json.dumps(msg).encode() + b"\n")
        while True:
            r = json.loads(self.f.readline())
            if "return" in r or "error" in r:
                return r

    def tasto(self, nome, giu):
        self.invia({"execute": "input-send-event", "arguments": {"events": [
            {"type": "key", "data": {"down": giu, "key": {"type": "qcode", "data": nome}}}]}})

    def clic(self, x, y, larghezza, altezza):
        """Porta il puntatore (la tavoletta usb-tablet, a coordinate assolute 0..32767) sul pixel
        x, y dello schermo e fa clic con il tasto sinistro."""
        self.invia({"execute": "input-send-event", "arguments": {"events": [
            {"type": "abs", "data": {"axis": "x", "value": x * 32767 // (larghezza - 1)}},
            {"type": "abs", "data": {"axis": "y", "value": y * 32767 // (altezza - 1)}}]}})
        time.sleep(0.05)
        for giu in (True, False):
            self.invia({"execute": "input-send-event", "arguments": {"events": [
                {"type": "btn", "data": {"down": giu, "button": "left"}}]}})
            time.sleep(0.05)


def esegui(copione, ser):
    mon, qmp = Monitor(), Qmp()
    FOTO.mkdir(parents=True, exist_ok=True)
    for riga in copione:
        parti = riga.split(None, 1)
        cosa, resto = parti[0], (parti[1] if len(parti) > 1 else "")
        if cosa == "tasti":
            mon.comando(f"sendkey {resto}")
        elif cosa == "tieni":
            qmp.tasto(resto, True)
        elif cosa == "lascia":
            qmp.tasto(resto, False)
        elif cosa == "scrivi":
            for c in resto:
                mon.comando("sendkey " + ("spc" if c == " " else c.lower()))
        elif cosa == "aspetta":
            time.sleep(int(resto) / 1000)
        elif cosa == "foto":
            ppm = FOTO / f"{resto}.ppm"
            mon.comando(f"screendump {ppm.as_posix()}")
            from PIL import Image
            Image.open(ppm).save(FOTO / f"{resto}.png")
            ppm.unlink()
            scrivi(f"foto: {FOTO / (resto + '.png')}")
        elif cosa == "clic":
            # la misura dello schermo serve per le coordinate assolute: si legge da una foto
            ppm = FOTO / "_misura.ppm"
            mon.comando(f"screendump {ppm.as_posix()}")
            from PIL import Image
            with Image.open(ppm) as im:
                larghezza, altezza = im.size
            ppm.unlink()
            x, y = (int(v) for v in resto.split())
            qmp.clic(x, y, larghezza, altezza)
        elif cosa == "app":
            ser.comando("(runuser -u utente1 -- env XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-0 "
                        "mshell-prova >/dev/null 2>&1 &)")
        elif cosa == "seriale":
            print(ser.comando(resto).strip())
        else:
            sys.exit(f"FALLITA: istruzione sconosciuta nel copione: {riga}")


def leggi_copione(args):
    testo = args.c if args.c is not None else Path(args.copione).read_text(encoding="utf-8")
    righe = []
    for r in testo.replace(";", "\n").splitlines():
        r = r.split("#")[0].strip()
        if r:
            righe.append(r)
    return righe


def spegni():
    """Chiede a QEMU di chiudersi (QMP "quit"); se dopo 5 secondi c'e' ancora, lo chiude a forza."""
    if porta_aperta(P_QMP):
        try:
            Qmp().invia({"execute": "quit"})
        except (OSError, ValueError):
            pass
    pid_file = PROVA / "qemu.pid"
    if pid_file.exists():
        pid = pid_file.read_text().strip()
        for _ in range(50):
            vivo = pid in subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/NH"],
                                         capture_output=True, text=True).stdout
            if not vivo:
                break
            time.sleep(0.1)
        else:
            subprocess.run(["taskkill", "/PID", pid, "/F"], capture_output=True)
        pid_file.unlink()
    scrivi("QEMU di prova spento")


def main():
    a = argparse.ArgumentParser(description="La prova veloce della scrivania (QEMU sempre acceso)")
    a.add_argument("copione", nargs="?")
    a.add_argument("-c", help="il copione scritto qui, istruzioni separate da ;")
    a.add_argument("--posto", type=int, default=0, help="il QEMU di prova da usare (agenti in parallelo)")
    a.add_argument("--spegni", action="store_true", help="spegne il QEMU di prova")
    args = a.parse_args()
    if args.spegni:
        spegni()
        return
    if args.copione is None and args.c is None:
        a.error("serve un copione (file o -c)")
    copione = leggi_copione(args)
    compila()
    appena_acceso = not porta_aperta(P_MONITOR)
    if appena_acceso:
        accendi()
    ser = Seriale()
    ser.pronta(120 if appena_acceso else 10)
    copia_dentro(ser)
    riavvia_scrivania(ser)
    esegui(copione, ser)
    scrivi("fine")


if __name__ == "__main__":
    main()
