#include <stdlib.h>
#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include <glade/glade.h>
#include <libaccounts/account-plugin.h>
#include <librtcom-accounts-widgets/rtcom-account-plugin.h>
#include <librtcom-accounts-widgets/rtcom-dialog-context.h>
#include <librtcom-accounts-widgets/rtcom-login.h>
#include <librtcom-accounts-widgets/rtcom-edit.h>
#include <librtcom-accounts-widgets/rtcom-param-int.h>
#include <hildon-uri.h>
#include <config.h>

#define GTALK_FORGOT_PASSWORD_URI "https://www.google.com/accounts/ForgotPasswd?service=mail&fpOnly=1"
#define GTALK_NEW_ACCOUNT_URI "https://www.google.com/accounts/NewAccount?service=mail&hl="

typedef struct _GtalkPluginClass GtalkPluginClass;
typedef struct _GtalkPlugin GtalkPlugin;

struct _GtalkPlugin {
  RtcomAccountPlugin parent_instance;
};

struct _GtalkPluginClass {
  RtcomAccountPluginClass parent_class;
};

RtcomAccountPluginClass *parent_class;

ACCOUNT_DEFINE_PLUGIN(GtalkPlugin, gtalk_plugin, RTCOM_TYPE_ACCOUNT_PLUGIN);

static void gtalk_plugin_init(GtalkPlugin *self)
{
  RTCOM_ACCOUNT_PLUGIN(self)->name = "google-talk";
  RTCOM_ACCOUNT_PLUGIN(self)->username_prefill = "@gmail.com";
  RTCOM_ACCOUNT_PLUGIN(self)->capabilities = RTCOM_PLUGIN_CAPABILITY_ALL;
  rtcom_account_plugin_add_service(RTCOM_ACCOUNT_PLUGIN(self), "google-talk");
  glade_init();
}

static void set_widget_setting (gpointer key, gpointer value, gpointer userdata)
{
  GtkWidget *widget = key;

  if (RTCOM_IS_PARAM_INT(widget)) {
      rtcom_param_int_set_value(RTCOM_PARAM_INT(widget),
                                GPOINTER_TO_INT(value));
  } else if (GTK_IS_ENTRY(widget)) {
      gtk_entry_set_text(GTK_ENTRY(widget), value);
  } else if (HILDON_IS_CHECK_BUTTON(widget)) {
      hildon_check_button_set_active(HILDON_CHECK_BUTTON(widget),
                                     GPOINTER_TO_INT(value));
  } else {
      g_warning ("%s: unhandled widget type %s (%s)",
                 G_STRFUNC,
                 g_type_name (G_TYPE_FROM_INSTANCE(widget)),
                 gtk_widget_get_name(widget));
  }
}

static void get_advanced_settings (GtkWidget *widget,
                                   GHashTable *advanced_settings)
{
  const gchar *name;

  name = gtk_widget_get_name (widget);

  if (RTCOM_IS_PARAM_INT (widget))
    {
      gint value = rtcom_param_int_get_value (RTCOM_PARAM_INT (widget));
      g_hash_table_replace (advanced_settings, widget,
          GINT_TO_POINTER (value));
    }
  else if (GTK_IS_ENTRY (widget))
    {
      gchar *data = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));
      g_object_set_data_full (G_OBJECT (widget), "adv_data", data, g_free);
      g_hash_table_replace (advanced_settings, widget, data);
    }
  else if (HILDON_IS_CHECK_BUTTON (widget))
    {
      gboolean active =
        hildon_check_button_get_active (HILDON_CHECK_BUTTON (widget));
      g_hash_table_replace (advanced_settings, widget,
          GINT_TO_POINTER (active));
    }
  else if (GTK_IS_CONTAINER (widget))
    {
      gtk_container_foreach (GTK_CONTAINER (widget),
          (GtkCallback)get_advanced_settings,
          advanced_settings);
    }
  else if (!GTK_IS_LABEL (widget))
    {
      g_warning ("%s: unhandled widget type %s (%s)", G_STRFUNC,
          g_type_name (G_TYPE_FROM_INSTANCE (widget)), name);
    }
}

static void on_advanced_settings_response(GtkWidget *dialog, gint response,
                                          RtcomDialogContext *context)
{
  GHashTable *advanced_settings =
      g_object_get_data(G_OBJECT(context), "settings");

  if (response == GTK_RESPONSE_OK) {
    GError *error = NULL;
    GladeXML *xml = glade_get_widget_tree(dialog);
    GtkWidget *page = glade_xml_get_widget(xml, "page");

    if (rtcom_page_validate(RTCOM_PAGE(page), &error)) {
      get_advanced_settings(dialog, advanced_settings);
      gtk_widget_hide(dialog);
    } else {
      g_warning("advanced page validation failed");
      if (error) {
        g_warning("%s: error \"%s\"", G_STRFUNC, error->message);
        hildon_banner_show_information(dialog, NULL, error->message);
        g_error_free(error);
      }
    }
  } else {
    g_hash_table_foreach(advanced_settings, set_widget_setting, dialog);
    gtk_widget_hide(dialog);
  }
}

static void gtalk_plugin_on_autostun_toggled_cb(gpointer data)
{
  GladeXML *xml;
  GtkWidget *stun_port;
  gboolean active;
  GtkWidget *stun_server;
  GtkWidget *stun_table;

  active = hildon_check_button_get_active(HILDON_CHECK_BUTTON(data));
  xml = glade_get_widget_tree(GTK_WIDGET(data));
  stun_table = glade_xml_get_widget(xml, "stun-table");
  stun_port = glade_xml_get_widget(xml, "stun-port");
  stun_server = glade_xml_get_widget(xml, "stun-server");
  /* WTF, why those are not used? */
  glade_xml_get_widget(xml, "stun-port-lbl");
  glade_xml_get_widget(xml, "stun-server-lbl");

  if (rtcom_param_int_get_value(RTCOM_PARAM_INT(stun_port)) == G_MININT32)
    rtcom_param_int_set_value(RTCOM_PARAM_INT(stun_port), 3478);

  if (active)
    gtk_widget_hide(stun_table);
  else
    gtk_widget_show(stun_table);

  gtk_widget_set_sensitive(stun_server, !active);
  gtk_widget_set_sensitive(stun_port, !active);
}

static GtkWidget *create_advanced_settings_page(RtcomDialogContext *context)
{
  GtkWidget *dialog;

  dialog = g_object_get_data(G_OBJECT(context), "page_advanced");
  if (!dialog) {
    AccountItem *account;
    GtkWidget *page;
    AccountService *service;
    const gchar *profile_name;
    GtkWidget *start_page;
    GtkWidget *autostun_button;
    GHashTable *hash;
    gchar title[200];
    const gchar *text;
    GladeXML *xml =
        glade_xml_new(PLUGIN_XML_DIR "/gtalk-advanced.glade",
                      NULL,
                      GETTEXT_PACKAGE);
    rtcom_dialog_context_take_obj(context, G_OBJECT(xml));
    dialog = glade_xml_get_widget(xml, "advanced");
    if (!dialog) {
      g_warning("Unable to load Advanced settings dialog");
      return dialog;
    }
    gtk_dialog_add_buttons(GTK_DIALOG(dialog),
                           dgettext("hildon-libs", "wdgt_bd_done"),
                           GTK_RESPONSE_OK,
                           NULL);
    account = account_edit_context_get_account(ACCOUNT_EDIT_CONTEXT(context));
    page = glade_xml_get_widget(xml, "page");
    rtcom_page_set_account(RTCOM_PAGE(page), RTCOM_ACCOUNT_ITEM (account));
    service = account_item_get_service(account);
    profile_name = account_service_get_display_name(service);
    g_snprintf(title,
               sizeof(title),
               g_dgettext(GETTEXT_PACKAGE,
                          "accountwizard_ti_advanced_settings"),
               profile_name);
    gtk_window_set_title(GTK_WINDOW(dialog), title);

    start_page = rtcom_dialog_context_get_start_page(context);
    if (start_page ) {
      GtkWidget *toplevel = gtk_widget_get_toplevel(start_page);
      if (toplevel) {
        gtk_window_set_transient_for(GTK_WINDOW(dialog),
                                     GTK_WINDOW(toplevel));
        gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
      }
    }
    g_object_ref(dialog);
    g_object_set_data_full( G_OBJECT (context),
                            "page_advanced",
                            dialog,
                            g_object_unref);
    glade_xml_signal_connect(xml,
                             "on_autostun_toggled",
                             G_CALLBACK(gtalk_plugin_on_autostun_toggled_cb));
    autostun_button = glade_xml_get_widget(xml, "autostun-Button-finger");

    text = gtk_entry_get_text(
          GTK_ENTRY(glade_xml_get_widget(xml, "stun-server")));
    if (!text || !*text)
      hildon_check_button_set_active(HILDON_CHECK_BUTTON(autostun_button), TRUE);

    gtalk_plugin_on_autostun_toggled_cb(autostun_button);
    hash = g_hash_table_new((GHashFunc)g_direct_hash,
                            (GEqualFunc)g_direct_equal);
    get_advanced_settings(dialog, hash);
    g_object_set_data_full( G_OBJECT(context),
                            "settings",
                            hash,
                            (GDestroyNotify)g_hash_table_destroy);
  }
  g_signal_connect_data(dialog,
                        "response",
                        G_CALLBACK(on_advanced_settings_response),
                        context,
                        NULL,
                        0);
  g_signal_connect_data(dialog,
                        "delete-event",
                        G_CALLBACK(gtk_true),
                        NULL,
                        NULL,
                        0);
  return dialog;
}

static void gtalk_plugin_on_advanced_cb(gpointer data)
{
  GtkWidget *dialog;
  RtcomDialogContext *context = RTCOM_DIALOG_CONTEXT(data);

  dialog = create_advanced_settings_page(context);
  gtk_widget_show(dialog);
}

static void gtalk_plugin_on_register_cb(gpointer userdata)
{
  gchar *uri;
  GError *error = NULL;

  uri = g_strconcat(GTALK_NEW_ACCOUNT_URI, getenv("LANG"), NULL);

  if (!hildon_uri_open(uri, NULL, &error)) {
    g_warning("Failed to open browser: %s", error->message);
    g_error_free(error);
  }

  g_free(uri);
}

static void gtalk_plugin_on_forgot_password_cb(gpointer userdata)
{
  GError *error = NULL;

  if (!hildon_uri_open(GTALK_FORGOT_PASSWORD_URI, NULL, &error)) {
    g_warning("Failed to open browser: %s", error->message);
    g_error_free(error);
  }
}

static void gtalk_plugin_context_init(RtcomAccountPlugin *plugin,
                                      RtcomDialogContext *context)
{
  gboolean editing;
  AccountItem *account;
  GtkWidget *page;

  static const gchar *invalid_chars_re = "[:'\"<>&;#\\s]";

  editing = account_edit_context_get_editing(ACCOUNT_EDIT_CONTEXT(context));
  account = account_edit_context_get_account(ACCOUNT_EDIT_CONTEXT(context));
  create_advanced_settings_page(context);

  if (editing) {
    page =
        g_object_new(
          RTCOM_TYPE_EDIT,
          "username-field", "account",
          "username-invalid-chars-re", invalid_chars_re,
          "items-mask", RTCOM_ACCOUNT_PLUGIN(plugin)->capabilities,
          "edit-info-uri", "https://www.google.com/accounts/ManageAccount",
          "account", account,
          NULL);
    rtcom_edit_connect_on_advanced(RTCOM_EDIT(page),
                                   G_CALLBACK(gtalk_plugin_on_advanced_cb),
                                   context);
  } else {
    page =
        g_object_new(
          RTCOM_TYPE_LOGIN,
          "username-field", "account",
          "username-invalid-chars-re", invalid_chars_re,
          "username-must-have-at-separator", TRUE,
          "username-prefill", RTCOM_ACCOUNT_PLUGIN(plugin)->username_prefill,
          "items-mask", RTCOM_ACCOUNT_PLUGIN(plugin)->capabilities,
          "account", account,
          NULL);

    rtcom_login_connect_on_register(
          RTCOM_LOGIN(page),
          G_CALLBACK(gtalk_plugin_on_register_cb),
          NULL);
    rtcom_login_connect_on_forgot_password(
          RTCOM_LOGIN(page),
          G_CALLBACK(gtalk_plugin_on_forgot_password_cb),
          NULL);
    rtcom_login_connect_on_advanced(
          RTCOM_LOGIN(page),
          G_CALLBACK(gtalk_plugin_on_advanced_cb),
          context);
  }

  rtcom_dialog_context_set_start_page(context, page);
}

static void gtalk_plugin_class_init(GtalkPluginClass *klass)
{
  parent_class = g_type_class_peek_parent(klass);
  RTCOM_ACCOUNT_PLUGIN_CLASS(klass)->context_init = gtalk_plugin_context_init;
}
