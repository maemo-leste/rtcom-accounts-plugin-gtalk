pluginlib_LTLIBRARIES = \
	libjabber-plugin.la \
	libsip-plugin.la \
	libidle-plugin.la

COMMON_CFLAGS = $(ACCOUNTS_CFLAGS) $(GLADE_CFLAGS) \
		-DG_LOG_DOMAIN=\"$(PACKAGE)\" \
		-DPLUGIN_XML_DIR=\"$(pluginlibdir)/xml\"

COMMON_LDFLAGS = -Wl,--as-needed $(ACCOUNTS_LIBS) $(GLADE_LIBS) \
		 -Wl,--no-undefined -module -avoid-version

libsip_plugin_la_SOURCES = sip-plugin.c
libsip_plugin_la_CFLAGS = $(COMMON_CFLAGS)
libsip_plugin_la_LDFLAGS = $(COMMON_LDFLAGS)

libidle_plugin_la_SOURCES = idle-plugin.c
libidle_plugin_la_CFLAGS = $(COMMON_CFLAGS)
libidle_plugin_la_LDFLAGS = $(COMMON_LDFLAGS)

libjabber_plugin_la_SOURCES = jabber-plugin.c
libjabber_plugin_la_CFLAGS = $(COMMON_CFLAGS)
libjabber_plugin_la_LDFLAGS = $(COMMON_LDFLAGS)

MAINTAINERCLEANFILES = Makefile.in

