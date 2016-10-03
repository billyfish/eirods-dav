/*
 * rest.c
 *
 *  Created on: 2 Oct 2016
 *      Author: billy
 */

#include <stdlib.h>

#include "rest.h"

#include "apr_tables.h"
#include "util_script.h"

#include "config.h"
#include "meta.h"
#include "auth.h"


typedef struct APICall
{
	const char *ac_action_s;
	int (*ac_callback_fn) (request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p);
} APICall;


/*
 * STATIC DECLARATIONS
 */

static int SearchMetadata (request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p);


/*
 * STATIC VARIABLES
 */

static const APICall S_API_ACTIONS_P [] =
{
	{ "metadata", SearchMetadata },
	{ NULL, NULL }
};



/*
 * API DEFINITIONS
 */

int DavrodsRestHandler (request_rec *req_p)
{
	int res = DECLINED;

  /* First off, we need to check if this is a call for the davrods rest handler.
   * If it is, we accept it and do our things, it not, we simply return DECLINED,
   * and Apache will try somewhere else.
   */
  if ((req_p -> handler) && (strcmp (req_p -> handler, "davrods-rest-handler") == 0))
  	{
  		if ((req_p -> method_number == M_GET) || (req_p -> method_number == M_POST))
  			{
  				davrods_dir_conf_t *config_p = ap_get_module_config (req_p -> per_dir_config, &davrods_module);
  				apr_table_t *params_p = NULL;

  				ap_args_to_table (req_p, &params_p);

  				/*
  				 * Parse the uri from req_p -> uri to get the API call
  				 */
  				const APICall *call_p = S_API_ACTIONS_P;

  				while (call_p -> ac_action_s != NULL)
  					{
  						size_t l = strlen (call_p -> ac_action_s);

  						if ((strncmp (req_p -> uri, call_p -> ac_action_s, l) == 0) && ((* (req_p -> uri) + l) == '/'))
  							{
  								res = call_p -> ac_callback_fn (req_p, params_p, config_p);

  								/* force exit from loop */
  								call_p = NULL;
  							}
  					}


  				if (!apr_is_empty_table (params_p))
  					{
  						apr_table_clear (params_p);
  					}
  			}
  	}

  return res;
}



/*
 * STATIC DEFINITIONS
 */

static int SearchMetadata (request_rec *req_p, apr_table_t *params_p, davrods_dir_conf_t *config_p)
{
	int res = DECLINED;
	const char * const key_s = apr_table_get (params_p, "key");

	if (key_s)
		{
			const char * const value_s = apr_table_get (params_p, "value");

			if (value_s)
				{
			    /* Get the iRods connection */
					rcComm_t *rods_connection_p = GetIRodsConnection (req_p);

					if (rods_connection_p)
						{
							DoMetadataSearch (key_s, value_s, req_p -> pool, rods_connection_p, req_p -> connection -> bucket_alloc, config_p);
						}
				}

		}

	return res;
}
