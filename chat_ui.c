#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h> 
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <linux/types.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <gtk/gtk.h>
#include "sdr.h"
#include "sdr_ui.h"
#include "modem_ft8.h"

static GtkWidget *chat_window = NULL;
static GtkWidget *contacts_list = NULL;
static GtkWidget *text_view = NULL;
static GtkWidget *header = NULL;
static GtkWidget *presence_combo = NULL;
static unsigned int last_update_from_main = 0;

/* Structure to hold widgets we need to access from callbacks */
typedef struct {
    GtkWidget *text_view;
    GtkWidget *entry;
} AppWidgets;

/* --- Helper Functions --- */

/* Clears all items from the contacts list widget. */
void clear_contact_list() {
    GList *rows = gtk_container_get_children(GTK_CONTAINER(contacts_list));
    for (GList *iter = rows; iter != NULL; iter = iter->next) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(rows);
}

/* Adds a new contact to the contacts list widget.
 * 'contact_name' is the text displayed for the contact.
 * Each contact is wrapped in a GtkListBoxRow and the label is left aligned.
 */
void add_item_to_contact_list(const char *contact_name) {
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *label = gtk_label_new(contact_name);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);  // Left align the label
    gtk_container_add(GTK_CONTAINER(row), label);
    gtk_container_add(GTK_CONTAINER(contacts_list), row);
    gtk_widget_show_all(row);
}

/* Clears all text from the text view widget. */
void chat_clear() {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_text(buffer, "", -1);
}

/* Appends new text (followed by a newline) to the text view widget. */
void chat_append(const char *new_text) {
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
  GtkTextIter end_iter;
  gtk_text_buffer_get_end_iter(buffer, &end_iter);
  gtk_text_buffer_insert(buffer, &end_iter, new_text, -1);
  gtk_text_buffer_insert(buffer, &end_iter, "\n", -1);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(text_view), &end_iter, 0.0, FALSE, 0, 1.0);	
}


void chat_alert(const char *message){
	GtkWidget *alert_box = 
		gtk_message_dialog_new(GTK_WINDOW(chat_window),
			GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
			message);
	gtk_dialog_run(GTK_DIALOG(alert_box));
	gtk_widget_destroy(alert_box);
}

/*
void chat_scroll_to_end(){
  GtkTextIter end_iter;

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
  gtk_text_buffer_get_end_iter(buffer, &end_iter);
	gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(text_view), 	
}
*/

/* --- Dialog Function for Adding Contacts --- */

/* Creates and shows a modal dialog for adding a contact.
 * The dialog contains a text input (with a maximum length of 10 characters)
 * and validates that the entered "Callsign" has at least 3 letters.
 */
static void show_add_contact_dialog(GtkWindow *parent) {
    GtkWidget *dialog, *content_area, *entry, *label, *hbox;

    dialog = gtk_dialog_new_with_buttons("Add Contact",
                                           parent,
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           "_OK", GTK_RESPONSE_ACCEPT,
                                           "_Cancel", GTK_RESPONSE_REJECT,
                                           NULL);
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);

    label = gtk_label_new("Callsign:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

    entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry), 10);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 5);

    gtk_container_add(GTK_CONTAINER(content_area), hbox);
    gtk_widget_show_all(dialog);

    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
        if (g_utf8_strlen(text, -1) < 3) {
            GtkWidget *error_dialog = gtk_message_dialog_new(parent,
                                            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_OK,
                                            "Callsign must be at least 3 letters.");
            gtk_dialog_run(GTK_DIALOG(error_dialog));
            gtk_widget_destroy(error_dialog);
        } else {
            g_print("New contact callsign: %s\n", text);
						msg_add_contact(text);
            /* Optionally, add the contact to the contacts list:
             * add_item_to_contact_list(contacts_list, text);
             */
        }
    }
    gtk_widget_destroy(dialog);
}

/* --- Callbacks --- */

/* Callback for the "Add..." menu item in the context menu.
 * Retrieves the parent window (passed as user data) and shows the dialog.
 */
static void on_add_item_activate_cb(GtkMenuItem *menuitem, gpointer user_data) {
    GtkWindow *parent = GTK_WINDOW(user_data);
    show_add_contact_dialog(parent);
}

/* Callback for the "Add.." button on the header bar.
 * Its behavior is identical: launching the "Add Contact" dialog.
 */
static void on_add_button_clicked(GtkButton *button, gpointer user_data) {
    GtkWindow *parent = GTK_WINDOW(user_data);
    show_add_contact_dialog(parent);
}

/* Callback for the "Send" button.
 * Reads the text from the entry, appends it to the text view (using our helper),
 * and then clears the entry field.
 */
static void on_send_button_clicked(GtkButton *button, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    const gchar *entry_text = gtk_entry_get_text(GTK_ENTRY(widgets->entry));
    if (g_strcmp0(entry_text, "") != 0) {
      //chat_append(entry_text);
			if(msg_post(entry_text))
				chat_alert("Select a contact to send the messsage.");
			else
      	gtk_entry_set_text(GTK_ENTRY(widgets->entry), "");
    }
}

/* Generic callback for "Delete" and "Save" menu items.
 * In this example, it simply prints the label of the activated item.
 */
static void menuitem_activate_cb(GtkMenuItem *menuitem, gpointer user_data) {
	char const *p =  gtk_menu_item_get_label(menuitem);
	if(!strcmp(p, "Delete"))
		msg_remove_contact(field_str("CONTACT"));

   g_print("Activated menu item: %s\n", gtk_menu_item_get_label(menuitem));
}

/* Callback for handling right-click events on the contacts list.
 * When a right-click is detected, a context menu is created with
 * "Delete", "Save", and "Add..." items.
 */
static gboolean contacts_list_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        GtkWidget *menu = gtk_menu_new();
        GtkWidget *delete_item, *save_item, *add_item;
        GtkWidget *parent_window = gtk_widget_get_toplevel(widget);

        delete_item = gtk_menu_item_new_with_label("Delete");
        save_item   = gtk_menu_item_new_with_label("Save");
        add_item    = gtk_menu_item_new_with_label("Add...");

        g_signal_connect(delete_item, "activate", G_CALLBACK(menuitem_activate_cb), NULL);
        g_signal_connect(save_item,   "activate", G_CALLBACK(menuitem_activate_cb), NULL);
        g_signal_connect(add_item,    "activate", G_CALLBACK(on_add_item_activate_cb), parent_window);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), delete_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), save_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), add_item);
        gtk_widget_show_all(menu);

        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
        return TRUE;
    }
    if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
		}

    return FALSE;
}

const char*get_presence(){
	return gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(presence_combo));
}

void set_presence(const gchar *selection_text) {
    GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(presence_combo));
    GtkTreeIter iter;
    gboolean valid;
    gint index = 0;
    gboolean found = FALSE;

    /* Start with the first element in the model */
    valid = gtk_tree_model_get_iter_first(model, &iter);
    while (valid) {
        gchar *text = NULL;
        /* Assuming the text is stored in column 0 */
        gtk_tree_model_get(model, &iter, 0, &text, -1);
        if (text && g_strcmp0(text, selection_text) == 0) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(presence_combo), index);
            found = TRUE;
            g_free(text);
            break;
        }
        g_free(text);
        index++;
        valid = gtk_tree_model_iter_next(model, &iter);
    }
    if (!found) {
        g_print("Warning: '%s' is not a valid presence option.\n", selection_text);
    }
}

void on_contact_selected(GtkListBox *box, GtkListBoxRow *row, gpointer ud){
	GtkWidget *label = gtk_bin_get_child(GTK_BIN(row));
	const gchar *contact_text = gtk_label_get_text(GTK_LABEL(label));
	char callsign[10];
	int i;
	for (i =- 0; i < sizeof(callsign)-1 && *contact_text > ' '; i++)
		callsign[i] = *contact_text++;	
	callsign[i] = 0;
	g_print("Selected contact: %s\n", callsign);
	msg_select(callsign);
}

void on_presence_changed(GtkComboBox *combo, gpointer user_data){
	const char *selection = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
	if (selection)
		msg_presence(selection);
	
}

void chat_title(const char *title){
   gtk_header_bar_set_title(GTK_HEADER_BAR(header), title);
}

void chat_ui_init(){

	 chat_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title(GTK_WINDOW(chat_window), "HF Messenger");
   gtk_window_set_default_size(GTK_WINDOW(chat_window), 600, 400);

   GtkCssProvider *provider = gtk_css_provider_new();
	/*
   gtk_css_provider_load_from_data(provider,
       "window { background-color: #2e2e2e; }"
       "headerbar { background-color: #1e1e1e; color: #c0c0c0; }"
       "button { background-color: #444444; color: #c0c0c0; }"
       "entry { background-color: #444444; color: #c0c0c0; }"
       "textview { background-color: #444444; color: #c0c0c0; }"
       "list-row { background-color: #444444; color: #c0c0c0; }"
       "label { background-color: #444444; color: #00c0c0; }",
        -1, NULL);
	*/
   gtk_css_provider_load_from_data(provider,
       "window { background-color: #ffffff; }"
       "headerbar { background-color: #ffffff; color: #000000; }"
       "button { background-color: #ffffff; color: #000000; }"
       "entry { background-color: #ffffff; color: #000000; }"
       "textview { background-color: #444444; color: #c0c0c0; }"
       "list-row { background-color: #ffffff; color: #000000; }"
       "label { background-color: #ffffff; color: #000000; }",
        -1, NULL);
   gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
       GTK_STYLE_PROVIDER(provider),
       GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
   g_object_unref(provider);

   /* Create a vertical box to hold the header bar and the main content */
   GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
   gtk_container_add(GTK_CONTAINER(chat_window), vbox);

   /* --- HEADER BAR --- */
   header = gtk_header_bar_new();
   gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), FALSE);
   gtk_header_bar_set_title(GTK_HEADER_BAR(header), "(Select a Contact)");
   gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);

   /* Presence Combo Box on the right end */
   presence_combo = gtk_combo_box_text_new();
   gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(presence_combo), "READY");
   gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(presence_combo), "AWAY");
   gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(presence_combo), "BUSY");
   gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(presence_combo), "SILENT");
   gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(presence_combo), "QUD");
   gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(presence_combo), "QSP");
   gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(presence_combo), "CQ");
   gtk_combo_box_set_active(GTK_COMBO_BOX(presence_combo), 0);
   gtk_header_bar_pack_end(GTK_HEADER_BAR(header), presence_combo);
   g_signal_connect(presence_combo, "changed", G_CALLBACK(on_presence_changed), NULL);
	

   /* New "Add.." button on the left side of the header bar */
   GtkWidget *add_button = gtk_button_new_with_label("Add..");
   g_signal_connect(add_button, "clicked", G_CALLBACK(on_add_button_clicked), chat_window);
   gtk_header_bar_pack_start(GTK_HEADER_BAR(header), add_button);

   /* --- MAIN CONTENT AREA --- */
   /* Horizontal box splits the window into Contacts (left) and Chat (right) */
   GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

   /* Left Pane: Scrollable Contacts List */
   GtkWidget *contacts_scrolled = gtk_scrolled_window_new(NULL, NULL);
   gtk_widget_set_size_request(contacts_scrolled, 150, -1);
   gtk_box_pack_start(GTK_BOX(hbox), contacts_scrolled, FALSE, FALSE, 0);

   contacts_list = gtk_list_box_new();
   gtk_container_add(GTK_CONTAINER(contacts_scrolled), contacts_list);

   /* Enable right-click events on the contacts list */
   gtk_widget_add_events(contacts_list, GDK_BUTTON_PRESS_MASK);
   g_signal_connect(contacts_list, "button-press-event", G_CALLBACK(contacts_list_button_press_cb), NULL);
   g_signal_connect(contacts_list, "row-activated", G_CALLBACK(on_contact_selected), NULL);


    /* Right Pane: Chat Area (Messages and Input) */
    GtkWidget *chat_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(hbox), chat_vbox, TRUE, TRUE, 0);

    /* Chat messages area: a scrollable text view */
    GtkWidget *messages_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(chat_vbox), messages_scrolled, TRUE, TRUE, 0);

    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(messages_scrolled), text_view);

    /* Input area: horizontal box containing a text entry and a Send button */
    GtkWidget *entry_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(chat_vbox), entry_hbox, FALSE, FALSE, 0);

    GtkWidget *entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(entry_hbox), entry, TRUE, TRUE, 0);

    GtkWidget *send_button = gtk_button_new_with_label("Send");
    gtk_box_pack_start(GTK_BOX(entry_hbox), send_button, FALSE, FALSE, 0);

    /* Structure for accessing widgets in the Send callback */
    AppWidgets *widgets = g_slice_new(AppWidgets);
    widgets->text_view = text_view;
    widgets->entry = entry;
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_button_clicked), widgets);

    gtk_widget_show_all(chat_window);
}

