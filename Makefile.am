#   PGCOP - PocoGraph Component Oriented Platform.
#   Copyright (C) 2013  David Wu <david@pocograph.com>

#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.

#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.

#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.

noinst_LTLIBRARIES = mod_tester.la \
	                   mod_tester_remote.la

mod_tester_la_SOURCES = pg_cop_mod_tester.c
mod_tester_la_LIBADD = ../libPGCOP/libPGCOP.la
mod_tester_la_LDFLAGS = -avoid-version -module -rpath /nowhere
mod_tester_la_CFLAGS = -I ../libPGCOP

mod_tester_remote_la_SOURCES = pg_cop_mod_tester_remote.c
mod_tester_remote_la_LIBADD = ../libPGCOP/libPGCOP.la
mod_tester_remote_la_LDFLAGS = -avoid-version -module -rpath /nowhere
mod_tester_remote_la_CFLAGS = -I ../libPGCOP

install-data-hook:
	mkdir -p $(datadir)/pgcop/modules
	cp -L .libs/*.so $(datadir)/pgcop/modules
