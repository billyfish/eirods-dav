/*
** Copyright 2014-2018 The Earlham Institute
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*
 * frictionless_data_package.h
 *
 *  Created on: 12 Aug 2020
 *      Author: billy
 */

#ifndef FRICTIONLESS_DATA_PACKAGE_H_
#define FRICTIONLESS_DATA_PACKAGE_H_


#include "apr_pools.h"

#include "mod_dav.h"

#include "config.h"


#ifdef __cplusplus
extern "C"
{
#endif


dav_error *DeliverFDDataPackage (const dav_resource *resource_p, ap_filter_t *output_p);

int IsFDDataPackageRequest (const char *request_uri_s, const davrods_dir_conf_t *conf_p);

char *GetFDDataPackageRequestCollectionPath (const char *request_uri_s, apr_pool_t *pool_p);

bool DoesFDDataPackageExist (const dav_resource *resource_p);

const char *GetDataPackageFilename (void);







#ifdef __cplusplus
}
#endif

#endif /* FRICTIONLESS_DATA_PACKAGE_H_ */
