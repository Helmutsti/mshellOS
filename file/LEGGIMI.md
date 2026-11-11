# file/

Quello che metti in questa cartella finisce nel disco del sistema, nella cartella `/File`.

`build.sh` la copia a ogni compilazione, sottocartelle comprese, per tutte e due le CPU.
Nel sistema i file sono del sistema e tutti li possono leggere (cartelle 755, file 644).
Questo `LEGGIMI.md` resta qui: non viene copiato.

Il disco del sistema si puo' scrivere (Linux scrive ext4), ma solo il sistema puo'
cambiare `/File`. Il disco si rifa' a ogni compilazione: per cambiare questi file per
sempre, cambiali qui e ricompila.
