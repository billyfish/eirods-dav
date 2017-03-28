# Davrods build system.
# Author: Chris Smeele
# Copyright (c) 2016, Utrecht University

-include user.prefs

MODNAME      ?= davrods
SHARED_FNAME := mod_$(MODNAME).so
SHARED       := build/$(SHARED_FNAME)
CC	:= gcc

#
# Create a file called user.prefs to store any make configuration data
#
# It can contain the following variables:
#
# APXS 
# ----
#
# This is the path to the APXS executable to use for installing 
# the davrods module. If you're not using a system-wide version of 
# httpd, this allows you to use your given httpd installation.
# 
# E.g.
#
#	APXS = /home/billy/Applications/apahe/bin/apxs
#
#
# IRODS_VERSION
# -------------
#
# The set of libraries required differ for different versions of
# iRODS, so set this variable to use the correct set. For instance,
# if you have version 4.2.0 installed then this would be 
#
#	IRODS_VERSION = 4.2
#
#
# IRODS_EXTERNALS
# ---------------
#
# This is the path to the external dependencies for iRODS.
#


ifeq ($(strip $(APXS)),)
APXS	:= $(shell which apxs)
endif


# XXX: These are currently unused as we rely on the apxs utility for module
#      installation.
INSTALL_DIR  ?= /usr/lib64/httpd/modules
INSTALLED    := $(INSTALL_DIR)/mod_$(MODNAME).so
BUILD_DIR := build

CFILES := mod_davrods.c auth.c common.c config.c prop.c propdb.c repo.c meta.c theme.c rest.c listing.c

# The DAV providers supported by default (you can override this in the shell using DAV_PROVIDERS="..." make).
DAV_PROVIDERS ?= LOCALLOCK NOLOCKS

# A string that's prepended to davrods's DAV provider names (<string>-nolocks, <string>-locallock, ...).
DAV_PROVIDER_NAME_PREFIX    ?= $(MODNAME)

# A string that's prepended to davrods's configuration directives (<string>Zone, <string>DefaultResource, ...).
# Note: This is NOT case-sensitive.
DAV_CONFIG_DIRECTIVE_PREFIX ?= $(MODNAME)

ifneq (,$(findstring LOCALLOCK,$(DAV_PROVIDERS)))
# Compile local locking support using DBM if requested.
CFILES += lock_local.c
endif

HFILES := $(CFILES:%.c=%.h)
SRCFILES := $(CFILES) $(HFILES)
OBJFILES := $(CFILES:%.c=%.o)

INCLUDE_PATHS := $(IRODS_DIR)/usr/include
LIB_PATHS := $(IRODS_DIR)/usr/lib

# Add in the appropriate irods libs and dependencies
IRODS_VERSION_MAJOR := $(shell echo $(IRODS_VERSION) | cut -f1 -d ".")
IRODS_VERSION_MINOR := $(shell echo $(IRODS_VERSION) | cut -f2 -d ".")

IRODS_GT_4_2 := $(shell [ $(IRODS_VERSION_MAJOR) -gt 4 -o \( $(IRODS_VERSION_MAJOR) -eq 4 -a $(IRODS_VERSION_MINOR) -ge 2 \) ] && echo true)

ifeq ($(IRODS_GT_4_2), true)
LIBS += \
	irods_client \
	irods_common \
	irods_plugin_dependencies 
LIB_PATHS += \
	$(IRODS_EXTERNALS)/boost1.60.0-0/lib \
	$(IRODS_EXTERNALS)/jansson2.7-0/lib
MACROS += IRODS_4_2
else
LIBS += \
	irods_client_core    \
	irods_client_api \
	irods_client_api_table \
	irods_client_plugins 
MACROS += IRODS_4_1        
LIB_PATHS += \
	      $(IRODS_EXTERNALS)
endif


INCLUDE_PATHS += $(APACHE_INCLUDES_DIR)

# Most of these are iRODS client lib dependencies.
LIBS +=                  \
	dl               \
	m                \
	pthread          \
	crypto           \
	boost_system     \
	boost_filesystem \
	boost_regex      \
	boost_thread     \
	boost_chrono     \
	boost_program_options \
	stdc++           \
	ssl              \
	jansson

LIBPATHS := \
	/home/billy/Applications/irods/usr/lib \
	$(IRODS_EXTERNALS) \
	/usr/lib/gcc/x86_64-linux-gnu/5
	
WARNINGS :=                           \
	all                           \
	extra                         \
	no-unused-parameter           \
	no-missing-field-initializers \
	no-format                     \
	fatal-errors \
	shadow

MACROS += \
	$(addprefix DAVRODS_ENABLE_PROVIDER_, $(DAV_PROVIDERS))      \
	DAVRODS_PROVIDER_NAME=\\\"$(DAV_PROVIDER_NAME_PREFIX)\\\"    \
	DAVRODS_CONFIG_PREFIX=\\\"$(DAV_CONFIG_DIRECTIVE_PREFIX)\\\" \
	DAVRODS_DEBUG_VERY_DESPERATE=1

ifdef DEBUG
MACROS += DAVRODS_DEBUG_VERY_DESPERATE
endif

CFLAGS +=                              \
	-g3                            \
	-ggdb                          \
	-O0							\
	-std=c99                       \
	-pedantic                      \
	$(addprefix -W, $(WARNINGS))   \
	$(addprefix -D, $(MACROS)) \
	$(addprefix -I, $(INCLUDE_PATHS))

LDFLAGS +=                           \
	$(addprefix -l, $(LIBS))     \
	$(addprefix -L, $(LIBPATHS)) \
	-shared 

comma := ,

.PHONY: all shared install test clean apxs init

all: init shared


init:
	mkdir -p $(BUILD_DIR)


install: $(SHARED)
	sudo $(APXS) -i -n $(MODNAME)_module $(SHARED)
	sudo service httpd restart

shared: $(SHARED)

#$(SHARED): $(OBJFILES)
#	$(LD) -o $(SHARED) $(OBJFILES) $(STATIC_LIBS) $(LDFLAGS)

clean:
	rm -rvf  $(BUILD_DIR)/*

	
$(SHARED): apxs $(SRCFILES) 
	$(APXS) -c   \
	$(addprefix -Wc$(comma), $(CFLAGS))  \
	$(addprefix -Wl$(comma), $(LDFLAGS)) \
	-o $(SHARED_FNAME) $(SRCFILES)  

apxs:
ifeq ($(strip $(APXS)),)
	echo "No APXS variable has been set, cannot compile"
	exit 1
endif


