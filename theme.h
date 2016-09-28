/*
 * theme.h
 *
 *  Created on: 28 Sep 2016
 *      Author: billy
 */

#ifndef THEME_H_
#define THEME_H_

#include "mod_dav.h"
#include "util_filter.h"



struct HtmlTheme
{
	const char *ht_head_s;

	const char *ht_top_s;

	const char *ht_bottom_s;

	const char *ht_collection_icon_s;

	const char *ht_object_icon_s;

	const char *ht_parent_icon_s;

	const char *ht_listing_class_s;

	int ht_show_metadata;
};



#ifdef __cplusplus
extern "C"
{
#endif


void InitHtmlTheme (struct HtmlTheme *theme_p);

dav_error *DeliverThemedDirectory (const dav_resource *resource_p, ap_filter_t *output_p);

#ifdef __cplusplus
}
#endif

#endif /* THEME_H_ */
