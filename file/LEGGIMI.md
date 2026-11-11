# file/

Quello che metti in questa cartella finisce nel disco, nella casa della persona:
`/data/users/1000/home`, che i programmi vedono come `/home`.

`build.sh` la copia a ogni compilazione, sottocartelle comprese, per tutte e due le CPU,
nella partizione `data` del disco (quella che si puo' scrivere). Nel sistema i file sono
della persona (uid 1000): cartelle 755, file 644, dentro la sua casa, che e' sua e di
nessun altro (700). Questo `LEGGIMI.md` resta qui: non viene copiato.

Mentre il sistema gira la persona (e le applicazioni, che hanno il suo uid) puo' cambiare
questi file. Il disco pero' si rifa' a ogni compilazione: per cambiarli per sempre,
cambiali qui e ricompila.

I file che ci sono adesso (Documenti, Immagini, `leggimi.txt`, `dati.bin`) servono anche
alla prova di Files (`test/prove/files.py`): la cartella dal nome lungo in Documenti e'
li' apposta, perche' il suo percorso supera i 63 caratteri e Files mostra lo stato
"Impossibile leggere la cartella". Se li cambi, rifai le foto di riferimento di Files.
`build.sh` mette a tutti la stessa data (31/12/2025 alle 18:02).
