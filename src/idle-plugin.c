/*
 * idle-plugin.c
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
#include <librtcom-accounts-widgets/rtcom-param-string.h>

typedef struct _IdlePluginClass IdlePluginClass;
typedef struct _IdlePlugin IdlePlugin;

struct _IdlePlugin
{
  RtcomAccountPlugin parent_instance;
};

struct _IdlePluginClass
{
  RtcomAccountPluginClass parent_class;
};

RtcomAccountPluginClass *parent_class;

ACCOUNT_DEFINE_PLUGIN(IdlePlugin, idle_plugin, RTCOM_TYPE_ACCOUNT_PLUGIN);

static void
idle_plugin_init(IdlePlugin *self)
{
  RtcomAccountService *service;
  GdkPixbuf *icon;

  RTCOM_ACCOUNT_PLUGIN(self)->name = "idle";
  RTCOM_ACCOUNT_PLUGIN(self)->capabilities =
      RTCOM_PLUGIN_CAPABILITY_ALLOW_MULTIPLE |
      RTCOM_PLUGIN_CAPABILITY_ADVANCED;
  service = rtcom_account_plugin_add_service(RTCOM_ACCOUNT_PLUGIN(self),
                                             "idle/irc");

  g_object_set(G_OBJECT(service),
               "display-name", "IRC",
               "supports-avatar", FALSE,
               NULL);

  icon = gtk_icon_theme_load_icon(
        gtk_icon_theme_get_default(), "im-irc", 48, 0, NULL);

  if (icon)
    g_object_set(G_OBJECT(service), "icon", icon, NULL);

  glade_init();
}

static void
on_advanced_settings_response(GtkWidget *dialog, gint response,
                              RtcomDialogContext *context)
{
  if (response == GTK_RESPONSE_OK)
  {
    GError *error = NULL;
    GladeXML *xml = glade_get_widget_tree(dialog);
    GtkWidget *page = glade_xml_get_widget(xml, "page");

    if (rtcom_page_validate(RTCOM_PAGE(page), &error))
      gtk_widget_hide(dialog);
    else
    {
      g_warning("advanced page validation failed");

      if (error)
      {
        g_warning("%s: error \"%s\"", G_STRFUNC, error->message);
        hildon_banner_show_information(dialog, NULL, error->message);
        g_error_free(error);
      }
    }
  }
  else
    gtk_widget_hide(dialog);
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
    gchar title[200];
    const gchar *msg;
    GladeXML *xml = glade_xml_new(PLUGIN_XML_DIR "/idle-advanced.glade",
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
        gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(toplevel));
        gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
      }
    }

    g_object_ref(dialog);
    g_object_set_data_full(
          G_OBJECT(context), "page_advanced", dialog, g_object_unref);
  }

  g_signal_connect(dialog, "response",
                   G_CALLBACK(on_advanced_settings_response), context);
  g_signal_connect(dialog, "delete-event", G_CALLBACK(gtk_true), NULL);

  return dialog;
}

static void
idle_plugin_on_advanced_cb(gpointer data)
{
  GtkWidget *dialog;
  RtcomDialogContext *context = RTCOM_DIALOG_CONTEXT(data);

  dialog = create_advanced_settings_page(context);
  gtk_widget_show(dialog);
}

static void
idle_plugin_context_init(RtcomAccountPlugin *plugin,
                         RtcomDialogContext *context)
{
  gboolean editing;
  AccountItem *account;
  GtkWidget *page;

  static const gchar *invalid_chars_re = "[:'\"<>&;#\\s]";

  editing = account_edit_context_get_editing(ACCOUNT_EDIT_CONTEXT(context));
  account = account_edit_context_get_account(ACCOUNT_EDIT_CONTEXT(context));
  /*create_advanced_settings_page(context);*/

  if (editing)
  {
    page =
      g_object_new(
        RTCOM_TYPE_EDIT,
        "username-field", "account",
        "username-label", _("accounts_fi_nickname"),
        "username-invalid-chars-re", invalid_chars_re,
        "items-mask", RTCOM_ACCOUNT_PLUGIN(plugin)->capabilities,
        "account", account,
        NULL);

    rtcom_edit_append_widget(
          RTCOM_EDIT(page),
          g_object_new(GTK_TYPE_LABEL,
                       "label", _("accounts_fi_server"),
                       "xalign", 0.0,
                       NULL),
          g_object_new (RTCOM_TYPE_PARAM_STRING,
                        "field", "server",
                        "can-next", FALSE,
                        "required", TRUE,
                        NULL));
    rtcom_edit_append_widget(
          RTCOM_EDIT(page),
          g_object_new(GTK_TYPE_LABEL,
                       "label", _("accounts_fi_real_name"),
                       "xalign", 0.0,
                       NULL),
          g_object_new(RTCOM_TYPE_PARAM_STRING,
                       "field", "fullname",
                       NULL));
    rtcom_edit_connect_on_advanced(
          RTCOM_EDIT(page), G_CALLBACK(idle_plugin_on_advanced_cb), context);
  }
  else
  {
    page =
      g_object_new(
        RTCOM_TYPE_LOGIN,
        "username-field", "account",
        "username-label", _("accounts_fi_nickname"),
        "username-invalid-chars-re", invalid_chars_re,
        "items-mask", RTCOM_ACCOUNT_PLUGIN(plugin)->capabilities,
        "account", account,
        NULL);
    rtcom_login_append_widget(
          RTCOM_LOGIN(page),
          g_object_new(GTK_TYPE_LABEL,
                       "label", _("accounts_fi_server"),
                       "xalign", 0.0,
                       NULL),
          g_object_new (RTCOM_TYPE_PARAM_STRING,
                        "field", "server",
                        "can-next", FALSE,
                        "required", TRUE,
                        NULL));
    rtcom_login_append_widget(
          RTCOM_LOGIN(page),
          g_object_new(GTK_TYPE_LABEL,
                       "label", _("accounts_fi_real_name"),
                       "xalign", 0.0,
                       NULL),
          g_object_new(RTCOM_TYPE_PARAM_STRING,
                       "field", "fullname",
                       NULL));

    rtcom_login_connect_on_advanced(
          RTCOM_LOGIN(page), G_CALLBACK(idle_plugin_on_advanced_cb), context);
  }

  rtcom_dialog_context_set_start_page(context, page);
}

static void
idle_plugin_class_init(IdlePluginClass *klass)
{
  parent_class = g_type_class_peek_parent(klass);
  RTCOM_ACCOUNT_PLUGIN_CLASS(klass)->context_init = idle_plugin_context_init;
}

