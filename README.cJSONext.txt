#------------------------------------
 cJSONext:	cJSON extension library
#------------------------------------

# cJSON is based on open source available at
	http://sourceforge.net/projects/cjson/
	https://github.com/kbranigan/cJSON	(commit: d975346351dc69d6d0241c4122f624b53d4d23be)

# cJSONext is an extension based on cJSON

	- see cJSONext.h for list of added utility functions
	- include cJSONext.h gets both cJSON and cJSONext functions

	- cJSONext add 'objPath' concept which allows easy manipulation of cJSON object

# objPath:

	- Use a concatenation of keys to locate any cJSON item within a cJSON object
	- The FIRST character of objPath is treated as 'delimiter' to separate keys
	- Be very careful, there is no restriction on delimiter character.  I.e.
	  '/' (slash) or '.' (dot) are legitmate delimiters, so does the 'A' character

# example

	see jsonCLI.c
	see also doc/cJSONext_intro.pptx

