/* UEFI: solo i pezzi che servono al caricatore, scritti a mano.
 *
 * UEFI non e' una libreria: e' una manciata di TABELLE DI PUNTATORI A FUNZIONI che il
 * firmware ci passa all'avvio. efi_main riceve la "tabella di sistema"; dentro ci sono
 * la console (per scrivere), i "servizi di avvio" (memoria, protocolli, uscita) e la
 * tabella di configurazione (dove il firmware appende, tra l'altro, l'ACPI).
 *
 * I PROTOCOLLI. Tutto il resto (il disco, il file system, lo schermo) e' un "protocollo":
 * un'altra tabella di funzioni, riconosciuta da un GUID (un numero di 128 bit). Si
 * chiede al firmware "dammi il protocollo con questo GUID" e lui da' il puntatore.
 *
 * L'ORDINE CONTA. Le strutture qui sotto devono avere i campi ESATTAMENTE nell'ordine
 * della specifica UEFI (2.x): il firmware le ha riempite cosi', e noi leggiamo i
 * puntatori per posizione. I campi che non usiamo restano come "void *" segnaposto.
 *
 * LA CONVENZIONE DI CHIAMATA. Il firmware UEFI su x86-64 usa quella di Microsoft (primo
 * argomento in RCX, poi RDX, R8, R9). Compiliamo per "x86_64-windows", cosi' il
 * compilatore la usa da solo per tutte le funzioni, anche quelle chiamate da qui. */
#ifndef UEFI_H
#define UEFI_H

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef u64                uintn;      /* "UINTN": grande come un puntatore */
typedef u16                char16;     /* i testi UEFI sono UTF-16 (-fshort-wchar: L"..") */
typedef uintn              efi_stato;  /* EFI_STATUS */
typedef void              *efi_handle;

/* I risultati. Gli errori hanno il bit piu' alto acceso. */
#define EFI_ERRORE(n)            (0x8000000000000000ULL | (n))
#define EFI_SUCCESS              0
#define EFI_LOAD_ERROR           EFI_ERRORE(1)
#define EFI_INVALID_PARAMETER    EFI_ERRORE(2)
#define EFI_UNSUPPORTED          EFI_ERRORE(3)
#define EFI_BUFFER_TOO_SMALL     EFI_ERRORE(5)
#define EFI_NOT_FOUND            EFI_ERRORE(14)

typedef struct {
    u32 a;
    u16 b, c;
    u8  d[8];
} efi_guid;

typedef struct {
    u64 firma;
    u32 revisione, dimensione, crc32, riservato;
} efi_intestazione;

/* --- la console di testo (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL) --- */
typedef struct efi_testo efi_testo;
struct efi_testo {
    void *Reset;
    efi_stato (*OutputString)(efi_testo *, const char16 *);
    void *TestString, *QueryMode, *SetMode, *SetAttribute;
    efi_stato (*ClearScreen)(efi_testo *);
    void *SetCursorPosition, *EnableCursor, *Mode;
};

/* --- la mappa della memoria --- */
/* I tipi di memoria UEFI (EFI_MEMORY_TYPE). */
enum {
    EfiReservedMemoryType, EfiLoaderCode, EfiLoaderData, EfiBootServicesCode,
    EfiBootServicesData, EfiRuntimeServicesCode, EfiRuntimeServicesData,
    EfiConventionalMemory, EfiUnusableMemory, EfiACPIReclaimMemory, EfiACPIMemoryNVS,
    EfiMemoryMappedIO, EfiMemoryMappedIOPortSpace, EfiPalCode, EfiPersistentMemory,
};

/* Una riga della mappa (EFI_MEMORY_DESCRIPTOR). ATTENZIONE: nella mappa le righe NON
 * sono lunghe sizeof(efi_descrittore), ma quanto dice GetMemoryMap ("dimensione del
 * descrittore"): il firmware puo' aggiungere campi in fondo. Si scorre a passi di quella. */
typedef struct {
    u32 tipo;
    u32 riempitivo;
    u64 fisico;
    u64 virtuale;
    u64 pagine;        /* pagine da 4096 byte */
    u64 attributi;
} efi_descrittore;

/* AllocatePages: come scegliere l'indirizzo. */
enum { AllocateAnyPages, AllocateMaxAddress, AllocateAddress };

/* --- i servizi di avvio (EFI_BOOT_SERVICES) --- */
typedef struct {
    efi_intestazione intestazione;
    void *RaiseTPL, *RestoreTPL;
    efi_stato (*AllocatePages)(u32 modo, u32 tipo, uintn pagine, u64 *indirizzo);
    efi_stato (*FreePages)(u64 indirizzo, uintn pagine);
    efi_stato (*GetMemoryMap)(uintn *dimensione, efi_descrittore *mappa, uintn *chiave,
                              uintn *dim_descrittore, u32 *versione);
    efi_stato (*AllocatePool)(u32 tipo, uintn byte, void **dove);
    efi_stato (*FreePool)(void *dove);
    void *CreateEvent, *SetTimer, *WaitForEvent, *SignalEvent, *CloseEvent, *CheckEvent;
    void *InstallProtocolInterface, *ReinstallProtocolInterface, *UninstallProtocolInterface;
    efi_stato (*HandleProtocol)(efi_handle, const efi_guid *, void **);
    void *Reserved, *RegisterProtocolNotify, *LocateHandle, *LocateDevicePath;
    void *InstallConfigurationTable;
    void *LoadImage, *StartImage, *Exit, *UnloadImage;
    efi_stato (*ExitBootServices)(efi_handle immagine, uintn chiave);
    void *GetNextMonotonicCount;
    efi_stato (*Stall)(uintn microsecondi);
    void *SetWatchdogTimer;
    void *ConnectController, *DisconnectController;
    void *OpenProtocol, *CloseProtocol, *OpenProtocolInformation;
    void *ProtocolsPerHandle, *LocateHandleBuffer;
    efi_stato (*LocateProtocol)(const efi_guid *, void *registrazione, void **);
    /* ... altri che non usiamo */
} efi_servizi_avvio;

/* --- la tabella di configurazione: coppie (GUID, puntatore) --- */
typedef struct {
    efi_guid guid;
    void *tabella;
} efi_configurazione;

/* --- la tabella di sistema (EFI_SYSTEM_TABLE) --- */
typedef struct {
    efi_intestazione intestazione;
    char16 *produttore_firmware;
    u32 revisione_firmware;
    efi_handle console_in_handle;
    void *ConIn;
    efi_handle console_out_handle;
    efi_testo *ConOut;
    efi_handle errori_handle;
    efi_testo *StdErr;
    void *RuntimeServices;
    efi_servizi_avvio *BootServices;
    uintn quante_configurazioni;
    efi_configurazione *configurazione;
} efi_tabella_sistema;

/* --- l'immagine caricata (EFI_LOADED_IMAGE_PROTOCOL): chi siamo e da dove veniamo --- */
#define GUID_IMMAGINE_CARICATA \
    { 0x5B1B31A1, 0x9562, 0x11d2, { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }
typedef struct {
    u32 revisione;
    efi_handle genitore;
    efi_tabella_sistema *sistema;
    efi_handle dispositivo;    /* DeviceHandle: il volume da cui il firmware ci ha letti */
    void *percorso;
    void *riservato;
    u32 dim_opzioni;
    void *opzioni;
    void *base;
    u64 dimensione;
    u32 tipo_codice, tipo_dati;
    void *Unload;
} efi_immagine_caricata;

/* --- il file system semplice (FAT) e i file --- */
#define GUID_FILE_SYSTEM \
    { 0x964E5B22, 0x6459, 0x11d2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }
#define GUID_INFO_FILE \
    { 0x09576E92, 0x6D3F, 0x11d2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }

typedef struct efi_file efi_file;
struct efi_file {
    u64 revisione;
    efi_stato (*Open)(efi_file *, efi_file **nuovo, const char16 *nome, u64 modo, u64 attr);
    efi_stato (*Close)(efi_file *);
    void *Delete;
    efi_stato (*Read)(efi_file *, uintn *byte, void *dove);
    void *Write, *GetPosition, *SetPosition;
    efi_stato (*GetInfo)(efi_file *, const efi_guid *, uintn *byte, void *dove);
    void *SetInfo, *Flush;
};
#define EFI_FILE_MODE_READ 1

/* EFI_FILE_INFO: ci serve solo la lunghezza del file. */
typedef struct {
    u64 dimensione_struttura;
    u64 lunghezza;
    u64 spazio_occupato;
    u8  date[48];
    u64 attributi;
    char16 nome[];
} efi_info_file;

typedef struct efi_file_system efi_file_system;
struct efi_file_system {
    u64 revisione;
    efi_stato (*OpenVolume)(efi_file_system *, efi_file **radice);
};

/* --- lo schermo (EFI_GRAPHICS_OUTPUT_PROTOCOL, "GOP") --- */
#define GUID_GOP \
    { 0x9042A9DE, 0x23DC, 0x4A38, { 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A } }

/* Come sono messi i colori in un pixel da 32 bit. */
enum {
    PixelRedGreenBlueReserved8BitPerColor,   /* byte 0 = rosso, 1 = verde, 2 = blu */
    PixelBlueGreenRedReserved8BitPerColor,   /* byte 0 = blu, 1 = verde, 2 = rosso */
    PixelBitMask,                            /* lo dicono le maschere */
    PixelBltOnly,                            /* niente memoria dei pixel: inutile a noi */
};

typedef struct {
    u32 versione;
    u32 larghezza, altezza;
    u32 formato;
    u32 maschera_rosso, maschera_verde, maschera_blu, maschera_riservata;
    u32 pixel_per_riga;
} efi_modo_info;

typedef struct {
    u32 quanti_modi;
    u32 modo;                  /* quello acceso adesso */
    efi_modo_info *info;
    uintn dim_info;
    u64 memoria;               /* FrameBufferBase */
    uintn dim_memoria;
} efi_gop_modo;

typedef struct efi_gop efi_gop;
struct efi_gop {
    efi_stato (*QueryMode)(efi_gop *, u32 modo, uintn *dim_info, efi_modo_info **info);
    efi_stato (*SetMode)(efi_gop *, u32 modo);
    void *Blt;
    efi_gop_modo *Mode;
};

/* --- l'ACPI nella tabella di configurazione --- */
#define GUID_ACPI_20 \
    { 0x8868E871, 0xE4F1, 0x11D3, { 0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81 } }
#define GUID_ACPI_10 \
    { 0xEB9D2D30, 0x2D88, 0x11D3, { 0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } }

#endif
