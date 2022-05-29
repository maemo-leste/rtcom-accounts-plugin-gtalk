#include "config.h"

#include <glade/glade.h>
#include <hildon-uri.h>
#include <hildon/hildon.h>
#include <libaccounts/account-plugin.h>
#include <libintl.h>
#include <librtcom-accounts-widgets/rtcom-account-plugin.h>
#include <librtcom-accounts-widgets/rtcom-dialog-context.h>
#include <librtcom-accounts-widgets/rtcom-edit.h>
#include <librtcom-accounts-widgets/rtcom-login.h>
#include <librtcom-accounts-widgets/rtcom-param-int.h>

#include "advanced-page.h"

#define GTALK_FORGOT_PASSWORD_URI \
  "https://www.google.com/accounts/ForgotPasswd?service=mail&fpOnly=1"
#define GTALK_NEW_ACCOUNT_URI \
  "https://www.google.com/accounts/NewAccount?service=mail&hl="

typedef struct _GtalkPluginClass GtalkPluginClass;
typedef struct _GtalkPlugin GtalkPlugin;

struct _GtalkPlugin
{
  RtcomAccountPlugin parent_instance;
};

struct _GtalkPluginClass
{
  RtcomAccountPluginClass parent_class;
};

RtcomAccountPluginClass *parent_class;

ACCOUNT_DEFINE_PLUGIN(GtalkPlugin, gtalk_plugin, RTCOM_TYPE_ACCOUNT_PLUGIN);

static void
gtalk_plugin_init(GtalkPlugin *self)
{
  RtcomAccountService *service;

  RTCOM_ACCOUNT_PLUGIN(self)->name = "google-talk";
  RTCOM_ACCOUNT_PLUGIN(self)->username_prefill = "@gmail.com";
  RTCOM_ACCOUNT_PLUGIN(self)->capabilities = RTCOM_PLUGIN_CAPABILITY_ALL;
  service = rtcom_account_plugin_add_service(RTCOM_ACCOUNT_PLUGIN(self),
                                             "gabble/jabber/google-talk");

  g_object_set(G_OBJECT(service),
               "display-name", "Google Talk",
               NULL);

  glade_init();
}

static void
gtalk_plugin_on_autostun_toggled_cb(gpointer data)
{
  GladeXML *xml;
  GtkWidget *stun_port;
  gboolean active;
  GtkWidget *stun_server;
  GtkWidget *stun_table;
  GtkWidget *area;

  active = hildon_check_button_get_active(HILDON_CHECK_BUTTON(data));
  xml = glade_get_widget_tree(GTK_WIDGET(data));
  stun_table = glade_xml_get_widget(xml, "stun-table");
  stun_port = glade_xml_get_widget(xml, "stun-port");
  stun_server = glade_xml_get_widget(xml, "stun-server");
  area = glade_xml_get_widget(xml, "panable-area");

  if (rtcom_param_int_get_value(RTCOM_PARAM_INT(stun_port)) == G_MININT32)
    rtcom_param_int_set_value(RTCOM_PARAM_INT(stun_port), 3478);

  if (active)
  {
    g_object_set(area, "height-request", 210, NULL);
    gtk_widget_hide(stun_table);
  }
  else
  {
    g_object_set(area, "height-request", 360, NULL);
    gtk_widget_show(stun_table);
  }

  gtk_widget_set_sensitive(stun_server, !active);
  gtk_widget_set_sensitive(stun_port, !active);
}

static GtkWidget *
create_advanced_settings_page(RtcomDialogContext *context)
{
  GtkWidget *dialog;

  dialog = g_object_get_data(G_OBJECT(context), "page_advanced");

  if (!dialog)
  {
    AccountItem *account;
    GtkWidget *page;
    AccountService *service;
    const gchar *profile_name;
    GtkWidget *start_page;
    GtkWidget *autostun_button;
    GHashTable *hash;
    gchar title[200];
    const gchar *text;
    const gchar *msg;
    GladeXML *xml = glade_xml_new(PLUGIN_XML_DIR "/gtalk-advanced.glade",
                                  NULL, GETTEXT_PACKAGE);

    rtcom_dialog_context_take_obj(context, G_OBJECT(xml));
    dialog = glade_xml_get_widget(xml, "advanced");

    if (!dialog)
    {
      g_warning("Unable to load Advanced settings dialog");
      return dialog;
    }

    gtk_dialog_add_buttons(GTK_DIALOG(dialog),
                           dgettext("hildon-libs", "wdgt_bd_done"),
                           GTK_RESPONSE_OK, NULL);
    account = account_edit_context_get_account(ACCOUNT_EDIT_CONTEXT(context));
    page = glade_xml_get_widget(xml, "page");
    rtcom_page_set_account(RTCOM_PAGE(page), RTCOM_ACCOUNT_ITEM(account));
    service = account_item_get_service(account);
    profile_name = account_service_get_display_name(service);
    msg = g_dgettext(GETTEXT_PACKAGE, "accountwizard_ti_advanced_settings");
    g_snprintf(title, sizeof(title), msg, profile_name);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    start_page = rtcom_dialog_context_get_start_page(context);

    if (start_page)
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel(start_page);

      if (toplevel)
      {
        gtk_window_set_transient_for(GTK_WINDOW(dialog),
                                     GTK_WINDOW(toplevel));
        gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
      }
    }

    g_object_ref(dialog);
    g_object_set_data_full(
          G_OBJECT(context), "page_advanced", dialog, g_object_unref);
    glade_xml_signal_connect(xml, "on_autostun_toggled",
                             G_CALLBACK(gtalk_plugin_on_autostun_toggled_cb));
    autostun_button = glade_xml_get_widget(xml, "autostun-Button-finger");

    text = gtk_entry_get_text(
          GTK_ENTRY(glade_xml_get_widget(xml, "stun-server")));

    if (!text || !*text)
    {
      hildon_check_button_set_active(
            HILDON_CHECK_BUTTON(autostun_button), TRUE);
    }

    gtalk_plugin_on_autostun_toggled_cb(autostun_button);
    hash = g_hash_table_new((GHashFunc)g_direct_hash,
                            (GEqualFunc)g_direct_equal);
    get_advanced_settings(dialog, hash);
    g_object_set_data_full(G_OBJECT(context), "settings", hash,
                           (GDestroyNotify)g_hash_table_destroy);
  }

  g_signal_connect(dialog, "response",
                   G_CALLBACK(on_advanced_settings_response), context);
  g_signal_connect(dialog, "delete-event", G_CALLBACK(gtk_true), NULL);

  return dialog;
}

static void
gtalk_plugin_on_advanced_cb(gpointer data)
{
  GtkWidget *dialog;
  RtcomDialogContext *context = RTCOM_DIALOG_CONTEXT(data);

  dialog = create_advanced_settings_page(context);
  gtk_widget_show(dialog);
}

static void
gtalk_plugin_on_register_cb(gpointer userdata)
{
  gchar *uri;
  GError *error = NULL;

  uri = g_strconcat(GTALK_NEW_ACCOUNT_URI, getenv("LANG"), NULL);

  if (!hildon_uri_open(uri, NULL, &error))
  {
    g_warning("Failed to open browser: %s", error->message);
    g_error_free(error);
  }

  g_free(uri);
}

static void
gtalk_plugin_on_forgot_password_cb(gpointer userdata)
{
  GError *error = NULL;

  if (!hildon_uri_open(GTALK_FORGOT_PASSWORD_URI, NULL, &error))
  {
    g_warning("Failed to open browser: %s", error->message);
    g_error_free(error);
  }
}

static void
gtalk_plugin_context_init(RtcomAccountPlugin *plugin,
                          RtcomDialogContext *context)
{
  gboolean editing;
  AccountItem *account;
  GtkWidget *page;

  static const gchar *invalid_chars_re = "[:'\"<>&;#\\s]";

  editing = account_edit_context_get_editing(ACCOUNT_EDIT_CONTEXT(context));
  account = account_edit_context_get_account(ACCOUNT_EDIT_CONTEXT(context));
  create_advanced_settings_page(context);

  if (editing)
  {
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
  }
  else
  {
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

static void
gtalk_plugin_class_init(GtalkPluginClass *klass)
{
  RTCOM_ACCOUNT_PLUGIN_CLASS(klass)->context_init = gtalk_plugin_context_init;
}
