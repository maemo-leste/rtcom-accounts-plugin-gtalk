pluginlib_LTLIBRARIES = \
	libgtalk-plugin.la

libgtalk_plugin_ladir = $(pluginlibdir)
libgtalk_plugin_la_SOURCES = \
	gtalk-plugin.c

libgtalk_plugin_la_CFLAGS = $(ACCOUNTS_CFLAGS) $(GLADE_CFLAGS) \
		-DPLUGIN_XML_DIR=\"$(pluginlibdir)/xml\"
libgtalk_plugin_la_LIBADD = $(ACCOUNTS_LIBS) $(GLADE_LIBS)

AM_CFLAGS = \
		$(ACCOUNTS_CFLAGS)
		-Wall -Werror \
		-DG_LOG_DOMAIN=\"$(PACKAGE)\"

AM_LIBS = $(ACCOUNTS_LIBS)
