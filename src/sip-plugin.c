/*
 * sip-plugin.c
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
#include <librtcom-accounts-widgets/rtcom-dialog-context.h>
#include <librtcom-accounts-widgets/rtcom-edit.h>
#include <librtcom-accounts-widgets/rtcom-login.h>
#include <librtcom-accounts-widgets/rtcom-param-int.h>

#include "advanced-page.h"

#define INVALID_CHARS_RE "[:'\"<>&;#\\s]"
#define BUTTON(id) id "-Button-finger"

typedef struct _SipPluginClass SipPluginClass;
typedef struct _SipPlugin SipPlugin;

struct _SipPlugin
{
  RtcomAccountPlugin parent_instance;
};

struct _SipPluginClass
{
  RtcomAccountPluginClass parent_class;
};

struct _SipPluginPrivate
{
  GList *accounts;
};

typedef struct _SipPluginPrivate SipPluginPrivate;

#define PRIVATE(plugin) \
  (SipPluginPrivate *)sip_plugin_get_instance_private((SipPlugin *)(plugin));

ACCOUNT_DEFINE_PLUGIN_WITH_PRIVATE(SipPlugin,
                                   sip_plugin,
                                   RTCOM_TYPE_ACCOUNT_PLUGIN);

struct _sip_account
{
  AccountItem *item;
  RtcomDialogContext *context;
};

typedef struct _sip_account sip_account;

struct _item_description
{
  const char *msgid;
  const char *value;
};

typedef struct _item_description item_description;

static const item_description
  transport_items[] =
{
  { "accountwizard_transport_va_auto", "auto" },
  { "TCP", "tcp" },
  { "UDP", "udp" },
  { "TLS", "tls" }
};

static const item_description
  keepalive_mechanizm_items[] =
{
  { "accountwizard_keepalive_va_auto", "auto" },
  { "REGISTER", "register" },
  { "OPTIONS", "options" },
  { "accountwizard_keepalive_va_off", "off" }
};

static const item_description keepalive_interval_items[] =
{
  { "accountwizard_fi_keepalive_period_va_auto", NULL },
  { "accountwizard_fi_keepalive_period_va_30_sec", "30" },
  { "accountwizard_fi_keepalive_period_va_1_min", "60" },
  { "accountwizard_fi_keepalive_period_va_2_min", "120" },
  { "accountwizard_fi_keepalive_period_va_15_min", "900" },
  { "accountwizard_fi_keepalive_period_va_60_min", "3600" }
};

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
    if (tp_connection_manager_has_protocol(l->data, "sip"))
    {
      gchar *id;

      id = g_strconcat(tp_connection_manager_get_name(l->data), "/sip", NULL);
      rtcom_account_plugin_add_service(plugin, id);
      g_free(id);
    }
  }

  rtcom_account_plugin_initialized(plugin);

  g_list_free(cms);

  g_object_unref(plugin);
}

static void
sip_plugin_init(SipPlugin *plugin)
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

  RTCOM_ACCOUNT_PLUGIN(plugin)->name = "sip";
  RTCOM_ACCOUNT_PLUGIN(plugin)->capabilities =
    RTCOM_PLUGIN_CAPABILITY_ADVANCED |
    RTCOM_PLUGIN_CAPABILITY_ALLOW_MULTIPLE |
    RTCOM_PLUGIN_CAPABILITY_PASSWORD;

  glade_init();
}

static gboolean
on_store_settings(RtcomAccountItem *item, GError **error, sip_account *sa)
{
  RtcomDialogContext *context = sa->context;
  GtkWidget *button;

  g_return_val_if_fail(RTCOM_IS_DIALOG_CONTEXT(context), TRUE);

  button = g_object_get_data(G_OBJECT(context), BUTTON("cellular-call"));

  if (button)
  {
    GList *l = NULL;

    if (hildon_check_button_get_active(HILDON_CHECK_BUTTON(button)))
      l = g_list_append(NULL, "tel");

    rtcom_account_item_store_secondary_vcard_fields(item, l);
    g_list_free(l);
  }

  button = g_object_get_data(G_OBJECT(context), BUTTON("transport"));

  if (button)
  {
    gint idx = hildon_picker_button_get_active(HILDON_PICKER_BUTTON(button));

    if (idx < 0)
      idx = 0;

    rtcom_account_item_store_param_string(item, "transport",
                                          transport_items[idx].value);
  }

  button = g_object_get_data(G_OBJECT(context), BUTTON("keepalive-mechanism"));

  if (button)
  {
    gint idx = hildon_picker_button_get_active(HILDON_PICKER_BUTTON(button));

    if (idx < 0)
      idx = 0;

    rtcom_account_item_store_param_string(item, "keepalive-mechanism",
                                          keepalive_mechanizm_items[idx].value);
  }

  button = g_object_get_data(G_OBJECT(context), BUTTON("keepalive-interval"));

  if (button)
  {
    gint idx = hildon_picker_button_get_active(HILDON_PICKER_BUTTON(button));
    const char *interval;

    if (idx < 0)
      idx = 0;

    interval = keepalive_interval_items[idx].value;

    if (interval)
    {
      rtcom_account_item_store_param_uint(item, "keepalive-interval",
                                          strtol(interval, NULL, 10));
    }
    else
      rtcom_account_item_unset_param(item, "keepalive-interval");
  }

  return TRUE;
}

static void
sip_account_destroy(gpointer data)
{
  sip_account *sa = data;

  if (sa->context)
  {
    g_signal_handlers_disconnect_matched(
      sa->item,
      G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
      0, 0, NULL, on_store_settings, sa);
  }

  g_slice_free(sip_account, sa);
}

static void
sip_plugin_finalize(GObject *object)
{
  SipPluginPrivate *priv = PRIVATE(object);

  g_list_free_full(priv->accounts, sip_account_destroy);

  G_OBJECT_CLASS(sip_plugin_parent_class)->finalize(object);
}

static gchar *
account_get_default_setting(RtcomAccountItem *item, const gchar *setting)
{
  TpProtocol *protocol = rtcom_account_item_get_tp_protocol(item);
  gchar *rv = NULL;

  if (protocol)
  {
    const TpConnectionManagerParam *param;

    param = tp_protocol_get_param(protocol, setting);

    if (param)
    {
      GValue v = G_VALUE_INIT;

      if (tp_connection_manager_param_get_default(param, &v) &&
          G_VALUE_HOLDS_STRING(&v))
      {
        rv = g_value_dup_string(&v);
      }
    }

    g_object_unref(protocol);
  }

  return rv;
}

static void
init_check_button(GladeXML *xml, const char *name, const char *setting,
                  RtcomDialogContext *context, RtcomAccountItem *item,
                  gboolean default_is_active)
{
  GtkWidget *widget = glade_xml_get_widget(xml, name);

  g_object_set_data(G_OBJECT(context), name, widget);

  if (!item->account)
  {
    gchar *s = account_get_default_setting(item, setting);

    if (s)
    {
      hildon_check_button_set_active(HILDON_CHECK_BUTTON(widget),
                                     !strcmp(s, "true"));
      g_free(s);
    }
    else
    {
      hildon_check_button_set_active(HILDON_CHECK_BUTTON(widget),
                                     default_is_active);
    }
  }
}

static void
picker_button_set_active(GtkWidget *button, const item_description *items,
                         int n_items, RtcomAccountItem *item, const char *param)
{
  gint i = n_items;
  gchar *s = NULL;

  if (!item->account)
  {
    s = account_get_default_setting(item, param);

    for (i = 0; i < n_items; i++)
    {
      if (s && strcmp(s, items[i].value))
        break;
    }
  }
  else
  {
    GHashTable *parameters;
    const GValue *v;

    parameters = (GHashTable *)tp_account_get_parameters(item->account);

    if (parameters)
    {
      v = g_hash_table_lookup(parameters, param);

      if (v)
      {
        if (G_VALUE_HOLDS_UINT(v))
        {
          for (i = 0; i < n_items; i++)
          {
            if (g_value_get_uint(v) == strtol(items[i].value, 0, 10))
              break;
          }
        }
        else if (G_VALUE_HOLDS_STRING(v))
        {
          const char *tmp = g_value_get_string(v);

          for (i = 0; i < n_items; i++)
          {
            if (tmp && !strcmp(tmp, items[i].value))
              break;
          }
        }
        else if (G_VALUE_HOLDS_INT(v))
        {
          for (i = 0; i < n_items; i++)
          {
            if (g_value_get_int(v) == strtol(items[i].value, 0, 10))
              break;
          }
        }
        else
        {
          g_warning(
            "value type '%s' is not supported for a picker button selection",
            g_type_name(v->g_type));
        }
      }
    }
  }

  g_free(s);

  if (i == n_items)
    hildon_picker_button_set_active(HILDON_PICKER_BUTTON(button), 0);
  else
    hildon_picker_button_set_active(HILDON_PICKER_BUTTON(button), i);
}

static void
transport_value_changed_cb(GtkWidget *button, gpointer user_data)
{
  GladeXML *xml = glade_get_widget_tree(button);
  GtkWidget *proxy_port;
  gint val;

  if (!xml)
    return;

  proxy_port = glade_xml_get_widget(xml, "proxy-port");

  if (!proxy_port)
    return;

  val = rtcom_param_int_get_value(RTCOM_PARAM_INT(proxy_port));

  /* TLS */
  if (hildon_picker_button_get_active(HILDON_PICKER_BUTTON(button)) == 3)
  {
    if ((val == G_MININT) || (val == 5060))
      rtcom_param_int_set_value(RTCOM_PARAM_INT(proxy_port), 5061);
  }
  else
  {
    if ((val == G_MININT) || (val == 5061))
      rtcom_param_int_set_value(RTCOM_PARAM_INT(proxy_port), 5060);
  }
}

static void
discover_stun_toggled_cb(GtkWidget *button, gpointer user_data)
{
  GladeXML *xml = glade_get_widget_tree(button);
  gboolean active = hildon_check_button_get_active(HILDON_CHECK_BUTTON(button));
  GtkWidget *server;
  GtkWidget *port;
  GtkWidget *server_lbl;
  GtkWidget *port_lbl;

  void (*fn)(GtkWidget *);

  if (!xml)
    return;

  server = glade_xml_get_widget(xml, "stun_server_entry");
  port = glade_xml_get_widget(xml, "stun_port_entry");
  server_lbl = glade_xml_get_widget(xml, "stun_server_lbl");
  port_lbl = glade_xml_get_widget(xml, "stun_port_lbl");

  if (rtcom_param_int_get_value(RTCOM_PARAM_INT(port)) == G_MININT)
    rtcom_param_int_set_value(RTCOM_PARAM_INT(port), 3478);

  if (active)
  {
    fn = gtk_widget_hide;
    gtk_widget_hide(server);
  }
  else
  {
    fn = gtk_widget_show;
    gtk_widget_show(server);
  }

  fn(port);
  fn(server_lbl);
  fn(port_lbl);
  gtk_widget_set_sensitive(server, !active);
  gtk_widget_set_sensitive(port, !active);
}

static GtkWidget *
create_advanced_settings_page(RtcomDialogContext *context)
{
  GtkWidget *dialog = g_object_get_data(G_OBJECT(context), "page_advanced");

  if (!dialog)
  {
    GladeXML *xml = glade_xml_new(
        PLUGIN_XML_DIR "/sip-advanced.glade", NULL, GETTEXT_PACKAGE);
    gboolean cellular_active = TRUE;
    gchar title[200];
    GtkWidget *page;
    AccountService *service;
    GtkWidget *start_page;
    TpProtocol *protocol;
    GtkWidget *selector;
    GtkWidget *button;
    GHashTable *settings;
    RtcomAccountItem *item;
    const gchar *msgid;
    int i;

    rtcom_dialog_context_take_obj(context, G_OBJECT(xml));
    dialog = glade_xml_get_widget(xml, "advanced");

    if (!dialog)
    {
      g_warning("Unable to load Advanced settings dialog");
      return dialog;
    }

    gtk_window_set_default_size(GTK_WINDOW(dialog), -1, 200);
    gtk_dialog_add_buttons(GTK_DIALOG(dialog),
                           dgettext("hildon-libs", "wdgt_bd_done"),
                           GTK_RESPONSE_OK, NULL);
    item = RTCOM_ACCOUNT_ITEM(account_edit_context_get_account(
                                ACCOUNT_EDIT_CONTEXT(context)));
    page = glade_xml_get_widget(xml, "page");
    rtcom_page_set_account(RTCOM_PAGE(page), item);

    service = account_item_get_service(ACCOUNT_ITEM(item));
    msgid = _("accountwizard_ti_advanced_settings");
    g_snprintf(title, sizeof(title), msgid,
               account_service_get_display_name(service));
    gtk_window_set_title(GTK_WINDOW(dialog), title);

    start_page = rtcom_dialog_context_get_start_page(context);

    if (start_page)
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel(start_page);

      if (toplevel)
      {
        gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(toplevel));
        gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
      }
    }

    g_object_ref(dialog);
    g_object_set_data_full(G_OBJECT(context), "page_advanced", dialog,
                           g_object_unref);
    init_check_button(xml, BUTTON("discover-binding"), "discover-binding",
                      context, item, TRUE);
    init_check_button(xml, BUTTON("loose-routing"), "loose-routing", context,
                      item, FALSE);

    /* cellular-call */
    button = glade_xml_get_widget(xml, BUTTON("cellular-call"));
    g_object_set_data(G_OBJECT(context), BUTTON("cellular-call"), button);
    protocol = rtcom_account_service_get_protocol(
        RTCOM_ACCOUNT_SERVICE(service));

    if (protocol)
    {
      TpCapabilities *caps = tp_protocol_get_capabilities(protocol);

      if (tp_capabilities_supports_audio_call(caps, TP_HANDLE_TYPE_CONTACT) ||
          tp_capabilities_supports_audio_call(caps, TP_HANDLE_TYPE_NONE) ||
          tp_capabilities_supports_audio_call(caps, TP_HANDLE_TYPE_ROOM))
      {
        gtk_widget_show(button);
      }
      else
        gtk_widget_hide(button);
    }

    if (item->account)
    {
      const gchar *const *schemes = tp_account_get_uri_schemes(item->account);

      cellular_active = FALSE;

      if (schemes)
      {
        while (*schemes)
        {
          if (!strcmp(*schemes, "tel"))
          {
            cellular_active = TRUE;
            break;
          }

          schemes++;
        }
      }
    }
    else
    {
      gchar *cellular_default_active;

      cellular_default_active = account_get_default_setting(item, "cellular");

      if (cellular_default_active)
      {
        cellular_active = !strcmp(cellular_default_active, "true");
        g_free(cellular_default_active);
      }
    }

    hildon_check_button_set_active(HILDON_CHECK_BUTTON(button),
                                   cellular_active);

    /* transport */
    selector = hildon_touch_selector_new_text();

    for (i = 0; i < G_N_ELEMENTS(transport_items); i++)
      hildon_touch_selector_append_text(HILDON_TOUCH_SELECTOR(selector),
                                        _(transport_items[i].msgid));

    button = glade_xml_get_widget(xml, BUTTON("transport"));
    hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(button),
                                      HILDON_TOUCH_SELECTOR(selector));
    picker_button_set_active(button, transport_items,
                             G_N_ELEMENTS(transport_items), item, "transport");
    g_signal_connect(button, "value-changed",
                     G_CALLBACK(transport_value_changed_cb), NULL);
    transport_value_changed_cb(button, NULL);
    g_object_set_data(G_OBJECT(context), BUTTON("transport"), button);

    /* keepalive-mechanism */
    selector = hildon_touch_selector_new_text();

    for (i = 0; i < G_N_ELEMENTS(keepalive_mechanizm_items); i++)
    {
      hildon_touch_selector_append_text(HILDON_TOUCH_SELECTOR(selector),
                                        _(keepalive_mechanizm_items[i].msgid));
    }

    button = glade_xml_get_widget(xml, BUTTON("keepalive-mechanism"));
    hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(button),
                                      HILDON_TOUCH_SELECTOR(selector));
    picker_button_set_active(button, keepalive_mechanizm_items,
                             G_N_ELEMENTS(keepalive_mechanizm_items), item,
                             "keepalive-mechanism");
    g_object_set_data(G_OBJECT(context), BUTTON("keepalive-mechanism"), button);

    /* keepalive-interval */
    selector = hildon_touch_selector_new_text();

    for (i = 0; i < G_N_ELEMENTS(keepalive_interval_items); i++)
    {
      hildon_touch_selector_append_text(HILDON_TOUCH_SELECTOR(selector),
                                        _(keepalive_interval_items[i].msgid));
    }

    button = glade_xml_get_widget(xml, BUTTON("keepalive-interval"));
    hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(button),
                                      HILDON_TOUCH_SELECTOR(selector));
    picker_button_set_active(button, keepalive_interval_items,
                             G_N_ELEMENTS(keepalive_interval_items), item,
                             "keepalive-interval");
    g_object_set_data(G_OBJECT(context), BUTTON("keepalive-interval"), button);

    /* discover-stun */
    init_check_button(xml, BUTTON("discover-stun"), "discover-stun", context,
                      item, TRUE);

    button = glade_xml_get_widget(xml, BUTTON("discover-stun"));
    g_signal_connect(button, "toggled",
                     G_CALLBACK(discover_stun_toggled_cb), context);
    discover_stun_toggled_cb(button, context);

    settings = g_hash_table_new(NULL, NULL);
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
sip_plugin_on_advanced_cb(RtcomDialogContext *context)
{
  GtkWidget *dialog = create_advanced_settings_page(context);

  gtk_widget_show(dialog);
}

static void
sip_plugin_context_init(RtcomAccountPlugin *plugin, RtcomDialogContext *context)
{
  SipPluginPrivate *priv = PRIVATE(plugin);
  sip_account *sa = g_slice_new(sip_account);
  AccountItem *item;
  GtkWidget *page;
  gboolean editing;

  editing = account_edit_context_get_editing(ACCOUNT_EDIT_CONTEXT(context));
  item = account_edit_context_get_account(ACCOUNT_EDIT_CONTEXT(context));

  sa->item = item;
  sa->context = context;
  priv->accounts = g_list_prepend(priv->accounts, sa);

  g_object_add_weak_pointer(G_OBJECT(sa->context), (gpointer *)&sa->context);
  g_signal_connect(item, "store-settings",
                   G_CALLBACK(on_store_settings), sa);
  create_advanced_settings_page(context);

  if (editing)
  {
    page = g_object_new(
        RTCOM_TYPE_EDIT,
        "username-field", "account",
        "username-label", _("accounts_fi_user_name_sip"),
        "username-invalid-chars-re", INVALID_CHARS_RE,
        "msg-empty", _("accounts_fi_enter_address_and_password_fields_first"),
        "items-mask", plugin->capabilities,
        "account", item,
        NULL);
    rtcom_edit_connect_on_advanced(
      RTCOM_EDIT(page), G_CALLBACK(sip_plugin_on_advanced_cb), context);
  }
  else
  {
    page = g_object_new(
        RTCOM_TYPE_LOGIN,
        "username-field", "account",
        "username-invalid-chars-re", INVALID_CHARS_RE,
        "username-placeholder", "sip.address@example.com",
        "username-label", _("accounts_fi_user_name_sip"),
        "msg-empty", _("accounts_fi_enter_address_and_password_fields_first"),
        "items-mask", plugin->capabilities,
        "account", item,
        NULL);
    rtcom_login_connect_on_advanced(
      RTCOM_LOGIN(page), G_CALLBACK(sip_plugin_on_advanced_cb), context);
  }

  if (item)
    rtcom_page_set_account(RTCOM_PAGE(page), RTCOM_ACCOUNT_ITEM(item));

  rtcom_dialog_context_set_start_page(context, page);
}

static void
sip_plugin_class_init(SipPluginClass *klass)
{
  G_OBJECT_CLASS(klass)->finalize = sip_plugin_finalize;
  RTCOM_ACCOUNT_PLUGIN_CLASS(klass)->context_init = sip_plugin_context_init;
}
