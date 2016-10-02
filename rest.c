/*
 * rest.c
 *
 *  Created on: 2 Oct 2016
 *      Author: billy
 */

#include "rest.h"
#include "config.h"

static const char * const API_ACTIONS_SS [] =
{
	"metadata",
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
  				 * Parse the  uri from req_p -> uri to get the API call
  				 */

  			}
  	}

  return res;
}
