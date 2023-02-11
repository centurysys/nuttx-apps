############################################################################
# apps/interpreters/luamodules/luasqlite3/Makefile
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

include $(APPDIR)/Make.defs

LUASQLITE3_DIR      = lsqlite3_fsl09y
LUASQLITE3_ZIP      = $(LUASQLITE3_DIR).zip
LUASQLITE3_UNPACK   = lsqlite3
LUASQLITE3_URL      = http://lua.sqlite.org/index.cgi/zip/lsqlite3_fsl09y.zip?uuid=fsl_9y
LUASQLITE3_SRC      = $(LUASQLITE3_UNPACK)

VPATH += $(LUASQLITE3_SRC)
CSRCS = lsqlite3.c

$(LUASQLITE3_ZIP):
	$(Q) echo "Downloading $(LUASQLITE3_ZIP) from $(LUASQLITE3_URL)"
	$(Q) curl -L $(LUASQLITE3_URL) -o $(LUASQLITE3_ZIP)

$(LUASQLITE3_UNPACK): $(LUASQLITE3_ZIP)
	$(Q) echo "Unzipping $(LUASQLITE3_ZIP) to $(LUASQLITE3_UNPACK)"
	$(Q) unzip $(LUASQLITE3_ZIP)
	$(Q) mv $(LUASQLITE3_DIR) $(LUASQLITE3_UNPACK)
	$(Q) patch -d $(LUASQLITE3_UNPACK) -p1 < disable_load_extension.patch

$(LUASQLITE3_UNPACK)/.patch: $(LUASQLITE3_UNPACK)
	$(Q) rm $(LUASQLITE3_UNPACK)/sqlite3.h
	$(Q) ln -s $(APPDIR)/include/database/sqlite3.h $(LUASQLITE3_UNPACK)/sqlite3.h
	touch $(LUASQLITE3_UNPACK)/.patch

# Download and unpack tarball if no git repo found
ifeq ($(wildcard $(LUASQLITE3_UNPACK)/.git),)
distclean::
	$(call DELDIR, $(LUASQLITE3_UNPACK))
	$(call DELFILE, $(LUASQLITE3_ZIP))

context:: $(LUASQLITE3_UNPACK)/.patch
endif

# Set LUAMODNAME and include Module.mk to add this module to the list of
# builtin modules for the Lua interpreter. LUAMODNAME should match the
# module's luaopen function.

LUAMODNAME = lsqlite3

include $(APPDIR)/interpreters/lua/Module.mk

include $(APPDIR)/Application.mk
