/**
 * \file
 * \brief     Common includes, functions and variables.
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
#include "common.h"
#include "config.h"
#include "prop.h"
#include "propdb.h"
#include "repo.h"
#include "auth.h"

#ifdef DAVRODS_ENABLE_PROVIDER_LOCALLOCK
#include "lock_local.h"
#endif /* DAVRODS_ENABLE_PROVIDER_LOCALLOCK */

#include <stdlib.h>

#include <http_request.h>
#include <http_protocol.h>
#include <ap_provider.h>

#include "apr_buckets.h"

#include <irods/rodsClient.h>

APLOG_USE_MODULE(davrods);

// Common utility functions {{{

const char *get_rods_error_msg(int rods_error_code) {
    char *submsg = NULL;
    return rodsErrorName(rods_error_code, &submsg);
}

/**
 * \brief Extract the davrods pool from a request, as set by the rods_auth component.
 *
 * \param r an apache request record.
 *
 * \return the davrods memory pool
 */
apr_pool_t *get_davrods_pool_from_req (request_rec *req_p) {
    // TODO: Remove function, move apr call to the single caller.
    apr_pool_t *pool_p = NULL;
    apr_status_t got_pool_status = apr_pool_userdata_get ((void **) &pool_p, GetDavrodsMemoryPoolKey (), req_p -> connection -> pool);

    if (!pool_p)
    	{
        /*
         * For publicly-accessible iRODS instances, check_rods will never have been called, so we'll need
         * to get the memory pool and iRODS connection for the public user.
         */

    		// Get config.
        davrods_dir_conf_t *conf_p = ap_get_module_config (req_p -> per_dir_config, &davrods_module);

        if (conf_p)
        	{
        		if (conf_p -> davrods_public_username_s)
        			{
            		pool_p = GetDavrodsMemoryPool (req_p);

            		if (pool_p)
            			{
            				rcComm_t *connection_p = NULL;

            				authn_status status = GetIRodsConnection (req_p, &connection_p, conf_p -> davrods_public_username_s, conf_p -> davrods_public_password_s ? conf_p -> davrods_public_password_s : "");

            				if (status != 0)
            					{
      									ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_ECONNREFUSED, req_p, "error %d: Failed to connect to iRODS as public user \"%s\"", status, conf_p -> davrods_public_username_s);

            						WHISPER ("GetIRodsConnection failed for anonymous user");
            					}
            			}
        			}

        	}		/* if (conf_p) */
    	}


    return pool_p;
}

// }}}
// DAV provider definition and registration {{{

#ifdef DAVRODS_ENABLE_PROVIDER_NOLOCKS

// The no-locking provider limits the DAV protocol to version 1. This
// can cause a slight increase in performance, but may prevent certain
// clients from connecting in read/write mode (e.g. Apple OS X).
const dav_provider davrods_dav_provider_nolocks = {
    &davrods_hooks_repository,
    &davrods_hooks_propdb,
    NULL, // locks   - disabled.
    NULL, // vsn     - unimplemented.
    NULL, // binding - unimplemented.
    NULL, // search  - unimplemented.

    NULL  // context - not needed.
};

#endif /* DAVRODS_ENABLE_PROVIDER_NOLOCKS */

#ifdef DAVRODS_ENABLE_PROVIDER_LOCALLOCK

const dav_provider davrods_dav_provider_locallock = {
    &davrods_hooks_repository,
    &davrods_hooks_propdb,
    &davrods_hooks_locallock,
    NULL, // vsn     - unimplemented.
    NULL, // binding - unimplemented.
    NULL, // search  - unimplemented.

    NULL  // context - not needed.
};

#endif /* DAVRODS_ENABLE_PROVIDER_LOCALLOCK */

void davrods_dav_register(apr_pool_t *p) {

    // Register the namespace URIs.
    dav_register_liveprop_group(p, &davrods_liveprop_group);

    // Register the DAV providers.

#ifdef DAVRODS_ENABLE_PROVIDER_NOLOCKS
    dav_register_provider(p, DAVRODS_PROVIDER_NAME "-nolocks",   &davrods_dav_provider_nolocks);
#endif /* DAVRODS_ENABLE_PROVIDER_NOLOCKS */

#ifdef DAVRODS_ENABLE_PROVIDER_LOCALLOCK
    dav_register_provider(p, DAVRODS_PROVIDER_NAME "-locallock", &davrods_dav_provider_locallock);
#endif /* DAVRODS_ENABLE_PROVIDER_LOCALLOCK */

#if !defined(DAVRODS_ENABLE_PROVIDER_NOLOCKS)   \
 && !defined(DAVRODS_ENABLE_PROVIDER_LOCALLOCK)
    #error No DAV provider enabled. Please define one of the DAVRODS_ENABLE_PROVIDER_.* switches.
#endif
}


void CloseBucketsStream (apr_bucket_brigade *bucket_brigade_p)
{
	apr_bucket *bucket_p = apr_bucket_eos_create (bucket_brigade_p -> bucket_alloc);

	if (bucket_p)
		{
			APR_BRIGADE_INSERT_TAIL (bucket_brigade_p, bucket_p);
		}
}


apr_status_t PrintBasicStringToBucketBrigade (const char *value_s, apr_bucket_brigade *brigade_p, request_rec *req_p, const char *file_s, const int line)
{
	apr_status_t status = apr_brigade_puts (brigade_p, NULL, NULL, value_s);

	if (status != APR_SUCCESS)
		{
			ap_log_rerror (file_s, line, APLOG_MODULE_INDEX, APLOG_ERR, status, req_p, "Failed to print \"%s\"", value_s);
		}

	return status;
}
