/*
 * cJSONext.h
 */

#ifndef _cJSONext__h
#define _cJSONext__h

#ifdef __cplusplus
extern "C"
{
#endif

#include "cJSON.h"

//
// defines
//
#define	CJSON_SEPARATOR	":"		// MUST be a non-NULL STRING (preferrably SINGLE character)
	// add-on to regular cJSON types
#define	cJSONext_Float	(cJSON_Number|0x0800)	// enable 0x0800 bit

//
// macros
//
/* cJSON Types: */
#define	show_cJSON_TYPE(type)	\
	((type)==cJSON_False)  ? "cJSON_False " :	\
	((type)==cJSON_True)   ? "cJSON_True  " :	\
	((type)==cJSON_NULL)   ? "cJSON_NULL  " :	\
	((type)==cJSON_Number) ? "cJSON_Number" :	\
	((type)==cJSON_String) ? "cJSON_String" :	\
	((type)==cJSON_Array)  ? "cJSON_Array " :	\
	((type)==cJSON_Object) ? "cJSON_Object" :	\
	"-undefined-cJSON-type-"

// cJSONext return values
enum {
	cJSONext_NoErr      = 0,
	cJSONext_GenericErr =-1,
	cJSONext_NotFound   =-2,		// cJSON objPath not found
	cJSONext_Invalid    =-3,		// invalid cJSON item
};

// callback function enum
enum { cJSON_Callback_Stop=0, cJSON_Callback_Continue=1, };

// callback function typedef
/*
 * Parameters:
 *	item		- a cJSON object
 *	pUserData	- opaque user data provided when calling cJSONext_traverse
 * Return:
 *	cJSON_Callback_Stop		- stop further JSON tree traverse
 *	cJSON_Callback_Continue	- allow continuing JSON tree traverse
 */
typedef int (*cJSONext_Callback)(cJSON *item, void *pUserData);
/*
 * Parameters:
 *	item		- a cJSON object
 *	pUserData	- opaque user data provided when calling cJSONext_traverse_WithPath
 *	path		- the full object path to the cJSON object (experimental)
 * Return:
 *	cJSON_Callback_Stop		- stop further JSON tree traverse
 *	cJSON_Callback_Continue	- allow continuing JSON tree traverse
 */
typedef int (*cJSONext_Callback_WithPath)(cJSON *item, void *pUserData, char *path);

//
// supported API functions
//
/*
// search and find a cJSON object
//
 * Parameters:
 * item			- any cJSON object
 * objPath		- full object path to search (MUST begin with a separator)
 *
 * Return:
 *		pointer to a cJSON object
 *		or NULL if not found
 */
extern cJSON *cJSONext_getItem(cJSON *item, char *objPath);

/*
// create a cJSON object not attached to any cJSON tree (i.e. w/o object path)
//
 * Parameters:
 * type			- cJSON object type
 * value		- cJSON object value (ignored for cJSON_True, cJSON_False, and cJSON_NULL)
 *
 * Return:
 *		pointer to the created cJSON object
 *		or NULL if creation failed
 */
cJSON *cJSONext_newItem(int type, void *value);

/*
// create a cJSON object
//
 * Parameters:
 * root			- any cJSON object
 * objPath		- full object path to search (MUST begin with a separator)
 * type			- cJSON object type
 * value		- cJSON object value (ignored for cJSON_True, cJSON_False, and cJSON_NULL)
 *
 * Return:
 *		pointer to the created cJSON object
 *		or NULL if creation failed
 */
cJSON *cJSONext_createItem(cJSON *root, char *objPath, int type, void *value);

/*
// update a cJSON object
//
 * Parameters:
 * root			- any cJSON object
 * objPath		- full object path to search
 * type			- cJSON object type
 * value		- new value
 *
 * Return:
 *		cJSONext_NoErr
 *		cJSONext_GenericErr
 *		cJSONext_NotFound
 *		cJSONext_Invalid
 */
extern int cJSONext_updateItem(cJSON *root, char *objPath, int type, void *value);

/*
// remove a cJSON object/item
//
 * Parameters:
 * root			- the root cJSON object contains objPath
 * objPath		- full object path to search
 * arIdx		- array index if the objPath points to an array (index starts at 0, -1 means entire array)
 *
 * Return:
 *		cJSONext_NoErr
 * 		cJSONext_Invalid
 *		cJSONext_NotFound
 */
extern int cJSONext_deleteItem(cJSON *root, char *objPath, int arIdx);

/*
// detach a cJSON object/item
//
 * Parameters:
 * root			- the root cJSON object contains objPath
 * objPath		- full object path to search
 * arIdx		- array index if the objPath points to an array (index starts at 0, -1 means entire array)
 *
 * Return:
 *		pointer to the created cJSON object
 *		or NULL if creation failed
 */
extern cJSON *cJSONext_detachItem(cJSON *root, char *objPath, int arIdx);

/*
// rename a cJSON object/item
//
 * Parameters:
 * root			- the root cJSON object contains objPath
 * objPath		- relative object path to the cJSON item
 * name			- the new name for the cJSON item
 *
 * Return:
 *		cJSONext_NoErr
 *		cJSONext_NotFound
 *
 * Note: unnamed cJSON object (e.g. root, array element) will remain anonymous
 */
extern int cJSONext_renameItem(cJSON *root, char *objPath, char *name);

/*
// traverse all entities under a cJSON object
//
 * Parameters:
 * cjitem		- any cJSON object
 * callback_f	- callback function for each element
 * pUserData	- opaque user data to be passed to the callback function
 *
 * Return:
 *		last cJSON item visited
 */
cJSON *cJSONext_traverse(cJSON *cjitem, cJSONext_Callback callback_f, void *pUserData);
/*
// traverse all entities under a cJSON object
//
 * Parameters:
 * cjitem		- any cJSON object
 * callback_f	- callback function for each element
 * pUserData	- opaque user data to be passed to the callback function
 * prefix		- prefix leads to the cjitem (use NULL for cJSON root)
 *
 * Return:
 *		last cJSON item visited
 */
cJSON *cJSONext_traverse_WithPath(cJSON *cjitem, cJSONext_Callback_WithPath callback_f, void *pUserData, char *prefix);

/*
// traverse all elements in a cJSON array object
//
 * Parameters:
 * item			- any cJSON array object
 * callback_f	- callback function for each element
 * pUserData	- opaque user data to be passed to the callback function
 *
 * Return:
 *		N/A
 */
extern void cJSONext_traverseArray(cJSON *item, cJSONext_Callback callback_f, void *pUserData);

// utility functions
extern int cJSONext_setDebug(int dbgValue);

#ifdef __cplusplus
}
#endif

#endif	// _cJSONext__h
