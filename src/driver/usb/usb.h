/* USB: quello che il controllore (xhci.c) offre ai driver dei dispositivi (hid.c).
 *
 * Un dispositivo USB parla per ENDPOINT, delle "caselle postali" numerate:
 *   - lo 0, di CONTROLLO, c'e' sempre: si fanno domande ("dammi il tuo descrittore")
 *     e si danno ordini ("usa la configurazione 1"). Ogni domanda e' un pacchetto di
 *     8 byte (il SETUP), poi eventualmente i dati, poi un "fatto" (lo STATUS);
 *   - gli altri li descrive il dispositivo. Tastiere e mouse ne hanno uno di
 *     INTERRUPT IN: il controllore lo interroga da solo a intervalli fissi, e quando
 *     il dispositivo ha qualcosa (un tasto, uno spostamento) arriva un "rapporto".
 *
 * Il controllore vede i dispositivi; chi sa cosa farne (una tastiera, un mouse) e'
 * il driver della CLASSE: qui solo HID, "dispositivi per le persone". */
#ifndef USB_H
#define USB_H

/* Le velocita', come le scrive il controllore. */
#define USB_FULL  1      /* 12 Mbit/s: tastiere e mouse */
#define USB_LOW   2      /* 1,5 Mbit/s: tastiere e mouse vecchi */
#define USB_HIGH  3      /* 480 Mbit/s: USB 2 */
#define USB_SUPER 4      /* 5 Gbit/s e oltre: USB 3 */

/* I descrittori: tipi e campi che servono. */
#define DESC_DISPOSITIVO     1
#define DESC_CONFIGURAZIONE  2
#define DESC_INTERFACCIA     4
#define DESC_ENDPOINT        5
#define DESC_HID_RAPPORTO    0x22

/* Le richieste standard (byte 1 del SETUP). */
#define RICH_GET_DESCRIPTOR    6
#define RICH_SET_CONFIGURATION 9

/* Il byte 0 del SETUP: direzione, tipo, destinatario. */
#define VERSO_IN         0x80   /* dal dispositivo a noi */
#define TIPO_CLASSE      0x20   /* una richiesta della classe (HID) */
#define A_INTERFACCIA    0x01

struct usb_dispositivo;
struct usb_endpoint;

/* Quando un endpoint interrupt consegna un rapporto: dentro l'interruzione del timer,
 * quindi corta. 'dati' vale solo durante la chiamata. */
typedef void (*usb_ricevi)(void *privato, const unsigned char *dati, unsigned int byte);

/* Una domanda sull'endpoint 0. Aspetta la risposta (fino a un secondo). Ritorna i
 * byte ricevuti (o 0 se non ci sono dati), -1 se il dispositivo ha detto no o non ha
 * risposto. 'buffer' deve stare in una pagina avuta da pagina_chiedi (il controllore
 * ci scrive da solo). */
int usb_controllo(struct usb_dispositivo *d, unsigned char tipo, unsigned char richiesta,
                  unsigned short valore, unsigned short indice, void *buffer, unsigned short byte);

/* Prepara un endpoint interrupt IN (dal suo descrittore). 0 se non ci riesce. */
struct usb_endpoint *usb_endpoint_prepara(struct usb_dispositivo *d, const unsigned char *desc_endpoint);

/* Comincia a ricevere i rapporti: da qui ogni rapporto arriva a 'ricevi'. */
void usb_endpoint_avvia(struct usb_endpoint *e, usb_ricevi ricevi, void *privato);

/* Il driver della classe: lo chiama il controllore per ogni dispositivo nuovo, con il
 * descrittore di configurazione completo. 1 se lo prende. */
int usb_hid_prova(struct usb_dispositivo *d, const unsigned char *config, unsigned int byte);

#endif
