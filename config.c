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
#include "common.h"

#include <apr_strings.h>

APLOG_USE_MODULE(davrods);


static const char * const S_DEFAULT_HOST_S = "localhost";
static const uint16_t S_DEFAULT_PORT = 1247;
static const char * const S_DEFAULT_ZONE_S = "tempZone";
static const char * const S_DEFAULT_RESOURCE_S = "";
static const char * const S_DEFAULT_ENV_S = "/etc/httpd/irods/irods_environment.json";
static const RodsAuthScheme S_DEFAULT_AUTH_SCHEME = DAVRODS_AUTH_NATIVE;
static const char * const S_DEFAULT_EXPOSED_ROOT_S = "User";
static const RodsExposedRootType S_DEFAULT_EXPOSED_ROOT_TYPE = DAVRODS_ROOT_USER_DIR;

static const size_t S_DEFAULT_TX_BUFFER_SIZE = 4 * 1024 * 1024;
static const size_t S_DEFAULT_RX_BUFFER_SIZE = 4 * 1024 * 1024;

static const TmpFileBehaviour S_DEFAULT_TMPFILE_ROLLBACK = DAVRODS_TMPFILE_ROLLBACK_NO;
static const char * const S_DEFAULT_LOCK_DBPATH_S = "/var/lib/davrods/lockdb_locallock";
static const int S_DEFAULT_AUTH_TTL = 1;

static const char * const S_DEFAULT_API_PATH_S = "/api/";
static const char * const S_DEFAULT_PUBLIC_USERNAME_S = NULL;
static const char * const S_DEFAULT_PUBLIC_PASSWORD_S = NULL;
static const int S_DEFAULT_THEMED_LISTINGS = 0;


static const char *MergeConfigStrings (const char *parent_s, const char *child_s, const char *default_s);

static int MergeConfigInts (const int parent_value, const int child_value, const int default_value);


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
        conf->rods_host              = S_DEFAULT_HOST_S;
        conf->rods_port              = S_DEFAULT_PORT;
        conf->rods_zone              = S_DEFAULT_ZONE_S;
        conf->rods_default_resource  = S_DEFAULT_RESOURCE_S;
        conf->rods_env_file          = S_DEFAULT_ENV_S;
        conf->rods_auth_scheme       = S_DEFAULT_AUTH_SCHEME;
        // Default to having the user's home directory as the exposed root
        // because that collection is more or less guaranteed to be readable by
        // the current user (as opposed to the /<zone>/home directory, which
        // seems to be hidden for rodsusers by default).
        conf->rods_exposed_root      = S_DEFAULT_EXPOSED_ROOT_S; // NOTE: Keep this in sync with the option below.
        conf->rods_exposed_root_type = S_DEFAULT_EXPOSED_ROOT_TYPE;

        conf->rods_tx_buffer_size    = S_DEFAULT_TX_BUFFER_SIZE;
        conf->rods_rx_buffer_size    = S_DEFAULT_RX_BUFFER_SIZE;

        conf->tmpfile_rollback       = S_DEFAULT_TMPFILE_ROLLBACK;
        conf->locallock_lockdb_path  = S_DEFAULT_LOCK_DBPATH_S;

        // Use the minimum PAM temporary password TTL. We
        // re-authenticate using PAM on every new HTTP connection, so
        // there's no use keeping the temporary password around for
        // longer than the maximum keepalive time. (We don't ever use
        // a temporary password more than once).
        conf->rods_auth_ttl          = S_DEFAULT_AUTH_TTL; // In hours.

        conf -> davrods_api_path_s = S_DEFAULT_API_PATH_S;
        conf -> davrods_public_username_s = S_DEFAULT_PUBLIC_USERNAME_S;
        conf -> davrods_public_password_s = S_DEFAULT_PUBLIC_PASSWORD_S;
        conf -> theme_p = AllocateHtmlTheme (p);
        conf -> themed_listings = S_DEFAULT_THEMED_LISTINGS;

    		conf -> exposed_roots_per_user_p = apr_table_make (p, 16);

    }
    return conf;
}

void *davrods_merge_dir_config(apr_pool_t *p, void *_parent, void *_child) {
    davrods_dir_conf_t *parent_p = _parent;
    davrods_dir_conf_t *child_p  = _child;
    davrods_dir_conf_t *conf_p   = davrods_create_dir_config(p, "merge__");
    const char *exposed_root_s = NULL;

    conf_p -> rods_host = MergeConfigStrings (parent_p -> rods_host, child_p -> rods_host, S_DEFAULT_HOST_S);
    conf_p -> rods_port = MergeConfigInts (parent_p -> rods_port, child_p -> rods_port, S_DEFAULT_PORT);
    conf_p -> rods_zone = MergeConfigStrings (parent_p -> rods_zone, child_p -> rods_zone, S_DEFAULT_ZONE_S);
    conf_p -> rods_default_resource = MergeConfigStrings (parent_p -> rods_default_resource, child_p -> rods_default_resource, S_DEFAULT_RESOURCE_S);
    conf_p -> rods_env_file = MergeConfigStrings (parent_p -> rods_env_file, child_p -> rods_env_file, S_DEFAULT_ENV_S);
    conf_p -> rods_auth_scheme = MergeConfigInts (parent_p -> rods_auth_scheme, child_p -> rods_auth_scheme, S_DEFAULT_AUTH_SCHEME);

    conf_p -> rods_tx_buffer_size = MergeConfigInts (parent_p -> rods_tx_buffer_size, child_p -> rods_tx_buffer_size, S_DEFAULT_TX_BUFFER_SIZE);
    conf_p -> rods_rx_buffer_size = MergeConfigInts (parent_p -> rods_rx_buffer_size, child_p -> rods_rx_buffer_size, S_DEFAULT_RX_BUFFER_SIZE);
    conf_p -> tmpfile_rollback = MergeConfigInts (parent_p -> tmpfile_rollback, child_p -> tmpfile_rollback, S_DEFAULT_TMPFILE_ROLLBACK);
    conf_p -> locallock_lockdb_path = MergeConfigStrings (parent_p -> locallock_lockdb_path, child_p -> locallock_lockdb_path, S_DEFAULT_LOCK_DBPATH_S);
    conf_p -> davrods_api_path_s = MergeConfigStrings (parent_p -> davrods_api_path_s, child_p -> davrods_api_path_s, S_DEFAULT_API_PATH_S);
    conf_p -> davrods_public_username_s = MergeConfigStrings (parent_p -> davrods_public_username_s, child_p -> davrods_public_username_s, S_DEFAULT_PUBLIC_USERNAME_S);
    conf_p -> davrods_public_password_s = MergeConfigStrings (parent_p -> davrods_public_password_s, child_p -> davrods_public_password_s, S_DEFAULT_PUBLIC_PASSWORD_S);
    conf_p -> themed_listings = MergeConfigInts (parent_p -> themed_listings, child_p -> themed_listings, S_DEFAULT_THEMED_LISTINGS);


    MergeThemeConfigs (conf_p, parent_p, child_p, p);


  	conf_p -> exposed_roots_per_user_p = MergeAPRTables (parent_p -> exposed_roots_per_user_p, child_p -> exposed_roots_per_user_p, p);


  	exposed_root_s = MergeConfigStrings (parent_p -> rods_exposed_root, child_p -> rods_exposed_root, S_DEFAULT_EXPOSED_ROOT_S);

    if (set_exposed_root (conf_p, exposed_root_s) < 0)
  		{
  			ap_log_perror (APLOG_MARK, APLOG_DEBUG, APR_EGENERAL, p, "davrods_merge_dir_config: set_exposed_root failed for \"%s\"", exposed_root_s);
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



static const char *MergeConfigStrings (const char *parent_s, const char *child_s, const char *default_s)
{
	const char *res_s = default_s;

	if (default_s)
		{
			if (child_s && (strcmp (child_s, default_s) != 0))
				{
					res_s = child_s;
				}
			else if (parent_s && (strcmp (parent_s, default_s) != 0))
				{
					res_s = parent_s;
				}
		}
	else
		{
			if (child_s)
				{
					res_s = child_s;
				}
			else if (parent_s)
				{
					res_s = parent_s;
				}
		}

	return res_s;
}


static int MergeConfigInts (const int parent_value, const int child_value, const int default_value)
{
	const int res = child_value != default_value ? child_value : parent_value;

	return res;
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
        DAVRODS_CONFIG_PREFIX "ThemedListings", SetShowThemedListings,
        NULL, ACCESS_CONF, "Set to true for themed listings, default is false"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "ShowResource", SetShowResources,
        NULL, ACCESS_CONF, "Show the resource, default is false"
    ),


		AP_INIT_RAW_ARGS(
	        DAVRODS_CONFIG_PREFIX "SelectedResources", SetShowSelectedResourcesOnly,
	        NULL, ACCESS_CONF, "List of resources to show with each entry separated by spaces"
	    ),

	AP_INIT_RAW_ARGS(
        DAVRODS_CONFIG_PREFIX "HTMLHead", SetHeadHTML,
        NULL, ACCESS_CONF, "Extra HTML to place within the <head> tag"
    ),

	AP_INIT_RAW_ARGS(
        DAVRODS_CONFIG_PREFIX "HTMLTop", SetTopHTML,
        NULL, ACCESS_CONF, "HTML to put before the collection listing"
    ),

	AP_INIT_RAW_ARGS(
        DAVRODS_CONFIG_PREFIX "HTMLBottom", SetBottomHTML,
        NULL, ACCESS_CONF, "HTML to put after the collection listingg"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLCollectionIcon", SetCollectionImage,
        NULL, ACCESS_CONF, "Icon to use for collections"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLObjectIcon", SetObjectImage,
        NULL, ACCESS_CONF, "Icon to use for data objects"
    ),

		AP_INIT_RAW_ARGS(
        DAVRODS_CONFIG_PREFIX "HTMLListingClass", SetTableListingClass,
        NULL, ACCESS_CONF, "CSS class to use for the listing <table> element"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLMetadata", SetMetadataDisplay,
        NULL, ACCESS_CONF, "Options for displaying metadata"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLMetadataEditable", SetMetadataIsEditable,
        NULL, ACCESS_CONF, "Options for editing metadata"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLDeleteMetadataImage", SetDeleteMetadataImage,
        NULL, ACCESS_CONF, "Image for the delete metadata button"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLEditMetadataImage", SetEditMetadataImage,
        NULL, ACCESS_CONF, "Image for the edit metadata button"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLAddMetadataImage", SetAddMetadataImage,
        NULL, ACCESS_CONF, "Image for the add metadata button"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLDownloadMetadataImage", SetDownloadMetadataImage,
        NULL, ACCESS_CONF, "Image for the download metadata button"
    ),


    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLDownloadMetadataAsJSONImage", SetDownloadMetadataImageAsJSON,
        NULL, ACCESS_CONF, "Image for the download metadata as JSON button"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLDownloadMetadataAsCSVImage", SetDownloadMetadataImageAsCSV,
        NULL, ACCESS_CONF, "Image for the download metadata as CSV button"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLShowDownloadMetadataAsLinks", SetShowMetadataDownloadLinks,
        NULL, ACCESS_CONF, "Show the download metadata functionality as links rather than as a form "
    ),



    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLOkImage", SetOkImage,
        NULL, ACCESS_CONF, "Image for the OK button"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLCancelImage", SetCancelImage,
        NULL, ACCESS_CONF, "Image for the Cancel button"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLShowMetadataSearch", SetShowMetadataSearchForm,
        NULL, ACCESS_CONF, "Show the metadata search form"
    ),

    AP_INIT_ITERATE2 (DAVRODS_CONFIG_PREFIX "AddIcon", SetIconForSuffix, NULL, ACCESS_CONF, "an icon URL followed by one or more filenames"),

    AP_INIT_TAKE2 (DAVRODS_CONFIG_PREFIX "AddExposedRoot", SetExposedRootForSpecifiedUser, NULL, ACCESS_CONF, "a username followed by their default root path"),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLShowIds", SetShowIds,
        NULL, ACCESS_CONF, "Options for displaying irods ids"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "Login", SetLoginURL,
        NULL, ACCESS_CONF, "Address to use for login procedure"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "Logout", SetLogoutURL,
        NULL, ACCESS_CONF, "Address to use for logout procedure"
    ),

    AP_INIT_TAKE1(
        DAVRODS_CONFIG_PREFIX "HTMLUserImage", SetUserImage,
        NULL, ACCESS_CONF, "Image for the user icon"
    ),

		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "APIPath", SetAPIPath,
				NULL, ACCESS_CONF, "Set the location used for the Rest API. This is relative to the <Location> that davrods is hosted on."
		),

		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "DefaultUsername", SetDefaultUsername,
				NULL, ACCESS_CONF, "Set the default username to use if none is provided."
		),

		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "DefaultPassword", SetDefaultPassword,
				NULL, ACCESS_CONF, "Set the default password to use if none is provided."
		),

		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "NameHeading", SetNameHeading,
				NULL, ACCESS_CONF, "Set the heading for the Name column in directory listings"
		),

		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "SizeHeading", SetSizeHeading,
				NULL, ACCESS_CONF, "Set the heading for the Size column in directory listings"
		),

		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "OwnerHeading", SetOwnerHeading,
				NULL, ACCESS_CONF, "Set the heading for the Owner column in directory listings"
		),

		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "DateHeading", SetDateHeading,
				NULL, ACCESS_CONF, "Set the heading for the date column in directory listings"
		),

		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "PropertiesHeading", SetPropertiesHeading,
				NULL, ACCESS_CONF, "Set the heading for the Properties column in directory listings"
		),

		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "ZoneLabel", SetZoneLabel,
				NULL, ACCESS_CONF, "Set the value to display instead of the zone name"
		),


		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "PreListingsHtml", SetPreListingsHTML,
				NULL, ACCESS_CONF, "Set any html you you want before the directory listings"
		),


		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "PostListingsHtml", SetPostListingsHTML,
				NULL, ACCESS_CONF, "Set any html you you want after the directory listings"
		),

		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "ToolsPlacemnt", SetToolsPlacement,
				NULL, ACCESS_CONF, "Set where you want the user and search sections to appear"
		),

		AP_INIT_TAKE1(
				DAVRODS_CONFIG_PREFIX "PreCloseBodyHtml", SetPreCloseBodyHTML,
				NULL, ACCESS_CONF, "Set any html you you want between the closing body and html tags"
		),

		{ NULL }
};
