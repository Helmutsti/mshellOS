/* LE ICONE della barra e dei menu, disegnate con cairo (niente librsvg, decisione della persona).
 * I tracciati sono copiati dalle icone Lucide (https://lucide.dev, licenza ISC): sono pensati su
 * una griglia di 24 con un tratto di 2, a capi e angoli tondi; qui si disegnano alla misura di
 * 16 px (la lente della ricerca del cassetto a 20), quindi tutto (tratto compreso) si scala di
 * 16/24 (o 20/24). Il colore e' quello del testo del
 * widget (CSS "color"), cosi' le icone seguono il tema come le scritte. */
#include "barra.h"

#define MISURA 16

/* Un arco di cerchio come lo vuole cairo: centro, raggio, angoli in gradi. Gli archi "A" dei
 * tracciati SVG di Lucide sono stati convertiti a mano in centro e angoli. */
static void arco(cairo_t *cr, double cx, double cy, double r, double da, double a, gboolean indietro) {
	if (indietro) {
		cairo_arc_negative(cr, cx, cy, r, da * G_PI / 180, a * G_PI / 180);
	} else {
		cairo_arc(cr, cx, cy, r, da * G_PI / 180, a * G_PI / 180);
	}
}

static void disegna(GtkDrawingArea *area, cairo_t *cr, int larghezza, int altezza, gpointer dati) {
	enum mshell_icona quale = GPOINTER_TO_INT(dati);
	GdkRGBA colore;
	gtk_widget_get_color(GTK_WIDGET(area), &colore);
	gdk_cairo_set_source_rgba(cr, &colore);
	cairo_scale(cr, larghezza / 24.0, altezza / 24.0);
	cairo_set_line_width(cr, 2);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
	switch (quale) {
	case ICONA_CAMPANELLA:
		/* lucide "bell":
		 *   M10.268 21a2 2 0 0 0 3.464 0
		 *   M3.262 15.326A1 1 0 0 0 4 17h16a1 1 0 0 0 .74-1.673C19.41 13.956 18 12.499 18 8
		 *   A6 6 0 0 0 6 8c0 4.499-1.411 5.956-2.738 7.326 */
		cairo_new_sub_path(cr);
		arco(cr, 12, 20, 2, 150, 30, TRUE);
		cairo_new_sub_path(cr);
		arco(cr, 4, 16, 1, 222.4, 90, TRUE);
		cairo_line_to(cr, 20, 17);
		arco(cr, 20, 16, 1, 90, -42.3, TRUE);
		cairo_curve_to(cr, 19.41, 13.956, 18, 12.499, 18, 8);
		arco(cr, 12, 8, 6, 0, -180, TRUE);
		cairo_curve_to(cr, 6, 12.499, 4.589, 13.956, 3.262, 15.326);
		break;
	case ICONA_MENU:
		/* lucide "menu": tre righe da x 4 a 20, a y 6, 12, 18 */
		for (int y = 6; y <= 18; y += 6) {
			cairo_move_to(cr, 4, y);
			cairo_line_to(cr, 20, y);
		}
		break;
	case ICONA_FRECCIA_SINISTRA:
		/* lucide "chevron-left": m15 18-6-6 6-6 */
		cairo_move_to(cr, 15, 18);
		cairo_line_to(cr, 9, 12);
		cairo_line_to(cr, 15, 6);
		break;
	case ICONA_FRECCIA_DESTRA:
		/* lucide "chevron-right": m9 18 6-6-6-6 */
		cairo_move_to(cr, 9, 18);
		cairo_line_to(cr, 15, 12);
		cairo_line_to(cr, 9, 6);
		break;
	case ICONA_CERCA:
		/* lucide "search": m21 21-4.34-4.34, cerchio di centro 11,11 e raggio 8 */
		cairo_move_to(cr, 21, 21);
		cairo_line_to(cr, 16.66, 16.66);
		cairo_new_sub_path(cr);
		cairo_arc(cr, 11, 11, 8, 0, 2 * G_PI);
		break;
	}
	cairo_stroke(cr);
}

GtkWidget *mshell_icona_nuova_misura(enum mshell_icona quale, int misura) {
	GtkWidget *area = gtk_drawing_area_new();
	gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(area), misura);
	gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(area), misura);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), disegna, GINT_TO_POINTER(quale), NULL);
	gtk_widget_add_css_class(area, "icona");
	return area;
}

GtkWidget *mshell_icona_nuova(enum mshell_icona quale) {
	return mshell_icona_nuova_misura(quale, MISURA);
}
