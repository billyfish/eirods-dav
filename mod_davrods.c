/**
 * \file
 * \brief     Davrods main module file.
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

#define ALLOCATE_MODULE
#include "mod_davrods.h"
#include "config.h"
#include "auth.h"
#include "common.h"
#include "rest.h"
#include "http_request.h"

#include <curl/curl.h>


#ifdef API_NUMBER_H__
#undef API_NUMBER_H__
#include "irods/apiNumber.h"
#define API_NUMBER_H__
#endif

APLOG_USE_MODULE(davrods);



static void EIRodsDavChildInit (apr_pool_t *pool_p, server_rec *server_p);

static apr_status_t EIRodsDavChildFinalize (void *data_p);



static void register_davrods_hooks(apr_pool_t *p) {
    davrods_auth_register(p);
    davrods_dav_register(p);

    ap_hook_child_init (EIRodsDavChildInit, NULL, NULL, APR_HOOK_FIRST);
    ap_hook_fixups (EIRodsDavFixUps, NULL, NULL, APR_HOOK_FIRST);

    ap_hook_handler (EIRodsDavAPIHandler, NULL, NULL, APR_HOOK_FIRST);

#ifdef IRODS_4_3
    load_client_api_plugins ();
#endif
}

module AP_MODULE_DECLARE_DATA davrods_module = {
    STANDARD20_MODULE_STUFF,
    davrods_create_dir_config, // Directory config setup.
    davrods_merge_dir_config,  //    ..       ..   merge function.
    NULL,                      // Server config setup.
    NULL,                      //   ..     ..   merge function.
    davrods_directives,        // Command table.
		register_davrods_hooks,            // Hook setup.
};



static void EIRodsDavChildInit (apr_pool_t *pool_p, server_rec *server_p)
{
	CURLcode res = curl_global_init (CURL_GLOBAL_DEFAULT);

	if (res == CURLE_OK)
		{
			apr_pool_cleanup_register (pool_p, NULL, EIRodsDavChildFinalize, apr_pool_cleanup_null);
		}
	else
		{
			ap_log_perror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_EGENERAL, pool_p, "Failed to initialise CURL library");
		}
}


static apr_status_t EIRodsDavChildFinalize (void *data_p)
{
	curl_global_cleanup ();

	return APR_SUCCESS;
}


