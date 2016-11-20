/*
 * cJSONext.c
 */

#include <stdio.h>
#include <stdlib.h>			// NULL, malloc
#include <string.h>			// strlen
#include "cJSON.h"
#include "cJSONext.h"

//
// global variables
//
static int gDebug = 0;

//
// macros
//
#define	isJSONroot(item)	\
		(((item)->type==cJSON_Object) && ((item)->string==NULL))
//
#define	cJSON_malloc	malloc
#define	cJSON_free		free
extern char* cJSON_strdup(const char* str);		// in cJSON.c

//
#ifdef	CJSON_SEPARATOR
	#define	SEPARATOR	CJSON_SEPARATOR
#else
	#define	SEPARATOR	"/"		// MUST be a non-NULL STRING (preferrably SINGLE character)
	//#define	SEPARATOR	":"	
	//#define	SEPARATOR	"."	
#endif	//!CJSON_SEPARATOR

//
// local support functions
//
	//
static cJSON *parse_and_find(cJSON *item, const char *objPath, const char *prefix);
static cJSON *parse_find_subtree(cJSON *root, char *objPath, char *separator);


//
// public API functions
//

/*
 * Parameters:
 * dbgValue		- debug level
 *
 * Return:
 *		debug level
 */
int cJSONext_setDebug(int dbgValue)
{
	gDebug = dbgValue;
	return gDebug;
}

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
cJSON *cJSONext_getItem(cJSON *item, char *objPath)
{
	char separator[2];
	// use first character of objPath as separator if not provided
	separator[0] = *objPath;
	separator[1] = 0;
	
	// 2015-03-12  special case
	/* if (isJSONroot(item) && (strlen(objPath)==1)) */
	if (strlen(objPath)==1) {
		// simply return the item as the objPath specified
		return item;
	}
	
	// 2015-03-14	/// DONE: the objPath now must be decedent of the item starting with the separator. (excluding the item)
	// 2015-03-09	/// TODO: eliminate the requirement that objPath MUST start with item->string
#if 0
	if (item->string) {
		if (strncmp(&objPath[1], item->string, strlen(item->string)) != 0) {
			// current requirement: objPath must start with item->string
			return NULL;
		}
	}
#endif
	
	//
	if (item->type == cJSON_Object) item = item->child;
	//
	return parse_find_subtree(item, objPath, separator);
}

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
cJSON *cJSONext_traverse(cJSON *cjitem, cJSONext_Callback callback_f, void *pUserData)
{
	// sanity check
	if (callback_f == NULL) { return NULL; }
	//
	cJSON *item = cjitem;
	while (item) {
		int ret = callback_f(item, pUserData);
		if (ret == cJSON_Callback_Stop) {
			/* stop further traversal as requested */
			break;
		}
		if ((item->type==cJSON_Object) || (item->type==cJSON_Array))
		{
			cJSONext_traverse(item->child, callback_f, pUserData);
		}
		item=item->next;
	}
	return item;
}

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
cJSON *cJSONext_traverse_WithPath(cJSON *cjitem, cJSONext_Callback_WithPath callback_f, void *pUserData, char *prefix)
{
	// sanity check
	if (callback_f == NULL) { return NULL; }
	//
	cJSON *item = cjitem;
	while (item) {
		// prepare path
		char *path = malloc((prefix?strlen(prefix):0)+(item->string?strlen(item->string):0)+2);		// 2='/'+'\0'
		if (prefix==NULL)
			sprintf(path,"/");
		else if (strlen(prefix)==1)
			sprintf(path,"%s%s",prefix,item->string?item->string:"");
		else
			sprintf(path,"%s%c%s",prefix,prefix[0],item->string?item->string:"");
		//
		//*DBG*	printf("---prefix=%s,path=%s\n",prefix,path);
		//
		int ret = callback_f(item, pUserData, path);
		if (ret == cJSON_Callback_Stop) {
			/* stop further traversal as requested */
			break;
		}
		//
		if ((item->type==cJSON_Object) || (item->type==cJSON_Array))
		{
			cJSONext_traverse_WithPath(item->child, callback_f, pUserData, path);
		}
		item=item->next;
		// release path
		free(path);
	}
	return item;
}

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
void cJSONext_traverseArray(cJSON *item, cJSONext_Callback callback_f, void *pUserData)
{
	// sanity check
	if (callback_f == NULL) { return; }
	//
	cJSON *child=item->child;
	while (child) {
		int ret = callback_f(child, pUserData);
		if (ret == cJSON_Callback_Stop) {
			/* stop further traversal as requested */
			break;
		}
		child=child->next;
	}
	return;
	
	/* cJSON.c
	 *
	cJSON *cJSON_DetachItemFromArray(cJSON *array,int which)
	{
		cJSON *c=array->child;
		while (c && which>0) c=c->next,which--;
		if (!c) return 0;
		if (c->prev) c->prev->next=c->next;
		if (c->next) c->next->prev=c->prev;
		if (c==array->child) array->child=c->next;
		c->prev=c->next=0;
		return c;
	}
	void   cJSON_DeleteItemFromArray(cJSON *array,int which)
	{
		cJSON_Delete(cJSON_DetachItemFromArray(array,which));
	}
	*/
}

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
cJSON *cJSONext_newItem(int type, void *value)
{
	cJSON *obj = NULL;
	switch (type) {
		case cJSON_Object:
			// need to create an object
			obj = cJSON_CreateObject();
			break;
		case cJSON_Number:
			obj = cJSON_CreateNumber((int)value);
			break;
		case cJSONext_Float:
			{
				double dblValue = *(double*)value;
				obj = cJSON_CreateNumber(dblValue);
			}
			break;
		case cJSON_String:
			obj = cJSON_CreateString((char*)value);
			break;
		case cJSON_False:
			obj = cJSON_CreateFalse();
			break;
		case cJSON_True:
			obj = cJSON_CreateTrue();
			break;
		case cJSON_NULL:
			obj = cJSON_CreateNull();
			break;
		case cJSON_Array:
			obj = cJSON_CreateArray();
			break;
		default:
			/// TODO: other cJSON data types if any
			//
			fprintf(stderr, "[cJSONext:Info:%d] unsupported type: %s\n", __LINE__, show_cJSON_TYPE(type));
			break;
	}
	return obj;
}

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
cJSON *cJSONext_createItem(cJSON *root, char *objPath, int type, void *value)
{
		// sanity check
		if ((objPath[0] == 0) 		/* no objPath given */
		    || (objPath[1] == 0))	/* only separator given */
		{
			fprintf(stderr, "[cJSONext:Warning:%d] [%s]: invalid objPath\n", __LINE__, objPath);
			return (cJSON *)NULL;
		}
	//
	cJSON *item = cJSONext_getItem(root, objPath);
	//
	if (item == NULL) {
		//
		// no object/key found... create one
		//
		
		// the parent cJSON item MUST be updated to properly deleting the specified item
		// parent cJSON objPath
		char *pObjPath = strdup(objPath);
		char *ptr = strrchr(pObjPath, (int)objPath[0]);
			// make pObjPath the object path to parent cJSON item
			*ptr = 0;
		
		// parent cJSON object
		cJSON *parentObj;
		if (*pObjPath == 0) {
			// special case... we are at root
			parentObj = root;
			ptr = objPath;
		}
		else {
			// ensure parent entity exists
			parentObj = cJSONext_createItem(root, pObjPath, cJSON_Object, NULL);
		//
		//*DBG*	printf("%s: objPath: %s, parent: %s, parent_type=%d (%s)\n", __FUNCTION__, objPath, pObjPath, parentObj->type, show_cJSON_TYPE(parentObj->type));
		//
		}
		
		//
		/* can only add item to a cJSON object */
		if (parentObj->type == cJSON_Object)
		{
			cJSON *obj = NULL;
			switch (type) {
				case cJSON_Object:
					if (value == NULL) {
						// need to create an object
						cJSON_AddItemToObject(parentObj, ++ptr, obj=cJSON_CreateObject());
					}
					else {
						cJSON_AddItemToObject(parentObj, ++ptr, (cJSON*)value);
					}
					break;
				case cJSON_Number:
					cJSON_AddNumberToObject(parentObj, ++ptr, (int)value);
					break;
				case cJSONext_Float:
					{
						double dblValue = *(double*)value;
						cJSON_AddNumberToObject(parentObj, ++ptr, dblValue);
					}
					break;
				case cJSON_String:
					cJSON_AddStringToObject(parentObj, ++ptr, (char*)value);
					break;
				case cJSON_False:
					cJSON_AddFalseToObject(parentObj, ++ptr);
					break;
				case cJSON_True:
					cJSON_AddTrueToObject(parentObj, ++ptr);
					break;
				case cJSON_NULL:
					cJSON_AddNullToObject(parentObj, ++ptr);
					break;
				case cJSON_Array:
					cJSON_AddItemToObject(parentObj, ++ptr, (cJSON*)value);
					break;
				default:
					/// TODO: other cJSON data types if any
					//
					fprintf(stderr, "[cJSONext:Info:%d] unsupported type: %s\n", __LINE__, show_cJSON_TYPE(type));
					break;
			}
			item = (obj==NULL) ? parentObj : obj;
		}
		else {
			fprintf(stderr, "[cJSONext:Error:%d] Cannot add %s as [%s] is not a cJSON object\n", __LINE__, objPath, pObjPath);
			item = NULL;		// nothing created
		}
	
		// release strdup
		free(pObjPath);
		
	}	/* if (item == NULL) */
	else {
		if (gDebug) fprintf(stderr, "[cJSONext:Warning%d] [%s]: cJSON entity already exists\n", __LINE__, objPath);
	}
	
	// return the found item
	return item;
}

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
int cJSONext_updateItem(cJSON *root, char *objPath, int type, void *value)
{
		// sanity check
		if ((objPath[0] == 0) 		/* no objPath given */
		    || (objPath[1] == 0))	/* only separator given */
		{
			fprintf(stderr, "[cJSONext:Warning:%d] [%s]: invalid objPath\n", __LINE__, objPath);
			return cJSONext_NotFound;
		}
	int  rc = cJSONext_NoErr;
	//
	cJSON *item = cJSONext_getItem(root, objPath);
	//
	if (item == NULL) {
		fprintf(stderr, "[cJSONext:Warning%d] [%s]: cJSON entity not found\n", __LINE__, objPath);
		return cJSONext_NotFound;
	}
	// update the cJSON item based on provided value
	switch (item->type) {
		case cJSON_Number:
			if (type == cJSONext_Float) {
				double dblValue = *(double*)value;
				cJSON_SetFloatValue(item, dblValue);
			}
			else {
				//cJSON_SetIntValue(item, (int)atoi(value));
				cJSON_SetIntValue(item, (int)(value));
			}
			break;
		case cJSON_String:
			cJSON_SetStringValue(item, (char*)value);
			//cJSON_AddStringToObject(item, item->string, (char*)value);
			break;
		case cJSON_False:
		case cJSON_True:
			item->type = ((int)value) ? cJSON_True : cJSON_False;
			//cJSON_AddTrueToObject(item, item->string);
			break;
		case cJSON_NULL:
			if (gDebug) fprintf(stderr, "[cJSONext:Warning:%d] [%s]: cannot change NULL entity\n", __LINE__, item->string);
			//cJSON_AddNullToObject(item, item->string);
			break;
		case cJSON_Object:
		case cJSON_Array:
		{
			cJSON *pObj = (cJSON *) value;
			//* sanity check
			if (pObj &&
				((pObj->type==cJSON_Object)||(pObj->type==cJSON_Array)))
			{
				// let's keep the cJSON object/array and replace its child
				if (item->child) { cJSON_Delete(item->child); }
				item->child = pObj->child;
				// release the root node provided by caller
				pObj->child = 0;
				cJSON_Delete((cJSON*) pObj);
			}
			else {
				// provided value NOT a cJSON object nor array
				fprintf(stderr, "[cJSONext:Error:%d] [%s] is not a cJSON object/array. Ignored\n", __LINE__, show_cJSON_TYPE(pObj->type));
				rc = cJSONext_Invalid;
			}
			break;
		}
		default:
		/// TODO: other cJSON data types if any
		//
			fprintf(stderr, "[cJSONext:Info:%d] unsupported type: %s\n", __LINE__, show_cJSON_TYPE(item->type));
			break;
	}
	
	// return update status
	return rc;
}

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
 *		cJSONext_Invalid
 *		cJSONext_NotFound
 */
int cJSONext_deleteItem(cJSON *root, char *objPath, int arIdx)
{
	//
	/// TODO: 2015-03-21: need to simplify the over-complicated implementation
	//
	
	// to properly remove an item, we first need to determine its cJSON type
	//
	// a. If it is a cJSON object, use cJSON_Delete()
	// b. If it is a cJSON item (non-object & non-array) use cJSON_DeleteItemFromObject()
	// c. If it is a cJSON array, use
	//			cJSON_DeleteItemFromObject() to delete entire array
	//			cJSON_DeleteItemFromArray()  to remove one array element
	//
	
	// 2015-03-08 sanity check
	if ((root->type != cJSON_Object) && (root->type != cJSON_Array)) {
		if (gDebug) fprintf(stderr, "[cJSONext:Error:%d] (%s) is not supported as root\n", __LINE__, show_cJSON_TYPE(root->type));
		return cJSONext_Invalid;
	}
	
	//
	cJSON *item = cJSONext_getItem(root, objPath);
	//
	if (item == NULL) {
		if (gDebug) fprintf(stderr, "[cJSONext:Warning:%d] [%s]: JSON entity not found\n", __LINE__, objPath);
		return cJSONext_NotFound;
	}
	
	// delete the JSON (sub-)tree
	if (item == root) {
		// detach before delete
			if (item->prev) { item->prev->next = item->next; }
			if (item->next) { item->next->prev = item->prev; }
			item->prev = item->next = 0;
		//
		cJSON_Delete(item);
		return cJSONext_NoErr;
	}
	
	// the parent cJSON item MUST be updated to properly deleting the specified item
	// duplicate objPath for parent cJSON objPath discovery
	char *pObjPath = strdup(objPath);
	char *ptr = strrchr(pObjPath, (int)objPath[0]);
		// make pObjPath the object path to parent cJSON item
		*ptr = 0;
	
	// parent cJSON object
	cJSON *parentObj;
	if (*pObjPath == 0) {
		// special case... we are at root
		parentObj = root;
		ptr = objPath;
	}
	else {
		// ensure parent entity exists
		parentObj = cJSONext_getItem(root, pObjPath);
		//
		//*DBG*	printf("%s: objPath: %s, parent: %s, parent_type=%d (%s)\n", __FUNCTION__, objPath, pObjPath, parentObj->type, show_cJSON_TYPE(parentObj->type));
		//
	}
	
	int rc = cJSONext_GenericErr;
	//
	/* perform the actual removal from parent cJSON object */
	if (parentObj)
	{
		//
		switch (item->type) {
			case cJSON_Object:
				if (parentObj->type == cJSON_Object) {
					cJSON_DeleteItemFromObject(parentObj, ++ptr);
					rc = cJSONext_NoErr;
				}
				break;
			case cJSON_Number:
			case cJSON_String:
			case cJSON_False:
			case cJSON_True:
			case cJSON_NULL:
				if (parentObj->type == cJSON_Object) {
					cJSON_DeleteItemFromObject(parentObj, ++ptr);
					rc = cJSONext_NoErr;
				}
				else {
					if (gDebug) fprintf(stderr, "[cJSONext:Warning:%d] [%s]: parent JSON entity type (%s) not supported\n", __LINE__, pObjPath, show_cJSON_TYPE(parentObj->type));
					rc = cJSONext_GenericErr;
				}
				break;
			case cJSON_Array:
				// there are two conditions
					// arIdx == -1:	delete entire array
				if (arIdx == -1) {
					if (parentObj->type == cJSON_Object) {
						cJSON_DeleteItemFromObject(parentObj, ++ptr);
						rc = cJSONext_NoErr;
					}
				}
					// arIdx != -1:	delete an array element
				else {
					cJSON_DeleteItemFromArray(item, arIdx);
					rc = cJSONext_NoErr;
				}
				break;
			default:
			/// TODO: other cJSON data types if any
			//
				fprintf(stderr, "[cJSONext:Info:%d] unsupported type: %s\n", __LINE__, show_cJSON_TYPE(item->type));
				break;
		}	/* switch (item->type) */
	}
	else {
		// cJSON tree is likely corrupted if this happens
		if (gDebug) fprintf(stderr, "[cJSONext:Error:%d] [%s]: parent cJSON entity not found\n", __LINE__, pObjPath);
		rc = cJSONext_GenericErr;
	}
	
	// release strdup
	free(pObjPath);
	//
	return rc;
}

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
cJSON *cJSONext_detachItem(cJSON *root, char *objPath, int arIdx)
{
	// to properly detach an item, we need to determine its cJSON type
	//
	// a. If it is a cJSON array && arIdx is within 0..arraySize, use
	//			cJSON_DetachItemFromArray()  to detach one array element
	//
	// b. Else, locate the parent object with cJSONext_getItem() and use
	//			cJSON_DetachItemFromObject() to detach the cJSON object/item
	//
	
	// sanity check
	if ((root->type != cJSON_Object) && (root->type != cJSON_Array)) {
		if (gDebug) fprintf(stderr, "[cJSONext:Error:%d] (%s) is not supported as root\n", __LINE__, show_cJSON_TYPE(root->type));
		return NULL;
	}
	
	//
	cJSON *item = cJSONext_getItem(root, objPath);
	//
	if (item == NULL) {
		if (gDebug) fprintf(stderr, "[cJSONext:Warning:%d] [%s]: JSON entity not found\n", __LINE__, objPath);
		return NULL;
	}
	
	// detach the JSON root
	if (item == root) {
			if (item->prev) { item->prev->next = item->next; }
			if (item->next) { item->next->prev = item->prev; }
			item->prev = item->next = 0;
		//
		return item;
	}
	
	// check if a cJSON array
	if (item->type == cJSON_Array) {
		// check if given index is within range
		if ((0 <= arIdx) && (arIdx < cJSON_GetArraySize(item))) {
			// good to go
			return cJSON_DetachItemFromArray(item, arIdx);
		}
	}
	
	// duplicate objPath for parent cJSON objPath discovery
	char *pObjPath = strdup(objPath);
	char *ptr = strrchr(pObjPath, (int)objPath[0]);
		// make pObjPath the object path to parent cJSON item
		*ptr = 0;
	
	// find parent cJSON object
	cJSON *parentObj;
	if (ptr == pObjPath) {
		// only one separator found.  detach something under root
		parentObj = root;
		ptr = objPath;
	}
	else {
		// ensure parent entity exists
		parentObj = cJSONext_getItem(root, pObjPath);
		// should not happen
		if ((parentObj == NULL) || (parentObj->type != cJSON_Object)) {
			fprintf(stderr, "[cJSONext:Error:%d] [%s] object not found or not a cJSON object\n", __LINE__, pObjPath);
			free(pObjPath);
			return NULL;
		}
		//
		//*DBG*	printf("%s: objPath: %s, parent: %s, parent_type=%d (%s)\n", __FUNCTION__, objPath, pObjPath, parentObj->type, show_cJSON_TYPE(parentObj->type));
		//
	}
	// detach from parent object
	item = cJSON_DetachItemFromObject(parentObj, ++ptr);
	
	// release strdup
	free(pObjPath);
	//
	return item;
}

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
int cJSONext_renameItem(cJSON *root, char *objPath, char *name)
{
	cJSON *item = cJSONext_getItem(root, objPath);
	if (item == NULL) {
		return cJSONext_NotFound;
	}
	if (item->string) {
		cJSON_free(item->string);
		item->string = cJSON_strdup(name);
	}
	return cJSONext_NoErr;
}

//
// local support functions
//

/*
 * Parameters:
 * item			- any cJSON object
 * objPath		- object path starts from the JSON item object (MUST begin with a separator)
 * separator	- the separator used in object path
 *
 * Return:
 *		pointer to a cJSON object
 *		or NULL if not found
 */
static cJSON *parse_find_subtree(cJSON *root, char *objPath, char *separator)
{
	cJSON *item = root;
	
	/* sanity check */
	if (item==NULL) { return NULL; }
	
	// should not happen
	if (item->string==NULL) { return NULL; }
	
	// normal cJSON node
	cJSON *retItem = NULL;
	char  *nStr = malloc(strlen(item->string)+2);		// 2='/'+'\0'
	sprintf(nStr, "%c%s", separator[0], item->string);
	if (strncmp(nStr, objPath, strlen(nStr)) == 0) {
		// we have a match, let's see what follows
		if (objPath[strlen(nStr)] == 0) {
			// end of objPath reached and we've found a match
			retItem = item;
		}
		else if (objPath[strlen(nStr)] == separator[0]) {
			// item is indeed part of the objPath
			if (item->child) {
				// continue on
				retItem = parse_find_subtree( item->child, &objPath[1+strlen(item->string)], separator );
			}
			else {
				// oops, cannot proceed
				retItem = NULL;
			}
		}
		else {
			// objPath does not exactly match item->string, more objPath remains
			// need to try item's siblings...
			retItem = parse_find_subtree( item->next, objPath, separator );
		}
	}
	else {
		/// item->string no match, how about its siblings?
		retItem = parse_find_subtree( item->next, objPath, separator );
	}
	free(nStr);
	return retItem;
}

#if 0	/* attic */
/*
 * Parameters:
 * item			- any cJSON object
 * objPath		- object path starts from the JSON item object (MUST begin with a separator)
 * prefix		- the object path leads to JSON item object (1st parameter)
 *
 * Return:
 *		pointer to a cJSON object
 *		or NULL if not found
 */
static cJSON *parse_and_find(cJSON *item, const char *objPath, const char *prefix)
{
	// use first character of objPath as separator if not provided
	char separator[2];
	separator[0] = *objPath;
	separator[1] = 0;
	
	cJSON *ret = NULL;
	while (item)
	{
		char *newprefix;
		if (item->string) {
			int namelen = (item->string) ? strlen(item->string) : 0;
			newprefix = malloc(strlen(prefix)+namelen+2+1);		// +2 for separator, +1 for NUL-terminator
			sprintf(newprefix,"%s%s%s",(strcmp(prefix,separator)==0)?"":prefix,separator,item->string);
		}
		else {
			// root node
			newprefix=malloc(strlen(prefix)+1);		// +1 for NUL-terminator
			sprintf(newprefix,"%s",prefix);
		}
		//*DBG*	printf("newprefix=%s, path=%s\n", newprefix, objPath);
		if (cJSON_strcasecmp(objPath, newprefix) == 0) {
			free(newprefix);
			ret = item;
			break;
		}
		//
		// do depth-first traversal
		//
		if (item->child) {
			ret = parse_and_find(item->child,objPath,newprefix);
			if (ret) break;
		}
		item=item->next;
		free(newprefix);
	}
	return ret;
}
#endif	//0

