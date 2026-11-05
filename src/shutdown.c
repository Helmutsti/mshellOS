// shutdown: chiede a init (PID 1) di spegnere il sistema.
// Non spegne da se': solo init sa cosa e' in esecuzione e cosa va chiuso prima.
// Il segnale e' lo stesso di Ctrl+Alt+Canc. Il kernel lo lascia mandare al PID 1
// solo a root: per un utente normale risponde "permesso negato".
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    if (kill(1, SIGINT) != 0) {
        fprintf(stderr, "shutdown: %s\n", strerror(errno));
        return 1;
    }
    printf("shutdown: spegnimento richiesto\n");
    return 0;
}
