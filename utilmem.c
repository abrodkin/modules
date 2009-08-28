/*****
 ** ** Module Header ******************************************************* **
 ** 									     **
 **   Modules Revision 3.0						     **
 **   Providing a flexible user environment				     **
 ** 									     **
 **   File:		utilmem.c					     **
 **   First Edition:	2009/08/28					     **
 ** 									     **
 **   Authors:	R.K. Owen, <rk@owen.sj.ca.us> or <rkowen@nersc.gov>	     **
 ** 									     **
 **   Description:	Broke out the memory handling routines used by	     **
 **			the module code.				     **
 ** 									     **
 **   Exports:		module_malloc					     **
 **			module_realloc					     **
 **			module_calloc					     **
 **			null_free					     **
 **									     **
 ** ************************************************************************ **
 ****/

/** ** Copyright *********************************************************** **
 ** 									     **
 ** Copyright 2000-2009 by R.K. Owen		                      	     **
 ** see LICENSE.LGPL, which must be provided, for details		     **
 ** 									     ** 
 ** ************************************************************************ **/

static char Id[] = "@(#)$Id: utilmem.c,v 1.1.2.1 2009/08/28 14:54:41 rkowen Exp $";
static void *UseId[] = { &UseId, Id };

/** ************************************************************************ **/
/** 				      HEADERS				     **/
/** ************************************************************************ **/

#include "modules_def.h"

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		module_malloc					     **
 ** 									     **
 **   Description:	A wrapper for the system malloc() function,	     **
 ** 			so the argument can be tested and set to a positive  **
 ** 			value.						     **
 ** 									     **
 **   First Edition:	2007/02/14					     **
 ** 									     **
 **   Parameters:	size_t	size		Number of bytes to allocate  **
 ** 									     **
 **   Result:		void    *		An allocated memory pointer  **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

#define USE_CKALLOC

void *module_malloc(size_t size) {

#ifdef USE_CKALLOC
	Tcl_ValidateAllMemory(__FILE__,__LINE__);
	return ckalloc(size > 0 ? size : 1);
#else
	return malloc(size > 0 ? size : 1);
#endif

} /** End of 'module_malloc' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		module_realloc					     **
 ** 									     **
 **   Description:	A wrapper for the system realloc() function,	     **
 ** 			so the argument can be tested and set to a positive  **
 ** 			value.						     **
 ** 									     **
 **   First Edition:	2009/08/22					     **
 ** 									     **
 **   Parameters:	char *	ptr		Memory to reallocate	     **
 **   			size_t	size		Number of bytes to allocate  **
 ** 									     **
 **   Result:		void    *		An allocated memory pointer  **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

void *module_realloc(void *ptr, size_t size) {

#ifdef USE_CKALLOC
	return ckrealloc(ptr, size > 0 ? size : 1);
#else
	return realloc(ptr, size > 0 ? size : 1);
#endif

} /** End of 'module_realloc' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		module_calloc					     **
 ** 									     **
 **   Description:	A wrapper for the system calloc() function,	     **
 ** 			so the argument can be tested and set to a positive  **
 ** 			value.						     **
 ** 									     **
 **   First Edition:	2009/08/22					     **
 ** 									     **
 **   Parameters:	size_t	size		Number of bytes to allocate  **
 ** 									     **
 **   Result:		void    *		An allocated memory pointer  **
 ** 									     **
 **   Parameters:	size_t	nmemb		Number of bytes of member    **
 **   			size_t	size		Number of bytes to allocate  **
 ** 									     **
 **   Result:		void    *		An allocated memory pointer  **
 ** 									     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

void *module_calloc(size_t nmemb, size_t size) {

	void * ptr = NULL;

	size = (size > 0 ? size : 1);
	ptr = module_malloc(nmemb * size);

	return memset(ptr, '\0', nmemb * size);

} /** End of 'module_calloc' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		null_free					     **
 ** 									     **
 **   Description:	does a free and then nulls the pointer.		     **
 ** 									     **
 **   First Edition:	2000/08/24					     **
 ** 									     **
 **   parameters:	void	**var		allocated memory	     **
 ** 									     **
 **   result:		void    		(nothing)		     **
 ** 									     **
 **   attached globals:	-						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

void null_free(void ** var) {

	if (! *var) return;	/* passed in a NULL ptr */

#ifdef USE_FREE
#  ifdef USE_CKALLOC
	ckfree( *var);
#  else
	free( *var);
#  endif
#endif
	*var = NULL;

} /** End of 'null_free' **/
