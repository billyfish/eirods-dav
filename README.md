# Eirods-dav - An Apache WebDAV and metadata REST API interface to iRODS


Eirods-dav provides access to iRODS servers using the WebDAV protocol and exposes a REST API for accessing and manipulating metadata from within a web browser. It takes the original [Davrods](https://github.com/UtrechtUniversity/davrods "") module written by Ton Smeele and Chris Smeele, which is a bridge between the WebDAV protocol and the iRODS API. Davrods leverages the Apache server implementation of the WebDAV
protocol, *mod_dav*, for compliance with the WebDAV Class 2 standard. 

Eirods-dav adds extra features such as themeable listings and anonymous access for public-facing websites. As well as these features, it also incorporates a REST API for interacting with the metadata stored within an iRODS system. All of these features are implemented as an Apache HTTPD module.


Some of the features that have been added within Eirods-dav are:
 
- Themeable listings similar to using mod_autoindex.
- Expose and navigate by metadata key-value pairs.
- Search the metadata catalogue.
- REST API for accessing and manipulating the iRODS data and metadata 
- Client-side interface to add, edit and delete metadata entries.
- Download all of the metadata for an iRODS entry in various formats.
- Full location breadcrumbs.
- Each user able to have their own custom default root path exposed.
- Users able to log in as well as having a default public user.


As well as features from the original Davrods such as:

- Supports WebDAV Class 2. Locks are local to the Apache server.
- Supports PAM and Native (a.k.a. STANDARD) iRODS authentication.
- Supports SSL encryption for the entire iRODS connection.
- Easy to configure using Apache configuration directives.
- Supports iRODS server versions 4+ and is backwards compatible with 3.3.1.


A working live site of this module showing the themeable listings along with metadata 
searching and linking is available at the [Designing Future Wheat Data Portal](http://opendata.earlham.ac.uk/wheat).

## Changelog

### 1.8 (10 Nov 2020)
 - For Frictionless Data Packages, any CSV or TSV files are now specified as tabular data resources. For information on how to configure retreiving the required values
from the iRODS repository, the details are [here](#frictionless-data-tabular-support). This initial support is for the general column types and support for the format 
and other fields defined by the [Frictionless Data Table Schema](https://specs.frictionlessdata.io/table-schema/) will follow in subsequent upgrades.

### 1.7 (15 Sep 2020)
 - Added support for [Frictionless Data](https://frictionlessdata.io) Data Packages. The details are [here](#frictionless-data-support).

### 1.6 (15 Jul 2020)
 - Added ability to show the data object checksums in the listings table. See the documentation on the 
**DavRodsShowChecksum** and **DavRodsChecksumHeading** directives for more information. If you are building with an existing *user.prefs* file, you will need to add the **DIR_BOOST** and **DIR_JANSSON** directives to this. See the included *example-user.prefs* file for more details.
  
### 1.5.1 (22 Oct 2018)
    
 - Added workaround for the fact that mod_dav.so assumes that it is the handler for the REST API calls (https://svn.apache.org/viewvc/httpd/httpd/branches/2.4.x/modules/dav/main/mod_dav.c?revision=1807753&view=markup#l4863)
 - Fixed response content-types for the views/... calls
 - Object type now included when using the REST API calls that get id information for given paths
 - The REST handler now makes sure that the request is really for it before getting the request parameters so it can be used with other modules that are also reading these.

### 1.5 (1 Aug 2018)
 
- REST API now accepts POST requests.
- Added REST API for generating virtual directory listings.
- Added Views API for performing searches and generating virtual directory listings
directly within the web page.
- Added extra places within the web page where custom HTML sections can be added.
- Added ability to get HTML chunks from any http(s) source such as another web server.
- Added dynamic capabilities to the URL used for getting the HTML chunks using the 
iRODS ids and metadata values.
- Added ability to customise the headings in the listings table and remove columns.


## Installation

### Prerequisites

EIrods-dav requires the following packages to be installed on your server:

- Apache httpd 2.4+
- iRODS 4.x client libraries and headers (in package `irods-runtime` and `irods-dev`, available
  from [the iRODS website](http://irods.org/download/))

Due to the way iRODS libraries are packaged, specifically, its network
plugins, one of the following packages must also be installed:

- `irods-icommands` *OR* `irods-icat` *OR* `irods-resource`.

These three packages all provide the necessary libraries in
`/var/lib/irods/plugins/network`.

### Using the binary distribution

For binary installation, download the package for your platform at
https://github.com/billyfish/eirods-dav/releases and copy the module to the 
`modules` directory of your Apache httpd installation. If the binary version 
is not available, then you can compile the module from source.


### Compiling 

To compile Eirods-dav from source, copy `example-user.prefs` to `user.prefs` and edit 
it to contain values valid for your system. The file is commented and should be able to 
to be set up with a minimum of effort.

Once this is complete, then ```make``` followed by ```make install``` will create 
and install `mod_eirods-dav.so` to your Apache httpd installation.

See the [configuration](#configuration) section for instructions on how to configure
Eirods-dav once it has been installed.

### Eirods-dav and SELinux

If the machine on which you install Eirods-dav  is protected by SELinux,
you may need to make changes to your policies to allow Eirods-dav  to run:

- Apache HTTPD must be allowed to connect to TCP port 1247
- Eirods-dav  must be allowed to dynamically load iRODS client plugin
  libraries in /var/lib/irods/plugins/network

For example, the following two commands can be used to resolve these
requirements:

    setsebool -P httpd_can_network_connect true
    chcon -t lib_t /var/lib/irods/plugins/network/lib*.so

## Configuration

Eirods-dav is configured in two locations: an HTTPD configuration
file, which is the main configuration file and in an iRODS environment 
file used for iRODS client library configuration, similar to the 
configuration of icommands.


### HTTPD vhost configuration

The Eirods-dav RPM distribution installs a commented out vhost template
in `davrods-vhost.conf`. With the comment marks
(`#`) removed, this provides you with a sane default configuration
that you can tune to your needs. The Eirods-dav configuration
options are documented in this file and can be changed to your liking.
As well as that documentation, the new features are also described
below.


### Configuration directives


#### User-specific and public access

If you wish to run Eirods-dav without the need for authentication, it 
can be set up to run as a normal public-facing website. This is done by
specifying a user name and password for the iRODS user whose data you 
wish to display. The directives for these are `DavRodsDefaultUsername`
and `DavRodsDefaultPassword`.

For example, to display the data for a user with the 
user name *anonymous* and password *foobar* the configuration directives
would be:

```
DavRodsDefaultUsername anonymous
DavRodsDefaultPassword foobar
```


* **DavRodsAddExposedRoot**:
This directive allows you to specify the default exposed roots on a per-user
basis. When a logged-in user hasn't been added using this directive, the 
**DavRodsExposedRoot** will be used instead.

 ```
 DavRodsAddExposedRoot lars /tempZone/home/lars/private
 DavRodsAddExposedRoot james /tempZone/home/admin
 ```
 


#### Themed Listings

By default, the html listings generated by mod_davrods do not use any 
styling. It is possible to style the listings much like [mod_autoindex](https://httpd.apache.org/docs/2.4/mod/mod_autoindex.html). There are
various directives that can be used.


* **DavRodsThemedListings**:
This directive is a global on/off switch which toggles the usage of 
themed listings. Unless this is set to *true* then none of the following
directives in this section will work. To turn it on, use the following 
directive:

 ```
 DavrodsThemedListings true
 ```

#### Configuring the HTML sections

There are various points in the web pages generated by Eirods-dav where custom
HTML can be inserted. Each of these configuration directives take three 
different types of different data:

* **text**: Treat the configuration value as raw HTML that will get used. This can either be 
a single-line directive or if you want to define this across multiple lines of text 
you will need a backslash (\\) at the end of each line. An example is
 ```
 DavRodsHTMLHead <link rel="stylesheet" type="text/css" \ 
 media="all" href="/eirods_dav_files/styles/styles.css">
 ```

* **file**: A file whose content will be used for the required HTML
chunk by beginning your value with "file:", *e.g.* to use the content of a file
at */opt/apache/eirods_dav_head.html*:

 ```
 DavRodsHTMLTop file:/opt/apache/eirods_dav_head.html
 ```
If a file is used, then it will be re-read for each incoming request. So any 
changes you make to the file won't need a restart of Apache to be made live.

* **http(s)**: A web page that is available via an http or https 
request. Eirods-dav will download all of the html and use this as the data
that will be inserted into its listing page. The configuration value needs 
to begin with the appropriate protocol, *e.g.* http or https. For example 
to use the content of a web page at *https://grassroots.tools/eirods_dav_bottom.html*

 ```
 DavRodsHTMLBottom https://grassroots.tools/eirods_dav_bottom.html
 ```

So each of these examples  all point to static resources in that the content is 
always the same regardless of the listing that Eirods-dav is currently displaying. 
One of the new features of Eirods-dav is that it can now use its internal variables 
to use dynamic resources. It currently has two types of internal variables 

* **id**: This is the iRODS id for the collection that Eriods-dav is currently 
displaying.
* **metadata**: This is used to get a given value from the metadata for the current
collection. For example if the collection had metadata key called *method*, then the 
value for this would be referred to by *metadata:method*. 

Each variable is denoted within a configuration directive by placing it in inside 
a @{ } block. So an example directive might be so to set URL parameters of *project* to 
the metadata value for the current collection's metadata key of *project_id* and setting
a parameter of *irods_id* to the current collection's iRODS id would be: 

 ```
 DavRodsHTMLTop https://test.server?project=@{metadata:project_id}&irods_id=@{id}
 ```

and, for example, if the id of the current collection was *2.123* and it had a 
metadata key-value pair of *project_id* = *mariner1*, then Eirods-dav would convert
this web address into  

https://test.server?project=mariner1&irods_id=2.123

Each of the configuration directives for these options to insert HTML into the web page 
are listed below:

* **DavRodsHTMLHead**:
This is an HTML text that will be placed within the *\<head\>* directive
of the HTML pages generated by Eirods-dav. More information on the types of
values that this can take is described in the [themed listings](#themed-listings)
section.

* **DavRodsHTMLTop**:
You can specify a chunk of HTML to appear above each listing using this 
directive.  More information on the types of
values that this can take is described in the [themed listings](#themed-listings)
section.

* **DavRodsPreListingsHtml**:
You can specify a chunk of HTML to appear directly prior to the directory listing 
using this directive.  More information on the types of
values that this can take is described in the [themed listings](#themed-listings)
section.

* **DavRodsPostListingsHtml**:
You can specify a chunk of HTML to appear directly after to the directory listing 
using this directive.  More information on the types of
values that this can take is described in the [themed listings](#themed-listings)
section.


* **DavRodsHTMLBottom**:
You can specify a chunk of HTML to appear below each listing using this 
directive.  More information on the types of
values that this can take is described in the [themed listings](#themed-listings)
section.


* **DavRodsPreCloseBodyHtml**:
You can specify a chunk of HTML to appear just before the closing \</body\> tag 
using this directive.  More information on the types of values that this can 
take is described in the [themed listings](#themed-listings) section.


* **DavRodsZoneLabel**:
This directive allows you specify an alternative value instead of the iRODS
zone name. For example to set this to be *public data*, then the directive 
would be: 

 ```
 DavRodsZoneLabel public data
 ```
 

### Configuring the directory listings

The directory listings can be customised using a variety of configuration 
directives.

* **DavRodsHTMLListingClass**:
The list of collections and data objects displayed by Eirods-dav are within 
an HTML table. If you wish to specify CSS classes for this table, you can 
use this directive. For instance, if you wish to use the CSS classes 
called *table* and *table-striped*, then you could use the following 
directive:

 ```
 DavRodsHTMLListingClass table table-striped
 ```

The listings table defaults to displaying five columns: 

 * Name
 * Owner
 * Size
 * Date
 * Properties


Each of these headings can be changed to one of your choice using the 
following configuration directives. If you would like a column to 
be hidden then set the value of any of these directives to **!**.
For example

 ```
 DavRodsOwnerHeading !
 ```

would hide the Owner column.


* **DavRodsNameHeading**: If you want to change the column heading for the *Name* column 
then you can use this directive. For instance to change it to *Object*

 ```
 DavRodsNameHeading Object
 ```

* **DavRodsSizeHeading**: If you want to change the column heading for the *Size* column 
then you can use this directive. For instance to change it to *File size*

 ```
 DavRodsSizeHeading File size
 ```

* **DavRodsOwnerHeading**: If you want to change the column heading for the *Owner* column 
then you can use this directive. For instance to hide it

 ```
 DavRodsOwnerHeading !
 ```

* **DavRodsDateHeading**: If you want to change the column heading for the *Date* column 
then you can use this directive. For instance to change it to *Last modified*

 ```
 DavRodsDateHeading Last modified
 ```

* **DavRodsPropertiesHeading**: If you want to change the column heading for the *Properties* column 
then you can use this directive. For instance to change it to *Metadata*

 ```
 DavRodsPropertiesHeading Metadata
 ```

* **DavRodsShowChecksum**: If you wish to add a column for displaying the file checksums, set this 
directive to true. By default it is *false* and checksums will not be displayed.

 ```
 DavRodsShowChecksum true
 ```
 
* **DavRodsChecksumHeading**: If you want to change the column heading for the *Checksum* column 
then you can use this directive. For instance to change it to *MD5*

 ```
 DavRodsChecksumHeading MD5
 ```

* **DavRodsHTMLCollectionIcon**:
If you wish to use a custom image to denote collections, you can use this
directive. This can be superseded by a matching call to the `DavRodsAddIcon`
directive. For instance, to use */eirods_dav_files/images/drawer* for all
collections, the directive  would be:

 ```
 DavRodsHTMLCollectionIcon /eirods_dav_files/images/drawer
 ```
 
* **DavRodsHTMLObjectIcon**:
If you wish to use a custom image to denote data objects, you can use 
this directive. This can be superseded by a matching call to the 
`DavRodsAddIcon` directive. For instance, to use 
*/eirods_dav_files/images/file* for all data objects, the directive would be:

 ```
 DavRodsHTMLObjectIcon /eirods_dav_files/images/file
 ```

* **DavRodsAddIcon**:
This directive allows you to specify icons that will be displayed in the 
listing for matching collections and data objects. This directive takes 
two or more arguments the first of which is the path to the image file 
to use. The remaining arguments are the file suffices that will be 
matched and use the given icon. For example, to use an archive image for
various compressed files and a picture image for image files, the 
directives would be: 

 ```
 DavRodsAddIcon /eirods_dav_files/images/archive .zip .tgz 
 DavRodsAddIcon /eirods_dav_files/images/image .jpeg .jpg .png
 ```


#### Configuring the metadata listing 

Each data object and collection can also display its metadata AVUs 
and have these as clickable links to allow a user to browse all 
data objects and collections with matching metadata too. 


* **DavRodsHTMLMetadata**:
This directive is toggles the usage of metadata within the listings. 
It can take one of the following values:

	* **full**: All of the metadata for each entry will get sent in the
HTML page for each request.
 * **on_demand**: None of the metadata is initially included with the 
HTML pages sent by Eirods-dav. Instead they can be accessed via AJAX requests
from these pages.
  * **none**: No metadata information will be made available.
 So to set the metadata to be available on demand, the directive would be:

 ```
 DavRodsHTMLMetadata on_demand
 ```

* **DavRodsHTMLMetadataEditable**:
This directive specifies whether the client-side functionality for editing
the metadata by accessing REST API calls is active or not. By default, it is
off and can be turned on by setting this directive to true.

 ```
 DavRodsHTMLMetadataEditable true
 ```


* **DavRodsHTMLAddMetadataImage**:
If ```DavRodsHTMLMetadataEditable``` is set to true, then you can use this
directive to specify the image used for the button to add metadata to a 
particular data object or collection.

 ```
 DavRodsHTMLAddMetadataImage /eirods_dav_files/images/list_add
 ```

* **DavRodsHTMLDeleteMetadataImage**:
If ```DavRodsHTMLMetadataEditable``` is set to true, then you can use this
directive to specify the image used for the button to delete metadata from a 
particular data object or collection.

 ```
 DavRodsHTMLDeleteMetadataImage /eirods_dav_files/images/list_delete
 ```

* **DavRodsHTMLEditMetadataImage**:
If ```DavRodsHTMLMetadataEditable``` is set to true, then you can use this
directive to specify the image used for the button to edit metadata for a 
particular data object or collection.

 ```
 DavRodsHTMLEditMetadataImage /eirods_dav_files/images/list_edit
 ```

* **DavRodsHTMLDownloadMetadataImage**:
You can use this directive to specify the image used for the button to 
download all of the metadata for a particular data object or collection.

 ```
DavRodsHTMLDownloadMetadataImage /eirods_dav_files/images/list_download
 ```


* **DavRodsHTMLOkImage**:
If ```DavRodsHTMLMetadataEditable``` is set to true, then you can use this
directive to specify the image used for the "OK" button of the metadata 
editor.

 ```
 DavRodsHTMLOkImage /eirods_dav_files/images/list_ok
 ```

* **DavRodsHTMLCancelImage**:
If ```DavRodsHTMLMetadataEditable``` is set to true, then you can use this
directive to specify the image used for the "Cancel" button of the metadata 
editor.

 ```
 DavRodsHTMLCancelImage /eirods_dav_files/images/list_cancel
 ```



#### REST API

Eirods-dav has a REST API for accessing and manipulating the iRODS metadata catalog. 
You can specify the reserved address that Eirods-dav will use to have its REST API
available on.

* **DavRodsAPIPath**:
This directive specifies the path used within Eirods-dav to link to the
various REST API functionality such as the metadata search. To specify 
it as */api/*, which is a good default, 
use the following directive:

 ```
 DavRodsAPIPath /api/

 ```

Then the various REST API endpoints would all begin with */api/* *e.g.* 
*/api/metadata/search* for the search function.

Currently the REST API has the following functions:


##### Metadata API

 * **metadata/get**: This is for getting all of the associated metadata for an iRODS item. It takes two parameter, the first is *id*, which is the iRODS id of the data object or collection that you wish to get the metadata pairs for. The second parameter is *output_format* which specifies the format that the metadata will be returned in. It currently can take one of the following values:

	- **json**: This will return the metadata as a [JSON (JavaScript Object Notation)](http://www.json.org/) array with each entry in the array having *attribute*, *value*, and where appropriate, *units* keys for its key-value pairs.
	- **csv**: This will return the metadata as a table of comma-separated values with the order of the columns being attribute, value, units. Each of these entries will be contained within double quotes to allow for commas within their values without causing errors. 
	- **tsv**: This will return the metadata as a table of tab-separated values with the order of the columns being attribute, value, units. Each of these entries will be contained within double quotes to allow for commas within their values without causing errors. 
 
 For example to get the metadata for a data object with the id of 1.10021 in a JSON output format, the URL to call would be  

 `/eirods-dav/api/metadata/get?id=1.10021&output_format=json`
 
 * **metadata/search**:  This API call is for getting a list of all data objects and collections that have a given metadata attribute-value pair. It takes two parameters: *key*, which is the attribute to search for and, *value*, which specifies the metadata value. There is a third optional parameter, *units* for specifying the units that the metadata attribute-value pair must also have. So to search for all of the data objects and collections that have an attribute called *volume* with a value of *11*,  the URL to call would be  

 `/eirods-dav/api/metadata/search?key=volume&value=11`
 
 * **metadata/edit**: This API call is for editing a metadata attribute-value pair for a data object of collection and replacing one or more of its attribute, value or units. It takes the following required parameters: *id*, which is the iRODS id of the data object or collection to delete the metadata from, *key*, which is the attribute to edit, *value*, which specifies the metadata value to edit. Again, there is an optional parameter, *units* for specifying the units that the metadata attribute-value pair must also have to match. There must also be one or more of the following parameters to specify how the metadata will be altered: *new_key*, which is for specifying the new name for the attribute, *new_value*, for specifying the new metadata value and *new_units* for specifying the units that the metadata attribute-value pair will now have. So to edit an attribute called *volume* with a value of *11* and units of *decibels* for a data object with the id of 1.10021 and give it a new value of 8 and units of litres, the URL to call would be  

 `/eirods-dav/api/metadata/edit?id=1.10021&key=volume&value=11&units=decibels&new_value=8&new_units=litres`
 
 * **metadata/add**: This API call is for adding a metadata attribute-value pair to a data object of collection. It takes three parameters: *id*, which is the iRODS id of the data object or collection to add the metadata to, *key*, which is the attribute to add  and, *value*, which specifies the metadata value to be added. As with the *search* call listed above, there is a fourth optional parameter, *units* for specifying the units that the metadata attribute-value pair will have. So to add an attribute called *volume* with a value of *11* to a data object with the id of 1.10021, the URL to call would be  

 `/eirods-dav/api/metadata/add?id=1.10021&key=volume&value=11`
 
 * **metadata/delete**: This API call is for deleting a metadata attribute-value pair from a data object of collection. It takes three parameters: *id*, which is the iRODS id of the data object or collection to delete the metadata from, *key*, which is the attribute to delete for and, *value*, which specifies the metadata value to delete. As before, there is a third optional parameter, *units* for specifying the units that the metadata attribute-value pair must also have to match. So to delete an attribute called *volume* with a value of *11* and units of *decibels* from a data object with the id of 1.10021, the URL to call would be 

  `/eirods-dav/api/metadata/delete?id=1.10021&key=volume&value=11&units=decibels`

 * **metadata/keys**: This API call is for getting a list of the metadata keys that have a partial match with a given key parameter denoted by *key*. For example, to get a list of all keys containing the phrase *proj*, the URL to call
would be

  `/eirods-dav/api/metadata/keys?key=proj`

 * **metadata/values**: This API call is for getting a list of the metadata key-value pairs that have a partial match for the value given by the parameter *value* for a given key parameter denoted by *key*. For example, to get a list of all key-value pairs for a key called *name* and a value containing the phrase *ob* so that it would match *R****ob****ert*, *B****ob***, *etc.*, the URL to call
would be     

  `/eirods-dav/api/metadata/values?key=name&value=ob`


##### General API

 * **general/info**: 
This API call is for getting information such as id, path, file size, *etc.* on a given iRODS data object or collection. The parameter, *path*, specifies the object or collection to get the details for. For example, to get the information for the item at */test/test.txt*, the URL to call
would be          

  `/eirods-dav/api/general/info?path=/test/test.txt`

 * **general/list**: This API call is for getting information such as id, path, file size, *etc.* for a list of given iRODS data object or collection ids. The parameter, *ids*, specifies a space- or comma-separated list of ids. For example to get the information for the ids 1.123 and 2.234, the URL to call
would be              

  `/eirods-dav/api/general/list?ids=1.123%202.234`

#### Views

As the REST API returns its results in JSON and other delimited formats, it's also useful to display the information 
from these API functions in the web page so Eirods-dav has ***views*** for some of these functions.  

 **DavRodsViewsPath**: 
This directive specifies the path used within Eirods-dav to link to the
various HTML views that use the REST API functionality such as the metadata search 
functionality. To specify it as */views/*, which is a good default, 
use the following directive:

 ```
 DavRodsViewsPath /views/
 ```

Then the various views endpoints would all begin with */views/* *e.g.* 
*/views/search* for the search view.

Currently there are the following views:

 * **search**: This generates a web page based upon the results of the REST metadata search function. It takes two parameters: *key*, which is the attribute to search for and, *value*, which specifies the metadata value. There is a third optional parameter, *units* for specifying the units that the metadata attribute-value pair must also have. So to search for all of the data objects and collections that have an attribute called *volume* with a value of *11*,  the URL to call would be  

 `/eirods-dav/views/search?key=volume&value=11`
 
 * **list**: This generates a web page based upon the results of the REST *list* function described in the [general](#general-api) section of the REST API documentation. It displays a virtual directory listing for a list of given iRODS data object or collection ids. The parameter, *ids*, specifies a space- or comma-separated list of ids. For example to get the information for the ids 1.123 and 2.234, the URL to call
would be              

  `/eirods-dav/views/list?ids=1.123%202.234`


### Frictionless Data support

[Frictionless Data](https://frictionlessdata.io) is an open source toolkit and set of standards to make data integration as simple as possible. The standards cover datasets, files and tabular data and over time the goal is to cover all of these. For now though, we are starting with datasets which are covered by the [Data Package](https://specs.frictionlessdata.io/data-package/) format. So for each configured iRODS collection, a virtual Data Package will be created and appear in the listings as a normal file 
available for download.

To populate the various parts of the data package specification for a given collection, 
eirods-dav will query the collection's iRODS metadata. This is for values such as the 
license, authors, title, description, *etc.* By default, the metadata keys used are:

 | Data Package field | Default iRODS metadata key |
 |--------------------|----------------------------|
 | license\_name      | license                    |
 | license\_url       | license_url                |
 | description        | description                |
 | name               | name                       |
 | authors            | authors                    |
 | title              | title                      |
 | id                 | id                         |
 

#### Configuring the Frictionless Data functionality

 * **DavRodsFrictionlessData**: 
If ```DavRodsFrictionlessData``` is set to true, then the Frictionless Data support is enabled.


 * **DavRodsFDResourceNameKey**: This is the key to use when querying iMeta catalog for the value to use for the *name* field in the data package. If thie configuration variable is not set, then the default key name of *name* will be used to search in iMeta. For example, to get the value associated with the metadata key *project\_name*, you would need the following configuration setting:
 
 ```
 DavRodsFDResourceLicenseUrlKey project_name
 ```

 * **DavRodsFDResourceLicenseNameKey**: This is the key to use when querying iMeta catalog for the value to use for the *license* field in the data package. If thie configuration variable is not set, then the default key name of *license* will be used to search in iMeta. For example, to get the value associated with the metadata key *project\_license\_name*, you would need the following configuration setting:

 
 ```
 DavRodsFDResourceLicenseUrlKey project_license_name
 ```

 * **DavRodsFDResourceLicenseUrlKey**: If this is set, then this key will be queried in the iMeta catalog for this collection. If it is not set, then the default key name of *license_url* will be used. For example, to get the value associated with the metadata key *project\_license\_web*, you would need the following configuration setting:
 
 ```
 DavRodsFDResourceLicenseUrlKey project_license_web
 ```

 * **DavRodsFDResourceTitleKey**: This is the key to use when querying iMeta catalog for the value to use for the *title* field in the data package. If thie configuration variable is not set, then the default key name of *title* will be used to search in iMeta. For example, to get the value associated with the metadata key *project\_title*, you would need the following configuration setting:

 
 ```
 DavRodsFDResourceDescriptionKey project_title
 ```

 * **DavRodsFDResourceAuthorsKey**: If this is set, then this key will be queried in the iMeta catalog for this collection. If it is not set, then the default key name of *authors* will be used. For example, to get the value associated with the metadata key *project\_authors*, you would need the following configuration setting:
 
 ```
 DavRodsFDResourceDescriptionKey project_authors
 ```

 * **DavRodsFDResourceDescriptionKey**: This is the key to use when querying iMeta catalog for the value to use for the *description* field in the data package. If thie configuration variable is not set, then the default key name of *description* will be used to search in iMeta. For example, to get the value associated with the metadata key *project\_description*, you would need the following configuration setting:

 
 ```
 DavRodsFDResourceDescriptionKey project_description
 ```

 * **DavRodsFDDataPackageImage**: You can use this directive to specify the image used for the data package.

 ```
DavRodsFDDataPackageImage /eirods_dav_files/images/archive
 ```

 * **DavRodsFDSaveDataPackages**: By default, the *datapackage.json* files are virtual and generated on the fly. Although this may be fine for smaller datasets, you may find that the time that is taken to generate these files is too long. So this configuration directive can be used to store the *datapackage.json* file within the relevant collection. This is equivalent to running the iRODS command *iput*. So you must make sure that the iRODS user configured by *DavRodsDefaultUsername* or the currently logged in user has the permissions to be able to write files to the given collection. By default, this directive is false. To enable it, set it to true.


 ```
DavRodsSetSaveFDDataPackages true
 ```

#### Combining multiple keys

These keys can be concatenated so that multiple metadata values can be combined where necessary as a comma-separated string. For instance, if the value that you wish to use for the description is the combination of *short\_info* and *detailed\_info* metadata keys, then the configuration would be.

 ```
DavRodsFDResourceDescriptionKey short_info,detailed_info
 ```

As well as specifying the keys, white space or full stops can be specified in the configuration value too. With the example above, if you would like to have a full stop and two blank lines the *short\_info* and a space and the *footnote* metadata value after the *detailed\_info*, the configuration would be

 ```
DavRodsFDResourceDescriptionKey short_info,.,\n,\n,detailed_info, ,footnote
 ```

#### Frictionless Data tabular support

Any csv or tsv files in a Frictionless Data package can now be configured to display their tabular-sepcific data fields within a *datapackage.json* file. This is done by querying the iMeta catalog for the given data object.
The first required key is *column_headings* which has a comma-separated list of the column headings for the tabular file. For each of these headings an additional key-value pair specify the type of data in the given column 
of the file. The keys for these are the column name with a *_type* suffix and the values being ones of the types defined [here](#https://specs.frictionlessdata.io/table-schema/#types-and-formats). 

So, using [this example](https://specs.frictionlessdata.io/tabular-data-package/#example) file *data.csv* which has there columns containing a string, an integer and a floating point number respectively.

 ```
var1,var2,var3
A,1,2.1
B,3,4.5
 ```
the required imeta fields would be 

 * *column_headings*: var1,var2,var3
 * *var1_type*: string
 * *var2_type*: integer
 * *var3_type*: number

E.g.

```
billy@desktop:~/$ imeta ls -d datasets/tabular/data.csv

AVUs defined for dataObj /grassrootsZone/home/rods/datasets/tabular/data.csv:
attribute: column_headings
value: var1,var2,var3
units: 
----
attribute: var1_type
value: string
units: 
----
attribute: var2_type
value: integer
units: 
----
attribute: var3_type
value: number
units: 
```
 

#### Apache configuration example

The Frictionless Data support can be configured to only be active at particular points in the directory hierarchy. For example, to have the data packages appear in all of the child directories of */data*, the configuration might be

 ```
 # Generate Data Packages for all child directories directly below /data
 <LocationMatch "/data/[^\/]+/">
	DavRodsFrictionlessData true

	DavRodsFDResourceTitleKey projectName
	DavRodsFDResourceIdKey uuid
	DavRodsFDResourceDescriptionKey projectName,.,\n,\n,description

	DavRodsFDDataPackageImage /images/archive
 </LocationMatch>

 # Since Data Packages are generated for the child directories configured in
 # the line above, exclude all directories further down
 <LocationMatch "/data/[^\/]+/[^\/]+/">
	 DavRodsFrictionlessData false
 </LocationMatch>
```



### The iRODS environment file ###

The binary distribution installs the `irods_environment.json` file in
`/etc/httpd/irods`. In most iRODS setups, this file can be used as
is.

Importantly, the first seven options (from `irods_host` up to and
including `irods_zone_name`) are **not** read from this file. These
settings are taken from their equivalent Eirods-dav configuration
directives in the vhost file instead.

The options in the provided environment file starting from
`irods_client_server_negotiation` *do* affect the behaviour of
Eirods-dav. See the [official documentation](https://docs.irods.org/master/system_overview/configuration/#irodsirods_environmentjson) for help on these settings.

For instance, if you want Eirods-dav to connect to iRODS 3.3.1, the
`irods_client_server_negotiation` option must be set to `"none"`.


## Building from source ##

To build from source, the following build-time dependencies must be
installed (package names may differ on your platform):

- `httpd-devel >= 2.4`
- `apr-devel`
- `apr-util-devel`
- `irods-dev`

Additionally, the following runtime dependencies must be installed:

- `httpd >= 2.4` which can either be installed using the platform package provider 
or downloaded and built from the [httpd](http://httpd.apache.org/) website.
- `irods-runtime >= 4.1.8`
- `jansson`
- `boost`
- `boost-system`
- `boost-filesystem`
- `boost-regex`
- `boost-thread`
- `boost-chrono`

First, browse to the directory where you have unpacked the Eirods-dav
source distribution.

Running `make` without parameters will generate the Davrods module .so
file in the `.libs` directory. `make install` will install the module
into Apache's modules directory.

If you are using the httpd provided by your platform, then copy the `davrods.conf` 
file into `/etc/httpd/conf.modules.d/. If you are using an httpd installed 
from a source package, then copy `davrods-vhost.conf` to the `conf/extra` folder 
inside httpd and enable it by adding 

```
include conf/extra/davrods-vhost.conf
```

to *conf/httpd.conf*.

Note: Non-Redhat platforms may have a different convention for the
location of the above file and the method for enabling/disabling
modules, consult the respective documentation for details.

Create an `irods` folder in a location where Apache HTTPD has read
access (e.g. `/etc/httpd/irods`). Place the provided
`irods_environment.json` file in this directory. For most setups, this
file can be used as is (but please read the [configuration](#configuration)
section).

Finally, set up httpd to serve Eirods-dav where you want it to. An
example vhost config is provided for your convenience.

## Tips and Tricks ##

If you are dealing with big files, you will almost certainly want to enable 
Apache's compression functionality using [mod_deflate](http://httpd.apache.org/docs/current/mod/mod_deflate.html).

For instance, to enable compressed response bodies from Apache, you might want a configuration such as

```
LoadModule deflate_module modules/mod_deflate.so
SetOutputFilter DEFLATE
SetEnvIfNoCase Request_URI "\.(?:gif|jpe?g|png|gzip|zip|bz2)$" no-gzip
```

## Bugs and ToDos ##

Please report any issues you encounter on the
[issues page](https://github.com/billyfish/eirods-dav/issues).

## Authors ##

[Simon Tyrrell](https://github.com/billyfish) and [Chris Smeele](https://github.com/cjsmeele).

## Contact information ##

For questions or support on the WebDAV functionality, Simon Tyrrell, Chris Smeele or Ton Smeele either
directly.
For questions or support on the themes and REST API, contact Simon Tyrrell
directly or via the [Earlham Institute](http://www.earlham.ac.uk/contact-us/) page.

## License ##

Copyright (c) 2017-20, Earlham Institute and (c) 2016 Utrecht University.

EIrods-dav is licensed under the GNU Lesser General Public License version
3 or higher (LGPLv3+). See the COPYING.LESSER file for details.

The `lock_local.c` file was adapted from the source of `mod_dav_lock`,
a component of Apache HTTPD, and is used with permission granted by
the Apache License. See the copyright and license notices in this file
for details.
