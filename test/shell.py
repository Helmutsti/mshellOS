# /// script
# requires-python = ">=3.10"
# dependencies = ["pillow"]
# ///
"""Prova della shell di sviluppo (solo PC): accende il disco di sviluppo in QEMU, con la
seriale su una presa TCP, scrive dei comandi come una persona e controlla le risposte.

    SVILUPPO=1 USCITA=build/sviluppo  (build.sh, vedi in cima)
    uv run test/shell.py                      # usa build/sviluppo/disco.img
    uv run test/shell.py --build build/sviluppo

Spegne solo il QEMU che ha acceso lei."""
import argparse
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import qemu  # noqa: E402  (trova_qemu, trova_firmware)


def leggi_fino_a(presa, testo, attesa):
    """Legge dalla seriale finche' compare 'testo' (dopo quello gia' letto). Torna il letto."""
    fine = time.time() + attesa
    letto = b""
    presa.settimeout(0.5)
    while time.time() < fine:
        try:
            dati = presa.recv(4096)
            if not dati:
                break
            letto += dati
        except socket.timeout:
            pass
        if testo.encode() in letto:
            return letto.decode("latin-1")
    raise SystemExit(f"FALLITA: dopo {attesa:.0f} s manca '{testo}'. Ricevuto:\n"
                     + letto.decode("latin-1")[-600:])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default="build/sviluppo")
    ap.add_argument("--porta", type=int, default=4480)
    a = ap.parse_args()
    disco = Path(a.build) / "disco.img"
    if not disco.exists():
        raise SystemExit(f"manca {disco}: compila con SVILUPPO=1 (vedi build.sh)")
    lavoro = Path(tempfile.mkdtemp(prefix="shell_"))
    shutil.copyfile(disco, lavoro / "disco.img")
    programma = qemu.trova_qemu("qemu-system-x86_64")
    cmd = [programma, "-machine", "pc", "-m", "2048", "-smp", "2",
           "-drive", f"if=pflash,format=raw,readonly=on,file={qemu.trova_firmware(programma)}",
           "-vga", "none", "-device", "virtio-vga,xres=1280,yres=800",
           "-drive", f"if=none,id=disco,file={lavoro / 'disco.img'},format=raw",
           "-device", "virtio-blk-pci,drive=disco,bootindex=1",
           "-rtc", "base=2026-01-01T10:00:00,clock=vm",
           "-device", "qemu-xhci", "-device", "usb-kbd", "-device", "usb-tablet",
           "-display", "none", "-monitor", "none",
           "-serial", f"tcp:127.0.0.1:{a.porta},server=on,wait=off"]
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        presa = None
        for _ in range(100):
            try:
                presa = socket.create_connection(("127.0.0.1", a.porta), timeout=5)
                break
            except OSError:
                time.sleep(0.2)
        if presa is None:
            raise SystemExit("FALLITA: la seriale non risponde")
        leggi_fino_a(presa, "/ # ", 240)
        presa.settimeout(0.5)

        def comando(riga, fine="# ", attesa=20):
            presa.sendall(riga.encode() + b"\r")
            return leggi_fino_a(presa, fine, attesa)

        giro = [("pwd", "/\r\n"), ("ls /", "system/"), ("ls -l /", "drwx"),
                ("cd /data", ""), ("pwd", "/data\r\n"), ("ls -l", "users/"),
                ("cd users/../..", ""), ("cd /nonexistent", "cartella non trovata"),
                ("cd /system/bin/shell", "non e' una cartella"),
                ("blah", "comando sconosciuto"),
                ("help", "exit")]
        for riga, atteso in giro:
            fuori = comando(riga)
            print(f"# {riga}\n{fuori.replace(chr(13), '').strip()}\n")
            if atteso and atteso not in fuori:
                raise SystemExit(f"FALLITA: dopo '{riga}' manca '{atteso}'")
        presa.sendall(b"exit\r")
        # init fa ripartire la shell
        leggi_fino_a(presa, "lo faccio ripartire", 20)
        leggi_fino_a(presa, "/ # ", 20)
        print("PROVA SHELL: passata")
    finally:
        proc.kill()
        proc.wait(10)
        shutil.rmtree(lavoro, ignore_errors=True)


if __name__ == "__main__":
    main()
