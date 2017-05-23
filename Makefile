# Davrods build system.
# Author: Chris Smeele
# Author: Simon Tyrrell
# Copyright (c) 2016, Utrecht University


#
# Create a file called user.prefs by copying the example-user.prefs file
# to user.prefs and editing its values.
#
-include user.prefs

MODNAME      ?= davrods
SHARED_FNAME := mod_$(MODNAME).so
CC	:= gcc
OUTPUT_DIR	 := build
SHARED       := $(OUTPUT_DIR)/$(SHARED_FNAME)

APXS := $(APACHE_DIR)/bin/apxs

APACHE_INCLUDES_DIR := $(APACHE_DIR)/include


ifeq ($(strip $(APXS)),)
APXS	:= $(shell which apxs)
endif


# XXX: These are currently unused as we rely on the apxs utility for module
#      installation.
INSTALL_DIR  ?= $(APACHE_DIR)/modules
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
OBJFILES := $(CFILES:%.c=$(OUTPUT_DIR)/%.o)
OUTFILES := $(OBJFILES) $(CFILES:%.c=%.lo) $(CFILES:%.c=%.slo) $(CFILES:%.c=%.la)

INCLUDE_PATHS += $(IRODS_DIR)/usr/include
LIB_PATHS += $(IRODS_DIR)/usr/lib

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


	
WARNINGS :=                           \
	all                           \
	extra                         \
	no-unused-parameter           \
	no-missing-field-initializers \
	no-format                     \
	fatal-errors \
	shadow

#MACROS += \
#	$(addprefix DAVRODS_ENABLE_PROVIDER_, $(DAV_PROVIDERS))      \
#	DAVRODS_PROVIDER_NAME=\\\"$(DAV_PROVIDER_NAME_PREFIX)\\\"    \
#	DAVRODS_CONFIG_PREFIX=\\\"$(DAV_CONFIG_DIRECTIVE_PREFIX)\\\" \
#	DAVRODS_DEBUG_VERY_DESPERATE=1

MACROS += \
	$(addprefix DAVRODS_ENABLE_PROVIDER_, $(DAV_PROVIDERS))      \
	DAVRODS_PROVIDER_NAME=\"$(DAV_PROVIDER_NAME_PREFIX)\"    \
	DAVRODS_CONFIG_PREFIX=\"$(DAV_CONFIG_DIRECTIVE_PREFIX)\" \
	DAVRODS_DEBUG_VERY_DESPERATE=1


ifdef DEBUG
MACROS += DAVRODS_DEBUG_VERY_DESPERATE
endif

CFLAGS +=                              \
	-g3                            \
	-ggdb                          \
	-O0							\
	-pg \
	-std=c99                       \
	-pedantic                      \
	$(addprefix -W, $(WARNINGS))   \
	$(addprefix -D, $(MACROS)) \
	$(addprefix -I, $(INCLUDE_PATHS))

LDFLAGS +=                           \
	$(addprefix -l, $(LIBS))     \
	$(addprefix -L, $(LIB_PATHS)) \
	-shared 

comma := ,

.PHONY: all shared install test clean apxs init

all: init shared




init:
	mkdir -p $(OUTPUT_DIR)

install: $(SHARED)
	cp $(SHARED) $(INSTALL_DIR)/

shared: $(SHARED)

$(SHARED): init $(OBJFILES)
	@echo "LIB_PATHS: $(LIB_PATHS)"
	$(LD) -o $(SHARED) $(OBJFILES) $(STATIC_LIBS) $(LDFLAGS)

clean:
	rm -rvf  $(BUILD_DIR)/*

	
#$(SHARED): apxs $(SRCFILES) 
#	$(APXS) -c   \
#	$(addprefix -Wc$(comma), $(CFLAGS))  \
#	$(addprefix -Wl$(comma), $(LDFLAGS)) \
#	-o $(SHARED_FNAME) $(SRCFILES)  

$(OUTPUT_DIR)/%.o: %.c 
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -fPIC -DLINUX -D_REENTRANT -D_GNU_SOURCE $< -o $@

apxs:
ifeq ($(strip $(APXS)),)
	echo "No APXS variable has been set, cannot compile"
	exit 1
endif


