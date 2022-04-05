pluginlib_LTLIBRARIES = \
	libgtalk-plugin.la

libgtalk_plugin_ladir = $(pluginlibdir)
libgtalk_plugin_la_SOURCES = \
		gtalk-plugin.c

libgtalk_plugin_la_CFLAGS =						\
		$(ACCOUNTS_CFLAGS) $(GLADE_CFLAGS)			\
		-DG_LOG_DOMAIN=\"$(PACKAGE)\"				\
		-DPLUGIN_XML_DIR=\"$(pluginlibdir)/xml\"

libgtalk_plugin_la_LDFLAGS =						\
		-Wl,--as-needed $(ACCOUNTS_LIBS) $(GLADE_LIBS)		\
		-Wl,--no-undefined -module -avoid-version

MAINTAINERCLEANFILES = Makefile.in

