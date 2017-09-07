/**
 * \file
 * \brief     Davrods configuration.
 * \author    Chris Smeele
 * \copyright Copyright (c) 2016, Utrecht University
 *
 * This file is part of Davrods.
 *
 * Davrods is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Davrods is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Davrods.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"
#include "theme.h"

#include <apr_strings.h>

APLOG_USE_MODULE(davrods);


static const char *cmd_davrods_default_username(cmd_parms *cmd_p, void *config_p, const char *arg_p);

static const char *cmd_davrods_default_password (cmd_parms *cmd_p, void *config_p, const char *arg_p);



static int set_exposed_root(davrods_dir_conf_t *conf, const char *exposed_root) {
    conf->rods_exposed_root = exposed_root;

    if (!strcasecmp("Zone", exposed_root)) {
        conf->rods_exposed_root_type = DAVRODS_ROOT_ZONE_DIR;
    } else if (!strcasecmp("Home", exposed_root)) {
        conf->rods_exposed_root_type = DAVRODS_ROOT_HOME_DIR;
    } else if (!strcasecmp("User", exposed_root)) {
        conf->rods_exposed_root_type = DAVRODS_ROOT_USER_DIR;
    } else {
        conf->rods_exposed_root_type = DAVRODS_ROOT_CUSTOM_DIR;
        if (!strlen(exposed_root) || exposed_root[0] != '/')
            return -1;
    }
    return 0;
}

void *davrods_create_dir_config(apr_pool_t *p, char *dir) {
    davrods_dir_conf_t *conf = apr_pcalloc(p, sizeof(davrods_dir_conf_t));
    if (conf) {
        // We have no 'enabled' flag. Module activation is triggered by
        // directives 'AuthBasicProvider irods' and 'Dav irods'.
        conf->rods_host              = "localhost";
        conf->rods_port              = 1247;
        conf->rods_zone              = "tempZone";
        conf->rods_default_resource  = "";
        conf->rods_env_file          = "/etc/httpd/irods/irods_environment.json";
        conf->rods_auth_scheme       = DAVRODS_AUTH_NATIVE;
        // Default to having the user's home directory as the exposed root
        // because that collection is more or less guaranteed to be readable by
        // the current user (as opposed to the /<zone>/home directory, which
        // seems to be hidden for rodsusers by default).
        conf->rods_exposed_root      = "User"; // NOTE: Keep this in sync with the option below.
        conf->rods_exposed_root_type = DAVRODS_ROOT_USER_DIR;

        conf->rods_tx_buffer_size    = 4 * 1024 * 1024;
        conf->rods_rx_buffer_size    = 4 * 1024 * 1024;

        conf->tmpfile_rollback       = DAVRODS_TMPFILE_ROLLBACK_NO;
        conf->locallock_lockdb_path  = "/var/lib/davrods/lockdb_locallock";

        // Use the minimum PAM temporary password TTL. We
        // re-authenticate using PAM on every new HTTP connection, so
        // there's no use keeping the temporary password around for
        // longer than the maximum keepalive time. (We don't ever use
        // a temporary password more than once).
        conf->rods_auth_ttl          = 1; // In hours.

        conf -> davrods_api_path_s = NULL;
        conf -> davrods_public_username_s = NULL;
        conf -> davrods_public_password_s = NULL;
        conf -> theme_p = AllocateHtmlTheme (p);
        conf -> themed_listings = 0;
    }
    return conf;
}

void *davrods_merge_dir_config(apr_pool_t *p, void *_parent, void *_child) {
    davrods_dir_conf_t *parent_p = _parent;
    davrods_dir_conf_t *child_p  = _child;
    davrods_dir_conf_t *conf_p   = davrods_create_dir_config(p, "merge__");

    DAVRODS_PROP_MERGE(rods_host);
    DAVRODS_PROP_MERGE(rods_port);
    DAVRODS_PROP_MERGE(rods_zone);
    DAVRODS_PROP_MERGE(rods_default_resource);
    DAVRODS_PROP_MERGE(rods_env_file);
    DAVRODS_PROP_MERGE(rods_auth_scheme);

    const char *exposed_root = child_p->rods_exposed_root
        ? child_p->rods_exposed_root
        : parent_p->rods_exposed_root
            ? parent_p->rods_exposed_root
            : conf_p->rods_exposed_root;

    DAVRODS_PROP_MERGE(rods_tx_buffer_size);
    DAVRODS_PROP_MERGE(rods_rx_buffer_size);

    DAVRODS_PROP_MERGE(tmpfile_rollback);
    DAVRODS_PROP_MERGE(locallock_lockdb_path);

    DAVRODS_PROP_MERGE(davrods_api_path_s);
    DAVRODS_PROP_MERGE(davrods_public_username_s);
    DAVRODS_PROP_MERGE(davrods_public_password_s);

    DAVRODS_PROP_MERGE(themed_listings);

    MergeThemeConfigs (conf_p, parent_p, child_p, p);


    if (set_exposed_root (conf_p, exposed_root) < 0)
  		{
  			ap_log_perror (APLOG_MARK, APLOG_DEBUG, APR_EGENERAL, p, "davrods_merge_dir_config: set_exposed_root failed");
  			conf_p = NULL;
  		}

    return conf_p;
}

// Config setters {{{

static const char *cmd_davrodsserver(
    cmd_parms *cmd, void *config,
    const char *arg1, const char *arg2
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;

    conf->rods_host = arg1;

#define DAVRODS_MIN(x, y) ((x) <= (y) ? (x) : (y))
#define DAVRODS_MAX(x, y) ((x) >= (y) ? (x) : (y))

    apr_int64_t port = apr_atoi64(arg2);
    if (port == DAVRODS_MIN(65535, DAVRODS_MAX(1, port))) {
        conf->rods_port = port;
        return NULL;
    } else {
        return "iRODS server port out of range (1-65535)";
    }

#undef DAVRODS_MIN
#undef DAVRODS_MAX

}

static const char *cmd_davrodsauthscheme(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    if (!strcasecmp("Native", arg1)) {
        conf->rods_auth_scheme = DAVRODS_AUTH_NATIVE;
    } else if (!strcasecmp("PAM", arg1)) {
        conf->rods_auth_scheme = DAVRODS_AUTH_PAM;
    } else if (!strcasecmp("Standard", arg1)) {
        return "Invalid iRODS authentication scheme. Did you mean 'Native'?";
    } else {
        return "Invalid iRODS authentication scheme. Valid options are 'Native' or 'PAM'.";
    }
    return NULL;
}

static const char *cmd_davrodsauthttl(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    apr_int64_t ttl = apr_atoi64(arg1);
    if (ttl <= 0) {
        return "The auth TTL must be higher than zero.";
    } else if (errno == ERANGE || ttl >> 31) {
        return "Auth TTL is too high - please specify a value that fits in an int32_t (i.e.: no more than 2 billion hours).";
    } else {
        conf->rods_auth_ttl = (int)ttl;
        return NULL;
    }
}

static const char *cmd_davrodszone(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    conf->rods_zone = arg1;
    return NULL;
}

static const char *cmd_davrodsdefaultresource(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    conf->rods_default_resource = arg1;
    return NULL;
}

static const char *cmd_davrodsenvfile(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    conf->rods_env_file = arg1;
    return NULL;
}

static const char *cmd_davrodsexposedroot(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    if (set_exposed_root(conf, arg1) < 0) {
        if (
            conf->rods_exposed_root_type == DAVRODS_ROOT_CUSTOM_DIR
            && (
                !strlen(conf->rods_exposed_root)
              || conf->rods_exposed_root[0] != '/'
            )
        ) {
            return "iRODS exposed root must be one of 'Zone', 'Home', 'User' or a custom path starting with a '/'";
        } else {
            return "Could not set iRODS exposed root, is your path sane?";
        }
    }
    return NULL;
}

static const char *cmd_davrodstxbufferkbs(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    size_t kb = apr_atoi64(arg1);
    conf->rods_tx_buffer_size = kb * 1024;

    if (errno == ERANGE || conf->rods_tx_buffer_size < kb) {
        // This must be an overflow.
        return "Please check if your transfer buffer size is sane";
    }

    return NULL;
}

static const char *cmd_davrodsrxbufferkbs(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;
    size_t kb = apr_atoi64(arg1);
    conf->rods_rx_buffer_size = kb * 1024;

    if (errno == ERANGE || conf->rods_rx_buffer_size < kb) {
        return "Please check if your receive buffer size is sane";
    }

    return NULL;
}

static const char *cmd_davrodstmpfilerollback(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;

    if (!strcasecmp(arg1, "yes")) {
        conf->tmpfile_rollback = DAVRODS_TMPFILE_ROLLBACK_YES;
    } else if (!strcasecmp(arg1, "no")) {
        conf->tmpfile_rollback = DAVRODS_TMPFILE_ROLLBACK_NO;
    } else {
        return "This directive accepts only 'Yes' and 'No' values";
    }

    return NULL;
}

static const char *cmd_davrodslockdb(
    cmd_parms *cmd, void *config,
    const char *arg1
) {
    davrods_dir_conf_t *conf = (davrods_dir_conf_t*)config;

    conf->locallock_lockdb_path = arg1;

    return NULL;
}




static const char *cmd_davrods_html_header (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

    conf_p -> theme_p -> ht_head_s = arg_p;

    return NULL;
}



static const char *cmd_davrods_html_top (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

    conf_p -> theme_p -> ht_top_s = arg_p;

    return NULL;
}



static const char *cmd_davrods_html_bottom (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

    conf_p -> theme_p -> ht_bottom_s = arg_p;

    return NULL;
}



static const char *cmd_davrods_html_collection_icon (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

    conf_p -> theme_p -> ht_collection_icon_s = arg_p;

    return NULL;
}



static const char *cmd_davrods_html_object_icon (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

    conf_p -> theme_p -> ht_object_icon_s = arg_p;

    return NULL;
}



static const char *cmd_davrods_html_parent_icon (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

    conf_p -> theme_p -> ht_parent_icon_s = arg_p;

    return NULL;
}


static const char *cmd_davrods_html_listing_class (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

    conf_p -> theme_p -> ht_listing_class_s = arg_p;

    return NULL;
}


static const char *cmd_davrods_html_themed_listings (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

    if (!strcasecmp (arg_p, "true"))
    	{
    		conf_p -> themed_listings = 1;
    	}

    return NULL;
}

static const char *cmd_davrods_html_metadata (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

    if (!strcasecmp (arg_p, "none"))
    	{
    		conf_p -> theme_p -> ht_show_metadata_flag = MD_NONE;
    	}
    else if (!strcasecmp (arg_p, "full"))
    	{
    		conf_p -> theme_p -> ht_show_metadata_flag = MD_FULL;
    	}
    else if (!strcasecmp (arg_p, "on_demand"))
    	{
    		conf_p -> theme_p -> ht_show_metadata_flag = MD_ON_DEMAND;
    	}

    return NULL;
}


static const char *cmd_davrods_html_ids (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

    if (!strcasecmp (arg_p, "true"))
    	{
    		conf_p -> theme_p -> ht_show_ids_flag = 1;
    	}

    return NULL;
}



static const char *cmd_davrods_add_metadata_image (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

		conf_p -> theme_p -> ht_add_metadata_icon_s = arg_p;

    return NULL;
}

static const char *cmd_davrods_delete_metadata_image (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

		conf_p -> theme_p -> ht_delete_metadata_icon_s = arg_p;

    return NULL;
}

static const char *cmd_davrods_edit_metadata_image (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

		conf_p -> theme_p -> ht_edit_metadata_icon_s = arg_p;

    return NULL;
}


static const char *cmd_davrods_ok_image (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

		conf_p -> theme_p -> ht_ok_icon_s = arg_p;

    return NULL;
}

static const char *cmd_davrods_cancel_image (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

		conf_p -> theme_p -> ht_cancel_icon_s = arg_p;

    return NULL;
}

static const char *cmd_davrods_api_path (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

		conf_p -> davrods_api_path_s = arg_p;

    return NULL;
}

static const char *cmd_davrods_default_username(cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

		conf_p -> davrods_public_username_s = arg_p;

    return NULL;
}

static const char *cmd_davrods_default_password (cmd_parms *cmd_p, void *config_p, const char *arg_p)
{
    davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

		conf_p -> davrods_public_password_s = arg_p;

    return NULL;
}

static const char *cmd_davrods_html_add_icon (cmd_parms *cmd_p, void *config_p, const char *icon_s, const char *suffix_s)
{
  davrods_dir_conf_t *conf_p = (davrods_dir_conf_t*) config_p;

  if (! (conf_p -> theme_p -> ht_icons_map_p))
  	{
  		const int INITIAL_TABLE_SIZE = 16;
  		conf_p -> theme_p -> ht_icons_map_p = apr_table_make (cmd_p -> pool, INITIAL_TABLE_SIZE);
  	}

  apr_table_set (conf_p -> theme_p -> ht_icons_map_p, suffix_s, icon_s);

  return NULL;
}

// }}}

const command_rec davrods_directives[] = {
    AP_INIT_TAKE2(
        DAVRODS_CONFIG_PREFIX "Server", cmd_davrodsserver,
        NULL, ACCESS_CONF, "iRODS host and port to connect to"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "AuthScheme", cmd_davrodsauthscheme,
        NULL, ACCESS_CONF, "iRODS authentication scheme to use (either Native or PAM)"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "AuthTTLHours", cmd_davrodsauthttl,
        NULL, ACCESS_CONF, "Time-to-live of the temporary password in hours (only applies to PAM currently)"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "Zone", cmd_davrodszone,
        NULL, ACCESS_CONF, "iRODS zone"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "DefaultResource", cmd_davrodsdefaultresource,
        NULL, ACCESS_CONF, "iRODS default resource (optional)"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "EnvFile", cmd_davrodsenvfile,
        NULL, ACCESS_CONF, "iRODS environment file (defaults to /etc/httpd/irods/irods_environment.json)"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "ExposedRoot", cmd_davrodsexposedroot,
        NULL, ACCESS_CONF, "Root iRODS collection to expose (one of: Zone, Home, User, or a custom path)"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "TxBufferKbs", cmd_davrodstxbufferkbs,
        NULL, ACCESS_CONF, "Amount of file KiBs to upload to iRODS at a time on PUTs"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "RxBufferKbs", cmd_davrodsrxbufferkbs,
        NULL, ACCESS_CONF, "Amount of file KiBs to download from iRODS at a time on GETs"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "TmpfileRollback", cmd_davrodstmpfilerollback,
        NULL, ACCESS_CONF, "Support PUT rollback through the use of temporary files on the target iRODS resource"
    ),
    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "LockDB", cmd_davrodslockdb,
        NULL, ACCESS_CONF, "Lock database location, used by the davrods-locallock DAV provider"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "ThemedListings", cmd_davrods_html_themed_listings,
        NULL, ACCESS_CONF, "Set to true for themed listings, default is false"
    ),

	AP_INIT_RAW_ARGS(
        DAVRODS_CONFIG_PREFIX "HTMLHead", cmd_davrods_html_header,
        NULL, ACCESS_CONF, "Extra HTML to place within the <head> tag"
    ),

	AP_INIT_RAW_ARGS(
        DAVRODS_CONFIG_PREFIX "HTMLTop", cmd_davrods_html_top,
        NULL, ACCESS_CONF, "HTML to put before the collection listing"
    ),

	AP_INIT_RAW_ARGS(
        DAVRODS_CONFIG_PREFIX "HTMLBottom", cmd_davrods_html_bottom,
        NULL, ACCESS_CONF, "HTML to put after the collection listingg"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLCollectionIcon", cmd_davrods_html_collection_icon,
        NULL, ACCESS_CONF, "Icon to use for collections"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLObjectIcon", cmd_davrods_html_object_icon,
        NULL, ACCESS_CONF, "Icon to use for data objects"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLParentIcon", cmd_davrods_html_parent_icon,
        NULL, ACCESS_CONF, "Icon to use for parent link in the directory listings"
    ),

		AP_INIT_RAW_ARGS(
        DAVRODS_CONFIG_PREFIX "HTMLListingClass", cmd_davrods_html_listing_class,
        NULL, ACCESS_CONF, "CSS class to use for the listing <table> element"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLMetadata", cmd_davrods_html_metadata,
        NULL, ACCESS_CONF, "Options for displaying metadata"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLDeleteMetadataImage", cmd_davrods_delete_metadata_image,
        NULL, ACCESS_CONF, "Image for the delete metadata button"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLEditMetadataImage", cmd_davrods_edit_metadata_image,
        NULL, ACCESS_CONF, "Image for the edit metadata button"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLAddMetadataImage", cmd_davrods_add_metadata_image,
        NULL, ACCESS_CONF, "Image for the add metadata button"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLOkImage", cmd_davrods_ok_image,
        NULL, ACCESS_CONF, "Image for the OK button"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLCancelImage", cmd_davrods_cancel_image,
        NULL, ACCESS_CONF, "Image for the Cancel button"
    ),

    AP_INIT_ITERATE2 (DAVRODS_CONFIG_PREFIX "AddIcon", cmd_davrods_html_add_icon, NULL, ACCESS_CONF, "an icon URL followed by one or more filenames"),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLShowIds", cmd_davrods_html_ids,
        NULL, ACCESS_CONF, "Options for displaying irods ids"
    ),

		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "APIPath", cmd_davrods_api_path,
				NULL, ACCESS_CONF, "Set the location used for the Rest api. This is relative to the <Location> that davrods is hosted on."
		),

		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "DefaultUsername", cmd_davrods_default_username,
				NULL, ACCESS_CONF, "Set the default username to use if none is provided."
		),

		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "DefaultPassword", cmd_davrods_default_password,
				NULL, ACCESS_CONF, "Set the default password to use if none is provided."
		),

		{ NULL }
};
