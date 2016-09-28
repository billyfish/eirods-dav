/*
 * meta.h
 *
 *  Created on: 27 Sep 2016
 *      Author: billy
 */

#ifndef META_H_
#define META_H_


#include "mod_dav.h"
#include "apr_pools.h"
#include "apr_tables.h"

#include "rodsGenQuery.h"

typedef struct IrodsMetadata
{
	char *im_key_s;
	char *im_value_s;
	char *im_units_s;
} IrodsMetadata;




#ifdef __cplusplus
extern "C"
{
#endif

apr_array_header_t *GetMetadataForDataObject (const dav_resource *resource_p, const objType_t object_type, const char * const id_s);


int printGenQI( genQueryInp_t *genQueryInp );


IrodsMetadata *AllocateIrodsMetadata (const char * const key_s, const char * const value_s, const char * const units_s, apr_pool_t *pool_p);


#ifdef __cplusplus
}
#endif


#endif /* META_H_ */
