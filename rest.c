/*
 * rest.c
 *
 *  Created on: 2 Oct 2016
 *      Author: billy
 */

#include <stdlib.h>

#define ALLOC_REST_PATHS
#include "rest.h"

#include "apr_escape.h"
#include "apr_tables.h"
#include "apr_strings.h"
#include "util_script.h"
#include "http_protocol.h"


#include "config.h"
#include "meta.h"
#include "auth.h"
#include "common.h"

struct APICall;

typedef struct APICall
{
	const char *ac_action_s;
	int (*ac_callback_fn) (const struct APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s);
} APICall;


/*
 * STATIC DECLARATIONS
 */

static int SearchMetadata (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s);

static const char *GetParameterValue (apr_table_t *params_p, const char * const param_s, apr_pool_t *pool_p);


/*
 * STATIC VARIABLES
 */

static const APICall S_API_ACTIONS_P [] =
{
	{ REST_METADATA_PATH_S, SearchMetadata },
	{ NULL, NULL }
};



/*
 * API DEFINITIONS
 */

int DavrodsRestHandler (request_rec *req_p)
{
	int res = DECLINED;

  /* Normally we would check if this is a call for the davrods rest handler,
   * but dav-handler will have gotten there first. So check it against our path
   * and see if we are interested in it.
   * If it is, we accept it and do our things, it not, we simply return DECLINED,
   * and Apache will try somewhere else.
   */
	if ((req_p -> method_number == M_GET) || (req_p -> method_number == M_POST))
		{
			davrods_dir_conf_t *config_p = ap_get_module_config (req_p -> per_dir_config, &davrods_module);

			if (config_p -> davrods_api_path_s)
				{
					const size_t api_path_length = strlen (config_p -> davrods_api_path_s);

					if ((config_p -> davrods_api_path_s) && (strncmp (config_p -> davrods_api_path_s, req_p -> path_info, api_path_length) == 0))
						{
							/*
							 * Parse the uri from req_p -> path_info to get the API call
							 */
							const APICall *call_p = S_API_ACTIONS_P;
							apr_table_t *params_p = NULL;
							const char *path_s = (req_p -> path_info) + api_path_length;
							char *davrods_path_s = NULL;

							/* Get the Location path where davrods is hosted */
							const char *ptr = strstr (req_p -> uri, config_p -> davrods_api_path_s);

							if (ptr)
								{
									davrods_path_s = apr_pstrmemdup (req_p -> pool, req_p -> uri, ptr - (req_p -> uri));
								}

							ap_args_to_table (req_p, &params_p);

							while ((call_p != NULL) && (call_p -> ac_action_s != NULL))
								{
									size_t l = strlen (call_p -> ac_action_s);

									if (strncmp (path_s, call_p -> ac_action_s, l) == 0)
										{
											res = call_p -> ac_callback_fn (call_p, req_p, params_p, config_p, davrods_path_s);

											/* force exit from loop */
											call_p = NULL;
										}
									else
										{
											++ call_p;
										}

								}		/* while (call_p -> ac_action_s != NULL) */


							if (!apr_is_empty_table (params_p))
								{
									apr_table_clear (params_p);
								}

						}

				}
		}

  return res;
}





/*
 * STATIC DEFINITIONS
 */

static int SearchMetadata (const APICall *call_p, request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p, const char *davrods_path_s)
{
	int res = DECLINED;
	apr_pool_t *pool_p = req_p -> pool;
	const char * const key_s = GetParameterValue (params_p, "key", pool_p);

	if (key_s)
		{
			const char * const value_s = GetParameterValue (params_p, "value", pool_p);

			if (value_s)
				{
					SearchOperator op = SO_LIKE;
					const char *op_s = apr_table_get (params_p, "op");
					apr_pool_t *davrods_pool_p = GetDavrodsMemoryPool (req_p);

					if (op_s)
						{
							apr_status_t status = GetSearchOperatorFromString (op_s, &op);

							if (status != APR_SUCCESS)
								{
									ap_log_rerror (__FILE__, __LINE__, APLOG_MODULE_INDEX, APLOG_ERR, APR_BADARG, req_p, "error %d: Unknown operator \"%s\"", status, op_s);
								}
						}

					if (davrods_pool_p)
						{
							rcComm_t *rods_connection_p = GetIRODSConnectionFromPool (davrods_pool_p);

							if (!rods_connection_p)
								{
									rods_connection_p  = GetIRODSConnectionForPublicUser (req_p, davrods_pool_p, config_p);
								}

							if (rods_connection_p)
								{
									char *result_s = DoMetadataSearch (key_s, value_s, op, rods_connection_p -> clientUser.userName, pool_p, rods_connection_p, req_p -> connection -> bucket_alloc, config_p, req_p, davrods_path_s);

									if (result_s)
										{
											ap_rputs (result_s, req_p);
										}

									res = OK;
								}		/* if (rods_connection_p) */

						}		/* if (davrods_pool_p) */

				}

		}

	return res;
}


static const char *GetParameterValue (apr_table_t *params_p, const char * const param_s, apr_pool_t *pool_p)
{
	const char *value_s = NULL;
	const char * const raw_value_s = apr_table_get (params_p, param_s);
	const char *forbid_s = NULL;
	const char *reserved_s = NULL;

	if (raw_value_s)
		{
			value_s = apr_punescape_url (pool_p, raw_value_s, forbid_s, reserved_s, 1);
		}

	return value_s;
}
