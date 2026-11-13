# La sandbox: compilare i programmi come build.sh (PC x86_64), ma solo quello che e' cambiato.
# La usa compila.sh, dentro il container degli attrezzi, nella cartella del repo (/w);
# gli oggetti stanno in O (un volume di Docker: restano da una volta all'altra).
#
# IL COMPILATORE E' QUELLO DI build.sh, con gli stessi argomenti (passo 1 di build.sh):
# se li si cambia la', vanno cambiati anche qui. -MMD -MP scrivono accanto a ogni oggetto
# da quali file .h dipende (non cambiano il codice compilato): cosi' make sa cosa rifare.
CC     := clang --target=x86_64-alpine-linux-musl --sysroot=/radice -O2 -Wall -Wextra -ffunction-sections -fdata-sections
DIP    := -MMD -MP
UTENTE := -Iutente/libreria
LIBC   := /radice/usr/lib
RT     := $(firstword $(wildcard /radice/usr/lib/llvm*/lib/clang/*/lib/*/libclang_rt.builtins-*.a))
O      ?= /o

# lega USCITA OGGETTI...: come la funzione lega di build.sh
define lega
@mkdir -p $(dir $(1))
ld.lld -static --gc-sections -s -o $(1) $(LIBC)/crt1.o $(LIBC)/crti.o $(2) -L$(LIBC) -lc $(RT) $(LIBC)/crtn.o
endef

# CoreKit, come build.sh: i suoi .c e le librerie di altri che usa solo lei (miniz, tomlc99)
# in una libreria statica, libcorekit.a, che nel comando di lega va dopo gli oggetti
COREKIT_OGGETTI := $(patsubst %.c,$(O)/%.o,$(sort $(wildcard utente/libreria/corekit/interno/*.c))) $(O)/esterni/miniz.o $(O)/esterni/toml.o
COREKIT  := $(O)/libcorekit.a
# GKit, come build.sh: i suoi .c e stb_image (le PNG: la usa solo lei) in libgkit.a, che
# nel comando di lega va dopo UIKit e il programma e prima di libcorekit.a
GKIT_OGGETTI := $(patsubst %.c,$(O)/%.o,$(sort $(wildcard utente/libreria/gkit/interno/*.c))) $(O)/esterni/stb_image.o
GKIT     := $(O)/libgkit.a
# UIKit, per ora oggetti sciolti, come build.sh
LIBRERIA := $(patsubst %.c,$(O)/%.o,$(sort $(wildcard utente/libreria/uikit/*.c)))
# per i programmi del sistema: lo schermo della sandbox al posto del DRM. E' un oggetto
# sciolto che definisce le tre funzioni dello schermo (gkit/sistema/schermo.h): chi lega le
# trova gia' li', e dall'archivio non prende schermo.o (si prende un oggetto da un archivio
# solo per una funzione che manca ancora)
LIBRERIA_SANDBOX := $(LIBRERIA) $(O)/strumenti/sandbox/schermo_sandbox.o

PROGRAMMI := init compositore launcher scrivania network shell
APP_CARTELLE := $(sort $(wildcard utente/applicazioni/*/ utente/prove/applicazioni/*/))
minuscolo = $(shell echo '$(1)' | tr A-Z a-z)

FATTI := $(addprefix $(O)/bin/,$(PROGRAMMI) eventi avvio prove/riempitivo prove/pacchetti prove/messaggi)

$(O)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(UTENTE) $(DIP) -c $< -o $@

$(O)/utente/applicazioni/%.o: utente/applicazioni/%.c
	@mkdir -p $(@D)
	$(CC) $(UTENTE) -I$(dir $<) $(DIP) -c $< -o $@

$(O)/strumenti/sandbox/%.o: strumenti/sandbox/%.c
	@mkdir -p $(@D)
	$(CC) $(UTENTE) -Istrumenti/sandbox $(DIP) -c $< -o $@

# l'archivio si rifa' da capo: un oggetto tolto non ci deve restare
$(COREKIT): $(COREKIT_OGGETTI)
	@rm -f $@
	ar rcs $@ $^
$(GKIT): $(GKIT_OGGETTI)
	@rm -f $@
	ar rcs $@ $^

ESTERNI_ARG := -w -DMINIZ_NO_STDIO -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES
$(O)/esterni/miniz.o: utente/libreria/esterni/miniz/miniz.c
	@mkdir -p $(@D)
	$(CC) $(ESTERNI_ARG) $(DIP) -c $< -o $@
$(O)/esterni/toml.o: utente/libreria/esterni/tomlc99/toml.c
	@mkdir -p $(@D)
	$(CC) $(ESTERNI_ARG) $(DIP) -c $< -o $@
$(O)/esterni/stb_image.o: utente/libreria/esterni/stb/stb_image.c
	@mkdir -p $(@D)
	$(CC) $(ESTERNI_ARG) $(DIP) -c $< -o $@

# i programmi del sistema
$(addprefix $(O)/bin/,$(PROGRAMMI)): $(O)/bin/%: $(O)/utente/programmi/%.o $(LIBRERIA_SANDBOX) $(GKIT) $(COREKIT)
	$(call lega,$@,$(LIBRERIA_SANDBOX) $< $(GKIT) $(COREKIT))
$(O)/bin/eventi: $(O)/strumenti/sandbox/eventi_sandbox.o $(LIBRERIA_SANDBOX) $(GKIT) $(COREKIT)
	$(call lega,$@,$(LIBRERIA_SANDBOX) $< $(GKIT) $(COREKIT))
$(O)/bin/avvio: $(O)/strumenti/sandbox/avvio_sandbox.o
	$(call lega,$@,$<)
# i programmi delle prove (utente/prove)
$(O)/bin/prove/%: $(O)/utente/prove/%.o $(LIBRERIA) $(GKIT) $(COREKIT)
	$(call lega,$@,$(LIBRERIA) $< $(GKIT) $(COREKIT))

# le applicazioni: $(O)/app/<id>/code, con la libreria vera (come build.sh)
define APP
$(2)_OGGETTI := $$(patsubst %.c,$(O)/%.o,$$(sort $$(wildcard $(1)*.c)))
$(O)/app/$(2)/code: $$($(2)_OGGETTI) $$(LIBRERIA) $$(GKIT) $$(COREKIT)
	$$(call lega,$$@,$$(LIBRERIA) $$($(2)_OGGETTI) $$(GKIT) $$(COREKIT))
FATTI += $(O)/app/$(2)/code
endef
$(foreach c,$(APP_CARTELLE),$(eval $(call APP,$(c),$(call minuscolo,$(notdir $(patsubst %/,%,$(c)))))))

tutto: $(FATTI)
.DEFAULT_GOAL := tutto
.PHONY: tutto

-include $(shell find $(O) -name '*.d' 2>/dev/null)
