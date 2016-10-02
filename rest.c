/*
 * rest.c
 *
 *  Created on: 2 Oct 2016
 *      Author: billy
 */

#include <stdlib.h>

#include "rest.h"
#include "config.h"




typedef struct APICall
{
	const char * const ac_action_s;
	int (*ac_callback_fn) (request_rec *req_p, const char *relative_uri_s)
} APICall;


static int SearchMetadata (request_rec *req_p)

static APICall S_API_ACTIONS_P [] =
{
		{ "metadata", SearchMetadata },
		NULL
};

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

  				/*
  				 * Parse the uri from req_p -> uri to get the API call
  				 */
  				APICall *call_p = S_API_ACTIONS_P;

  				while (call_p != NULL)
  					{
  						size_t l = strlen (call_p -> ac_action_s);

  						if ((strncmp (req_p -> uri, call_p -> ac_action_s, l) == 0) && ((* (req_p -> uri) + l) == '/'))
  							{
  								res = call_p -> ac_callback_fn (req_p, (reg_p -> uri) + (l + 1));

  								/* force exit from loop */
  								call_p = NULL;
  							}
  					}

  			}
  	}

  return res;
}



static int SearchMetadata (request_rec *req_p, const char *relative_uri_s)
{
	int res = DECLINED;



	return res;
}
