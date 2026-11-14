# I PROGRAMMI DI MSHELL: come si compilano, in un posto solo. Lo usano build.sh (per il
# disco) e la sandbox (strumenti/sandbox/Makefile, che lo include e aggiunge i suoi pezzi).
# Gira dentro il container degli attrezzi, nella cartella del repo (/w).
#
#   make -f strumenti/compilazione/programmi.mk ARCH=aarch64 CPU=arm64 RADICE=/radice O=build/arm64/oggetti
#
#   ARCH    il processore per Alpine e il compilatore (x86_64, aarch64)
#   CPU     il processore per mshell (x86_64, arm64): va nei manifesti dei pacchetti
#   RADICE  la radice di Alpine per quel processore (musl, header del kernel, compiler-rt)
#   O       dove vanno gli oggetti e i programmi pronti: RESTANO da una volta all'altra, e
#           make rifa' solo quello che e' cambiato (un .c, un .h che include, un oggetto da
#           cui dipende un programma, un file di un pacchetto, il compilatore o la radice)
#
# Cosa esce, in O:
#   lib{corekit,gkit,uikit}.a      le tre librerie (LIBRERIE.md)
#   bin/<nome>                     i programmi del sistema (utente/programmi/<nome>.c)
#   bin/comandi/<nome>             i comandi (utente/comandi/<nome>.c), solo CoreKit
#   bin/prove/<nome>               i programmi delle prove (utente/prove/<nome>.c)
#   bin/avvio                      il programma dell'archivio di avvio (utente/avvio/avvio.c)
#   app/<id>/code                  il programma di ogni applicazione
#   pacchetti/<id>/                il pacchetto di ogni applicazione, scompattato (va cosi'
#                                  in /system/apps/<id> per le app del sistema)
#   pacchetti/<id>.app             lo stesso chiuso in uno ZIP (impacchetta.sh)

ifndef ARCH
$(error manca ARCH)
endif
ifndef RADICE
$(error manca RADICE)
endif
O ?= oggetti

# IL COMPILATORE. clang compila per il processore (--target) con gli header e le librerie
# della radice di Alpine (--sysroot). I programmi sono STATICI: dentro ognuno c'e' tutto
# quello che gli serve della libreria C, e nel disco non serve un "caricatore" (ld.so).
#   -ffunction-sections -fdata-sections e --gc-sections: dentro va solo quello che si usa
#   -s: senza i nomi delle funzioni (programmi piu' piccoli)
#   -MMD -MP: accanto a ogni oggetto, da quali .h dipende (non cambia il codice): cosi'
#             make sa cosa rifare
CC     := clang --target=$(ARCH)-alpine-linux-musl --sysroot=$(RADICE) -O2 -Wall -Wextra -ffunction-sections -fdata-sections
DIP    := -MMD -MP
UTENTE := -Iutente/libreria
LIBC   := $(RADICE)/usr/lib
# compiler-rt: le funzioni del compilatore (es. i long double a 128 bit su ARM, che printf
# e scanf di musl usano): senza, legare un programma che usa printf non riesce
RT     := $(firstword $(wildcard $(RADICE)/usr/lib/llvm*/lib/clang/*/lib/*/libclang_rt.builtins-*.a))
# Le librerie di altri (utente/libreria/esterni/, ognuna con la sua LICENSE):
#   miniz 3.0.2 (MIT), la release "amalgamata": leggere gli ZIP (i pacchetti .app).
#     Senza le funzioni sui FILE della libreria C e senza i nomi di zlib.
#   tomlc99 (MIT), commit 62ce93c del 4/11/2026: leggere il TOML (i manifesti).
#   stb_image 2.30 (pubblico dominio o MIT), commit f58f558: leggere le PNG, per GKit.
#     Solo PNG e senza FILE (esterni/stb/stb_image.c).
# Non sono nostre: i loro avvisi si spengono (-w), per non coprire i nostri.
ESTERNI := -w -DMINIZ_NO_STDIO -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES

# LA FIRMA: il comando del compilatore e i pacchetti installati nella radice (musl, header).
# Se cambia, il file $(O)/.firma diventa piu' nuovo e ogni oggetto si rifa'.
FIRMA := $(CC) $(ESTERNI) $(shell md5sum < $(RADICE)/lib/apk/db/installed | cut -d' ' -f1)
$(shell mkdir -p $(O); [ "$$(cat $(O)/.firma 2>/dev/null)" = "$(FIRMA)" ] || printf '%s' "$(FIRMA)" > $(O)/.firma)

# lega USCITA,OGGETTI: si lega con ld.lld direttamente: i pezzi di avvio di musl (crt1.o,
# crti.o, crtn.o: chiamano main e poi exit), i nostri oggetti e la libreria C (-lc).
# L'ordine degli archivi conta: il programma, UIKit, GKit, CoreKit, la libreria C (ognuno
# usa le funzioni di quelli che vengono dopo).
define lega
@mkdir -p $(dir $(1))
ld.lld -static --gc-sections -s -o $(1) $(LIBC)/crt1.o $(LIBC)/crti.o $(2) -L$(LIBC) -lc $(RT) $(LIBC)/crtn.o
endef

# --- gli oggetti ---
$(O)/%.o: %.c $(O)/.firma
	@mkdir -p $(@D)
	$(CC) $(UTENTE) $(DIP) -c $< -o $@

# le applicazioni vedono anche la loro cartella (-I)
$(O)/utente/applicazioni/%.o: utente/applicazioni/%.c $(O)/.firma
	@mkdir -p $(@D)
	$(CC) $(UTENTE) -I$(dir $<) $(DIP) -c $< -o $@
$(O)/utente/prove/applicazioni/%.o: utente/prove/applicazioni/%.c $(O)/.firma
	@mkdir -p $(@D)
	$(CC) $(UTENTE) -I$(dir $<) $(DIP) -c $< -o $@

$(O)/esterni/miniz.o: utente/libreria/esterni/miniz/miniz.c $(O)/.firma
	@mkdir -p $(@D)
	$(CC) $(ESTERNI) $(DIP) -c $< -o $@
$(O)/esterni/toml.o: utente/libreria/esterni/tomlc99/toml.c $(O)/.firma
	@mkdir -p $(@D)
	$(CC) $(ESTERNI) $(DIP) -c $< -o $@
$(O)/esterni/stb_image.o: utente/libreria/esterni/stb/stb_image.c $(O)/.firma
	@mkdir -p $(@D)
	$(CC) $(ESTERNI) $(DIP) -c $< -o $@

# --- le librerie: archivi statici (ar), rifatti da capo (un oggetto tolto non ci resta) ---
# CoreKit: i suoi .c e le librerie di altri che usa solo lei (miniz, tomlc99)
COREKIT_OGGETTI := $(patsubst %.c,$(O)/%.o,$(sort $(wildcard utente/libreria/corekit/interno/*.c))) $(O)/esterni/miniz.o $(O)/esterni/toml.o
COREKIT := $(O)/libcorekit.a
# GKit: i suoi .c e stb_image (le PNG: la usa solo lei)
GKIT_OGGETTI := $(patsubst %.c,$(O)/%.o,$(sort $(wildcard utente/libreria/gkit/interno/*.c))) $(O)/esterni/stb_image.o
GKIT := $(O)/libgkit.a
# UIKit: i suoi .c
UIKIT_OGGETTI := $(patsubst %.c,$(O)/%.o,$(sort $(wildcard utente/libreria/uikit/interno/*.c)))
UIKIT := $(O)/libuikit.a
LIBRERIE := $(UIKIT) $(GKIT) $(COREKIT)

$(COREKIT): $(COREKIT_OGGETTI)
	@rm -f $@
	ar rcs $@ $^
$(GKIT): $(GKIT_OGGETTI)
	@rm -f $@
	ar rcs $@ $^
$(UIKIT): $(UIKIT_OGGETTI)
	@rm -f $@
	ar rcs $@ $^

# --- i programmi ---
PROGRAMMI := $(basename $(notdir $(wildcard utente/programmi/*.c)))
COMANDI   := $(basename $(notdir $(wildcard utente/comandi/*.c)))
PROVE     := $(basename $(notdir $(wildcard utente/prove/*.c)))

# i programmi del sistema (init, i servizi, la shell di prova)
$(addprefix $(O)/bin/,$(PROGRAMMI)): $(O)/bin/%: $(O)/utente/programmi/%.o $(LIBRERIE)
	$(call lega,$@,$< $(LIBRERIE))
# i comandi: programmi senza grafica, solo CoreKit (SISTEMA.md)
$(addprefix $(O)/bin/comandi/,$(COMANDI)): $(O)/bin/comandi/%: $(O)/utente/comandi/%.o $(COREKIT)
	$(call lega,$@,$< $(COREKIT))
# i programmi delle prove (utente/prove): solo nei dischi delle prove
$(addprefix $(O)/bin/prove/,$(PROVE)): $(O)/bin/prove/%: $(O)/utente/prove/%.o $(LIBRERIE)
	$(call lega,$@,$< $(LIBRERIE))
# avvio, il programma dell'archivio di avvio: niente librerie nostre
$(O)/bin/avvio: $(O)/utente/avvio/avvio.o
	$(call lega,$@,$<)

# --- le applicazioni: il programma e il pacchetto ---
# L'id dell'app (nel manifesto) e' il nome della cartella dei sorgenti in minuscolo.
# Il pacchetto dipende dal programma, dal manifesto e da ogni altro file della cartella
# (le risorse): se uno cambia, impacchetta.sh lo rifa'.
APP_CARTELLE := $(sort $(wildcard utente/applicazioni/*/ utente/prove/applicazioni/*/))
minuscolo = $(shell echo '$(1)' | tr A-Z a-z)
APP_ID :=
define APP
$(2)_OGGETTI := $$(patsubst %.c,$(O)/%.o,$$(sort $$(wildcard $(1)*.c)))
$(2)_FILE := $$(filter-out %.c %.h,$$(shell find $(1) -type f))
$(O)/app/$(2)/code: $$($(2)_OGGETTI) $$(LIBRERIE)
	$$(call lega,$$@,$$($(2)_OGGETTI) $$(LIBRERIE))
$(O)/pacchetti/$(2).app: $(O)/app/$(2)/code $$($(2)_FILE) strumenti/compilazione/impacchetta.sh
	sh strumenti/compilazione/impacchetta.sh $(1) $(2) $(CPU) $(O)/app/$(2)/code $(O)/pacchetti
APP_ID += $(2)
endef
$(foreach c,$(APP_CARTELLE),$(eval $(call APP,$(c),$(call minuscolo,$(notdir $(patsubst %/,%,$(c)))))))

# --- tutto ---
PROGRAMMI_PRONTI := $(addprefix $(O)/bin/,$(PROGRAMMI) avvio) \
                    $(addprefix $(O)/bin/comandi/,$(COMANDI)) \
                    $(addprefix $(O)/bin/prove/,$(PROVE))
PACCHETTI_PRONTI := $(foreach a,$(APP_ID),$(O)/pacchetti/$(a).app)

tutto: $(PROGRAMMI_PRONTI) $(PACCHETTI_PRONTI)
.PHONY: tutto
.DEFAULT_GOAL := tutto

-include $(shell find $(O) -name '*.d' 2>/dev/null)
