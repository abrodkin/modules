/*****
 ** ** Module Header ******************************************************* **
 ** 									     **
 **   Modules Revision 3.0						     **
 **   Providing a flexible user environment				     **
 ** 									     **
 **   File:		cmdAlias.c					     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Authors:	John Furlan, jlf@behere.com				     **
 **		Jens Hamisch, jens@Strawberry.COM			     **
 **		R.K. Owen, <rk@owen.sj.ca.us> or <rkowen@nersc.gov>	     **
 ** 									     **
 **   Description:	The Tcl set-alias command			     **
 ** 									     **
 **   Exports:		cmdSetAlias					     **
 ** 									     **
 **   Notes:								     **
 ** 									     **
 ** ************************************************************************ **
 ****/

/** ** Copyright *********************************************************** **
 ** 									     **
 ** Copyright 1991-1994 by John L. Furlan.                      	     **
 ** see LICENSE.GPL, which must be provided, for details		     **
 ** 									     ** 
 ** ************************************************************************ **/

static char Id[] = "@(#)$Id: cmdAlias.c,v 1.8.2.2 2009/09/01 19:12:16 rkowen Exp $";
static void *UseId[] = { &UseId, Id };

/** ************************************************************************ **/
/** 				      HEADERS				     **/
/** ************************************************************************ **/

#include "modules_def.h"

/** ************************************************************************ **/
/** 				  LOCAL DATATYPES			     **/
/** ************************************************************************ **/

/** not applicable **/

/** ************************************************************************ **/
/** 				     CONSTANTS				     **/
/** ************************************************************************ **/

/** not applicable **/

/** ************************************************************************ **/
/**				      MACROS				     **/
/** ************************************************************************ **/

/** not applicable **/

/** ************************************************************************ **/
/** 				    LOCAL DATA				     **/
/** ************************************************************************ **/

static	char	module_name[] = __FILE__;

/** ************************************************************************ **/
/**				    PROTOTYPES				     **/
/** ************************************************************************ **/

/** not applicable **/


/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		cmdSetAlias					     **
 ** 									     **
 **   Description:	Callback function for (re)setting aliases	     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	ClientData	 client_data			     **
 **			Tcl_Interp	*interp		According Tcl interp.**
 **			int		 objc		Number of arguments  **
 **			Tcl_Obj		*objv[]		Argument array	     **
 ** 									     **
 **   Result:		int	TCL_OK		Successful completion	     **
 **				TCL_ERROR	Any error		     **
 ** 									     **
 **   Attached Globals:	g_flags		These are set up accordingly before  **
 **					this function is called in order to  **
 **					control everything		     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

int cmdSetAlias(
	ClientData client_data,
	Tcl_Interp * interp,
	int objc,
	Tcl_Obj * CONST84 objv[]
) {

	char           *arg0, *arg1, *arg2;

    /**
     **  Whatis mode?
     **/
	if (g_flags & (M_WHATIS | M_HELP))
		return (TCL_OK);	/** ------- EXIT PROCEDURE -------> **/
    /**
     **  Parameter check. Valid commands are:
     **
     **     unset-alias <alias>
     **     set-alias <alias> <value>
     **/
	arg0 = Tcl_GetString(objv[0]);
	if ((!strncmp(arg0, "un", 2) && (objc != 2)) ||
	    (!strncmp(arg0, "set", 3) && (objc != 3))) {
		if (OK != ErrorLogger(ERR_USAGE, LOC, arg0, "variable", NULL))
			return (TCL_ERROR); /** ------ EXIT (FAILURE) -----> **/
	}
    /**
     **  Display only mode?
     **/
	if (g_flags & M_DISPLAY) {
		fprintf(stderr, "%s\t ", Tcl_GetString(*objv++));
		while (--objc)
			fprintf(stderr, "%s ", Tcl_GetString(*objv++));
		fprintf(stderr, "\n");
		return (TCL_OK);	/** -------- EXIT PROCEDURE -------> **/
	}
    /**
     **  Switch command
     **/
	arg1 = Tcl_GetString(objv[1]);
	arg2 = Tcl_GetString(objv[2]);
	if (g_flags & M_SWSTATE1) {
		mhash_add(markAliasHashTable, arg1, M_SWSTATE1);
		return (TCL_OK);	/** -------- EXIT PROCEDURE -------> **/
	} else if (g_flags & M_SWSTATE2) {
		mhash_add(markAliasHashTable, arg1, M_SWSTATE2);
	} else if (g_flags & M_SWSTATE3) {
		intptr_t	marked_val;
		if ((marked_val =
		     (intptr_t) mhash_value(markAliasHashTable, arg1))) {
			if (marked_val == M_SWSTATE1)
				mhash_add(aliasUnsetHashTable, arg1, arg2);
			else
				return (TCL_OK);    /** -- EXIT PROCEDURE -> **/
		}
	} else if (g_flags & M_REMOVE) {
		mhash_add(aliasUnsetHashTable, arg1, arg2);
	}
    /**
     **  Finally remove or set the alias
     **/
	if (*arg0 == 'u' || (g_flags & M_REMOVE))
		mhash_add(aliasUnsetHashTable, arg1, arg2);
	else
		mhash_add(aliasSetHashTable, arg1, arg2);

	return (TCL_OK);

} /** End of 'cmdSetAlias' **/
