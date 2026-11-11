# /// script
# requires-python = ">=3.10"
# dependencies = ["pillow"]
# ///
"""collaudo.py: le prove automatiche di mshell, sul PC (x86_64, UEFI) e su ARM64.

    uv run test/collaudo.py                       tutte le prove, su tutte e due le CPU
    uv run test/collaudo.py --cpu x86_64          solo sul PC ("x86" vale lo stesso)
    uv run test/collaudo.py --ram 8192            il PC con 8 GB di RAM (2048 se non si dice)
    uv run test/collaudo.py --solo avvio mouse    solo quelle prove
    uv run test/collaudo.py --elenco              quali prove ci sono
    uv run test/collaudo.py --riferimenti         rifa' le foto di riferimento
    uv run test/collaudo.py --compila             prima compila (Docker, come run.sh)

COME FUNZIONA. Ogni prova e' un file in test/prove/. Per ogni prova e per ogni CPU si
accende un mshell NUOVO in QEMU (senza finestra: vedi qemu.py), si aspetta la scrivania
e si chiama la funzione 'esegui' della prova, che muove il mouse, preme tasti, fotografa
lo schermo e controlla. Ogni prova parte da zero: se una si rompe, non trascina le altre.

IL DISCO. Ogni prova parte dal disco normale (disco.img), lo stesso di run.sh. Una prova
che scrive DISCO = "disco_prove.img" parte invece dal disco delle prove: lo stesso, con
in piu' i programmi di utente/prove/ in /data/tests, che init fa partire (per esempio
qualche "riempitivo", un processo che dorme e basta: vedi build.sh). Le altre prove non
li vedono, e i loro conti (righe, processi, foto di riferimento) restano quelli.

Una prova FALLISCE se solleva ErroreProva (o qualsiasi altro errore), oppure se alla fine
nella seriale c'e' un "Kernel panic" o un messaggio d'errore. Le foto e la seriale di
una prova fallita restano in test/risultati/<cpu>/<prova>/; quelle delle prove riuscite
si cancellano (con --tieni restano).

Le porte: QEMU apre il QMP sulla porta --porte (4465 se non si dice) e il monitor HMP
sulla successiva. Se sono occupate (un altro QEMU, un'altra prova) se ne scelgono altre.
"""

import argparse
import importlib.util
import os
import shutil
import subprocess
import sys
import time
import traceback
from pathlib import Path

from PIL import Image, ImageChops

import qemu
from qemu import ErroreProva, Macchina

QUI = Path(__file__).resolve().parent          # test/
REPO = QUI.parent                              # la cartella di mshell
PROVE = QUI / "prove"
RIFERIMENTI = QUI / "riferimenti"
RISULTATI = QUI / "risultati"
CPU_TUTTE = ["x86_64", "arm64"]
ALIAS_CPU = {"x86": "x86_64"}            # come build.sh e run.sh

# Le righe che nella seriale non devono MAI comparire (in qualsiasi prova).
VIETATE = ["Kernel panic", "PANICO", "ERRORE", "non riesco"]


# --- il "contesto" che riceve ogni prova -----------------------------------------------

class Prova:
    """Quello che una prova ha in mano: la macchina accesa, la CPU, la foto della
    scrivania vuota appena accesa, e due aiuti (controlla, riferimento)."""

    def __init__(self, nome, cpu, macchina, cartella, aggiorna_riferimenti):
        self.nome = nome
        self.cpu = cpu
        self.m = macchina
        self.ram = macchina.ram     # i MB di RAM dati a QEMU
        self.cartella = cartella
        self.aggiorna = aggiorna_riferimenti
        self.scrivania = None       # la foto della scrivania vuota (la riempie il collaudo)
        self.note = []              # cose da dire anche se la prova riesce

    def controlla(self, condizione, messaggio):
        if not condizione:
            raise ErroreProva(messaggio)

    def nota(self, testo):
        self.note.append(testo)

    def riferimento(self, nome, foto, maschera=qemu.OROLOGIO, tollerati=0, soglia=0):
        """Confronta 'foto' con la foto di riferimento riferimenti/<cpu>/<nome>.png.
        Con --riferimenti invece la salva come nuovo riferimento.
        'tollerati' pixel possono essere diversi (oltre 'soglia' di differenza)."""
        file = RIFERIMENTI / self.cpu / f"{nome}.png"
        if self.aggiorna:
            file.parent.mkdir(parents=True, exist_ok=True)
            foto.save(file)
            self.nota(f"riferimento salvato: {file.relative_to(REPO)}")
            return
        if not file.exists():
            foto.save(self.cartella / f"{nome}_ottenuta.png")
            raise ErroreProva(f"manca il riferimento {file.relative_to(REPO)}: "
                              "crealo con --riferimenti")
        with Image.open(file) as im:
            atteso = im.convert("RGB")
        if atteso.size != foto.size:
            foto.save(self.cartella / f"{nome}_ottenuta.png")
            raise ErroreProva(f"'{nome}' e' grande {foto.size[0]}x{foto.size[1]}, "
                              f"il riferimento {atteso.size[0]}x{atteso.size[1]}")
        diversi = qemu.pixel_diversi(atteso, foto, maschera, soglia)
        if diversi > tollerati:
            # una foto delle differenze: in rosso i pixel cambiati
            d = ImageChops.difference(atteso, foto).convert("L")
            d = d.point(lambda v: 255 if v > soglia else 0)
            rosso = Image.new("RGB", foto.size, (255, 0, 0))
            segnata = Image.composite(rosso, foto, d)
            segnata.save(self.cartella / f"{nome}_differenze.png")
            foto.save(self.cartella / f"{nome}_ottenuta.png")
            atteso.save(self.cartella / f"{nome}_attesa.png")
            raise ErroreProva(f"'{nome}' diversa dal riferimento: {diversi} pixel, "
                              f"nel riquadro {qemu.riquadro_diverso(atteso, foto, maschera, soglia)}")


# --- trovare le prove -------------------------------------------------------------------

def carica_prove():
    """Ogni file di test/prove/ (tranne quelli che cominciano con _) e' una prova.
    Si mettono in fila per ORDINE (un numero nel file), poi per nome."""
    prove = []
    for file in sorted(PROVE.glob("*.py")):
        if file.name.startswith("_"):
            continue
        spec = importlib.util.spec_from_file_location(f"prove.{file.stem}", file)
        modulo = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(modulo)
        modulo.NOME = getattr(modulo, "NOME", file.stem)
        prove.append(modulo)
    prove.sort(key=lambda p: (getattr(p, "ORDINE", 100), p.NOME))
    return prove


# --- compilare ---------------------------------------------------------------------------

def compila(cpu_scelte, build):
    """Compila con Docker come run.sh. build.sh scrive in build/<cpu>/ (o in USCITA, una
    cartella DENTRO il repo: e' l'unica che il container vede); qui USCITA e'
    <--build>/<cpu>, cosi' le prove la trovano dove la cercano."""
    try:
        relativa = build.resolve().relative_to(REPO)
    except ValueError:
        sys.exit("--compila: la cartella --build deve stare dentro il repo")
    for cpu in cpu_scelte:
        uscita = f"{relativa.as_posix()}/{cpu}"
        print(f"compilo per {cpu} in {uscita} ...", flush=True)
        r = subprocess.run(["docker", "run", "--rm", "--platform", "linux/amd64",
                            "-e", f"USCITA={uscita}", "-v", f"{REPO}:/w",
                            "alpine:3.24", "sh", "/w/build.sh", cpu])
        if r.returncode:
            sys.exit(f"la compilazione per {cpu} non e' riuscita")


# --- eseguire una prova ------------------------------------------------------------------

def esegui_una(modulo, cpu, args):
    """Accende, aspetta la scrivania, esegue, controlla la seriale, spegne.
    Torna (riuscita, messaggio, secondi)."""
    cartella = RISULTATI / cpu / modulo.NOME
    shutil.rmtree(cartella, ignore_errors=True)
    cartella.mkdir(parents=True)
    m = Macchina(cpu, args.build, cartella, args.porte, args.ram,
                 getattr(modulo, "DISCO", "disco.img"))
    p = Prova(modulo.NOME, cpu, m, cartella, args.riferimenti)
    inizio = time.time()
    riuscita, messaggio = True, ""
    try:
        m.accendi()
        p.scrivania = m.aspetta_scrivania()
        modulo.esegui(p)
        seriale = m.testo_seriale()
        for v in VIETATE:
            if v in seriale:
                riga = next(r for r in seriale.splitlines() if v in r)
                raise ErroreProva(f"nella seriale c'e' '{v}': {riga.strip()}")
    except ErroreProva as e:
        riuscita, messaggio = False, str(e)
    except Exception:
        riuscita, messaggio = False, "errore nella prova:\n" + traceback.format_exc()
    finally:
        if not riuscita and m.qmp:
            try:
                m.foto("al_fallimento")
            except Exception:
                pass
        m.spegni()
        disco = cartella / "disco.img"
        if disco.exists():
            disco.unlink()
        (cartella / "_ultima.png").unlink(missing_ok=True)
    if riuscita and not args.tieni:
        shutil.rmtree(cartella, ignore_errors=True)
    if riuscita and p.note:
        messaggio = "; ".join(p.note)
    return riuscita, messaggio, time.time() - inizio


def main():
    a = argparse.ArgumentParser(description="Le prove automatiche di mshell")
    a.add_argument("--cpu", choices=[*CPU_TUTTE, *ALIAS_CPU, "tutte"], default="tutte",
                   help="x86_64 (il PC; anche x86), arm64, o tutte (se non si dice)")
    a.add_argument("--ram", type=int, default=2048, metavar="MB",
                   help="la RAM del PC in MB (2048 se non si dice); ARM64 ha sempre 512")
    a.add_argument("--build", type=Path, default=REPO / "build",
                   help="la cartella compilata, con dentro una cartella per CPU "
                        "(x86_64/, arm64/, come build.sh); se non si dice, build/")
    a.add_argument("--solo", nargs="+", metavar="PROVA", help="solo queste prove")
    a.add_argument("--porte", type=int, default=4465,
                   help="la porta del QMP; il monitor HMP usa la successiva (4465)")
    a.add_argument("--compila", action="store_true", help="prima compila con Docker")
    a.add_argument("--riferimenti", action="store_true",
                   help="rifa' le foto di riferimento invece di confrontarle")
    a.add_argument("--tieni", action="store_true",
                   help="tiene le foto anche delle prove riuscite")
    a.add_argument("--elenco", action="store_true", help="elenca le prove ed esce")
    args = a.parse_args()
    args.build = args.build.resolve()

    prove = carica_prove()
    if args.elenco:
        for p in prove:
            print(f"  {p.NOME:14} {getattr(p, 'DESCRIZIONE', '')}")
        return 0
    if args.solo:
        nomi = {p.NOME for p in prove}
        sconosciute = [n for n in args.solo if n not in nomi]
        if sconosciute:
            sys.exit(f"prove sconosciute: {', '.join(sconosciute)} (vedi --elenco)")
        prove = [p for p in prove if p.NOME in args.solo]
    elif args.riferimenti:
        prove = [p for p in prove if getattr(p, "RIFERIMENTI", False)]

    cpu_scelte = CPU_TUTTE if args.cpu == "tutte" else [ALIAS_CPU.get(args.cpu, args.cpu)]
    if args.compila:
        compila(cpu_scelte, args.build)

    print(f"mshell: collaudo su {', '.join(cpu_scelte)} (build: {args.build}, "
          f"RAM del PC {args.ram} MB, porte {args.porte}-{args.porte + 1})")
    risultati = []
    for cpu in cpu_scelte:
        for modulo in prove:
            print(f"  [{cpu:6}] {modulo.NOME:14} ", end="", flush=True)
            riuscita, messaggio, secondi = esegui_una(modulo, cpu, args)
            print(f"{'OK' if riuscita else 'FALLITA':8} {secondi:5.1f} s")
            if messaggio:
                for riga in messaggio.splitlines():
                    print(f"          {riga}")
            if not riuscita:
                print(f"          foto e seriale in {(RISULTATI / cpu / modulo.NOME).relative_to(REPO)}")
            risultati.append((cpu, modulo.NOME, riuscita))

    fallite = [(c, n) for c, n, r in risultati if not r]
    print()
    print(f"Riassunto: {len(risultati)} prove, {len(risultati) - len(fallite)} riuscite, "
          f"{len(fallite)} fallite")
    for c, n in fallite:
        print(f"  FALLITA: {n} su {c}")
    return 1 if fallite else 0


if __name__ == "__main__":
    sys.exit(main())
