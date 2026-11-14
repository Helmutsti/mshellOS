# La sandbox: i programmi di mshell come per il disco (strumenti/compilazione/programmi.mk,
# lo stesso compilatore e gli stessi argomenti di build.sh), piu' i pezzi della sandbox.
# La usa compila.sh, dentro il container degli attrezzi, nella cartella del repo (/w); gli
# oggetti stanno in O (un volume di Docker: restano da una volta all'altra) e make rifa' solo
# quello che e' cambiato.
RADICE ?= /radice
include strumenti/compilazione/programmi.mk

# I pezzi della sandbox, in $(O)/sandbox/:
#   - i programmi del sistema legati con lo schermo della sandbox al posto del DRM. E' un
#     oggetto sciolto che definisce le tre funzioni dello schermo (gkit/sistema/schermo.h):
#     chi lega le trova gia' li', e dall'archivio non prende schermo.o (si prende un oggetto
#     da un archivio solo per una funzione che manca ancora);
#   - eventi: mouse e tastiera da una presa TCP (eventi_sandbox.c) al posto di evdev;
#   - avvio: il primo programma del container (avvio_sandbox.c).
SCHERMO_SANDBOX := $(O)/strumenti/sandbox/schermo_sandbox.o
SB_PROGRAMMI := init compositore launcher scrivania network shell

$(O)/strumenti/sandbox/%.o: strumenti/sandbox/%.c $(O)/.firma
	@mkdir -p $(@D)
	$(CC) $(UTENTE) -Istrumenti/sandbox $(DIP) -c $< -o $@

$(addprefix $(O)/sandbox/,$(SB_PROGRAMMI)): $(O)/sandbox/%: $(O)/utente/programmi/%.o $(SCHERMO_SANDBOX) $(LIBRERIE)
	$(call lega,$@,$(SCHERMO_SANDBOX) $< $(LIBRERIE))
$(O)/sandbox/eventi: $(O)/strumenti/sandbox/eventi_sandbox.o $(SCHERMO_SANDBOX) $(LIBRERIE)
	$(call lega,$@,$(SCHERMO_SANDBOX) $< $(LIBRERIE))
$(O)/sandbox/avvio: $(O)/strumenti/sandbox/avvio_sandbox.o
	$(call lega,$@,$<)

sandbox: $(addprefix $(O)/sandbox/,$(SB_PROGRAMMI) eventi avvio) \
         $(addprefix $(O)/bin/comandi/,$(COMANDI)) $(addprefix $(O)/bin/prove/,$(PROVE)) \
         $(foreach a,$(APP_ID),$(O)/app/$(a)/code)
.PHONY: sandbox
.DEFAULT_GOAL := sandbox
