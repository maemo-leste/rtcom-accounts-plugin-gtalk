#!/bin/sh
cc -I. -DG_LOG_DOMAIN="\"rtcom-accounts-ui\"" -DGETTEXT_PACKAGE="\"osso-applet-accounts\"" -DPLUGIN_XML_DIR="\"`pkg-config --variable=profiles_dir libmcclient`\"" `pkg-config --cflags --libs rtcom-accounts-widgets libglade-2.0 telepathy-glib` -W -Wall -O2 -shared -Wl,-soname=libgtalk-plugin.so.0 gtalk-plugin.c -o libgtalk-plugin.so.0.0.0
