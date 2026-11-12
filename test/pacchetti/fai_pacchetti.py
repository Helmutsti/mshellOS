"""fai_pacchetti.py: prepara i pacchetti .app di prova (buoni e cattivi) per prova.c.

    python3 fai_pacchetti.py <cartella>

Ogni pacchetto e' uno ZIP fatto a mano con zipfile, cosi' si possono mettere anche i nomi
pericolosi ("../", "/...", collegamenti simbolici) che un programma normale non scrive.
"""

import stat
import sys
import zipfile
from pathlib import Path

SVG = b'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><rect width="64" height="64" rx="14"/></svg>'


def manifesto(id="prova", versione="1.0.0", processore="x86_64", sistema=1, permessi="[]",
              deviazioni="[]", funzioni="[]", programma="code", icone=True, extra=""):
    testo = f'''id = "{id}"
nome = "Prova"
versione = "{versione}"
autore = "Le prove"
descrizione = "Un'app di prova."
processore = "{processore}"
sistema_minimo = {sistema}
programma = "{programma}"
permessi = {permessi}
deviazioni = {deviazioni}
funzioni = {funzioni}
{extra}
'''
    if icone:
        testo += '''
[icone.chiaro]
svg = "resources/icona-chiara.svg"
png = { 18 = "resources/icona-chiara-18.png", 32 = "resources/icona-chiara-32.png" }

[icone.scuro]
svg = "resources/icona-scura.svg"
'''
    return testo.encode()


def zip_(percorso, voci):
    """voci: (nome, dati) oppure (nome, dati, "link") per un collegamento simbolico."""
    with zipfile.ZipFile(percorso, "w", zipfile.ZIP_DEFLATED) as z:
        for v in voci:
            nome, dati = v[0], v[1]
            info = zipfile.ZipInfo(nome)
            info.create_system = 3          # Unix: i permessi nei 16 bit alti
            if len(v) > 2 and v[2] == "link":
                info.external_attr = (stat.S_IFLNK | 0o777) << 16
            elif nome.endswith("/"):
                info.external_attr = (stat.S_IFDIR | 0o755) << 16
            else:
                info.external_attr = (stat.S_IFREG | 0o644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, dati)


def base(m=None, senza=(), piu=()):
    voci = [("manifest.toml", m if m is not None else manifesto()),
            ("code", b"\x7fELF finto programma"),
            ("resources/", b""),
            ("resources/icona-chiara.svg", SVG),
            ("resources/icona-scura.svg", SVG),
            ("resources/icona-chiara-18.png", b"png"),
            ("resources/icona-chiara-32.png", b"png")]
    return [v for v in voci if v[0] not in senza] + list(piu)


def main():
    d = Path(sys.argv[1])
    d.mkdir(parents=True, exist_ok=True)
    zip_(d / "buono.app", base())
    zip_(d / "buono-1.1.app", base(manifesto(versione="1.1.0")))
    zip_(d / "buono-0.9.app", base(manifesto(versione="0.9")))
    zip_(d / "slip.app", base(piu=[("../fuori.txt", b"preso!")]))
    zip_(d / "slip2.app", base(piu=[("resources/../../fuori.txt", b"preso!")]))
    zip_(d / "assoluto.app", base(piu=[("/tmp/assoluto.txt", b"preso!")]))
    zip_(d / "collegamento.app", base(piu=[("resources/passwd", b"/etc/passwd", "link")]))
    zip_(d / "arm.app", base(manifesto(processore="arm64")))
    zip_(d / "futuro.app", base(manifesto(sistema=2)))
    zip_(d / "sistema.app", base(manifesto(id="files")))
    zip_(d / "permesso-ignoto.app", base(manifesto(permessi='["file.leggere", "telecamera"]')))
    zip_(d / "permesso-sistema.app", base(manifesto(id="prova-sis", permessi='["processi"]',
                                                     deviazioni='["sottofondo"]', funzioni='["testo.apri"]')))
    zip_(d / "deviazione-ignota.app", base(manifesto(deviazioni='["invisibile"]')))
    zip_(d / "funzione-ignota.app", base(manifesto(funzioni='["caffe.fai"]')))
    zip_(d / "senza-manifesto.app", base(senza=("manifest.toml",)))
    zip_(d / "toml-rotto.app", base(b'id = "prova\nnome = 3\n'))
    zip_(d / "voci-mancanti.app", base(b'id = "prova"\nnome = "Prova"\n'))
    zip_(d / "id-sbagliato.app", base(manifesto(id="Prova_Uno")))
    zip_(d / "tipo-sbagliato.app", base(manifesto(extra="", sistema='"uno"')))
    zip_(d / "senza-programma.app", base(senza=("code",)))
    zip_(d / "senza-scura.app", base(manifesto(id="prova-chiara"), senza=("resources/icona-scura.svg",)))
    zip_(d / "senza-icone.app", base(manifesto(id="prova-nuda", icone=False)))
    (d / "non-zip.app").write_bytes(b"questo non e' uno zip\n" * 10)


main()
