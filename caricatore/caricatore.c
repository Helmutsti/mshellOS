/* IL CARICATORE UEFI: il primo programma nostro che gira su un PC.
 *
 * CHI LO FA PARTIRE. All'accensione il firmware UEFI (quello che una volta era il BIOS)
 * cerca un disco con una partizione FAT e dentro il file \EFI\BOOT\BOOTX64.EFI: e' il
 * posto "di riserva" che ogni firmware prova, senza dover configurare niente. Quel file
 * siamo noi: un programma in formato PE (lo stesso degli .exe di Windows), gia' a 64 bit,
 * con la paginazione accesa e la memoria mappata uno a uno (indirizzo = indirizzo fisico).
 *
 * COSA FA, IN ORDINE:
 *   1. legge il kernel, \mkernel\kernel, dallo stesso disco da cui e' partito;
 *   2. lo mette in memoria agli indirizzi fisici scritti nel kernel (un ELF a 64 bit);
 *   3. chiede al firmware lo schermo (GOP) e dove sono le tabelle ACPI;
 *   4. si fa dare la mappa della memoria e CHIUDE i servizi del firmware
 *      (ExitBootServices): da li' in poi il firmware non c'e' piu', la macchina e' nostra;
 *   5. traduce la mappa nella forma che capisce il kernel (comune/consegna.h);
 *   6. spegne le interruzioni e salta al kernel, passandogli la "consegna".
 *
 * PERCHE' UN CARICATORE NOSTRO. Fino a ieri il PC partiva con GRUB e il protocollo
 * Multiboot, a 32 bit. Un kernel a 32 bit vede al massimo 4 GB, e sui PC di oggi la
 * memoria sopra i 4 GB e' quasi tutta la RAM. Il caricatore e' piccolo, e cosi' il
 * kernel riceve esattamente quello che gli serve, nella forma che gli serve.
 *
 * NIENTE LIBRERIE. Non usiamo gnu-efi ne' EDK2: le poche strutture UEFI che servono
 * sono scritte a mano in uefi.h. */
#include "uefi.h"
#include "consegna.h"

/* Il compilatore puo' chiamare da solo memset e memcpy (per azzerare una struttura,
 * per copiarne una): non c'e' libreria, quindi le scriviamo noi. */
void *memset(void *d, int c, uintn n)
{
    u8 *p = d;
    while (n--) *p++ = (u8)c;
    return d;
}

void *memcpy(void *d, const void *s, uintn n)
{
    u8 *p = d;
    const u8 *q = s;
    while (n--) *p++ = *q++;
    return d;
}

/* ===================== il formato ELF (solo i pezzi che leggiamo) ===================== */

typedef struct {
    u8  ident[16];        /* 0x7F 'E' 'L' 'F', classe, ordine dei byte, ... */
    u16 tipo;             /* 2 = eseguibile (EXEC) */
    u16 macchina;         /* 62 = x86-64 */
    u32 versione;
    u64 ingresso;         /* il punto d'ingresso */
    u64 inizio_segmenti;  /* dove sono, nel file, le righe dei segmenti */
    u64 inizio_sezioni;
    u32 flag;
    u16 dim_intestazione;
    u16 dim_segmento, quanti_segmenti;
    u16 dim_sezione, quante_sezioni, indice_nomi;
} elf64_intestazione;

typedef struct {
    u32 tipo;             /* 1 = PT_LOAD: da mettere in memoria */
    u32 flag;
    u64 posizione;        /* da dove, nel file */
    u64 virtuale;         /* a che indirizzo lo vede il kernel */
    u64 fisico;           /* a che indirizzo fisico va messo */
    u64 byte_file;        /* quanti byte copiare dal file */
    u64 byte_memoria;     /* quanti ne occupa in memoria (il resto si azzera: il .bss) */
    u64 allineamento;
} elf64_segmento;

#define PT_LOAD   1
#define ELF_EXEC  2
#define ELF_X8664 62

/* ===================== il necessario ===================== */

#define PAGINA 4096ULL
#define GIU(x)  ((x) & ~(PAGINA - 1))
#define SU(x)   (((x) + PAGINA - 1) & ~(PAGINA - 1))

static efi_tabella_sistema *sistema;
static efi_servizi_avvio *servizi;

/* I pezzi di memoria che contengono il kernel (pagine intere, in ordine di indirizzo,
 * senza sovrapposizioni): nella mappa diventano ZONA_KERNEL. */
#define MAX_PEZZI 32
static struct { u64 inizio, fine; } pezzi[MAX_PEZZI];
static int quanti_pezzi;

/* Scrivere sullo schermo, finche' il firmware c'e'. */
static void scrivi(const char16 *s)
{
    sistema->ConOut->OutputString(sistema->ConOut, s);
}

static void scrivi_numero(u64 n, int base)
{
    char16 buf[24];
    int i = 23;
    buf[i] = 0;
    do {
        int c = (int)(n % base);
        buf[--i] = (char16)(c < 10 ? '0' + c : 'A' + c - 10);
        n /= base;
    } while (n);
    if (base == 16) {
        buf[--i] = 'x';
        buf[--i] = '0';
    }
    scrivi(&buf[i]);
}

/* Un errore che non si puo' aggirare: lo si dice, si aspetta qualche secondo (perche'
 * qualcuno lo possa leggere) e si torna al firmware, che provera' il prossimo disco. */
static efi_stato fallisci(const char16 *messaggio, efi_stato stato)
{
    scrivi(L"\r\nmkernel: ERRORE: ");
    scrivi(messaggio);
    scrivi(L"\r\n");
    servizi->Stall(5 * 1000 * 1000);
    return stato ? stato : EFI_LOAD_ERROR;
}

static int stesso_guid(const efi_guid *a, const efi_guid *b)
{
    const u8 *p = (const u8 *)a, *q = (const u8 *)b;
    for (int i = 0; i < 16; i++)
        if (p[i] != q[i]) return 0;
    return 1;
}

/* ===================== 1. leggere il kernel dal disco ===================== */

/* Legge tutto \mkernel\kernel in memoria (del caricatore). */
static efi_stato leggi_kernel(efi_handle immagine, u8 **dati, u64 *lunghezza)
{
    static const efi_guid guid_immagine = GUID_IMMAGINE_CARICATA;
    static const efi_guid guid_fs = GUID_FILE_SYSTEM;
    static const efi_guid guid_info = GUID_INFO_FILE;
    efi_immagine_caricata *noi;
    efi_file_system *fs;
    efi_file *radice, *file;
    efi_stato s;

    /* Da quale disco veniamo? Ce lo dice il protocollo "immagine caricata". */
    s = servizi->HandleProtocol(immagine, &guid_immagine, (void **)&noi);
    if (s) return fallisci(L"non so da quale disco sono partito", s);
    /* Su quel disco, il file system (il firmware sa leggere la FAT). */
    s = servizi->HandleProtocol(noi->dispositivo, &guid_fs, (void **)&fs);
    if (s) return fallisci(L"il disco di avvio non ha un file system leggibile", s);
    s = fs->OpenVolume(fs, &radice);
    if (s) return fallisci(L"non riesco ad aprire il disco di avvio", s);
    s = radice->Open(radice, &file, L"\\mkernel\\kernel", EFI_FILE_MODE_READ, 0);
    if (s) return fallisci(L"manca il file \\mkernel\\kernel sul disco di avvio", s);

    /* Quanto e' lungo? Lo dice la "scheda" del file (EFI_FILE_INFO). Il nome in
     * fondo alla scheda ha lunghezza variabile: 512 byte bastano per il nostro. */
    u64 spazio[64];
    uintn dim = sizeof spazio;
    s = file->GetInfo(file, &guid_info, &dim, spazio);
    if (s) return fallisci(L"non riesco a sapere quanto e' lungo il kernel", s);
    *lunghezza = ((efi_info_file *)spazio)->lunghezza;

    s = servizi->AllocatePool(EfiLoaderData, *lunghezza, (void **)dati);
    if (s) return fallisci(L"non c'e' memoria per leggere il kernel", s);
    /* Read puo' dare meno byte di quelli chiesti: si continua finche' non sono tutti. */
    u64 letti = 0;
    while (letti < *lunghezza) {
        dim = *lunghezza - letti;
        s = file->Read(file, &dim, *dati + letti);
        if (s) return fallisci(L"errore leggendo il kernel dal disco", s);
        if (dim == 0) return fallisci(L"il file del kernel finisce prima del previsto", 0);
        letti += dim;
    }
    file->Close(file);
    radice->Close(radice);
    return EFI_SUCCESS;
}

/* ===================== 2. mettere il kernel in memoria ===================== */

/* Controlla l'ELF, chiede al firmware le pagine agli indirizzi fisici dei segmenti, ci
 * copia il kernel. In *ingresso l'indirizzo FISICO da cui partire. */
static efi_stato carica_kernel(u8 *dati, u64 lunghezza, u64 *ingresso,
                               u64 *inizio, u64 *fine)
{
    elf64_intestazione *e = (elf64_intestazione *)dati;

    if (lunghezza < sizeof *e || e->ident[0] != 0x7F || e->ident[1] != 'E' ||
        e->ident[2] != 'L' || e->ident[3] != 'F')
        return fallisci(L"\\mkernel\\kernel non e' un file ELF", 0);
    if (e->ident[4] != 2 || e->ident[5] != 1)   /* classe 64 bit, byte "little endian" */
        return fallisci(L"il kernel non e' un ELF a 64 bit", 0);
    if (e->macchina != ELF_X8664)
        return fallisci(L"il kernel non e' per x86-64", 0);
    if (e->tipo != ELF_EXEC)
        return fallisci(L"il kernel non e' un eseguibile (ELF EXEC)", 0);
    if (e->dim_segmento < sizeof(elf64_segmento) ||
        e->inizio_segmenti + (u64)e->quanti_segmenti * e->dim_segmento > lunghezza)
        return fallisci(L"le righe dei segmenti del kernel escono dal file", 0);

    *inizio = ~0ULL;
    *fine = 0;
    *ingresso = 0;
    quanti_pezzi = 0;
    u64 gia_preso = 0;   /* fin dove arrivano le pagine gia' chieste */

    /* I segmenti PT_LOAD sono in ordine di indirizzo (lo vuole il formato ELF). Due
     * segmenti vicini possono stare nella stessa pagina: la pagina si chiede una volta. */
    for (int i = 0; i < e->quanti_segmenti; i++) {
        elf64_segmento *p = (elf64_segmento *)(dati + e->inizio_segmenti +
                                               (u64)i * e->dim_segmento);
        if (p->tipo != PT_LOAD || p->byte_memoria == 0) continue;
        if (p->byte_file > p->byte_memoria || p->posizione + p->byte_file > lunghezza)
            return fallisci(L"un segmento del kernel e' rovinato", 0);

        u64 da = GIU(p->fisico), a = SU(p->fisico + p->byte_memoria);
        if (da < gia_preso) da = gia_preso;
        if (da < a) {
            u64 indirizzo = da;
            efi_stato s = servizi->AllocatePages(AllocateAddress, EfiLoaderData,
                                                 (a - da) / PAGINA, &indirizzo);
            if (s) {
                scrivi(L"\r\nmkernel: le pagine da ");
                scrivi_numero(da, 16);
                scrivi(L" a ");
                scrivi_numero(a, 16);
                scrivi(L" sono gia' occupate dal firmware");
                return fallisci(L"non posso mettere il kernel dove chiede di stare", s);
            }
            if (quanti_pezzi > 0 && pezzi[quanti_pezzi - 1].fine == da)
                pezzi[quanti_pezzi - 1].fine = a;
            else if (quanti_pezzi < MAX_PEZZI) {
                pezzi[quanti_pezzi].inizio = da;
                pezzi[quanti_pezzi].fine = a;
                quanti_pezzi++;
            } else
                return fallisci(L"il kernel ha troppi segmenti sparsi", 0);
            gia_preso = a;
        }
        /* la parte che c'e' nel file si copia, il resto (il .bss) si azzera */
        memcpy((void *)p->fisico, dati + p->posizione, p->byte_file);
        memset((void *)(p->fisico + p->byte_file), 0, p->byte_memoria - p->byte_file);

        if (GIU(p->fisico) < *inizio) *inizio = GIU(p->fisico);
        if (SU(p->fisico + p->byte_memoria) > *fine) *fine = SU(p->fisico + p->byte_memoria);

        /* Il punto d'ingresso e' un indirizzo come lo vede il kernel (virtuale). Ora la
         * memoria e' mappata uno a uno: lo traduciamo nel fisico corrispondente. */
        if (e->ingresso >= p->virtuale && e->ingresso < p->virtuale + p->byte_memoria)
            *ingresso = e->ingresso - p->virtuale + p->fisico;
    }
    if (quanti_pezzi == 0)
        return fallisci(L"il kernel non ha niente da mettere in memoria", 0);
    if (*ingresso == 0)
        return fallisci(L"il punto d'ingresso del kernel non sta in nessun segmento", 0);
    return EFI_SUCCESS;
}

/* ===================== 3. lo schermo e l'ACPI ===================== */

static int formato_buono(const efi_modo_info *i)
{
    if (i->formato == PixelRedGreenBlueReserved8BitPerColor ||
        i->formato == PixelBlueGreenRedReserved8BitPerColor)
        return 1;
    return i->formato == PixelBitMask && i->maschera_rosso && i->maschera_verde &&
           i->maschera_blu;
}

/* Di quanti bit e' spostato un colore: dove sta il primo bit acceso della maschera. */
static u8 spostamento(u32 maschera)
{
    u8 n = 0;
    while (!(maschera & 1)) {
        maschera >>= 1;
        n++;
    }
    return n;
}

/* Sceglie il modo dello schermo: 1280x800 se c'e' (QEMU ce l'ha), se no quello acceso
 * adesso (di solito quello nativo del monitor), se no il piu' grande che va bene. */
static void prepara_schermo(struct consegna *c)
{
    static const efi_guid guid_gop = GUID_GOP;
    efi_gop *gop;
    if (servizi->LocateProtocol(&guid_gop, 0, (void **)&gop)) {
        scrivi(L"mkernel: attenzione, il firmware non da' uno schermo grafico\r\n");
        return;
    }
    u32 scelto = ~0u, piu_grande = ~0u;
    u64 area_massima = 0;
    for (u32 m = 0; m < gop->Mode->quanti_modi; m++) {
        efi_modo_info *info;
        uintn dim;
        if (gop->QueryMode(gop, m, &dim, &info) || !formato_buono(info)) continue;
        if (info->larghezza == 1280 && info->altezza == 800) scelto = m;
        if ((u64)info->larghezza * info->altezza > area_massima) {
            area_massima = (u64)info->larghezza * info->altezza;
            piu_grande = m;
        }
    }
    if (scelto == ~0u && formato_buono(gop->Mode->info)) scelto = gop->Mode->modo;
    if (scelto == ~0u) scelto = piu_grande;
    if (scelto == ~0u) {
        scrivi(L"mkernel: attenzione, nessun modo dello schermo a 32 bit per pixel\r\n");
        return;
    }
    if (scelto != gop->Mode->modo && gop->SetMode(gop, scelto)) {
        scrivi(L"mkernel: attenzione, non riesco a cambiare modo allo schermo\r\n");
        if (!formato_buono(gop->Mode->info)) return;
    }

    efi_modo_info *info = gop->Mode->info;
    c->schermo = gop->Mode->memoria;
    c->larghezza = info->larghezza;
    c->altezza = info->altezza;
    c->passo = info->pixel_per_riga * 4;
    if (info->formato == PixelRedGreenBlueReserved8BitPerColor) {
        c->rosso = 0; c->verde = 8; c->blu = 16;
    } else if (info->formato == PixelBlueGreenRedReserved8BitPerColor) {
        c->rosso = 16; c->verde = 8; c->blu = 0;
    } else {
        c->rosso = spostamento(info->maschera_rosso);
        c->verde = spostamento(info->maschera_verde);
        c->blu = spostamento(info->maschera_blu);
    }
    scrivi(L"mkernel: schermo ");
    scrivi_numero(c->larghezza, 10);
    scrivi(L"x");
    scrivi_numero(c->altezza, 10);
    scrivi(L"\r\n");
}

/* L'RSDP, la "radice" delle tabelle ACPI: il firmware la mette nella tabella di
 * configurazione. Meglio quella della versione 2 (ha anche l'XSDT a 64 bit). */
static u64 trova_rsdp(void)
{
    static const efi_guid guid_20 = GUID_ACPI_20, guid_10 = GUID_ACPI_10;
    u64 v1 = 0;
    for (uintn i = 0; i < sistema->quante_configurazioni; i++) {
        efi_configurazione *t = &sistema->configurazione[i];
        if (stesso_guid(&t->guid, &guid_20)) return (u64)t->tabella;
        if (stesso_guid(&t->guid, &guid_10)) v1 = (u64)t->tabella;
    }
    return v1;
}

/* ===================== 5. tradurre la mappa della memoria ===================== */
/* Tutto quello che segue gira DOPO ExitBootServices: niente firmware, niente
 * allocazioni. Si lavora in un quaderno fisso (lavoro[], nel .bss del caricatore). */

#define MAX_LAVORO 2048
static struct zona_consegna lavoro[MAX_LAVORO];
static int quante_lavoro;

static unsigned int tipo_zona(u32 tipo_uefi)
{
    switch (tipo_uefi) {
    /* Il codice e i dati del firmware e del caricatore: dopo ExitBootServices non
     * servono piu' a nessuno (ma dentro ci sono ancora la pila e le tabelle delle
     * pagine con cui il kernel parte: vedi consegna.h). */
    case EfiConventionalMemory:
    case EfiBootServicesCode:
    case EfiBootServicesData:
    case EfiLoaderCode:
    case EfiLoaderData:
        return ZONA_LIBERA;
    case EfiACPIReclaimMemory:
        return ZONA_ACPI;
    case EfiACPIMemoryNVS:
        return ZONA_ACPI_NVS;
    default:   /* i servizi "runtime" del firmware, i dispositivi, la RAM guasta... */
        return ZONA_RISERVATA;
    }
}

static void annota(u64 inizio, u64 fine, unsigned int tipo)
{
    if (inizio >= fine || quante_lavoro >= MAX_LAVORO) return;
    lavoro[quante_lavoro].inizio = inizio;
    lavoro[quante_lavoro].byte = fine - inizio;
    lavoro[quante_lavoro].tipo = tipo;
    lavoro[quante_lavoro].riservato = 0;
    quante_lavoro++;
}

static void traduci_mappa(struct consegna *c, u8 *mappa, uintn dim, uintn dim_desc)
{
    /* a) ogni riga UEFI diventa una zona; quelle libere si tagliano dove c'e' il kernel */
    quante_lavoro = 0;
    for (uintn o = 0; o + dim_desc <= dim; o += dim_desc) {
        efi_descrittore *d = (efi_descrittore *)(mappa + o);
        u64 a = d->fisico, b = d->fisico + d->pagine * PAGINA;
        unsigned int tipo = tipo_zona(d->tipo);
        if (tipo == ZONA_LIBERA) {
            for (int k = 0; k < quanti_pezzi; k++) {
                u64 pi = pezzi[k].inizio, pf = pezzi[k].fine;
                if (pf <= a || pi >= b) continue;
                annota(a, pi, ZONA_LIBERA);
                annota(pi > a ? pi : a, pf < b ? pf : b, ZONA_KERNEL);
                a = pf;
            }
        }
        annota(a, b, tipo);
    }

    /* b) in ordine di indirizzo (la specifica non promette che la mappa lo sia) */
    for (int i = 1; i < quante_lavoro; i++) {
        struct zona_consegna z = lavoro[i];
        int j = i - 1;
        while (j >= 0 && lavoro[j].inizio > z.inizio) {
            lavoro[j + 1] = lavoro[j];
            j--;
        }
        lavoro[j + 1] = z;
    }

    /* c) le zone attaccate dello stesso tipo diventano una */
    int n = 0;
    for (int i = 0; i < quante_lavoro; i++) {
        if (n > 0 && lavoro[n - 1].tipo == lavoro[i].tipo &&
            lavoro[n - 1].inizio + lavoro[n - 1].byte == lavoro[i].inizio)
            lavoro[n - 1].byte += lavoro[i].byte;
        else
            lavoro[n++] = lavoro[i];
    }

    /* d) troppe zone per la consegna? Si toglie la riservata piu' piccola, finche'
     * non stanno. Togliere una riservata non e' un rischio: cio' che non e' nella
     * mappa il kernel non lo usa comunque. (Se restano solo libere, si sacrifica la
     * libera piu' piccola: qualche pagina di RAM in meno, mai una pagina sbagliata.) */
    while (n > MAX_ZONE) {
        int via = -1;
        for (int pass = 0; pass < 2 && via < 0; pass++) {
            unsigned int cerco = pass == 0 ? ZONA_RISERVATA : ZONA_LIBERA;
            for (int i = 0; i < n; i++)
                if (lavoro[i].tipo == cerco &&
                    (via < 0 || lavoro[i].byte < lavoro[via].byte))
                    via = i;
        }
        if (via < 0) via = n - 1;
        for (int i = via; i < n - 1; i++) lavoro[i] = lavoro[i + 1];
        n--;
    }

    for (int i = 0; i < n; i++) c->zone[i] = lavoro[i];
    c->quante_zone = (unsigned int)n;
}

/* ===================== il programma ===================== */

typedef void __attribute__((sysv_abi)) (*ingresso_kernel)(struct consegna *);

efi_stato efi_main(efi_handle immagine, efi_tabella_sistema *st)
{
    efi_stato s;
    sistema = st;
    servizi = st->BootServices;

    scrivi(L"mkernel: caricatore UEFI\r\n");

    /* 1-2. il kernel */
    u8 *file;
    u64 lunghezza, ingresso, k_inizio, k_fine;
    s = leggi_kernel(immagine, &file, &lunghezza);
    if (s) return s;
    s = carica_kernel(file, lunghezza, &ingresso, &k_inizio, &k_fine);
    if (s) return s;
    servizi->FreePool(file);
    scrivi(L"mkernel: kernel caricato da ");
    scrivi_numero(k_inizio, 16);
    scrivi(L" a ");
    scrivi_numero(k_fine, 16);
    scrivi(L", ingresso ");
    scrivi_numero(ingresso, 16);
    scrivi(L"\r\n");

    /* 3. la consegna: sotto i 4 GB, cosi' il kernel la trova anche se all'inizio
     * mappa solo la memoria bassa */
    u64 indirizzo = 0xFFFFFFFFULL;
    s = servizi->AllocatePages(AllocateMaxAddress, EfiLoaderData,
                               SU(sizeof(struct consegna)) / PAGINA, &indirizzo);
    if (s) return fallisci(L"non c'e' memoria per la consegna", s);
    struct consegna *c = (struct consegna *)indirizzo;
    memset(c, 0, sizeof *c);
    c->firma = CONSEGNA_FIRMA;
    c->kernel_inizio = k_inizio;
    c->kernel_fine = k_fine;
    prepara_schermo(c);
    c->acpi_rsdp = trova_rsdp();
    if (!c->acpi_rsdp) scrivi(L"mkernel: attenzione, il firmware non da' le tabelle ACPI\r\n");

    /* 4. la mappa della memoria. Prima si chiede quanto e' grande (con spazio zero il
     * firmware risponde "troppo piccolo" e dice quanto serve), poi si prende un posto
     * con margine: chiedere il posto aggiunge righe alla mappa stessa. */
    uintn dim = 0, chiave, dim_desc;
    u32 versione;
    u8 *mappa = 0;
    s = servizi->GetMemoryMap(&dim, 0, &chiave, &dim_desc, &versione);
    if (s != EFI_BUFFER_TOO_SMALL) return fallisci(L"il firmware non da' la mappa della memoria", s);
    uintn capienza = dim + 16 * dim_desc;
    s = servizi->AllocatePool(EfiLoaderData, capienza, (void **)&mappa);
    if (s) return fallisci(L"non c'e' memoria per la mappa", s);
    scrivi(L"mkernel: chiudo il firmware e parto\r\n");

    /* ExitBootServices vuole la "chiave" dell'ultima mappa letta: se nel frattempo la
     * memoria e' cambiata (anche solo per un evento del firmware) risponde
     * EFI_INVALID_PARAMETER, e si rilegge la mappa. Tra la lettura e la chiusura non si
     * chiama NIENT'ALTRO del firmware: neanche scrivere sullo schermo, che puo'
     * allocare memoria e cambiare la mappa. */
    for (int tentativo = 0;; tentativo++) {
        dim = capienza;
        s = servizi->GetMemoryMap(&dim, (efi_descrittore *)mappa, &chiave, &dim_desc,
                                  &versione);
        if (s == EFI_BUFFER_TOO_SMALL && tentativo < 8) {
            /* la mappa e' cresciuta: posto nuovo (lecito: si rilegge comunque) */
            servizi->FreePool(mappa);
            capienza = dim + 16 * dim_desc;
            if (servizi->AllocatePool(EfiLoaderData, capienza, (void **)&mappa))
                return fallisci(L"non c'e' memoria per la mappa", s);
            continue;
        }
        if (s) return fallisci(L"il firmware non da' la mappa della memoria", s);
        s = servizi->ExitBootServices(immagine, chiave);
        if (s == EFI_SUCCESS) break;
        /* Dopo un ExitBootServices fallito si puo' solo rileggere la mappa: niente
         * messaggi. Se dopo tanti tentativi ancora non va, si torna al firmware. */
        if (s != EFI_INVALID_PARAMETER || tentativo >= 8) return s;
    }

    /* Da qui il firmware non c'e' piu'. */
    traduci_mappa(c, mappa, dim, dim_desc);

    /* 6. al kernel: interruzioni spente, RDI = la consegna (convenzione System V) */
    __asm__ volatile("cli");
    ((ingresso_kernel)ingresso)(c);

    /* il kernel non torna mai; se tornasse, ci si ferma qui */
    for (;;) __asm__ volatile("cli; hlt");
}
