/*****
 ** ** Module Header ******************************************************* **
 ** 									     **
 **   Modules Revision 3.0						     **
 **   Providing a flexible user environment				     **
 ** 									     **
 **   File:		cmdVersion.c					     **
 **   First Edition:	1995/12/28					     **
 ** 									     **
 **   Authors:	Jens Hamisch, jens@Strawberry.COM			     **
 **		R.K. Owen, <rk@owen.sj.ca.us> or <rkowen@nersc.gov>	     **
 ** 									     **
 **   Description:	The Tcl module-version routine which provides the    **
 **			definition of symbolic version names and the module- **
 **			alias command providing the definition of module and **
 **			version aliases					     **
 ** 									     **
 **   Exports:		cmdModuleVersion				     **
 **			cmdModuleAlias					     **
 **			CleanupVersion					     **
 **			AliasLookup					     **
 **			ExpandVersions					     **
 ** 									     **
 **   Notes:	This module defines the callback functions for the defi-     **
 **		nition of symbolic module names and module aliases. The      **
 **		syntax of the according commands is defined as:		     **
 ** 									     **
 **	    Module-Versions:						     **
 **		module-version <module>/<version> <name> [ <name> ... ]	     **
 **		module-version ./<version> <name> [ <name> ... ]	     **
 **		module-version /<version> <name> [ <name> ... ]		     **
 **		module-version <module> <name> [ <name> ... ]		     **
 **		module-version <alias> <name> [ <name> ... ]		     **
 ** 									     **
 **	    Module-Alias:						     **
 **		module-alias <alias> <module>/<version>			     **
 **		module-alias <alias> /<version>				     **
 **		module-alias <alias> <module>				     **
 **		module-alias <alias> <alias>				     **
 ** 									     **
 ** 									     **
 ** ************************************************************************ **
 ****/

/** ** Copyright *********************************************************** **
 ** 									     **
 ** Copyright 1991-1994 by John L. Furlan.                      	     **
 ** see LICENSE.GPL, which must be provided, for details		     **
 ** 									     ** 
 ** ************************************************************************ **/

static char Id[] = "@(#)$Id$";
static void *UseId[] = { &UseId, Id };

/** ************************************************************************ **/
/** 				      HEADERS				     **/
/** ************************************************************************ **/

#include "modules_def.h"

/** ************************************************************************ **/
/** 				  LOCAL DATATYPES			     **/
/** ************************************************************************ **/

/** ************************************************************************ **/
/**									     **/
/**   The whole thing is handled in memory. The structure is built of module **/
/**   and name records. There are 3 types of name records: version, name     **/
/**   and alias. 							     **/
/**									     **/
/**         |                                                    |	     **/
/**     +---+---+<-------------------------------+           +---+---+	     **/
/**     | module| ------------------------+      |           | alias |	     **/
/**     +---+---+ --------+               |      |           +-------+	     **/
/**         |   ^    +----+----+          |      |               |	     **/
/**         |   +--- | version |       +--+--+   |           +---+---+	     **/
/**         |   |    +----+----+       | name|---+     <-----| alias |	     **/
/**         |   |         |            +--+--+   |           +---+---+	     **/
/**         |   |    +----+----+          |      |               |	     **/
/**         |   +--- | version |------>+--+--+   |           +---+---+	     **/
/**         |   |    +----+----+<-+--- | name|---+           | alias |	     **/
/**         |   |         |       | +- +--+--+   |           +---+---+	     **/
/**         |   |    +----+----+  | |     |      |               |	     **/
/**         |   +--- | version |  | |  +--+--+   |           +---+---+	     **/
/**         |   |    +----+----+  | |  | name|---+     <-----| alias |	     **/
/**         |   |         |       | |  +--+--+   |           +---+---+	     **/
/**         |                     | |     |      |               |	     **/
/**         |                     | +->+--+--+   |           +---+---+	     **/
/**     +---+---+                 +--- | name|---+           | alias |	     **/
/**     | module|                      +--+--+   |           +---+---+	     **/
/**     +---+---+                         |      |               |	     **/
/**         |								     **/
/**			       alphabetic ordered     alphabtic ordered	     **/
/**			    list of names depending    list of aliases	     **/
/**			    on a single module file			     **/
/**									     **/
/**   Each module name points to a list of symbolic names and versions.	     **/
/**   The versions themselves can be symbolic names and therefore are of the **/
/**   same record type as the names.					     **/
/**   The name and the version list are alphabetically sorted (even the      **/
/**   module list is). A version record points to a related name record	     **/
/**   containing a symbolic name for the version. Starting at this record,   **/
/**   the name records build a queue of symbolic names for the version.	     **/
/**   Both, the version and the name record, do have a backward pointer to   **/
/**   the module record.						     **/
/**									     **/
/**   The alias list builds a alphabetic ordered list of defined aliases.    **/
/**   Each alias record points to the related name record.		     **/
/**									     **/
/** ************************************************************************ **/

typedef	struct	_mod_module	{
    struct _mod_module	*next;		/** alphabetic queue		     **/
    struct _mod_name	*version;	/** version queue   		     **/
    struct _mod_name	*name;		/** name queue      		     **/
    char		*module;	/** the name itself		     **/
} ModModule;

typedef	struct	_mod_name	{
    struct _mod_name	*next;		/** alphabetic queue		     **/
    struct _mod_name	*ptr;		/** logical next    		     **/
    struct _mod_name	*version;	/** backwards version pointer	     **/
    struct _mod_module	*module;	/** related module  		     **/
    char		*name;		/** the name itself		     **/
} ModName;

/** ************************************************************************ **/
/** 				     CONSTANTS				     **/
/** ************************************************************************ **/

#define	HISTTAB	100

/** ************************************************************************ **/
/**				      MACROS				     **/
/** ************************************************************************ **/

/** not applicable **/

/** ************************************************************************ **/
/** 				    LOCAL DATA				     **/
/** ************************************************************************ **/

static	char	module_name[] = __FILE__;

/**
 **  The module and aliases list
 **/

static	ModModule	*modlist = (ModModule *) NULL;
static	ModName		*aliaslist = (ModName *) NULL;

/** ************************************************************************ **/
/**				    PROTOTYPES				     **/
/** ************************************************************************ **/

static	void		 CleanupVersionSub(	ModModule	 *ptr);

static	void		 CleanupName(		ModName		 *ptr);

static	ModModule	*AddModule(		char		 *name);

static	ModModule	*FindModule(		char		 *name,
						ModModule	**prev);

static	ModName		*AddName(		char		 *name,
						ModName		**start,
						ModModule	 *module);

static	ModName		*FindName(		char		 *name,
						ModName		 *start,
						ModName		**prev);

static	char		*CheckModuleVersion(	char 		 *name);

static	char		*scan_versions(		char 		 *buffer,
						char		 *base,
						ModName 	 *ptr,
						ModModule 	 *modptr);

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		cmdModuleVersion				     **
 ** 									     **
 **   Description:	Callback function for 'version'			     **
 ** 									     **
 **   First Edition:	1995/12/28					     **
 ** 									     **
 **   Parameters:	ClientData	 client_data			     **
 **			Tcl_Interp	*interp		According Tcl interp.**
 **			int		 objc		Number of arguments  **
 **			Tcl_Obj		*objv[]		Argument array	     **
 ** 									     **
 **   Result:		int	TCL_OK		Successful completion	     **
 **				TCL_ERROR	Any error		     **
 ** 									     **
 **   Attached Globals:	modlist		List containing all version names    **
 **   			g_flags		These are set up accordingly before  **
 **					this function is called in order to  **
 **					control everything		     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

int cmdModuleVersion(
	ClientData client_data,
	Tcl_Interp * interp,
	int objc,
	Tcl_Obj * CONST84 objv[]
) {
	char           *version, *module;
	ModModule      *modptr;
	ModName        *versptr, *nameptr, *tmp, *ptr;
	int             i;
    /**
     **  Whatis mode?
     **/
	if (g_flags & M_WHATIS)
		return (TCL_OK);	/** ------- EXIT PROCEDURE -------> **/
    /**
     **  Parameter check
     **/
	if (objc < 3) {
		if (OK != ErrorLogger(ERR_USAGE, LOC, Tcl_GetString(objv[0]),
				" modulename or .",
				" symbolic-version [symbolic-version ...] ",
				NULL))
			return (TCL_ERROR); /** ------ EXIT (FAILURE) -----> **/
	}

	if (!(module = CheckModuleVersion((char *)Tcl_GetString(objv[1])))) {
		ErrorLogger(ERR_BADMODNAM, LOC, Tcl_GetString(objv[1]), NULL);
		return (TCL_ERROR);	/** -------- EXIT (FAILURE) -------> **/
	}
    /**
     **  Display mode?
     **/
	if (g_flags & M_DISPLAY) {
		fprintf(stderr, "%s\t ", Tcl_GetString(objv[0]));
		for (i = 1; i < objc; i++)
			fprintf(stderr, "%s ", Tcl_GetString(objv[i]));
		fprintf(stderr, "\n");
	}
    /**
     **  get the version from the argument
     **/
	if (!(version = strrchr(module, *psep))) {
		if (OK != ErrorLogger(ERR_INTERAL, LOC, NULL))
			return (TCL_ERROR); /** ------ EXIT (FAILURE) -----> **/
	}
	*version++ = '\0';
    /**
     **  Now we have a module and a version.
     **  Check whether it exists (cond. create them). Check both, the version
     **  and the name queue in order to locate the desired version ...
     **/
	if (!(modptr = AddModule(module))) {
		ErrorLogger(ERR_BADMODNAM, LOC,Tcl_GetString(objv[1]), NULL);
		return (TCL_ERROR);	    /** ------ EXIT (FAILURE) -----> **/
	}

	if (!(ptr = FindName(version, modptr->version, &tmp))) {
		if (!(ptr = FindName(version, modptr->name, &tmp)))
			versptr = AddName(version, &modptr->version, modptr);
		else
			versptr = ptr->version;
	} else
		versptr = ptr;
    /**
     **  Check all symbolic names now and allocate a name record for them
     **/
	for (i = 2; i < objc; i++) {
		if (	strcmp(Tcl_GetString(objv[i]), _(em_default))
		&&	FindName(Tcl_GetString(objv[i]), modptr->name, &tmp)) {
			if (OK != ErrorLogger(ERR_DUP_SYMVERS, LOC,
					      Tcl_GetString(objv[i]), NULL))
				break;
			else
				continue;
		}
		if (!(nameptr = AddName((char *)Tcl_GetString(objv[i]),
						 &modptr->name, modptr))) {
			if (OK != ErrorLogger(ERR_INTERAL, LOC, NULL))
				break;
			else
				continue;
		}

	/**
	 **  Concat the new element at the beginning of the name queue ...
	 **/
		nameptr->ptr = versptr->ptr;
		versptr->ptr = nameptr;
		nameptr->version = versptr;
	}

	return (TCL_OK);

} /** End of 'cmdModuleVersion' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		ExpandVersions					     **
 ** 									     **
 **   Description:	Callback function for 'version'			     **
 ** 									     **
 **   First Edition:	1995/12/28					     **
 ** 									     **
 **   Parameters:	char	*name		Name to be expanded	     **
 ** 									     **
 **   Result:		char*	NULL		No symbols found	     **
 **				Otherwise	Pointer to the list string   **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

char	*ExpandVersions( char	*name)
{
    char	*version, *module, *s;
    static char	 buffer[ BUFSIZ];
    ModModule	*modptr, *tmp1;
    ModName	*ptr, *tmp2;

    /**
     **  Parameter check
     **/

    if(!(module = CheckModuleVersion( name))) 
	return((char *) NULL );		/** -------- EXIT (FAILURE) -------> **/

    if(!(version = strrchr( module, *psep))) {
	if( OK != ErrorLogger( ERR_INTERAL, LOC, NULL))
	    return((char *) NULL );	/** -------- EXIT (FAILURE) -------> **/
    }

    *version++ = '\0';

    /**
     **  Now we have a module and a version.
     **  Check wheter it exists
     **/

    if(!(modptr = FindModule( module, &tmp1)))
	return((char *) NULL );		/** -------- EXIT (FAILURE) -------> **/

    if(!(ptr = FindName( version, modptr->version, &tmp2))) {
	if(!(ptr = FindName( version, modptr->name, &tmp2))) 
	    return((char *) NULL );	/** -------- EXIT (FAILURE) -------> **/
	ptr = ptr->version;
    }

    if( !ptr->ptr)
	return((char *) NULL );		/** -------- EXIT (FAILURE) -------> **/

    /**
     **  Now scan in all the symbolic version names
     **/
 
    *buffer = '\0';
    if( (s = scan_versions( buffer, buffer, ptr->ptr, modptr)) ) 
	*--s = '\0';			/** remove trailing ':'		     **/

    return( buffer);

} /** End of 'ExpandVersions' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		scan_versions					     **
 ** 									     **
 **   Description:	Scan all symbolic versions pointed to be the passed  **
 **			ModName pointer and print them as a list into the    **
 **			passed buffer.					     **
 ** 									     **
 **   First Edition:	1995/12/28					     **
 ** 									     **
 **   Parameters:	char	*buffer		Buffer for printing in	     **
 **			ModName	*ptr		Name structure pointer	     **
 **			ModModule  *modptr	Assigned module name	     **
 ** 									     **
 **   Result:		char*	NULL		Nothing printed into the     **
 **						buffer			     **
 **				Otherwise	Pointer to the end of the    **
 **						string in the buffer	     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static	char	*scan_versions( char		 *buffer,
				char		 *base,
				ModName		 *ptr,
				ModModule	 *modptr)
{
    ModName     *tmp, *vers;
    char 	*s;
    char 	*mayloop;

    /**
     **  Recursively print the queue of names
     **/

    if( !ptr)
	return((char *) NULL);			/** ------ EXIT (END) -----> **/

    if( !ptr->name) {
	if( OK != ErrorLogger( ERR_INTERAL, LOC, NULL))
	    return((char *) NULL);		/** ---- EXIT (FAILURE) ---> **/

    } else {

	/**
	 **  Prevent endless loops
	 **  To allow for version names that are substrings of other
	 **  version names, match against "(^|:)name:" not just "name"... 
	 **/
	mayloop = strstr( base, ptr->name);
	if( mayloop != NULL ) {
	    if( mayloop == base || *(mayloop-1) == ':' ) {
		if( *(mayloop + strlen(ptr->name)) == ':' ) {
		    if (strcmp(ptr->name,_(em_default)))
	    		ErrorLogger( ERR_SYMLOOP, LOC, ptr->name, NULL);
		    return((char *) NULL);	/** ---- EXIT (FAILURE) ---> **/
		}
	    }
	}

	/**
	 **  Now print ...
	 **/

	/* sprintf( buffer, "%s:", ptr->name); */
	strcpy( buffer, ptr->name);
	/* if you change this, may affect the loop checker above */
	strcat( buffer, ":");
	buffer += strlen( buffer);

	/**
	 **  Check wheter this is a version name again ...
	 **  This is a recursion, too
	 **/

	if((ModName *) NULL != (vers = FindName( ptr->name, modptr->version,
	    &tmp))) {
	    if( (s = scan_versions( buffer, base, vers->ptr, modptr)) )
		buffer = s;
	}
    }

    /**
     **  This is the recursion. Preserve the buffer end pointer
     **/

    if( (s = scan_versions( buffer, base, ptr->ptr, modptr)) )
	buffer = s;

    return( buffer);

} /** End of 'scan_versions' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		CheckModuleVersion				     **
 ** 									     **
 **   Description:	Reduce the passed module name into a <mod>/<vers>    **
 **			string						     **
 ** 									     **
 **   First Edition:	1995/12/28					     **
 ** 									     **
 **   Parameters:	char	*name		name to be checked	     **
 ** 									     **
 **   Result:		char*	NULL		any error		     **
 **				Otherwise	Pointer to a <mod>/<vers>    **
 **						string			     **
 ** 									     **
 **   Attached Globals:	modlist		List containing all version names    **
 **			aliaslist	List containing all aliases	     **
 **			g_current_module	The module which is handled  **
 **						by the current command	     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static	char	*CheckModuleVersion( char *name)
{
    static char	 buffer[ BUFSIZ];
    char	*s, *t;

    /**
     **  Check the first parameter and extract modulename and version
     **/
    if( *psep == *name) { /** only the version specified (top level only)    **/
	/**
	 **  get the module name from the current module ...
	 **/
	if( !g_current_module)
	    return((char *) NULL);

	strcpy( buffer, g_current_module);
	if(!(t = strrchr( buffer, *psep)))
	    t = buffer + strlen( buffer);
	*t++ = *psep;
	*t = '\0';

	/**
	 **  The version has been specified as a parameter
	 **/
	if( (s = strrchr( name, *psep)) ) {
	    s++;
	} else {
	    ErrorLogger( ERR_INTERAL, LOC, NULL);
	    return((char *) NULL);
	}

	strcpy( t, s);

    } else if( '.' == *name) { /** . = current directory or version    **/
	/**
	 **  get the module name from the current module ...
	 **/
	if( !g_current_module)
	    return((char *) NULL);

	if( (t = strrchr( g_current_module, *psep)))
	    t++;
	else
	    t = g_current_module;	/* at top level */
	
	strcpy(buffer, t);

	/**
	 **  The version has been specified as a parameter
	 **/
	if(!(s = strrchr( name, *psep)) ) {
	    ErrorLogger( ERR_INTERAL, LOC, NULL);
	    return((char *) NULL);
	}
	/* keep / */
	strcat( buffer, s);		/** directory / version		     **/

    } else {				/** Maybe an alias or a module	     **/
	strcpy( buffer, name);
	if( !strrchr( buffer, *psep)) {
	    /**
	     **  Check wheter this is an alias ...
	     **/
	    if( AliasLookup( buffer, &s, &t)) {
		strcpy( buffer, s);
		strcat( buffer, psep);
		strcat( buffer, t);
	    } else {
		/**
		 **  The default version is being selected
		 **/
		t = buffer + strlen( buffer);
		if( *psep != *t)
		    *t++ = *psep;
		strcpy( t, _(em_default));
	    }
	}
    }
    /**
     **  Pass the buffer reference to the caller
     **/
    return( buffer);

} /** End of 'CheckModuleVersion' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		cmdModuleAlias					     **
 ** 									     **
 **   Description:	Callback function for 'alias'			     **
 ** 									     **
 **   First Edition:	1995/12/28					     **
 ** 									     **
 **   Parameters:	ClientData	 client_data			     **
 **			Tcl_Interp	*interp		According Tcl interp.**
 **			int		 objc		Number of arguments  **
 **			Tcl_Obj		*objv[]		Argument array	     **
 ** 									     **
 **   Result:		int	TCL_OK		Successful completion	     **
 **				TCL_ERROR	Any error		     **
 ** 									     **
 **   Attached Globals:	aliaslist	List containing all aliases	     **
 **   			g_flags		These are set up accordingly before  **
 **					this function is called in order to  **
 **					control everything		     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

int cmdModuleAlias(
	ClientData client_data,
	Tcl_Interp * interp,
	int objc,
	Tcl_Obj * CONST84 objv[]
) {
	ModName        *tmp, *ptr;
	char           *version, *module;
	ModName        *trg_alias;
	ModModule      *modptr;

    /**
     **  Parameter check
     **/
	if (objc != 3) {
		if (OK !=
		    ErrorLogger(ERR_USAGE, LOC, Tcl_GetString(objv[0]),
				" aliasname ", "modulename", NULL))
			return (TCL_ERROR); /** ------ EXIT (FAILURE) -----> **/
	}
    /**
     **  Whatis mode?
     **/
	if (g_flags & M_WHATIS)
		return (TCL_OK);	/** -------- EXIT PROCEDURE -------> **/

	if (g_flags & M_DISPLAY) {
		fprintf(stderr, "%s\t %s %s\n", Tcl_GetString(objv[0]),
			Tcl_GetString(objv[1]), Tcl_GetString(objv[2]));
	}
    /**
     **  Check if the target is an alias ...
     **  Conditionally split up the passed <module>/<version> pair.
     **/
	trg_alias = FindName((char *)Tcl_GetString(objv[2]), aliaslist, &tmp);
	if (!trg_alias) {
		if (!(module = CheckModuleVersion(Tcl_GetString(objv[2]))))
			module = (char *)Tcl_GetString(objv[2]);

		if ((version = strrchr(module, *psep)))
			*version++ = '\0';
	}
    /**
     **  Any alias record existing with this name?
     **  If it does, we're finished ...
     **/
	if ((ptr = FindName((char *)Tcl_GetString(objv[1]), aliaslist, &tmp))) {

		if (!ptr->ptr || !ptr->ptr->name ||
		    (!trg_alias
		     && (!ptr->ptr->module || !ptr->ptr->module->module))) {
			ErrorLogger(ERR_INTERAL, LOC, NULL);
			return (TCL_ERROR); /** ------ EXIT (FAILURE) -----> **/
		}

		if((trg_alias && !strcmp(ptr->ptr->name,Tcl_GetString(objv[2])))
		    || (!trg_alias && !strcmp(ptr->ptr->name, version)
			&& !strcmp(ptr->ptr->module->module, module)))
			return (TCL_OK);/** -------- EXIT (SUCCESS) -------> **/

		if (OK != ErrorLogger(ERR_DUP_ALIAS,LOC,Tcl_GetString(objv[1]),
				NULL))
			return (TCL_ERROR); /** ------ EXIT (FAILURE) -----> **/
	} else {
	/**
	 **  We have to allocate a new alias entry
	 **/
		if (!(ptr = AddName((char *)Tcl_GetString(objv[1]), &aliaslist,
			     NULL))) {
			ErrorLogger(ERR_INTERAL, LOC, NULL);
			return (TCL_ERROR); /** ------ EXIT (FAILURE) -----> **/
		}
	}
    /**
     **  Now ptr points to the affected module alias record ...
     **  Conditionally we have to create the module and the version record now.
     **/
	if (trg_alias) {
		ptr->ptr = trg_alias;
	} else {
		if (!(modptr = AddModule(module))) {
			ErrorLogger(ERR_BADMODNAM, LOC, Tcl_GetString(objv[2]),
				    NULL);
			ptr->ptr = (ModName *) NULL;
			return (TCL_ERROR); /** ------ EXIT (FAILURE) -----> **/
		}
		ptr->ptr = AddName((version ? version : _(em_default)),
				   &modptr->version, modptr);
	}

	return (TCL_OK);

} /** End of 'cmdModuleAlias' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		AliasLookup					     **
 ** 									     **
 **   Description:	Resolves a given alias to a module/version string    **
 ** 									     **
 **   First Edition:	1995/12/28					     **
 ** 									     **
 **   Parameters:	char	*alias		Name of the alias to be re-  **
 **						solved			     **
 **			char	**module	Buffer for the module name   **
 **			char	**version	Buffer for the module version**
 ** 									     **
 **   Result:		int	1		Success, value in the buffer **
 **						is valid		     **
 **				0		Any error, or not found	     **
 ** 									     **
 **   Attached Globals:	aliaslist	List containing all aliases	     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

int	AliasLookup(	char	*alias,
			char	**module,
			char	**version)
{
    ModName	*ptr, *tmp, *oldptr = NULL;

    while( 1) {

	/**
	 **  Lokate the alias entry and check intergrity
	 **/

	if(!(ptr = FindName( alias, aliaslist, &tmp)))
	    return( 0);			/** ------- EXIT (not found) ------> **/

	if( ptr == oldptr || !ptr->ptr || !ptr->ptr->name ) {
	    ErrorLogger( ERR_INTERAL, LOC, NULL);
	    return( 0);			/** -------- EXIT (FAILURE) -------> **/
	}

	/**
	 **  Do we have to loop? Another alias has no module reference ...
	 **/

        if( !ptr->ptr->module) {
	    alias = ptr->ptr->name;
	    oldptr = ptr;
	    continue;
	}

	/**
	 **  Got it. Get the module name and the version from the found
	 **  entry.
	 **  Dereference symbolic module versions
	 **/

	*module = ptr->ptr->module->module;
	*version = ptr->ptr->name;

	if((ModName *) NULL != (ptr = FindName( *version,
	    ptr->ptr->module->name, &tmp))) {
	    if( !ptr->version || !ptr->version->name) {
		if( OK != ErrorLogger( ERR_INTERAL, LOC, NULL))
		    return( 0);
	    } else 
		*version = ptr->version->name;
	}

	break;

    } /** while **/

    return( 1);

} /** End of 'AliasLookup' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		VersionLookup					     **
 ** 									     **
 **   Description:	Resolves a given alias to a module/version string    **
 ** 									     **
 **   First Edition:	1995/12/28					     **
 ** 									     **
 **   Parameters:	char	*alias		Name of the alias to be re-  **
 **						solved			     **
 **			char	**module	Buffer for the module name   **
 **			char	**version	Buffer for the module version**
 ** 									     **
 **   Result:		int	1		Success, value in the buffer **
 **						is valid		     **
 **				0		Any error, or not found	     **
 ** 									     **
 **   Attached Globals:	g_current_module	The module which is handled  **
 **						by the current command	     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

int	VersionLookup(char *name, char **module, char **version)
{
    static char  buffer[ BUFSIZ];
    ModModule	*mptr, *mtmp;
    ModName	*vptr, *vtmp;
    ModName	**history;
    char	*s, *t;
    int		 histsize = 0, histndx = 0, i;

    /**
     **  Check whether this is an alias ...
     **  BTW: Alias lookups return the FQMN (full qualifed module name ;-)
     **/

    if( *psep == *name) {
	strcpy( buffer, g_current_module);
	if( (s = strrchr( buffer, *psep)) )
	    *s = '\0';
	*module = buffer;
	*version = name + 1;
    
    } else {

	strcpy( buffer, name);
	*module = buffer;

	if(!(*version = strrchr( buffer, *psep))) {

	    if( AliasLookup( buffer, &s, &t)) {
		*module = s; *version = t;

	    } else
		*version = _(em_default);

	} else 
	    *(*version)++ = '\0';
    }

    /**
     **  Look up modulename ...
     **  We call it success, if we do not find a registered name.
     **  In this case <module>/<version> will be returned as passed.
     **/
    if(!(mptr = FindModule( *module, &mtmp))) {
	return( 1);			/** -------- EXIT (SUCCESS) -------> **/
    }

    /**
     **  This is for preventing endless loops
     **/
    histsize = HISTTAB;
    histndx = 0;

    if(!(history = (ModName **) module_malloc( histsize * 
	sizeof( ModName *)))) {
	ErrorLogger( ERR_ALLOC, LOC, NULL);
	return( 0);			/** -------- EXIT (FAILURE) -------> **/
    }

    /**
     **  Now look up the version name. Check symbolic names first. If some-
     **  thing is found, check if the related version record itself relates
     **  to a name record ...
     **/
    while( 1) {

	/**
	 **  Check the symbolic names ...
	 **/
	if((ModName *) NULL != (vptr = FindName( *version, mptr->name, &vtmp))){
	    if( !vptr->version || !vptr->version->name) {
		if( OK != ErrorLogger( ERR_INTERAL, LOC, NULL)) 
		    *version = (char *) NULL;
		break;
	    }

	    *version = vptr->version->name;

	    /**
	     **  Prevent from looping ...
	     **/
	    for( i=0; i<histndx; i++) {
		if( history[ i] == vptr) {		/** That's the loop  **/
		    ErrorLogger( ERR_SYMLOOP, LOC, *version, NULL);
		    *version = (char *) NULL;
		    break;
		}
	    }

	    if( !*version)
		break;

	    if( histndx >= histsize) {
		histsize += HISTTAB;

		if(!(history = (ModName **) module_realloc(
		    history, histsize * sizeof( ModName *)))) {
		    ErrorLogger( ERR_ALLOC, LOC, NULL);
		    return( 0);		/** -------- EXIT (FAILURE) -------> **/
		}
	    }

	    history[ histndx++] = vptr;

	} else {
	    break;

	} /** if( FindName) **/
    } /** while( 1) **/

    /**
     **  Free the loop preventing list
     **  If version is NULL now, something went wrong in the lookup loop above
     **/
    null_free((void *) &history);
    return((char *) NULL != *version);

} /** End of 'VersionLookup' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		CleanupVersion					     **
 ** 									     **
 **   Description:	Cleanup the version structure			     **
 ** 									     **
 **   First Edition:	1995/12/28					     **
 ** 									     **
 **   Parameters:	-						     **
 ** 									     **
 **   Result:		-						     **
 ** 									     **
 **   Attached Globals:	modlist		List containing all version names    **
 **			aliaslist	List containing all aliases	     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

void	CleanupVersion(ModModule *ptr)
{

    CleanupVersionSub( modlist);
    modlist = (ModModule *) NULL;

    CleanupName( aliaslist);
    aliaslist = (ModName *) NULL;

} /** End of 'CleanupVersion' **/

static void	CleanupVersionSub( ModModule *ptr)
{
    /**
     **  Recursion
     **/

    if( !ptr)
	return;

    CleanupVersion( ptr->next);

    /**
     **  Cleanup everything that relates to this record
     **/

    CleanupName( ptr->version);
    CleanupName( ptr->name);
    null_free((void *) &(ptr->module));

} /** End of 'CleanupVersionSub' **/

static void	CleanupName( ModName *ptr)
{
    /**
     **  Recursion
     **/

    if( !ptr)
	return;

    CleanupName( ptr->next);

    /**
     **  Cleanup everything that relates to this record
     **/

    null_free((void *) &(ptr->name));

} /** End of 'CleanupName' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		AddModule					     **
 ** 									     **
 **   Description:	Add a new entry to the modules queue		     **
 ** 									     **
 **   First Edition:	1995/12/28					     **
 ** 									     **
 **   Parameters:	char	*name		Name of the new module	     **
 ** 									     **
 **   Result:		ModModule*	NULL	Any error                    **
 **					Else	Pointer to the new record    **
 ** 									     **
 **   Attached Globals:	modlist		List containing all version names    **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static	ModModule	*AddModule(	char	*name)
{
    ModModule	*app_ptr, *ptr;

    /**
     **  We do not trust in NULL module names
     **/
    if( !name || !*name)
	return((ModModule *) NULL);
    /**
     **  Check if the module name already exists and save the 'prev' pointer
     **  for appending the new one.
     **/
    if( (ptr = FindModule( name, &app_ptr)) )
	return( ptr);

    /**
     **  Allocate a new guy
     **/

    if(!(ptr = (ModModule *) module_malloc( sizeof(ModModule)))) {
	ErrorLogger( ERR_ALLOC, LOC, NULL);
	return((ModModule *) NULL);
    }

    /**
     **  Fill the name in and put it in the queue
     **/

    if(!(ptr->module = stringer(NULL,0, name, NULL))) {
	ErrorLogger( ERR_ALLOC, LOC, NULL);
	null_free((void *) &ptr);
	return((ModModule *) NULL);
    }

    if( app_ptr) {
	ptr->next = app_ptr->next;
	app_ptr->next = ptr;
    } else {
	ptr->next = modlist;
	modlist = ptr;
    }

    ptr->version = (ModName *) NULL;
    ptr->name = (ModName *) NULL;

    /**
     **  Pass back the pointer to the new entry
     **/

    return( ptr);

} /** End of 'AddModule' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		FindModule					     **
 ** 									     **
 **   Description:	Find a new entry in the modules queue		     **
 ** 									     **
 **   First Edition:	1995/12/28					     **
 ** 									     **
 **   Parameters:	char		*name	Name of be found	     **
 **			ModModule	**prev	Buffer for the 'previous'    **
 **						pointer			     **
 ** 									     **
 **   Result:		ModModule*	NULL	Any error or not found       **
 **					Else	Pointer to the record	     **
 ** 									     **
 **   Attached Globals:	modlist		List containing all version names    **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static	ModModule	*FindModule(	char		 *name,
					ModModule	**prev)
{
    ModModule	*ptr = modlist;
    int		 cmp = 1;

    *prev = (ModModule *) NULL;
    while( ptr && 0 < (cmp = strcmp( name, ptr->module))) {
	*prev = ptr;
	ptr = ptr->next;
    }

    return( cmp ? (ModModule *) NULL : ptr);

} /** End of 'FindModule' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		AddName						     **
 ** 									     **
 **   Description:	Add a new entry to the name queue		     **
 ** 									     **
 **   First Edition:	1995/12/28					     **
 ** 									     **
 **   Parameters:	char	 *name		Name of the new entry	     **
 **			ModName	**start		Start of the queue	     **
 **			ModModule *module	Parent module record pointer **
 ** 									     **
 **   Result:		ModName*	NULL	Any error                    **
 **					Else	Pointer to the new record    **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static	ModName	*AddName(	char	 *name,
				ModName	**start,
				ModModule *module)
{
    ModName	*app_ptr, *ptr;

    /**
     **  Check if the name already exists and save the 'prev' pointer
     **  for appending the new one.
     **/

    if( (ptr = FindName( name, *start, &app_ptr)) )
	return( ptr);

    /**
     **  Allocate a new guy
     **/

    if(!(ptr = (ModName *) module_malloc(sizeof(ModName)))) {
	ErrorLogger( ERR_ALLOC, LOC, NULL);
	return((ModName *) NULL);
    }

    /**
     **  Fill the name in and put it in the queue
     **/

    if(!(ptr->name = stringer(NULL,0, name, NULL))) {
	ErrorLogger( ERR_ALLOC, LOC, NULL);
	null_free((void *) &ptr);
	return((ModName *) NULL);
    }

    if( app_ptr) {
	ptr->next = app_ptr->next;
	app_ptr->next = ptr;
    } else {
	ptr->next = *start;
	*start = ptr;
    }

    ptr->module = module;
    ptr->version = ptr->ptr = (ModName *) NULL;

    /**
     **  Pass back the pointer to the new entry
     **/

    return( ptr);

} /** End of 'AddName' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		FindName					     **
 ** 									     **
 **   Description:	Find a new entry in the modules queue		     **
 ** 									     **
 **   First Edition:	1995/12/28					     **
 ** 									     **
 **   Parameters:	char	 *name		Name of be found	     **
 **			ModName  *start		Start of the name queue      **
 **			ModName	**prev		Buffer for the 'previous'    **
 **						pointer			     **
 ** 									     **
 **   Result:		ModName*	NULL	Any error or not found       **
 **					Else	Pointer to the record	     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static	ModName	*FindName(	char	 *name,
				ModName  *start,
				ModName	**prev)
{
    ModName	*ptr = start;
    int		 cmp = 1;

    *prev = (ModName *) NULL;
    while( ptr && 0 < (cmp = strcmp( name, ptr->name))) {
	*prev = ptr;
	ptr = ptr->next;
    }

    return( cmp ? (ModName *) NULL : ptr);

} /** End of 'FindName' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		DumpName					     **
 ** 									     **
 **   Description:	Dumps the list of ModNames to stderr		     **
 ** 									     **
 **   First Edition:	2009/09/24					     **
 ** 									     **
 **   Parameters:	ModName  *start		Start of the name queue      **
 **   			char	 *follow	<a>lphabetic or <l>ogical    **
 ** 									     **
 **   Result:		void						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static void DumpName(
	ModName *start,
	char *follow
) {
	ModName        *ptr = start;
	char	       *name, *module, *version, *logical, *alpha,
			null_[] = "(null)";

	while (ptr) {
		name = ptr->name;
		if (ptr->module && ptr->module->module)
			module = ptr->module->module;
		else
			module = null_;

		if (ptr->version && ptr->version->name)
			version = ptr->version->name;
		else
			version = null_;

		if (ptr->next && ptr->next->name)
			alpha = ptr->next->name;
		else
			alpha = null_;

		if (ptr->ptr && ptr->ptr->name)
			logical = ptr->ptr->name;
		else
			logical = null_;

		fprintf(stderr,"\tname: %s\n"
			"\t\t->logical: %s\n\t\t->alpha: %s\n"
			"\t\tmodule: %s\n\t\tversion: %s\n",
			name, logical, alpha, module, version);

		if (ptr->ptr)
			DumpName(ptr->ptr, "logical");

		if (*follow == 'l' || *follow == 'L')
			ptr = ptr->ptr;
		else
			ptr = ptr->next;
	}

} /** End of 'DumpName' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		DumpMod						     **
 ** 									     **
 **   Description:	Dumps the list of ModModules to stderr		     **
 ** 									     **
 **   First Edition:	2009/09/24					     **
 ** 									     **
 **   Parameters:	ModModules  *start	Start of the module queue    **
 ** 									     **
 **   Result:		void						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static void DumpMod(
	ModModule *start
) {
	ModModule      *ptr = start;
	char	       *module, *name, *version,
			null_[] = "(null)";

	while (ptr) {
		module = ptr->module;
		if (ptr->name && ptr->name->name)
			name = ptr->name->name;
		else
			name = null_;

		if (ptr->version && ptr->version->name)
			version = ptr->version->name;
		else
			version = null_;

		fprintf(stderr,"\tmodule: %s\n"
			"\t\tname: %s\n\t\tversion: %s\n",
			module, name, version);

		ptr = ptr->next;
	}

} /** End of 'DumpMod' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		DumpX						     **
 ** 									     **
 **   Description:	Dumps the modlist or alias to stderr		     **
 ** 									     **
 **   First Edition:	2009/09/24					     **
 ** 									     **
 **   Parameters:	char  *what		<M>odules or <A>liases	     **
 ** 									     **
 **   Attached Globals:	modlist		List containing all version names    **
 **			aliaslist	List containing all aliases	     **
 ** 									     **
 **   Result:		void						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

void DumpX(
	char * what
) {
	if (*what == 'a' || *what == 'A') {
		fprintf(stderr,">>>Alias List:\n");
		DumpName(aliaslist, "alphabetic");
	} else if (*what == 'm' || *what == 'M') {
		fprintf(stderr,">>>Module Version List:\n");
		DumpMod(modlist);
	}

} /** End of 'DumpMod' **/
