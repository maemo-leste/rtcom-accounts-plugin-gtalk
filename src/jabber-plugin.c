/*
 * jabber-plugin.c
 *
 * Copyright (C) 2022 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "config.h"

#include <glade/glade.h>
#include <glib/gi18n-lib.h>
#include <hildon/hildon.h>
#include <libaccounts/account-plugin.h>
#include <librtcom-accounts-widgets/rtcom-account-plugin.h>
#include <librtcom-accounts-widgets/rtcom-edit.h>
#include <librtcom-accounts-widgets/rtcom-login.h>
#include <librtcom-accounts-widgets/rtcom-param-bool.h>
#include <librtcom-accounts-widgets/rtcom-param-int.h>
#include <librtcom-accounts-widgets/rtcom-param-string.h>
#include <telepathy-glib/telepathy-glib.h>

#include "advanced-page.h"

#define JABBER_TYPE_PLUGIN (jabber_plugin_get_type())
#define JABBER_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), JABBER_TYPE_PLUGIN, JabberPlugin))

typedef struct _JabberPluginClass JabberPluginClass;
typedef struct _JabberPlugin JabberPlugin;

struct _JabberPlugin
{
  RtcomAccountPlugin parent_instance;
};

struct _JabberPluginClass
{
  RtcomAccountPluginClass parent_class;
};

struct _JabberPluginPrivate
{
  GtkWidget *registering_dialog;
};

typedef struct _JabberPluginPrivate JabberPluginPrivate;

ACCOUNT_DEFINE_PLUGIN_WITH_PRIVATE(
  JabberPlugin,
  jabber_plugin,
  RTCOM_TYPE_ACCOUNT_PLUGIN
);

static void
cms_ready_cb(GObject *object, GAsyncResult *res, gpointer user_data)
{
  RtcomAccountPlugin *plugin = user_data;
  GError *error = NULL;
  GList *cms = tp_list_connection_managers_finish(res, &error);
  GList *l;

  if (error != NULL)
  {
    g_warning("Error getting list of CMs: %s", error->message);
    g_error_free(error);
  }
  else if (!cms)
    g_warning("No Telepathy connection managers found");

  for (l = cms; l; l = l->next)
  {
    if (tp_connection_manager_has_protocol(l->data, "jabber"))
    {
      gchar *service_id = g_strconcat(tp_connection_manager_get_name(l->data),
                                      "/jabber", NULL);

      rtcom_account_plugin_add_service(plugin, service_id);
      g_free(service_id);
    }
  }

  rtcom_account_plugin_initialized(plugin);

  g_list_free(cms);

  g_object_unref(plugin);
}

static void
jabber_plugin_init(JabberPlugin *plugin)
{
  GError *error = NULL;
  TpDBusDaemon *tp_dbus = tp_dbus_daemon_dup(&error);

  if (tp_dbus)
  {
    tp_list_connection_managers_async(tp_dbus, cms_ready_cb,
                                      g_object_ref(plugin));
  }
  else
  {
    g_warning("%s: tp_dbus_daemon_dup() failed [%s]", __FUNCTION__,
              error->message);
    g_error_free(error);
  }

  RTCOM_ACCOUNT_PLUGIN(plugin)->name = "jabber";
  RTCOM_ACCOUNT_PLUGIN(plugin)->username_prefill = NULL;
  RTCOM_ACCOUNT_PLUGIN(plugin)->capabilities =
    RTCOM_PLUGIN_CAPABILITY_ALL & ~RTCOM_PLUGIN_CAPABILITY_FORGOT_PWD;

  glade_init();
}

static void
on_require_encryption_toggled_cb(GtkWidget *buttin)
{
  GladeXML *xml = glade_get_widget_tree(buttin);
  GtkWidget *ignore_ssl_errors_button =
    glade_xml_get_widget(xml, "ignore-ssl-errors-Button-finger");
  GtkWidget *force_old_ssl_button =
    glade_xml_get_widget(xml, "force-old-ssl-Button-finger");

  if (hildon_check_button_get_active(HILDON_CHECK_BUTTON(buttin)))
  {
    gtk_widget_show(ignore_ssl_errors_button);
    gtk_widget_show(force_old_ssl_button);
  }
  else
  {
    gtk_widget_hide(ignore_ssl_errors_button);
    gtk_widget_hide(force_old_ssl_button);
    hildon_check_button_set_active(HILDON_CHECK_BUTTON(force_old_ssl_button),
                                   FALSE);
  }
}

static void
on_force_old_ssl_toggled_cb(GtkWidget *button)
{
  gboolean active = hildon_check_button_get_active(HILDON_CHECK_BUTTON(button));
  GladeXML *xml = glade_get_widget_tree(button);
  RtcomParamInt *port = RTCOM_PARAM_INT(glade_xml_get_widget(xml, "port"));
  gint def_val;
  gint new_val;
  gint val;

  if (active)
  {
    def_val = 5222;
    new_val = 5223;
  }
  else
  {
    def_val = 5223;
    new_val = 5222;
  }

  val = rtcom_param_int_get_value(port);

  if ((val == G_MININT) || (def_val == val))
    rtcom_param_int_set_value(port, new_val);
}

static GtkWindow *
get_parent_window(RtcomDialogContext *context)
{
  GtkWidget *page;
  GtkWindow *parent = NULL;

  page = rtcom_dialog_context_get_start_page(context);

  if (page)
    parent = GTK_WINDOW(gtk_widget_get_toplevel(page));

  return parent;
}

static GtkWidget *
create_advanced_settings_page(RtcomDialogContext *context)
{
  GtkWidget *dialog;

  dialog = g_object_get_data(G_OBJECT(context), "page_advanced");

  if (!dialog)
  {
    GladeXML *xml = glade_xml_new(
        PLUGIN_XML_DIR "/jabber-advanced.glade", NULL, GETTEXT_PACKAGE);
    AccountItem *account;
    GtkWidget *require_encryption_button;
    GtkWidget *force_old_ssl_button;
    GtkWidget *ignore_ssl_errors_button;
    GtkWidget *page;
    AccountService *service;
    gchar *title;
    const gchar *fmt;
    GHashTable *settings;

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
    g_object_ref(dialog);
    g_object_set_data_full(
      G_OBJECT(context), "page_advanced", dialog, g_object_unref);
    account = account_edit_context_get_account(ACCOUNT_EDIT_CONTEXT(context));

    require_encryption_button = glade_xml_get_widget(
        xml, "require-encryption-Button-finger");
    glade_xml_signal_connect(xml, "on_require_encryption_toggled",
                             G_CALLBACK(on_require_encryption_toggled_cb));
    on_require_encryption_toggled_cb(require_encryption_button);

    force_old_ssl_button = glade_xml_get_widget(
        xml, "force-old-ssl-Button-finger");
    glade_xml_signal_connect(xml, "on_force_old_ssl_toggled",
                             G_CALLBACK(on_force_old_ssl_toggled_cb));
    on_force_old_ssl_toggled_cb(force_old_ssl_button);

    ignore_ssl_errors_button = glade_xml_get_widget(
        xml, "ignore-ssl-errors-Button-finger");
    hildon_check_button_set_active(
      HILDON_CHECK_BUTTON(ignore_ssl_errors_button), TRUE);

    page = glade_xml_get_widget(xml, "page");
    rtcom_page_set_account(RTCOM_PAGE(page), RTCOM_ACCOUNT_ITEM(account));

    service = account_item_get_service(account);
    fmt = _("accountwizard_ti_advanced_settings");
    title = g_strdup_printf(fmt, account_service_get_display_name(service));
    gtk_window_set_title((GtkWindow *)dialog, title);
    g_free(title);

    gtk_window_set_transient_for(GTK_WINDOW(dialog),
                                 get_parent_window(context));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);

    settings = g_hash_table_new(g_direct_hash, g_direct_equal);
    get_advanced_settings(dialog, settings);
    g_object_set_data_full(G_OBJECT(context), "settings", settings,
                           (GDestroyNotify)g_hash_table_destroy);
  }

  g_signal_connect(dialog, "response",
                   G_CALLBACK(on_advanced_settings_response), context);
  g_signal_connect(dialog, "delete-event",
                   G_CALLBACK(gtk_true), NULL);

  return dialog;
}

static void
jabber_plugin_on_advanced_cb(RtcomDialogContext *context)
{
  GtkWidget *dialog = create_advanced_settings_page(context);

  gtk_widget_show(dialog);
}

static void
param_copy(gpointer key, gpointer value, GHashTable *register_settings)
{
  if (RTCOM_IS_PARAM_STRING(key) && value && *(gchar *)value)
  {
    GValue *v = tp_g_value_slice_new(G_TYPE_STRING);

    g_value_set_static_string(v, value);
    g_hash_table_insert(register_settings,
                        (gchar *)rtcom_param_string_get_field(key), v);
  }
  else if (RTCOM_IS_PARAM_BOOL(key))
  {
    GValue *v = tp_g_value_slice_new(G_TYPE_BOOLEAN);

    g_value_set_boolean(v, GPOINTER_TO_INT(value));
    g_hash_table_insert(register_settings,
                        (gchar *)rtcom_param_bool_get_field(key), v);
  }
  else if (RTCOM_IS_PARAM_INT(key))
  {
    GValue *v = tp_g_value_slice_new(G_TYPE_UINT);

    g_value_set_uint(v, GPOINTER_TO_UINT(value));
    g_hash_table_insert(register_settings,
                        (gchar *)rtcom_param_int_get_field(key), v);
  }
}

static void
service_connection_cb(GObject *requester, TpConnection *connection,
                      GError *error, gpointer user_data)
{
  AccountPlugin *plugin;
  JabberPluginPrivate *priv;

  plugin = account_edit_context_get_plugin(ACCOUNT_EDIT_CONTEXT(requester));
  priv = jabber_plugin_get_instance_private(JABBER_PLUGIN(plugin));

  if (priv->registering_dialog)
  {
    gtk_widget_destroy(priv->registering_dialog);
    priv->registering_dialog = NULL;
  }

  if (error)
    hildon_banner_show_information(user_data, NULL, error->message);
  else
  {
    GtkWindow *dialog = get_parent_window(RTCOM_DIALOG_CONTEXT(requester));

    rtcom_dialog_context_set_start_page(RTCOM_DIALOG_CONTEXT(requester), NULL);
    gtk_dialog_response(GTK_DIALOG(dialog), 1);
  }
}

static void
on_register_response_cb(GtkWidget *dialog, gint response,
                        RtcomDialogContext *context)
{
  GladeXML *xml;
  GtkWidget *page;
  const gchar *username;
  const gchar *password;
  const gchar *password2;
  GHashTable *settings;
  GValue *v;
  AccountItem *account;
  AccountService *service;
  AccountPlugin *plugin;
  JabberPluginPrivate *priv;
  const gchar *fmt;
  gchar *registering_msg;
  GtkWidget *registering_dialog;
  GHashTable *register_settings;
  GError *error = NULL;

  if (response != GTK_RESPONSE_OK)
  {
    gtk_widget_destroy(dialog);
    return;
  }

  xml = glade_get_widget_tree(dialog);
  page = glade_xml_get_widget(xml, "page");

  if (!rtcom_page_validate(RTCOM_PAGE(page), &error))
    goto err;

  username =
    gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(xml, "username")));

  if (!g_strstr_len(username, -1, "@"))
  {
    g_set_error(&error, ACCOUNT_ERROR, ACCOUNT_ERROR_INVALID_VALUE,
                _("accountwizard_ib_illegal_server_address"));
    goto err;
  }

  password =
    gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(xml, "password")));
  password2 =
    gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(xml, "password2")));

  if (strcmp(password, password2))
  {
    g_set_error(&error, ACCOUNT_ERROR, ACCOUNT_ERROR_PASSWORD_MATCH,
                _("accountwizard_ib_passwords_do_not_match"));
    goto err;
  }

  account = account_edit_context_get_account(ACCOUNT_EDIT_CONTEXT(context));
  service = account_item_get_service(account);
  register_settings = g_hash_table_new_full(
      g_str_hash, g_str_equal, NULL, (GDestroyNotify)tp_g_value_slice_free);
  settings = g_object_get_data(G_OBJECT(context), "settings");

  if (settings)
  {
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init (&iter, settings);

    while (g_hash_table_iter_next (&iter, &key, &value))
    {
      gchar *field;

      g_object_get(key, "field", &field, NULL);

      if (rtcom_account_service_get_param_type(RTCOM_ACCOUNT_SERVICE(service),
                                               field) != G_TYPE_INVALID)
      {
        param_copy(key, value, register_settings);
      }
      else
      {
        g_warning("Parameter %s is not supported by service %s", field,
                  service->name);
      }

      g_free(field);
    }
  }

  v = tp_g_value_slice_new(G_TYPE_BOOLEAN);
  g_value_set_boolean(v, TRUE);
  g_hash_table_insert(register_settings, "register", v);

  v = tp_g_value_slice_new(G_TYPE_STRING);
  g_value_set_static_string(v, username);
  g_hash_table_insert(register_settings, "account", v);

  v = tp_g_value_slice_new(G_TYPE_STRING);
  g_value_set_static_string(v, password);
  g_hash_table_insert(register_settings, "password", v);

  plugin = account_edit_context_get_plugin(&context->parent_instance);
  rtcom_account_service_connect(RTCOM_ACCOUNT_SERVICE(service),
                                register_settings, G_OBJECT(context), TRUE,
                                service_connection_cb, dialog);
  g_hash_table_unref(register_settings);

  priv = jabber_plugin_get_instance_private(JABBER_PLUGIN(plugin));

  fmt = _("accounts_ti_registering");
  registering_msg = g_strdup_printf(fmt, username);
  registering_dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(registering_dialog), registering_msg);
  gtk_dialog_set_has_separator(GTK_DIALOG(registering_dialog), FALSE);
  g_free(registering_msg);
  hildon_gtk_window_set_progress_indicator(GTK_WINDOW(registering_dialog), 1);

  if (account->service_icon)
  {
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(registering_dialog)->vbox),
                       gtk_image_new_from_pixbuf(account->service_icon),
                       FALSE, FALSE, 16);
  }

  if (dialog)
  {
    gtk_window_set_transient_for((GtkWindow *)registering_dialog,
                                 (GtkWindow *)dialog);
    gtk_window_set_modal((GtkWindow *)registering_dialog, 0);
  }

  priv->registering_dialog = registering_dialog;
  gtk_widget_show_all(registering_dialog);

  return;

err:

  if (error)
  {
    hildon_banner_show_information(dialog, NULL, error->message);
    g_error_free(error);
  }
}

static void
jabber_plugin_on_register_cb(RtcomDialogContext *context)
{
  GtkWidget *dialog;

  GladeXML *xml = glade_xml_new(
      PLUGIN_XML_DIR "/jabber-new-account.glade", NULL, GETTEXT_PACKAGE);

  rtcom_dialog_context_take_obj(context, G_OBJECT(xml));
  dialog = glade_xml_get_widget(xml, "register");

  if (dialog)
  {
    GtkWidget *advanced_button =
      glade_xml_get_widget(xml, "advanced-button-Button-finger");
    AccountItem *account;
    GtkWidget *page;

    g_signal_connect_swapped(advanced_button, "clicked",
                             G_CALLBACK(jabber_plugin_on_advanced_cb), context);
    gtk_dialog_add_buttons(GTK_DIALOG(dialog), _("accounts_bd_register"),
                           GTK_RESPONSE_OK, NULL);
    account = account_edit_context_get_account(&context->parent_instance);
    page = glade_xml_get_widget(xml, "page");
    rtcom_page_set_account(RTCOM_PAGE(page), RTCOM_ACCOUNT_ITEM(account));
    gtk_window_set_title(GTK_WINDOW(dialog),
                         _("accounts_ti_new_jabber_account"));
    gtk_window_set_transient_for(GTK_WINDOW(dialog), get_parent_window(context));
    g_signal_connect(dialog, "response",
                     G_CALLBACK(on_register_response_cb), context);
    gtk_widget_show_all(dialog);
  }
  else
    g_warning("Unable to load Register dialog");
}

static void
jabber_plugin_context_init(RtcomAccountPlugin *plugin,
                           RtcomDialogContext *context)
{
  GtkWidget *page;
  gboolean editing;
  AccountItem *account;

  editing = account_edit_context_get_editing(ACCOUNT_EDIT_CONTEXT(context));
  account = account_edit_context_get_account(ACCOUNT_EDIT_CONTEXT(context));
  create_advanced_settings_page(context);

  if (editing)
  {
    page = g_object_new(
        RTCOM_TYPE_EDIT,
        "username-field", "account",
        "username-invalid-chars-re", "[:'\"<>&;#\\s]",
        "username-label", _("accounts_fi_user_name_sip"),
        "msg-empty", _("accounts_fi_enter_address_and_password_fields_first"),
        "items-mask", plugin->capabilities,
        "account", account,
        NULL);
    rtcom_edit_connect_on_advanced(
      RTCOM_EDIT(page), G_CALLBACK(jabber_plugin_on_advanced_cb), context);
  }
  else
  {
    page = g_object_new(
        RTCOM_TYPE_LOGIN,
        "username-field", "account",
        "username-invalid-chars-re", "[:'\"<>&;#\\s]",
        "username-must-have-at-separator", TRUE,
        "username-prefill",
        plugin->username_prefill,
        "username-label", _("accounts_fi_user_name_sip"),
        "msg-empty", _("accounts_fi_enter_address_and_password_fields_first"),
        "items-mask", plugin->capabilities,
        "account", account,
        NULL);

    rtcom_login_connect_on_register(
      RTCOM_LOGIN(page), G_CALLBACK(jabber_plugin_on_register_cb), context);
    rtcom_login_connect_on_advanced(
      RTCOM_LOGIN(page), G_CALLBACK(jabber_plugin_on_advanced_cb), context);
  }

  rtcom_dialog_context_set_start_page(context, page);
}

static void
jabber_plugin_class_init(JabberPluginClass *klass)
{
  RTCOM_ACCOUNT_PLUGIN_CLASS(klass)->context_init = jabber_plugin_context_init;
}
