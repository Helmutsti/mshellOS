# file/

Quello che metti in questa cartella finisce nel disco del sistema, nella cartella `/File`.

`build.sh` la copia a ogni compilazione, sottocartelle comprese, per tutte e due le CPU.
Nel sistema i file sono del sistema e tutti li possono leggere (cartelle 755, file 644).
Questo `LEGGIMI.md` resta qui: non viene copiato.

Per ora il disco del sistema e' in sola lettura: i file di `/File` si leggono, ma non si
possono cambiare da dentro mkernel. Per cambiarli, cambiali qui e ricompila.
