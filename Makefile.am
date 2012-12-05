noinst_LTLIBRARIES = mod_socket.la \
                     mod_transceiver.la \
										 mod_cop_main_protocol.la \
	                   mod_transceiver_cli.la

mod_socket_la_SOURCES = pg_cop_mod_socket.c
mod_socket_la_LIBADD = ../libPGCOP/libPGCOP.la
mod_socket_la_LDFLAGS = -avoid-version -module -rpath /nowhere
mod_socket_la_CFLAGS = -I ../libPGCOP

mod_transceiver_la_SOURCES = pg_cop_mod_transceiver.c
mod_transceiver_la_LIBADD = ../libPGCOP/libPGCOP.la
mod_transceiver_la_LDFLAGS = -avoid-version -module -rpath /nowhere
mod_transceiver_la_CFLAGS = -I ../libPGCOP

mod_transceiver_cli_la_SOURCES = pg_cop_mod_transceiver_cli.c
mod_transceiver_cli_la_LIBADD = ../libPGCOP/libPGCOP.la
mod_transceiver_cli_la_LDFLAGS = -avoid-version -module -rpath /nowhere
mod_transceiver_cli_la_CFLAGS = -I ../libPGCOP

mod_cop_main_protocol_la_SOURCES = pg_cop_mod_cop_protocol.c
mod_cop_main_protocol_la_LIBADD = ../libPGCOP/libPGCOP.la
mod_cop_main_protocol_la_LDFLAGS = -avoid-version -module -rpath /nowhere
mod_cop_main_protocol_la_CFLAGS = -I ../libPGCOP

install-data-hook:
	mkdir -p $(datadir)/pgcop/modules
	cp -L .libs/*.so $(datadir)/pgcop/modules
