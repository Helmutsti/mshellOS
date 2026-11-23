/* MSHELL: I TOKEN, cioe' i valori del disegno (colori, misure, ombre). Questo file non si compila
 * da solo: tinywl.c lo include prima delle sue misure.
 *
 * I valori non stanno nel codice: stanno in un solo elenco, /usr/share/mshell/token.css
 * (risorse/token.css nel progetto), che legge anche la barra (mshell-barra). Il compositore lo
 * legge una volta, quando parte (mshell_token_carica, da main): per cambiare un valore basta
 * cambiarlo li' e riavviare la scrivania, senza ricompilare.
 *
 * DEL FOGLIO DI STILE si leggono solo i blocchi ("window", ".chiaro", ".scuro") e le loro righe
 * "--nome: valore;". Un valore puo' essere un colore (#rrggbb, #rrggbbaa, rgba(r, g, b, a)), un
 * numero (con o senza "px") o var(--altro), cioe' il valore di un altro token. Un nome si cerca
 * prima nel blocco del tema e poi in "window".
 *
 * IL TEMA e' quello del sistema, come per la barra: la variabile d'ambiente MSHELL_TEMA
 * (chiaro o scuro; se manca, scuro). Lo seguono i controlli sulle finestre, il pannello "Disponi",
 * i suggerimenti e le ombre (scrivania.c, effetti.c). */
#define MSHELL_TOKEN_FILE "/usr/share/mshell/token.css"
#define MSHELL_TOKEN_MAX 1024

struct mshell_token {
	char *blocco, *nome, *valore;
};
static struct mshell_token mshell_token_elenco[MSHELL_TOKEN_MAX];
static int mshell_token_quanti;

/* I valori che usa il compositore, letti all'avvio. */
static struct {
	int raggio_finestra, raggio_controllo;  /* raggio/finestra, raggio/controllo */
	int spazio_16, spazio_24, spazio_32;    /* spazio/16, spazio/24, spazio/32 */
	int indicatore;                         /* indicatore/spessore */
	float focus[4];                         /* bordo/focus */
	float vetro[4];                         /* superficie/vetro */
} mshell_t;

/* Una copia di testo senza spazi ai bordi. */
static char *mshell_token_pulito(const char *da, const char *a) {
	while (da < a && (*da == ' ' || *da == '\t' || *da == '\n' || *da == '\r')) {
		da++;
	}
	while (a > da && (a[-1] == ' ' || a[-1] == '\t' || a[-1] == '\n' || a[-1] == '\r')) {
		a--;
	}
	return strndup(da, a - da);
}

/* Il valore scritto per "nome" (senza i due trattini), cercato nel blocco del tema e poi in
 * "window"; NULL se non c'e'. */
static const char *mshell_token_scritto(const char *nome) {
	const char *tema = getenv("MSHELL_TEMA");
	const char *blocchi[] = { tema != NULL && strcmp(tema, "chiaro") == 0 ? ".chiaro" : ".scuro", "window" };
	for (int b = 0; b < 2; b++) {
		for (int i = 0; i < mshell_token_quanti; i++) {
			struct mshell_token *t = &mshell_token_elenco[i];
			if (strcmp(t->blocco, blocchi[b]) == 0 && strcmp(t->nome, nome) == 0) {
				return t->valore;
			}
		}
	}
	return NULL;
}

/* Il valore di "nome", seguendo i var(--altro); NULL (e una riga nel registro) se non c'e'. */
static const char *mshell_token_valore(const char *nome) {
	const char *v = mshell_token_scritto(nome);
	for (int giri = 0; v != NULL && strncmp(v, "var(--", 6) == 0 && giri < 16; giri++) {
		char altro[128];
		size_t n = strcspn(v + 6, ")");
		if (n >= sizeof(altro)) {
			break;
		}
		memcpy(altro, v + 6, n);
		altro[n] = '\0';
		v = mshell_token_scritto(altro);
	}
	if (v == NULL) {
		wlr_log(WLR_ERROR, "mshell: manca il token --%s in %s", nome, MSHELL_TOKEN_FILE);
	}
	return v;
}

/* Un numero (le misure, in px). */
static int mshell_token_numero(const char *nome) {
	const char *v = mshell_token_valore(nome);
	return v != NULL ? (int)strtol(v, NULL, 10) : 0;
}

/* Un colore, con i componenti da 0 a 1 (non moltiplicati per l'opacita'). */
static void mshell_token_colore(const char *nome, float colore[4]) {
	const char *v = mshell_token_valore(nome);
	colore[0] = colore[1] = colore[2] = colore[3] = 0.0f;
	if (v == NULL) {
		return;
	}
	unsigned int r, g, b, a = 255;
	float fr, fg, fb, fa = 1.0f;
	size_t cifre = v[0] == '#' ? strspn(v + 1, "0123456789abcdefABCDEF") : 0;
	if ((cifre == 6 || cifre == 8) && sscanf(v + 1, "%2x%2x%2x%2x", &r, &g, &b, &a) >= 3) {
		colore[0] = r / 255.0f;
		colore[1] = g / 255.0f;
		colore[2] = b / 255.0f;
		colore[3] = a / 255.0f;
	} else if (sscanf(v, "rgba(%f , %f , %f , %f )", &fr, &fg, &fb, &fa) == 4 ||
			sscanf(v, "rgb(%f , %f , %f )", &fr, &fg, &fb) == 3) {
		colore[0] = fr / 255.0f;
		colore[1] = fg / 255.0f;
		colore[2] = fb / 255.0f;
		colore[3] = fa;
	} else {
		wlr_log(WLR_ERROR, "mshell: il token --%s non e' un colore: %s", nome, v);
	}
}

/* Legge il file dei token e prende i valori del compositore. */
static void mshell_token_carica(void) {
	FILE *f = fopen(MSHELL_TOKEN_FILE, "r");
	if (f == NULL) {
		wlr_log(WLR_ERROR, "mshell: non trovo %s: niente colori ne' misure del disegno",
			MSHELL_TOKEN_FILE);
		return;
	}
	char *testo = NULL;
	size_t lungo = 0;
	FILE *m = open_memstream(&testo, &lungo);
	char pezzo[4096];
	size_t n;
	while ((n = fread(pezzo, 1, sizeof(pezzo), f)) > 0) {
		fwrite(pezzo, 1, n, m);
	}
	fclose(f);
	fclose(m);
	/* via i commenti */
	for (char *c = strstr(testo, "/*"); c != NULL; c = strstr(c, "/*")) {
		char *fine = strstr(c + 2, "*/");
		char *dopo = fine != NULL ? fine + 2 : c + strlen(c);
		memset(c, ' ', dopo - c);
	}
	/* i blocchi "nome { --token: valore; ... }" */
	char *p = testo;
	char *apre;
	while ((apre = strchr(p, '{')) != NULL) {
		char *chiude = strchr(apre, '}');
		if (chiude == NULL) {
			break;
		}
		char *blocco = mshell_token_pulito(p, apre);
		for (char *riga = apre + 1; riga < chiude;) {
			char *fine = memchr(riga, ';', chiude - riga);
			if (fine == NULL) {
				fine = chiude;
			}
			char *due_punti = memchr(riga, ':', fine - riga);
			char *nome = due_punti != NULL ? mshell_token_pulito(riga, due_punti) : NULL;
			if (nome != NULL && strncmp(nome, "--", 2) == 0 &&
					mshell_token_quanti < MSHELL_TOKEN_MAX) {
				struct mshell_token *t = &mshell_token_elenco[mshell_token_quanti++];
				t->blocco = strdup(blocco);
				t->nome = strdup(nome + 2);
				t->valore = mshell_token_pulito(due_punti + 1, fine);
			}
			free(nome);
			riga = fine + 1;
		}
		free(blocco);
		p = chiude + 1;
	}
	free(testo);

	mshell_t.raggio_finestra = mshell_token_numero("raggio-finestra");
	mshell_t.raggio_controllo = mshell_token_numero("raggio-controllo");
	mshell_t.spazio_16 = mshell_token_numero("spazio-16");
	mshell_t.spazio_24 = mshell_token_numero("spazio-24");
	mshell_t.spazio_32 = mshell_token_numero("spazio-32");
	mshell_t.indicatore = mshell_token_numero("indicatore-spessore");
	mshell_token_colore("bordo-focus", mshell_t.focus);
	mshell_token_colore("superficie-vetro", mshell_t.vetro);
}
