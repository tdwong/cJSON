/*
 * cJSON CLI
 *
 * 2015-01-08	TODO: consolidate main() and setup_object()
 * 2015-03-15	TODO: 1. unify error message with [err] prefix
 * 				      2. add command line parameters
 * 				      3. add readline (for line history)
 *
 */

#include <stdio.h>			// printf
#include <stdlib.h>			// malloc
#include <string.h>			// strlen
#include <ctype.h>			// isblank
#include "cJSON.h"
#include "cJSONext.h"

#define	MAX_BUFSIZE		8*1024		// 8K for now

/*
////////////////////////
//
//  vocabulary in cJSON
//
//  item	- can be an object or a key/value pair
//  object	- an item with child item (but has no key/value)
//	prefix	~ (similar to) object path
//
//	(mine)
//	entity		~ i.e. item
//	object path	- the "path" lead to the key/value pair
//
//
//
*/


////////////////////////
//
/* global variables */
int  gDebug = 0;
static int getDebug() { return gDebug; }
static int setDebug(int dbgLevel)
{
	gDebug = dbgLevel;
	return gDebug;
}

/* cJSON Types (for create cJSON object) */
#define	get_cJSON_TYPE(chr)	\
	((chr)=='F' || (chr)=='f') ? cJSON_False  :	\
	((chr)=='T' || (chr)=='t') ? cJSON_True   :	\
	((chr)=='N' || (chr)=='n') ? cJSON_NULL   :	\
	((chr)=='#' || (chr)=='i') ? cJSON_Number :	\
	((chr)=='S' || (chr)=='s') ? cJSON_String :	\
	((chr)=='A' || (chr)=='a') ? cJSON_Array  :	\
	((chr)=='O' || (chr)=='o') ? cJSON_Object :	\
	-1

/* sample JSON data */
static char *sample_JSON[2] = {
"\
{ \"C\" : 3, \"B\" : 2,		\n\
  \"A\" : [ { \"a\" : 1, \"b\" : 22 }, { \"c\" : 2 } ],	\n\
  \"D\" : \"stringD\",		\n\
  \"S\" : { \"z\" : \"zs\" }	\n\
}	\
",
"\
{ \"C\" : 3, \"B\" : 2,		\n\
  \"A\" : [ { \"a\" : 1, \"b\" : 22, \"eA\" : [ { \"c\" : 1, \"b\" : 22 } ] }, { \"f\" : 10 } ],	\n\
  \"E\" :   { \"a\" : 1, \"b\" : 22, \"eA\" : [ { \"c\" : 1, \"b\" : 22 }, { \"f\" : \"10\" } ], \"S\": { \"z\" : \"zs\" } },	\n\
  \"D\" : \"stringD\"		\n\
}	\
",
};
/* sample JSON array data */
static char *sample_JSON_array[2] = {
"\
  [ { \"a\" : 1, \"b\" : 22 }, { \"b\" : 2 } ]	\
",
"\
  [ { \"a\" : 1, \"b\" : 22, \"eA\" : [ { \"c\" : 1, \"b\" : 22 } ] }, { \"f\" : 10 } ]	\
",
};

////////////////////////
//
/* local functions */
static char *traverse_array (cJSON *item, char *prefix, void *pUserData);
static char *traverse_object(cJSON *item, char *prefix, void *pUserData);
static cJSON *setup_array(void);
static cJSON *setup_object(void);
//
static void zCommand(char *pans, cJSON **pRoot);
//
static int   arrayDepth=0;
static char  arrayTabs[8];			// up to 8-level of array
static char *arrayIndent(int depth)
{
	int  ix;
	for (ix=0; ix<depth; ix++) { arrayTabs[ix]='\t'; }
	arrayTabs[depth] = 0;
	return arrayTabs;
}

// CLI prompt
int  gCliLevel = 0;
static inline void prompt(const char *str)
{
	if (gCliLevel == 0) {
		printf("%s> ", str);
	}
	else {
		printf("%s:%d> ", str, gCliLevel);
	}
	/// TODO: read from stdin (and optionally print what's read for automatic test)
}

////////////////////////
//
/* support function (can be inline?) */
#include <sys/stat.h>		// stat()
int getFileSize(char *name)
{
	struct stat st;
	if (stat(name, &st) != 0) {
		return -1;
	}
	return st.st_size;
}
//
////////////////////////


////////////////////////
///

///////////////	simple callback functions

// callback function
/*
 * Parameters:
 *	item		- a cJSON object
 *	pUserData	- user data provided in cJSONext traverse call
 *
 * Return:		cJSON_Callback_Stop or cJSON_Callback_Continue
 */
static int sArrayLevel = 0;
static int showJsonArrayElement(cJSON *item, void *pUserData)
{
	// sanity check
	if (item == NULL) {
		printf("Fatal: encounter a NULL cJSON item.  Stop traversal...\n");
		return cJSON_Callback_Stop;
	}
	// print value of item
	switch (item->type) {
		case cJSON_String:
			printf("%s[%s]\t\"%s\"\n",    arrayIndent(sArrayLevel), item->valuestring, item->string?item->string:"{?}"); break;
		case cJSON_Number:
			printf("%s[%d]\t\"%s\"\n",    arrayIndent(sArrayLevel), item->valueint, item->string?item->string:"{?}"); break;
		case cJSON_False:
			printf("%s[false]\t\"%s\"\n", arrayIndent(sArrayLevel), item->string?item->string:"{?}"); break;
		case cJSON_True:
			printf("%s[true]\t\"%s\"\n",  arrayIndent(sArrayLevel), item->string?item->string:"{?}"); break;
		case cJSON_NULL:
			printf("%s[null]\t\"%s\"\n",  arrayIndent(sArrayLevel), item->string?item->string:"{?}"); break;
		case cJSON_Array:
		{
			printf("%s:array:[\n", arrayIndent(sArrayLevel));
			sArrayLevel++;
			cJSONext_traverseArray(item, showJsonArrayElement, NULL);
			sArrayLevel--;
			printf("%s]\n",        arrayIndent(sArrayLevel));
			break;
		}
		case cJSON_Object:
		{
			printf("%s:object:[\n", arrayIndent(sArrayLevel));
			sArrayLevel++;
			cJSONext_traverse(item->child, showJsonArrayElement, NULL);
			sArrayLevel--;
			printf("%s]\n",        arrayIndent(sArrayLevel));
			break;
		}
		default:
			printf("\n");
	}
	fflush(stdout);
	//
	return cJSON_Callback_Continue;
}

// callback function
/*
 * Parameters:
 *	item		- a cJSON object
 *	pUserData	- user data provided in cJSONext traverse call
 *	objPath		- the full object path to the cJSON object
 *
 * Return:		cJSON_Callback_Stop or cJSON_Callback_Continue
 */
int showJsonNode(cJSON *item, void *pUserData, char *objPath)
{
	// sanity check
	if (item == NULL) {
		return cJSON_Callback_Stop;
	}
	
	int rc = cJSON_Callback_Continue;
	if (gDebug) {
		printf("%s: %s[%s] [%s]\titem=%p [next=%p,prev=%p,child=%p]\t",		/* continue to the item value... */
					__FUNCTION__,
					arrayIndent(arrayDepth),
					show_cJSON_TYPE(item->type),
	// objPath IS the full object path for the JSON attribute/property
					objPath,
					item,
					item->next, item->prev, item->child
					);
	}
	else {
		printf("%s[%s] [%s]\t",			/* output continue to the item value... */
					arrayIndent(arrayDepth),
					show_cJSON_TYPE(item->type),
					objPath);			// objPath IS the full object path for the JSON attribute/property
	}
	
	/// 2015-03-03	use pUserData to filter desired item
	if (pUserData && (item->type != cJSON_Object) && (strcmp((const char*)pUserData, (const char*)item->string)!=0)) {
		// skip showing this item
		return cJSON_Callback_Continue;
	}
	
	// print value of item
	{
		// show cJSON item name
		if (item->type!=cJSON_Object) { printf("{%s}", item->string); }
		// show cJSON item value
		switch (item->type) {
			case cJSON_String:
				printf("[%s]\n", item->valuestring); break;
			case cJSON_Number:
				printf("[%d]\n", item->valueint); break;
			case cJSON_False:
				printf("[false]\n"); break;
			case cJSON_True:
				printf("[true]\n" ); break;
			case cJSON_NULL:
				printf("[null]\n" ); break;
			case cJSON_Array:
				{
				printf(":array:[\n"); arrayDepth++;
				traverse_array(item,objPath,NULL);
				printf("%s]\n", arrayIndent(arrayDepth)); arrayDepth--;
				}
				break;
			case cJSON_Object:
				{
				printf(":object:{\n"); arrayDepth++;
				traverse_object(item,objPath,NULL);
				printf("%s}\n", arrayIndent(arrayDepth)); arrayDepth--;
				}
				break;
			default:
				printf("\n");
		}
	}
	fflush(stdout);
	
	return rc;
}

// callback function
/*
 * Parameters:
 *	item		- a cJSON object
 *	pUserData	- user data provided in cJSONext traverse call
 *
 * Return:		cJSON_Callback_Stop or cJSON_Callback_Continue
 */
#include <assert.h>
int callback_ShowJsonObject(cJSON *item, void *pUserData)
{
	// error handling
	if (item == NULL) return cJSON_Callback_Continue;
	// show cJSON item name
	if (item->type==cJSON_Object)     { printf("{}"); }
	else if (item->type==cJSON_Array) { printf("{}"); }
	else { printf("{%s}", (item->string)?(item->string):"_?_"); }
	// show cJSON item value
	switch (item->type) {
		case cJSON_String:
			printf("[%s]\n", item->valuestring); break;
		case cJSON_Number:
			printf("[%d]\n", item->valueint); break;
		case cJSON_False:
			printf("[false]\n"); break;
		case cJSON_True:
			printf("[true]\n" ); break;
		case cJSON_NULL:
			printf("[null]\n" ); break;
		case cJSON_Array:
			printf(":array:[%s]\n", item->string?item->string:"");
			break;
		case cJSON_Object:
			printf(":object:[%s]\n", item->string?item->string:"");
			break;
		default:
			printf("\n");
	}
	return cJSON_Callback_Continue;
}

// callback function
/*
 * Parameters:
 *	item		- a cJSON object
 *	pUserData	- user data provided in cJSONext traverse call
 *	path		- objPath to the cJSON object
 *
 * Return:		cJSON_Callback_Stop or cJSON_Callback_Continue
 */
#include <assert.h>
int callback_FilterJsonItem(cJSON *item, void *pUserData, char *objPath)
{
	if (pUserData) {
	// user-defined filter
	// non-matched item will be IGNORED
		if (strcmp((char*)pUserData, item->string?item->string:"") != 0)
			return cJSON_Callback_Continue;
	}
	//
	assert(objPath);
	printf("[%s]\t",objPath);
	return callback_ShowJsonObject(item, pUserData);
}
int callback_ShowJsonPathAndItemCount(cJSON *item, void *pUserData, char *objPath)
{
	//
	assert(objPath);
	printf("[%s]\t",objPath);
	//
	int *pInt = (int *)pUserData;
//	if (pInt) { printf("itemCount=%02d\t", ++(*pInt)); }
	if (pInt) { ++(*pInt); }
	return callback_ShowJsonObject(item, pUserData);
}

///
////////////////////////

#if	defined(_local_getItem_) 
////////////////////////
//
cJSON *mmy_parse_find_subtree(cJSON *root, char *objPath, char separator)
{
	cJSON *item = root;
	
	/* sanity check */
	if (item==NULL) { return NULL; }
	
#if 0
	// if cJSON root
	if ((item->type==cJSON_Object) && (item->string==NULL)) {
		// this would failed for now...
		return mmy_parse_find_subtree(item->child, objPath, separator);
	}
#endif
	
	// should not happen
	if (item->string==NULL) { return NULL; }
	
	// normal cJSON node
	cJSON *retItem = NULL;
	char *nStr = malloc(strlen(item->string)+2);		// 2='/'+'\0'
	sprintf(nStr, "%c%s", separator, item->string);
	if (strncmp(nStr, objPath, strlen(nStr)) == 0) {
		// we have a match, let's see what follows
		if (objPath[strlen(nStr)] == 0) {
			// end of objPath reached and we've found a match
			retItem = item;
		}
		else if (objPath[strlen(nStr)] == separator) {
			// item is indeed part of the objPath
			if (item->child) {
				// continue on
				retItem = mmy_parse_find_subtree( item->child, &objPath[1+strlen(item->string)], separator );
			}
			else {
				// oops, cannot proceed
				retItem = NULL;
			}
		}
		else {
			// objPath does not exactly match item->string, more objPath remains
			// need to try item's siblings...
			retItem = mmy_parse_find_subtree( item->next, objPath, separator );
		}
	}
	else {
		/// item->string no match, how about its siblings?
		retItem = mmy_parse_find_subtree( item->next, objPath, separator );
	}
	free(nStr);
	return retItem;
}
cJSON *mmy_getItem(cJSON *root, char *objPath)
{
	cJSON *item = root;
	
	// 2015-03-12  special case
	if ((strlen(objPath)==1)) {
		// simply return the item as the objPath specified
		return item;
	}
	
	if (item->type == cJSON_Object) item = item->child;
	return mmy_parse_find_subtree(item, objPath, objPath[0]);
}
///
////////////////////////
#endif	//defined(_local_getItem_) 

////////////////////////
//

	static inline void inline_check_find_result(cJSON *found, char *path)
	{
		if (found) { showJsonNode(found, NULL, path); }
		else { printf("\"%s\": not found\n", path); }
	}

	static inline void inline_check_update_result(int ret, char *path)
	{
		switch (ret) {
			case cJSONext_GenericErr:
				{ printf("%s: failed in update (ret=%d)\n", path, ret); break; }
			case cJSONext_NotFound:
				{ printf("%s: objPath not found (ret=%d)\n", path, ret); break; }
			case cJSONext_Invalid:
				{ printf("%s: invalid cJSON item (ret=%d)\n", path, ret); break; }
			case cJSONext_NoErr:
				{ printf("%s: update succeeded (ret=%d)\n", path, ret); break; }
		};
	}

	static cJSON *inline_get_subtree(cJSON *root, char *subtree)
	{
		cJSON *item = cJSONext_getItem(root, subtree);
		if (item == NULL) {
			// error handling
			printf("%s: objPath not found\n", subtree);
		}
		else if (item->type != cJSON_Object) {
			// error handling
			printf("%s: objPath not a cJSON object\n", subtree);
			item = NULL;
		}
		return item;
	}

//
////////////////////////


int main(int argc, char **argv)
{
	// MUST set CLI level to -1 (as gCliLevel==0 means top level)
	gCliLevel = -1;
	//
	cJSON *root = setup_object();
	//
	// clean-up
	if (root) {
		char *out = cJSON_Print(root);
		if (out) {
			printf("final result:\n%s\n",out);
			free(out);
		}
		cJSON_Delete(root);
	}
	//
	return 0;
}

// set gDebug interactively
//
static inline void setgDebugValue(char *ans)
{
	char cmd[8], response[16];
	int  dbgValue;
	int cnt = sscanf(ans, "%s %d", cmd, &dbgValue);
	if (cnt <= 1) {
		printf("debug? ");
		gDebug = atoi(fgets(response, sizeof(response), stdin));
	}
	else {
		gDebug = dbgValue;
	}
	printf("gDebug=%d\n", gDebug);
}

/* Render an array to text */
static char *traverse_array(cJSON *item, char *prefix, void *pUserData)
{
//	char **entries;
//	char *out=0,*ptr,*ret;int len=5;
	cJSON *child=item->child;
//	int numentries=0,i=0,fail=0;
	
	/* How many entries in the array? */
	while (child) {
		showJsonNode(child, pUserData, prefix);
		child=child->next;
	}
	return NULL;
}
/* Render an object to text */
static char *traverse_object(cJSON *item, char *prefix, void *pUserData)
{
	return traverse_array(item, prefix, pUserData);
#if 0
	cJSON *next=item->child;
	/* go thru all items in the object */
	while (next) {
		showJsonNode(next, pUserData, prefix);
		next=next->next;
	}
	return NULL;
#endif
}

/* command loop to enable setting up a JSON array */
static cJSON *setup_array(void)
{
	cJSON *root = NULL;
	char  arBuffer[MAX_BUFSIZE] = {0};
	char  ans[128];
	int   arloop=1;
	//
	char cmd[8], param[64];
	int  cnt;

	printf("gathering array details...\n");

	gCliLevel++;
	while (arloop) {
		prompt("array");
		char *pans = fgets(ans, sizeof(ans), stdin);
		if (pans == NULL) { fputc((int)'\n', stdout); arloop=0; clearerr(stdin); continue; }	/* handle <ctrl-d> */
			// chop \n
			if (ans[strlen(ans)-1] == '\n') { ans[strlen(ans)-1] = '\0'; }
			// skip leading whitespaces
			while (isblank((int)*pans)) ++pans;
		switch (*pans) {
			case '\0':
				break;
			case '?':
			case 'h':
			{
				printf( "available array commands:\n"
						"  ?,h:  this menu\n"
						"  r:    read JSON array data (with enclosed square brackets)\n"
						"  p:    pretty print cached JSON array data\n"
						"  c:    create JSON array element\n"
						"  u:    update JSON array element\n"
						"  d:    delete JSON array element\n"
						"  e:    detach JSON array element\n"
						"  s:    load sample JSON array data\n"
						"  q,x:  exit\n"
						);
				break;
			}
			case 'r':
			{
				printf("read...\n");
				cnt = sscanf(ans, "%s %[^\n]s", cmd, param);
				if ((cnt > 1) && (param[0] == '?')) {
					// quick help
					printf("usage: %c [filename]\n", *pans);
					break;
				}
				if (cnt > 1) {
					// check file size against buffer size
					if (getFileSize(param) > sizeof(arBuffer)) {
						printf("Oops! [%s]: file size larger than internal buffer (%d bytes)\n", param, (int)sizeof(arBuffer));
						break;
					}
					// read from file
					FILE *fp = fopen(param, "r");
					if (fp == NULL) {
						printf("%s: cannot open file\n", param);
						break;
					}
						// read every byte upto sizeof(arBuffer)
					cnt = fread(arBuffer, 1, sizeof(arBuffer), fp);
					if (cnt<=0) {
						printf("%s: failed reading from file\n", param);
						break;
					}
					fclose(fp);
				}
				else {
					// read from stdin
					char ch;
					cnt=0;
					while ((ch = fgetc(stdin)) != EOF && cnt < sizeof(arBuffer)) {
						arBuffer[cnt++] = ch;
					}
					clearerr(stdin);			/* handle EOF */
					if (cnt==0) { break; }
					if (cnt==sizeof(arBuffer)) {
						printf("Oops! Run of input buffer (buffer size: %d)\n", (int)sizeof(arBuffer));
						break;
					}
				}
				arBuffer[cnt] = 0;
				if (arBuffer[0] == 0) { break; }
				// now build the JSON tree
					// release previous JSON tree if any
					if (root) { cJSON_Delete(root); }
				root = cJSON_Parse(arBuffer);
				if (!root) {
					printf("cJSON_Parse() failed. Error before: [%s]\n", cJSON_GetErrorPtr());
				}
				break;
			}
			case 'p':
			{
				printf("pprint%s\n", (root==NULL)?": no JSON data":"");
				if (root==NULL) { break; }
				char *out = cJSON_PrintArray(root);
				if (out) {
					printf("%s\n",out);
					free(out);
				}
				else {
					printf("empty JSON array\n");
				}
				break;
			}
			case 't':
			{
				if (root==NULL) { printf("no JSON data\n"); break; }
				cJSONext_traverseArray(root, showJsonArrayElement, NULL);
				break;
			}
			case 'c':
			{
			//
			// - create & insert an element into a cJSON array
			//
			//	For cJSON_Array and cJSON_Object, a helper function (setup_array/setup_object) is called to
			//	proper construct an array or an object, then cJSON_AddItemToArray() is used
			//		// note: the helper function gets into next CLI interactive level in order to build up
			//		// the wanted cJSON structure
			//
			//	For other cJSON object types (True,False,NULL,Number,String), cJSONext_newItem() is used.
			//
			//	- newly created element is simply appended to the array (as last element)
			//
				char type[4];
				cnt = sscanf(ans, "%s %s %[^\r]s", cmd, type, param);
				if ((cnt > 1) && (type[0] == '?')) {
					// quick help
					printf("usage: %c obj_path obj_type value\n", *pans);
					break;
				}
				if (cnt < 2) {
					printf("create (type(F|T|N|#|s|a|o) value)? ");
					if (fgets(ans, sizeof(ans), stdin) == NULL)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					cnt = sscanf(ans, "%s %[^\n]s", type, param);
					++cnt;			// add 'cmd' to total 'cnt'
				}
				// make sure root object exists
				if (root == NULL) {
					root=cJSON_CreateArray();	
				}
				// handle different object type
				switch (type[0]) {
					case 'a':
					case 'A':
						cJSON_AddItemToArray(root, setup_array());
						break;
					case 'o':
					case 'O':
						cJSON_AddItemToArray(root, setup_object());
						break;
					case '#':
					case 'i':
					case 's':
					case 'S':
						// make sure object value is provided
						if (cnt < 3) {
							printf("value? "); fflush(stdout);
							if (scanf("%[^\n]s", param) < 0)
								/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
							fgetc(stdin);		// chop \n
						}
						/* fall thru */
					case 'F':
					case 'f':
					case 'T':
					case 't':
					case 'N':
					case 'n':
					{
						cJSON *pObj = cJSONext_newItem(get_cJSON_TYPE(type[0]),
							// changed atoi >> atol for os/x
							(((type[0])=='#')||((type[0])=='i')) ? (void*)atol(param) : param);
						if (pObj) {
							cJSON_AddItemToArray(root, pObj);
		//					printf("%s created\n", show_cJSON_TYPE(get_cJSON_TYPE(type[0])));
							printf("%s created\n", type);
						}
						else {
		//					printf("failed to create %s\n", show_cJSON_TYPE(get_cJSON_TYPE(type[0])));
							printf("failed to create %s\n", type);
						}
						break;
					}
					// everything else...
					default:
						printf("%s: unsupported type\n", type);
						break;
				} /*switch (type[0])*/
				break;
			}
			case 'u':
			{
			//
			// - update a cJSON array element with a different value
			//
			// - two parameters are required
			//		-- an index				(array index starts with 0)
			//		-- the object value		(e.g. 8080)
			//
			//		For cJSON_Array and cJSON_Object, a helper function (setup_array/setup_object) is called to
			//		proper construct an array or an object for replaement
			//		// note: the helper function has its own interactive CLI in building the cJSON structure
			//
			//		Other cJSON types use cJSONext_newItem() to create replacement element
			//
			//		cJSON_ReplaceItemInArray() is used to update all cJSON object types
			//
				if ((sscanf(ans, "%s %[^\r]s", cmd, param) > 1) && (param[0] == '?')) {
					// quick help
					printf("usage: %c index value\n", *pans);
					break;
				}
				printf("update%s\n", (root==NULL)?": no JSON data":"");
				if (root==NULL) { break; }
				int   idx;
				// collect JSON object path and data (the data applies only to certain JSON types)
				cnt = sscanf(ans, "%s %d %[^\r]s", cmd, &idx, param);
				if (cnt < 2) {
					printf("update (idx [value])? ");
					if (fgets(ans, sizeof(ans), stdin) == NULL)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					cnt = sscanf(ans, "%d %[^\n]s", &idx, param);
					++cnt;			// add 'cmd' to total 'cnt'
				}
				// validate given idx in range
				int size = cJSON_GetArraySize(root);
				if (idx < 0 || size <= idx) {
					printf("idx=%d out of range [0..%d]\n", idx, (size-1));
					break;
				}
				// identify JSON array element object type
				int   which=idx;
				cJSON *pObj=root->child;
				while (pObj && which>0) pObj=pObj->next,which--;
				if (pObj == NULL) {
					// error handling
					printf("idx=%d out of range [0..%d]\n", idx, (size-1));
					break;
				}
					// acquire object value if not available for certain object types
				if ((cnt < 3) && ((pObj->type==cJSON_Number)||(pObj->type==cJSON_String)))
				{
					printf("update (value)? ");
					if (scanf("%[^\n]s", param) < 0)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					fgetc(stdin);		// chop \n
				}
					// set default object value if not available for certain object types
				if ((cnt < 3) && ((pObj->type==cJSON_True)||(pObj->type==cJSON_False)))
				{
					strcpy(param, (pObj->type==cJSON_True)?"true":"false");
				}
				// update cJSON object based on its type
				cJSON *item = NULL;
				switch (pObj->type) {
					case cJSON_Object:
					case cJSON_Array:
						item = (pObj->type==cJSON_Object) ? setup_object() : setup_array();
						break;
					case cJSON_True:
					case cJSON_False:
					{
							// changed int >> long for os/x
						long value = ((strcasecmp(param, "false")==0)||(strcasecmp(param, "f")==0)||(strcasecmp(param, "0")==0)) ? cJSON_False : cJSON_True;
						item = cJSONext_newItem(pObj->type, (void*)value);
						break;
					}
					case cJSON_NULL:
						printf("cannot update a JSON NULL object\n");
						break;
					case cJSON_Number:
							// changed atoi >> atol for os/x
						item = cJSONext_newItem(pObj->type, (void*)atol(param));
						break;
					case cJSON_String:
						item = cJSONext_newItem(pObj->type, (void*)param);
						break;
					default:
						printf("%d: unknown JSON type\n", (pObj->type));
						break;
				} /*switch (pObj->type)*/
				// update (i.e. replace) array element
				if (item) {
					cJSON_ReplaceItemInArray(root, idx, item);
					printf("array element [%d] updated\n", idx);
				}
				break;
			}
			case 'd':
			{
				if ((sscanf(ans, "%s %[^\r]s", cmd, param) > 1) && (param[0] == '?')) {
					// quick help
					printf("usage: %c index\n", *pans);
					break;
				}
				int idx = -1;
				if (root==NULL) { printf("no JSON data\n"); break; }
				cnt = sscanf(ans, "%s %d", cmd, &idx);
				if (cnt <= 1) {
					printf("delete (array index start with 0)? ");
					if (scanf("%d", &idx) < 0)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					fgetc(stdin);		// chop \n
				}
				cJSON_DeleteItemFromArray(root, idx);
				break;
			}
			case 'e':
			{
				if ((sscanf(ans, "%s %[^\r]s", cmd, param) > 1) && (param[0] == '?')) {
					// quick help
					printf("usage: %c index\n", *pans);
					break;
				}
				int idx = -1;
				if (root==NULL) { printf("no JSON data\n"); break; }
				cnt = sscanf(ans, "%s %d", cmd, &idx);
				if (cnt <= 1) {
					printf("detach (array index start with 0)? ");
					if (scanf("%d", &idx) < 0)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					fgetc(stdin);		// chop \n
				}
				int size = cJSON_GetArraySize(root);
				if (idx < 0 || size <= idx) {
					printf("idx=%d out of range [0..%d]\n", idx, (size-1));
					break;
				}
				cJSON *pDetached = cJSON_DetachItemFromArray(root, idx);
				//
				callback_ShowJsonObject(pDetached, NULL);
				//
				char *out = cJSON_Print(pDetached);
				printf("detached cJSON item: %s\n", out);
				free(out);
					// release detached item... (similar effect to cJSONext_deleteItem()
					cJSON_Delete(pDetached);
				break;
			}
			case 's':
			{
				int idx = 0;
				cnt = sscanf(ans, "%s %[^\n]s", cmd, param);
				if ((cnt > 1) && (param[0] == '?')) {
					// quick help
					printf("# of sample json data set: %d\n", (int)(sizeof(sample_JSON_array)/sizeof(char *)));
					break;
				}
				if (cnt > 1) {
					idx = atoi(param);
					idx %= sizeof(sample_JSON_array)/sizeof(char *);
				}
				printf("loading sample json array data [%d]\n", idx);
				strcpy(arBuffer, sample_JSON_array[idx]);
				if (root) { cJSON_Delete(root); }
				root = cJSON_Parse(arBuffer);
				if (!root) {
					printf("cJSON_Parse() failed. Error before: [%s]\n", cJSON_GetErrorPtr());
				}
				break;
			}
			case 'g':
			{
				if (gCliLevel!=0) { break; }
				setgDebugValue(ans);
				break;
			}
			case 'q':
			case 'x':
				printf("quit array\n");
				arloop = 0;
				break;
			case ';':
				// echo
				printf("%s\n", ans);
				// fall thru
			case '#':
				// ignore comments
				break;
			case '!':
				// run shell command
				cnt = sscanf(ans, "%s %[^\n]s", cmd, param);
				if (cnt > 1) { system(param); }
				break;
			default:
				printf("unknown command: %s\n", ans);
				break;
		///////////////
			case 'z':
			{
				printf("%c: test different cJSONext_traverse implementation\n", *pans);
				if (root==NULL) { break; }
				int count = 0;
//				cJSONext_traverse(root,callback_ShowJsonObject,&count);
				cJSONext_traverse_WithPath(root,callback_ShowJsonPathAndItemCount,&count,NULL);
				printf("%s: total item count=%d\n", __FUNCTION__, count);
				break;
			}
		}	/* switch (*pans) */
	}
	--gCliLevel;
	return root;
}

/* command loop to enable setting up a JSON object */
static cJSON *setup_object(void)
{
	cJSON *root = NULL;
	char  objBuffer[MAX_BUFSIZE] = {0};
	char  ans[128];
	int   objloop = 1;
	//
	char cmd[8], param[64];
	int  cnt;

	printf("gathering object details...\n");

	gCliLevel++;
	while (objloop) {
		prompt((gCliLevel==0) ? "cmd" : "object");
		char *pans = fgets(ans, sizeof(ans), stdin);
		if (pans == NULL) { fputc((int)'\n', stdout); objloop=0; clearerr(stdin); continue; }	/* handle <ctrl-d> */
			// chop \n
			if (ans[strlen(ans)-1] == '\n') { ans[strlen(ans)-1] = '\0'; }
			// skip leading whitespaces
			while (isblank((int)*pans)) ++pans;
		switch (*pans) {
			case '\0':
				break;
			case '?':
			case 'h':
			{
				printf( "available commands:\n"
						"  ?,h:  this menu\n"
						"  r:    read JSON data\n"
						"  s:    load sample JSON data\n"
						"  p:    pretty print cached JSON data\n"
						"  f:    find JSON data\n"
//						"  t:    traverse from a JSON item\n"
						"  c:    create JSON data\n"
						"  u:    update JSON item\n"
						"  d:    delete JSON item\n"
						"  e:    detach JSON item\n"
						"  k:    rename JSON key\n"
						"  q,x:  exit\n"
						);
				if (gCliLevel==0) { printf(
						"  a:    setup JSON array (trial)\n"
						"  o:    setup JSON object (trial)\n"
						"  n:    show internal information\n"
						"  g:    set debug level\n"
						);
				}
				if (gCliLevel==0) { printf(
						"  z:    test different cJSONext_traverse implementation\n"
						"  y:    test cJSONext_deleteItem on different root\n"
						"  w:    test cJSONext_createItem on different root\n"
						"  v:    test cJSONext_updateItem on different root\n"
						);
				}
				break;
			}
			case 'r':
			{
				cnt = sscanf(ans, "%s %[^\n]s", cmd, param);
				if ((cnt > 1) && (param[0] == '?')) {
					// quick help
					printf("usage: %c [filename]\n", *pans);
					break;
				}
				printf("read...\n");
				if (cnt > 1) {
					// check file size against buffer size
					if (getFileSize(param) > sizeof(objBuffer)) {
						printf("Oops! [%s]: file size larger than internal buffer (%d bytes)\n", param, (int)sizeof(objBuffer));
						break;
					}
					// read from file
					FILE *fp = fopen(param, "r");
					if (fp == NULL) {
						printf("%s: cannot open file\n", param);
						break;
					}
						// read every byte upto sizeof(objBuffer)
					cnt = fread(objBuffer, 1, sizeof(objBuffer), fp);
					if (cnt<=0) {
						printf("%s: failed reading from file\n", param);
						break;
					}
					fclose(fp);
				}
				else {
					// read from stdin
					char ch;
					cnt=0;
					while ((ch = fgetc(stdin)) != EOF && cnt < sizeof(objBuffer)) {
						objBuffer[cnt++] = ch;
					}
					clearerr(stdin);			/* handle EOF */
					if (cnt==0) { break; }
					if (cnt==sizeof(objBuffer)) {
						printf("Oops! Run of input buffer (buffer size: %d)\n", (int)sizeof(objBuffer));
						break;
					}
				}
				objBuffer[cnt] = 0;
				if (objBuffer[0] == 0) { break; }
				// now build the JSON tree
					// release previous JSON tree if any
					if (root) { cJSON_Delete(root); }
				root = cJSON_Parse(objBuffer);
				if (!root) {
					printf("cJSON_Parse() failed. Error before: [%s]\n", cJSON_GetErrorPtr());
				}
				break;
			}
			case 's':
			{
				int idx = 0;
				cnt = sscanf(ans, "%s %[^\n]s", cmd, param);
				if ((cnt > 1) && (param[0] == '?')) {
					// quick help
					printf("# of sample json data set: %d\n", (int)(sizeof(sample_JSON)/sizeof(char *)));
					break;
				}
				if (cnt > 1) {
					idx = atoi(param);
					idx %= sizeof(sample_JSON)/sizeof(char *);
				}
				printf("loading sample json data [%d]\n", idx);
				strcpy(objBuffer, sample_JSON[idx]);
				// now build the JSON tree
					// release previous JSON tree if any
					if (root) { cJSON_Delete(root); }
				root = cJSON_Parse(objBuffer);
				if (!root) {
					printf("cJSON_Parse() failed. Error before: [%s]\n", cJSON_GetErrorPtr());
				}
				break;
			}
			case 'p':
			{
				cnt = sscanf(ans, "%s %[^\n]s", cmd, param);
				if ((cnt > 1) && (param[0] == '?')) {
					// quick help
					printf("usage: %c [objPath]\n", *pans);
					break;
				}
				printf("pprint%s\n", (root==NULL)?": no JSON data":"");
				cnt = sscanf(ans, "%s %[^\r]s", cmd, param);
				char *out;
				if (cnt <=1) {
					// print from root
					if (root==NULL) { break; }
					out = cJSON_Print(root);
				}
				else {
					// print from given subtree
					cJSON *item = cJSONext_getItem(root, param);
					if (item == NULL) {
						printf("%s: objPath not found\n", param);
						break;
					}
					out = cJSON_Print(item);
				}
				if (out) {
					printf("%s\n",out);
					free(out);
				}
				else {
					printf("empty JSON tree\n");
				}
				break;
			}
			case 'f':
			{
				if ((sscanf(ans, "%s %[^\r]s", cmd, param) > 1) && (param[0] == '?')) {
					// quick help
					printf("usage: %c obj_path\n", *pans);
					break;
				}
				if (root==NULL) { printf("no JSON data\n"); break; }
				cnt = sscanf(ans, "%s %[^\n]s", cmd, param);
				if (cnt <= 1) {
					printf("find (start with delimiter)? ");
					if (scanf("%[^\n]s", param) < 0)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					fgetc(stdin);		// chop \n
				}
					//* a sanity check for jsonCLI itself
					if ((strlen(param)==1) && isalnum(param[0])) {
						printf("please start with a valid delimiter [%c]\n", param[0]);
						break;
					}
				inline_check_find_result(cJSONext_getItem(root, param), param);
				break;
			}
			case 't':
			{
				if ((sscanf(ans, "%s %[^\r]s", cmd, param) > 1) && (param[0] == '?')) {
					// quick help
					printf("traversal: %c obj_path\n", *pans);
					printf("filtering: %c obj_path pattern\n", *pans);
					break;
				}
				if (root==NULL) { printf("no JSON data\n"); break; }
				char pattern[32];
				memset(pattern, 0, sizeof(pattern));
				cnt = sscanf(ans, "%s %s %[^\n]s", cmd, param, pattern);
				if (cnt <= 1) {
					printf("traverse (start with delimiter)? ");
					if (fgets(ans, sizeof(ans), stdin) == NULL)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					cnt = sscanf("%s %[^\n]s", param, pattern);
				}
				cJSONext_traverse_WithPath(cJSONext_getItem(root, param), callback_FilterJsonItem, (pattern[0]==0)?NULL:(void*)&pattern[0], NULL);
				break;
			}
			case 'c':
			{
			//
			// - create & insert a cJSON object into into a JSON object
			//
			// - three parameters are required
			//		-- an objPath			(e.g. .topLevel.2ndLevel.Level3)
			//		-- the object type		(e.g. cJSON_Number)
			//		-- the object value		(e.g. 8080)
			//
			//		For cJSON_Array and cJSON_Object, a helper function (setup_array/setup_object) is called to
			//		proper construct an array or an object for replaement
			//		// note: the helper function has its own interactive CLI in building the cJSON structure
			//
			//		For cJSON object types (Number & String), object value is obtained CLI input.
			//		For cJSON object types (True,False,NULL), object value is not needed.
			//
			//		cJSONext_createItem() is used to create the cJSON item for all cJSON object types
			//
			//	- example result:
			//		{ "topLevel" :  { "2ndLevel" : { "Level3" : 8080 }
			//		                }
			//		}
			//
				char objPath[64], type[4];
				cnt = sscanf(ans, "%s %s %s %[^\r]s", cmd, objPath, type, param);
				if ((cnt > 1) && (objPath[0] == '?')) {
					// quick help
					printf("usage: %c obj_path obj_type value\n", *pans);
					break;
				}
				if (cnt < 2) {
					printf("create (objectPath type(F|T|N|#|s|a|o) value)? ");
					if (fgets(ans, sizeof(ans), stdin) == NULL)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					cnt = sscanf(ans, "%s %s %[^\n]s", objPath, type, param);
					if (cnt < 2) {
						/* insufficient input */ break;
					}
					cnt += 1;			// add 'cmd' to total 'cnt'
				}
				else if (cnt < 3) {
					printf("create (type(F|T|N|#|s|a|o) value)? ");
					if (fgets(ans, sizeof(ans), stdin) == NULL)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					cnt = sscanf(ans, "%s %[^\n]s", type, param);
					cnt += 2;			// add 'cmd' & 'objPath' to total 'cnt'
				}
				// make sure root object exists
				if (root == NULL) {
					root=cJSON_CreateObject();	
				}
				// handle different object type
				switch (type[0]) {
					case 'a':
					case 'A':
					{
						cJSON *pObj = setup_array();
						if (pObj) {
							cJSONext_createItem(root, objPath, get_cJSON_TYPE(type[0]), pObj);
						}
						printf("JSON array setup %s\n", (pObj==NULL)?"not done":"completed");
						break;
					}
					case 'o':
					case 'O':
					{
						cJSON *pObj = setup_object();
						if (pObj) {
							cJSONext_createItem(root, objPath, get_cJSON_TYPE(type[0]), pObj);
						}
						printf("JSON object setup %s\n", (pObj==NULL)?"not done":"completed");
						break;
					}
					case '#':
					case 'i':
					case 's':
					case 'S':
						// make sure object value is provided
						if (cnt < 4) {
							printf("value? "); fflush(stdout);
							if (scanf("%[^\n]s", param) < 0)
								/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
							fgetc(stdin);		// chop \n
						}
						/* fall thru */
					case 'F':
					case 'f':
					case 'T':
					case 't':
					case 'N':
					case 'n':
					{
						cJSON *pObj = cJSONext_createItem(root, objPath, get_cJSON_TYPE(type[0]),
							// changed atoi >> atol for os/x
							(((type[0])=='#')||((type[0])=='i')) ? (void*)atol(param) : param);
						if (pObj) {
							printf("%s created\n", &objPath[1]);
						}
						else {
							printf("failed to create %s\n", &objPath[1]);
						}
						break;
					}
					// everything else...
					default:
						printf("%s: unsupported type\n", type);
						break;
				} /*switch (type[0])*/
				break;
			}
			case 'u':
			{
			//
			// - update a cJSON object with a different value
			//
			// - two parameters are required
			//		-- an objPath			(e.g. .topLevel.2ndLevel.Level3)
			//		-- the object value		(e.g. 8080)
			//
			//		For cJSON_Array and cJSON_Object, a helper function (setup_array/setup_object) is called to
			//		proper construct an array or an object for replaement
			//		// note: the helper function has its own interactive CLI in building the cJSON structure
			//
			//		cJSONext_updateItem() is used to update all cJSON object types
			//
				if ((sscanf(ans, "%s %[^\r]s", cmd, param) > 1) && (param[0] == '?')) {
					// quick help
					printf("usage: %c obj_path value\n", *pans);
					break;
				}
				printf("update%s\n", (root==NULL)?": no JSON data":"");
				if (root==NULL) { break; }
				char objPath[64];
				// collect JSON object path and data (the data applies only to certain JSON types)
				cnt = sscanf(ans, "%s %s %[^\r]s", cmd, objPath, param);
				if (cnt < 2) {
					printf("update (objPath [value])? ");
					if (fgets(ans, sizeof(ans), stdin) == NULL)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					cnt = sscanf(ans, "%s %[^\n]s", objPath, param);
					++cnt;			// add 'cmd' to total 'cnt'
				}
				// identify JSON object type
				cJSON *pObj = cJSONext_getItem(root, objPath);
				if (pObj == NULL) {
					// error handling
					printf("%s: objPath not found\n", objPath);
					break;
				}
					// acquire object value if not available for certain object types
				if ((cnt < 3) && ((pObj->type==cJSON_Number)||(pObj->type==cJSON_String)))
				{
					printf("update (value)? ");
					if (scanf("%[^\n]s", param) < 0)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					fgetc(stdin);		// chop \n
				}
					// set default object value if not available for certain object types
				if ((cnt < 3) && ((pObj->type==cJSON_True)||(pObj->type==cJSON_False)))
				{
					strcpy(param, (pObj->type==cJSON_True)?"true":"false");
				}
				// update cJSON object based on its type
				switch (pObj->type) {
					case cJSON_Object:
					case cJSON_Array:
					{
						cJSON *jroot = (pObj->type==cJSON_Object) ? setup_object() : setup_array();
						if (jroot) {
							inline_check_update_result(cJSONext_updateItem(root, objPath, pObj->type, (void*)jroot), objPath);
						}
						break;
					}
					case cJSON_True:
					case cJSON_False:
					{
							// changed int >> long for os/x
						long value = ((strcasecmp(param, "false")==0)||(strcasecmp(param, "f")==0)||(strcasecmp(param, "0")==0)) ? cJSON_False : cJSON_True;
						inline_check_update_result(cJSONext_updateItem(root, objPath, pObj->type, (void*)value), objPath);
						break;
					}
					case cJSON_NULL:
						printf("%s: cannot update a JSON NULL object\n", objPath);
						break;
					case cJSON_Number:
							// changed atoi >> atol for os/x
						inline_check_update_result(cJSONext_updateItem(root, objPath, pObj->type, (void*)atol(param)), objPath);
						break;
					case cJSON_String:
						inline_check_update_result(cJSONext_updateItem(root, objPath, pObj->type, (void*)param), objPath);
						break;
					default:
						printf("%d: unknown JSON type\n", (pObj->type));
				} /*switch (pObj->type)*/
				break;
			}
			case 'd':
			{
				if ((sscanf(ans, "%s %[^\r]s", cmd, param) > 1) && (param[0] == '?')) {
					// quick help
					printf("usage: %c obj_path\n", *pans);
					break;
				}
				if (root==NULL) { printf("no JSON data\n"); break; }
				cnt = sscanf(ans, "%s %[^\n]s", cmd, param);
				if (cnt <= 1) {
					printf("delete (start with delimiter)? ");
					if (scanf("%[^\n]s", param) < 0)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					fgetc(stdin);		// chop \n
				}
				cJSON *item = cJSONext_getItem(root, param);
				if (item == NULL) {
					// error handling
					printf("%s: objPath not found\n", param);
					break;
				}
				int idx = 0;
				int objType = item->type;			// [valgrind] cache item type as it may be free'd
				if (item->type == cJSON_Array) {
					int size = cJSON_GetArraySize(item);
					printf("which element (-1,0..%d)? ", (size-1));
					if (scanf("%d", &idx) < 0)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					fgetc(stdin);		// chop \n
					if (idx >= size) {
						printf("%s[%d] no such element\n", param, idx);
						break;
					}
					else if (idx < 0) {
						idx = -1;		// any negative value means entire array
					}
				}
				if (cJSONext_deleteItem(root, param, idx) == cJSONext_NoErr) {
					if ((objType == cJSON_Array) && (idx != -1)) {
						printf("%s[%d] deleted\n", param, idx);
					}
					else {
						printf("%s deleted\n", param);
					}
					// are we deleting the entire JSON tree?
					if (item == root) { root = NULL; }
				}
				else {
					printf("failed in deleting %s\n", param);
				}
				break;
			}
			case 'e':
			{
				char objPath[64];
				cnt = sscanf(ans, "%s %s", cmd, param);
				if ((cnt > 1) && (param[0] == '?')) {
					// quick help
					printf("usage: %c objPath\n", *pans);
					break;
				}
				if (root==NULL) { printf("no JSON data\n"); break; }
				if (cnt < 2) {
					printf("detach (objectPath)? ");
					if (fgets(ans, sizeof(ans), stdin) == NULL)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					cnt = sscanf(ans, "%s", param);
					if (cnt < 1) {
						/* insufficient input */ break;
					}
				}
	//#define	_local_detach_	1
	#ifdef	_local_detach_
				cJSON *item = NULL;
				if ((item = cJSONext_getItem(root, param)) == NULL) {
					printf("%s: objPath not found\n", param);
					break;
				}
				//
				cJSON *pDetached;
				if (strlen(param) == 1) {
					// detach entire object?
					if (item->prev) { item->prev->next = item->next; }
					if (item->next) { item->next->prev = item->prev; }
					item->prev = item->next = 0;
					pDetached = item;
				}
				else {
					char *ptr = strrchr(param, (int)param[0]); 
					if (ptr == param) {
						// only one separator found.  detach something under root
						item = root;
					}
					else {
						*ptr = 0;	// now param contains to parent path
						item = cJSONext_getItem(root, param);
						if (item->type != cJSON_Object) {
							printf("parent [%s] is NOT a cJSON object, cannot proceed\n", param);
							break;
						}
					}
					++ptr;		// now ptr points to the name of the item to be detached
					pDetached = cJSON_DetachItemFromObject(item, ptr);
				}
	#else
				cJSON *pDetached = cJSONext_detachItem(root, param, -1);
	#endif	//_local_detach_
				// pDetached canNOT be NULL, we've checked it already using cJSONext_getItem()
				// pDetached should be SAME AS item
				//
				callback_ShowJsonObject(pDetached, NULL);
				//
				char *out = cJSON_Print(pDetached);
				printf("detached cJSON item: %s\n", out);
				free(out);
				//
					// release detached item... (similar effect to cJSONext_deleteItem()
					cJSON_Delete(pDetached);
					// are we detaching the entire JSON tree?
					if (pDetached == root) { root = NULL; }
				break;
			}
			case 'k':
			{
				char objPath[64];
				cnt = sscanf(ans, "%s %s %[^\n]s", cmd, objPath, param);
				if ((cnt > 1) && (param[0] == '?')) {
					// quick help
					printf("usage: %c objPath new-name\n", *pans);
					break;
				}
				if (root==NULL) { printf("no JSON data\n"); break; }
				if (cnt < 2) {
					printf("rename (objectPath new-name)? ");
					if (fgets(ans, sizeof(ans), stdin) == NULL)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					cnt = sscanf(ans, "%s %[^\n]s", objPath, param);
					if (cnt < 2) {
						/* insufficient input */ break;
					}
					cnt += 1;			// add 'objPath' to total 'cnt'
				}
				else if (cnt < 3) {
					printf("rename %s (new-name)? ", objPath);
					if (fgets(ans, sizeof(ans), stdin) == NULL)
						/* handle <ctrl-d> */ { clearerr(stdin); fputc((int)'\n', stdout); break; }
					cnt = sscanf(ans, "%[^\n]s", param);
					cnt += 1;			// add 'param' to total 'cnt'
				}
	//#define	_local_rename_	1
	#ifdef	_local_rename_
				cJSON *item = cJSONext_getItem(root, objPath);
				if (item->string) {
					free(item->string);
					item->string=strdup(param);
				}
				else {
					printf("noname object (type:%s)\n", show_cJSON_TYPE(item->type));
				}
	#else
				int rc = cJSONext_renameItem(root, objPath, param);
				if (rc != cJSONext_NoErr) {
					printf("err: failed in rename %s (w/ %s) rc=%d\n", objPath, param, rc);
					break;
				}
				else { printf("renamed %s\n", objPath); }
	#endif	//_local_rename_
				break;
			}
		// _ROOT_OBJECT_
			case 'a':
			{
				if (gCliLevel!=0) { break; }
				printf("setup JSON array (trial)\n");
				cJSON *aroot = setup_array();
				printf("JSON array setup %s\n", (aroot==NULL)?"not done":"completed");
				if (aroot) {
					char *out = cJSON_Print(aroot);
					if (out) {
						printf("%s\n",out);
						free(out);
					}
					cJSON_Delete(aroot);
				}
				break;
			}
			case 'o':
			{
				if (gCliLevel!=0) { break; }
				printf("setup JSON object (trial)\n");
				cJSON *oroot = setup_object();
				printf("JSON object setup %s\n", (oroot==NULL)?"not done":"completed");
				if (oroot) {
					char *out = cJSON_Print(oroot);
					if (out) {
						printf("%s\n",out);
						free(out);
					}
					cJSON_Delete(oroot);
				}
				break;
			}
			case 'n':
			{
				if (gCliLevel!=0) { break; }
				char *out = NULL;
				printf("internal info:\n");
				printf("gDebug=%d\n", gDebug);
				printf("objBuffer size=%d\n", (int)sizeof(objBuffer));
				if (objBuffer[0]) { printf("gbuffer content:\n%s\n", objBuffer); }
				if (root)  { printf("cJSON *root:%p\n",  root); }
				break;
			}
		// _ROOT_OBJECT_
			case 'g':
			{
				if (gCliLevel!=0) { break; }
				setgDebugValue(ans);
				break;
			}
			case 'q':
			case 'x':
				(gCliLevel==0) ? printf("quit\n") : printf("quit object\n");
				objloop = 0;
				break;
			case ';':
				// echo
				printf("%s\n", ans);
				// fall thru
			case '#':
				// ignore comments
				break;
			case '!':
				// run shell command
				cnt = sscanf(ans, "%s %[^\n]s", cmd, param);
				if (cnt > 1) { system(param); }
				break;
			default:
				printf("unknown command: %s\n", ans);
				break;
		///////////////
			case 'm':
			{
				printf("%c: subtree traversal\n", *pans);
				char subtree[64];
				cnt = sscanf(pans, "%s %s %[^\r]s", cmd, subtree, param);
				if (cnt < 3) {
					// quick help
					printf("usage: z%c sub_tree obj_path\n", *pans);
					break;
				}
				cJSON *item = inline_get_subtree(root, subtree);
				if (item == NULL) { break; }
				cJSON *myItem = cJSONext_getItem(item, param);
				if (myItem && ((myItem->type==cJSON_Object) && (myItem->string==NULL)))
					printf("result: {cJSON root}\n");
				else {
					printf("result: %s\n", myItem?myItem->string:"not found");
					if (myItem) callback_ShowJsonObject(myItem, myItem?myItem->string:"not found");
				}
				break;
			}
		///////////////
			case 'z':
			{
				if (root==NULL) { break; }
				zCommand(++pans, &root);
				break;
			}
		///////////////
		}	/* switch (*pans) */
	}
	--gCliLevel;
	return root;
}

/// for 'zi' & 'zj' command
typedef struct user_data_array {
	char *subtree;	// subtree leads to array
	char *name;		// relative cJSON objPath
	int   type;		// cJSON object type
	void *value;	// object value
} user_data_array_t;
/// for 'zi' command
static int callback_ins_array(cJSON *object, void *pUserData)
{
	user_data_array_t *pData = (user_data_array_t *)pUserData;
	// quick check
	if (object->type != cJSON_Object) {
		printf("%s: not a cJSON object (type:%s) - skipped\n", object->string, show_cJSON_TYPE(object->type));
		return cJSON_Callback_Continue;
	}
	cJSON *pObj = cJSONext_createItem(object, pData->name, pData->type, pData->value);
//	printf("%s%s%s%s%s\n", (pObj==NULL)?"failed to create ":"", pData->subtree, object->string?object->string:"", pData->name, (pObj)?" created":"");
	if (pObj) {
		printf("%s%s%s created\n", pData->subtree, object->string?object->string:"", pData->name);
	}
	else {
		printf("failed to create %s%s%s\n", pData->subtree, object->string?object->string:"", pData->name);
	}
	return cJSON_Callback_Continue;
}
/// for 'zj' command
static int callback_del_array(cJSON *object, void *pUserData)
{
	user_data_array_t *pData = (user_data_array_t *)pUserData;
	// quick check
	if (object->type != cJSON_Object) {
		printf("%s: not a cJSON object (type:%s) - skipped\n", object->string, show_cJSON_TYPE(object->type));
		return cJSON_Callback_Continue;
	}
	int rc = cJSONext_deleteItem(object, pData->name, -1);
//	printf("%s%s%s%s%s\n", (rc!=cJSONext_NoErr)?"failed to delete ":"", pData->subtree, object->string?object->string:"", pData->name, (rc==cJSONext_NoErr)?" deleted":"");
	if (rc==cJSONext_NoErr) {
		printf("%s%s%s deleted\n", pData->subtree, object->string?object->string:"", pData->name);
	}
	else {
		printf("failed to delete %s%s%s\n", pData->subtree, object->string?object->string:"", pData->name);
	}
	return cJSON_Callback_Continue;
}

/* catch-all for all test commands */
static void zCommand(char *pans, cJSON **pRoot)
{
	//
	cJSON *root = *pRoot;
	char cmd[8], subtree[64], param[64];
	int  cnt;
	
		// apply this same check for all cases
	if (root==NULL) { printf("no JSON data\n"); return; }
	//
	switch (*pans) {
		///////////////
		case 'z':
		{
			printf("%c: exercise cJSONext_traverse[_WithPath] implementation\n", *pans);
			int count = 0;
//			cJSONext_traverse(root,callback_ShowJsonObject,&count);
			cJSONext_traverse_WithPath(root,callback_ShowJsonPathAndItemCount,&count,NULL);
			printf("%s: total item count=%d\n", __FUNCTION__, count);
			break;
		}
		case 'u':
		case 'v':
		{
			printf("%c: test cJSONext_updateItem on different root\n", *pans);
			char objPath[64];
			cnt = sscanf(pans, "%s %s %s %[^\r]s", cmd, subtree, objPath, param);
			if (cnt < 4) {
				// quick help
				printf("usage: z%c sub_tree obj_path value\n", *pans);
				break;
			}
			cJSON *item = inline_get_subtree(root, subtree);
			if (item == NULL) { break; }
			cJSON *pObj = cJSONext_getItem(item, objPath);
			if (pObj == NULL) {
				// error handling
				printf("[%s] %s: objPath not found\n", subtree, objPath);
				break;
			}
			// update cJSON object based on its type
			switch (pObj->type) {
				case cJSON_True:
				case cJSON_False:
				{
						// changed int >> long for os/x
					long value = ((strcasecmp(param, "false")==0)||(strcasecmp(param, "f")==0)||(strcasecmp(param, "0")==0)) ? cJSON_False : cJSON_True;
					inline_check_update_result(cJSONext_updateItem(item, objPath, pObj->type, (void*)value), objPath);
					break;
				}
				case cJSON_NULL:
					printf("%s: cannot update a JSON NULL object\n", objPath);
					break;
				case cJSON_Number:
						// changed atoi >> atol for os/x
					inline_check_update_result(cJSONext_updateItem(item, objPath, pObj->type, (void*)atol(param)), objPath);
					break;
				case cJSON_String:
					inline_check_update_result(cJSONext_updateItem(item, objPath, pObj->type, (void*)param), objPath);
					break;
				default:
					printf("%s: unsupported object type\n", show_cJSON_TYPE(pObj->type));
					break;
			}
			break;
		}
	///////////////
		case 'c':
		case 'w':
		{
			printf("%c: test cJSONext_createItem on different root\n", *pans);
			char objPath[64], type[4];
			cnt = sscanf(pans, "%s %s %s %s %[^\r]s", cmd, subtree, objPath, type, param);
			if (cnt < 5) {
				// quick help
				printf("usage: z%c sub_tree obj_path obj_type value\n", *pans);
				break;
			}
			cJSON *item = inline_get_subtree(root, subtree);
			if (item == NULL) { break; }
			// support only limited (simple) (simple) (simple) (simple) (simple) (simple) (simple) (simple) obj_type (for testing purpose)
			switch (type[0]) {
				case 'i':
				case 's':
				case 't':
				case 'f':
				case 'n':
				{
					cJSON *pObj = cJSONext_createItem(item, objPath, get_cJSON_TYPE(type[0]),
						// changed atoi >> atol for os/x
						(type[0]=='i') ? (void*)atol(param) : param);
					if (pObj) {
						printf("[%s] %s created\n", subtree, &objPath[1]);
					}
					else {
						printf("failed to create [%s] %s\n", subtree, &objPath[1]);
					}
					break;
				}
				default:
					printf("%c: unsupported object type\n", type[0]);
					break;
			}
			break;
		}
	///////////////
		case 'd':
		case 'y':
		{
			printf("%c: test cJSONext_deleteItem on different root\n", *pans);
			cnt = sscanf(pans, "%s %s %[^\r]s", cmd, subtree, param);
			if (cnt < 3) {
				// quick help
				printf("usage: z%c sub_tree obj_path\n", *pans);
				break;
			}
			cJSON *item = inline_get_subtree(root, subtree);
			if (item == NULL) { break; }
			else {
				// switch to use item as root
				if (cJSONext_deleteItem(item, param, -1) == cJSONext_NoErr) {
					printf("[%s] %s deleted\n", subtree, param);
					// are we deleting the entire JSON tree?	// 2015-03-16: must nullify the REAL root
					if ((item==root) && (strlen(param)==1)) { *pRoot = NULL; }
				}
				else {
					printf("failed in deleting %s%s\n", subtree, param);
				}
			}
			break;
		}
	///////////////
		case 'e':
		{
			printf("%c: test cJSONext_detachItem on different root\n", *pans);
			int idx = -1;
			cnt = sscanf(pans, "%s %s %s %d", cmd, subtree, param, &idx);
			if (cnt < 3) {
				// quick help
				printf("usage: z%c sub_tree obj_path [idx]\n", *pans);
				break;
			}
			cJSON *item = inline_get_subtree(root, subtree);
			if (item == NULL) { break; }
			else {
				// switch to use item as root
				cJSON *pDetached = cJSONext_detachItem(item, param, idx);
				if (pDetached) {
					printf("[%s] %s detached\n", subtree, param);
					//
					callback_ShowJsonObject(pDetached, NULL);
					//
					char *out = cJSON_Print(pDetached);
					printf("detached cJSON item: %s\n", out);
					free(out);
						// release detached item... (similar effect to cJSONext_deleteItem()
						cJSON_Delete(pDetached);
						// are we detaching the entire JSON tree?	// 2015-03-21: must nullify the REAL root
						if ((pDetached==root) && (strlen(param)==1)) { *pRoot = NULL; }
				}
				else {
					printf("failed in detaching [%s]%s\n", subtree, param);
				}
			}
			break;
		}
	///////////////
#if	defined(_local_getItem_) 
		case 'm':
		{
			printf("%c: test different cJSONext_getItem implementation\n", *pans);
			cnt = sscanf(pans, "%s %s %[^\r]s", cmd, subtree, param);
			if (cnt < 3) {
				// quick help
				printf("usage: z%c sub_tree obj_path\n", *pans);
				break;
			}
			cJSON *item = inline_get_subtree(root, subtree);
			if (item == NULL) { break; }
			cJSON *myItem = mmy_getItem(item, param);
			if (myItem && ((myItem->type==cJSON_Object) && (myItem->string==NULL)))
				printf("result: {cJSON root}\n");
			else {
				printf("result: %s\n", myItem?myItem->string:"not found");
				if (myItem) callback_ShowJsonObject(myItem, myItem?myItem->string:"not found");
			}
			break;
		}
#endif	//defined(_local_getItem_) 
	///////////////
		case 't':
		{
			printf("%c: multi-level subtree traversal\n", *pans);
			cnt = sscanf(pans, "%s %[^\n]s", cmd, param);
			if ((cnt <=1) || ((cnt > 1) && (param[0] == '?'))) {
				// quick help
				printf("z%c subtree [ [objPath] ...]\n", *pans);
				break;
			}
			cJSON *item = root;
			char *ptr = strtok(param, " \t");
			while (ptr) {
				cJSON *myItem = cJSONext_getItem(item, ptr);
				if (myItem) { callback_ShowJsonObject(myItem, NULL); }
				ptr = strtok(NULL, " \t");
				item = myItem;
			}
			break;
		}
	///////////////
		case 'i':		// insert
		{
			printf("%c: insert specified key to the array\n", *pans);
			char objPath[64], type[4];
			cnt = sscanf(pans, "%s %s %s %s %[^\r]s", cmd, subtree, objPath, type, param);
			if (cnt < 5) {
				// quick help
				printf("usage: z%c path_to_array obj_name obj_type value\n", *pans);
				break;
			}
			cJSON *item = cJSONext_getItem(root, subtree);
			if (item == NULL) {
				printf("%s: path_to_array not found\n", subtree);
				break;
			}
			else if (item->type != cJSON_Array) {
				printf("%s: path_to_array not a cJSON array\n", subtree);
				break;
			}
			// prepare user data
			user_data_array_t userData;
			userData.subtree = subtree;
			userData.name = objPath;
			//	support only limited (simple) (simple) (simple) (simple) (simple) (simple) (simple) (simple) obj_type (for testing purpose)
			switch (type[0]) {
				case 'i':
				case 's':
				case 't':
				case 'f':
				case 'n':
				{
					userData.type = get_cJSON_TYPE(type[0]);
					userData.value = (type[0]=='i') ? (void*)atol(param) : (void *)param;	// changed atoi >> atol for os/x
					//
					// visit every objects in the array
					cJSONext_traverseArray(item, callback_ins_array, (void *)&userData);
					//
					printf("array size: %d\n", cJSON_GetArraySize(item));
					break;
				}
				default:
					printf("%c: unsupported object type\n", type[0]);
					break;
			}
			break;
		}
		case 'j':		// junk (i.e. remove)
		{
			printf("%c: remove specified key from the array\n", *pans);
			char objPath[64];
			cnt = sscanf(pans, "%s %s %[^\r]s", cmd, subtree, objPath);
			if (cnt < 3) {
				// quick help
				printf("usage: z%c path_to_array obj_name\n", *pans);
				break;
			}
			cJSON *item = cJSONext_getItem(root, subtree);
			if (item == NULL) {
				printf("%s: path_to_array not found\n", subtree);
				break;
			}
			else if (item->type != cJSON_Array) {
				printf("%s: path_to_array not a cJSON array\n", subtree);
				break;
			}
			// prepare user data
			user_data_array_t userData;
			userData.subtree = subtree;
			userData.name = objPath;
			userData.type = 0;
			userData.value = NULL;
			//
			// visit every objects in the array
			cJSONext_traverseArray(item, callback_del_array, (void *)&userData);
			//
			printf("array size: %d\n", cJSON_GetArraySize(item));
			break;
		}
	///////////////
		case '?':
			printf("z commands:\n"
					"zj: array element deleteion\n"
					"zi: array element insertion\n"
					"zt: subtree traversal\n"
//					"zm: getItem\n"
					"ze:    detach\n"
					"zd/zy: delete\n"
					"zc/zw: create\n"
					"zu/zv: update\n"
					"zz: root traversal\n"
				  );
			break;
		default:
			printf("unknown z command: z%s\n", pans);
			break;
	} /* switch (*pans) */
}

