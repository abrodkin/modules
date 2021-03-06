/*****
 ** ** Module Header ******************************************************* **
 ** 									     **
 **   Modules Revision 3.0						     **
 **   Providing a flexible user environment				     **
 ** 									     **
 **   File:		locate_module.c					     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Authors:	John Furlan, jlf@behere.com				     **
 **		Jens Hamisch, jens@Strawberry.COM			     **
 **		R.K. Owen, <rk@owen.sj.ca.us> or <rkowen@nersc.gov>	     **
 ** 									     **
 **   Description: 	Contains the routines which locate the actual	     **
 **			modulefile given a modulefilename by looking in all  **
 **			of the paths in MODULEPATH. 			     **
 ** 									     **
 **   Exports:		Locate_ModuleFile				     **
 **			SourceRC					     **
 **			SourceVers					     **
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

static char Id[] = "@(#)$Id$";
static void *UseId[] = { &UseId, Id };

/** ************************************************************************ **/
/** 				      HEADERS				     **/
/** ************************************************************************ **/

#include "modules_def.h"
#include "uvec.h"

/** ************************************************************************ **/
/** 				  LOCAL DATATYPES			     **/
/** ************************************************************************ **/

/** not applicable **/

/** ************************************************************************ **/
/** 				     CONSTANTS				     **/
/** ************************************************************************ **/

#define	SRCFRAG	100

/** ************************************************************************ **/
/**				      MACROS				     **/
/** ************************************************************************ **/

/** not applicable **/

/** ************************************************************************ **/
/** 				    LOCAL DATA				     **/
/** ************************************************************************ **/

static	char	module_name[] = __FILE__;
static	char	strbuffer[ MOD_BUFSIZE];
static	char	modfil_buf[ MOD_BUFSIZE];

/** ************************************************************************ **/
/**				    PROTOTYPES				     **/
/** ************************************************************************ **/

static	char	 *GetModuleName( Tcl_Interp*, char*, char*, char*);

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		Locate_ModuleFile				     **
 ** 									     **
 **   Description:	Searches for a modulefile given a string argument    **
 **			which is either a full path or a modulefile name     **
 **			-- usually the argument the user gave. If it's not a **
 **			full path, the directories in the MODULESPATH	     **
 **			environment variable are searched to find a match    **
 **			for the given name.				     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	Tcl_Interp	*interp		Attached Tcl interpr.**
 **			char		*modulename	Name of the module to**
 **							be located	     **
 **			char		*realname	Real modulename (with**
 **							version)	     **
 **			char		*filename	Real    full module  **
 **							file path	     **
 ** 									     **
 **   Result:		int		TCL_OK or TCL_ERROR		     **
 **			filename	the full path of the required module **
 **					file is copied in here		     **
 ** 									     **
 **   Attached Globals:	g_current_module	The module which is handled  **
 **						by the current command	     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

int Locate_ModuleFile(
	Tcl_Interp * interp,
	char *modulename,
	char *realname,
	char *filename
) {
	char           *p;		/** Tokenization pointer	     **/
	char           *result = NULL;	/** This functions result	     **/
	char          **pathlist;	/** List of paths to scan	     **/
	int             numpaths,	/** Size of this list		     **/
	                i;		/** Loop counter		     **/
	char           *mod, *vers;	/** Module and version name for sym- **/
					/** bolic name lookup		     **/
    /**
     **  If it is a full path name, that's the module file to load.
     **/
	if (!modulename)
		if (OK != ErrorLogger(ERR_PARAM, LOC, "modulename", NULL))
			goto unwind0;

	if (modulename[0] == *psep || modulename[0] == '.') {
		p = (char *)strrchr(modulename, *psep);
		if (p) {
			*p = '\0';
	    /**
	     **  Check, if what has been specified is a valid version of
	     **  the specified module ...
	     **/
			if (!(result =
			     GetModuleName(interp, modulename, NULL, (p + 1))))
				goto unwind0;
	    /**
	     **  Reinstall the 'modulefile' which has been corrupted by
	     **   tokenization
	     **/
			*p = *psep;
	    /**
	     **  ... Looks good! Conditionally (if there has been no version
	     **  specified) we have to add the default version
	     **/
			if (!strcmp((p + 1), result)) {
				if (!(stringer(filename, MOD_BUFSIZE,
					     modulename, NULL)))
					goto unwind1;
			} else {
				if (!(stringer(filename, MOD_BUFSIZE,
					     modulename, psep, result, NULL)))
					goto unwind1;
			}
		} else {
	    /**
	     **  Hmm! There's no backslash in 'modulename'. So it MUST begin
	     **  on '.' and MUST be part of the current directory
	     **/
			if (!(result = GetModuleName(interp, modulename, NULL,
					   modulename)))
				goto unwind0;

			if (!strcmp(modulename, result) ||
			    (strlen(modulename) + 1 + strlen(result) + 1 >
			     MOD_BUFSIZE)) {
				if (!(stringer(filename, MOD_BUFSIZE,
					     modulename, NULL)))
					goto unwind1;
			} else {
				if (!(stringer(filename, MOD_BUFSIZE,
					modulename, psep, result, NULL)))
					goto unwind1;
			}
		}
	} else {
    /**
     **  So it is not a full path name what has been specified. Scan the 
     **  MODULESPATH
     **/
	/**
	 **  If I don't find a path in MODULEPATH, there's nothing to search.
	 **/
		if (!ModulePathVec || !uvec_number(ModulePathVec))
			if (OK != ErrorLogger(ERR_MODULE_PATH, LOC, NULL)) {
				g_current_module = NULL;
				goto unwind0;
			}
	/**
	 **  Expand the module name (in case it is a symbolic one). This must
	 **  be done once here in order to expand any aliases
	 **/
		if (VersionLookup(modulename, &mod, &vers)) {
			if (!(stringer(strbuffer, MOD_BUFSIZE,
						     mod, psep, vers, NULL)))
				goto unwindp;
			modulename = strbuffer;
		}
	/**
	 **  Split up the MODULEPATH values into multiple directories
	 **/
		numpaths = uvec_number(ModulePathVec);
		pathlist = ModulePath;
	/**
	 **  Check each directory to see if it contains the module
	 **/
		while (pathlist && *pathlist && **pathlist) {
			/* skip empty paths */
			if ((result = GetModuleName(interp,
				    *pathlist, NULL, modulename))) {

				if (strlen(*pathlist) + 2 + strlen(result) >
				    MOD_BUFSIZE) {
					if (!(stringer(filename, MOD_BUFSIZE,
						     *pathlist, NULL)))
						goto unwindp;
				} else {
					if (!(stringer(filename, MOD_BUFSIZE,
						     *pathlist, psep, result,
						     NULL)))
						goto unwindp;
				}
				break;
			}
#if 0
	    /**
	     **  If we havn't found it, we should try to re-expand the module
	     **  name, because some rc file have been sourced
	     **/
			if (VersionLookup(modulename, &mod, &vers)) {
				if (!stringer(strbuffer, MOD_BUFSIZE, mod, psep,
					     vers, NULL))
					goto unwindp;
				modulename = strbuffer;
			}
#endif
			pathlist++;
		} /** while **/
	/**
	 **  If result still NULL, then we really never found it and we should
	 **  return ERROR and clear the full_path array for cleanliness.
	 **/
		if (!result) {
			filename[0] = '\0';
			goto unwind0;
		}
		if (0) {
unwindp:
			goto unwind1;
		}
	} /** not a full path name **/
    /**
     **  Free up what has been allocated and pass the result back to
     **  the caller and save the real module file name returned by
     **  GetModuleName
     **/
	strncpy(realname, result, MOD_BUFSIZE);
	if (!(stringer(realname, MOD_BUFSIZE, result, NULL)))
		goto unwind1;
	null_free((void *)&result);

	return (TCL_OK);

unwind1:
	null_free((void *)&result);
unwind0:
	return (TCL_ERROR);
}

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		GetModuleName					     **
 ** 									     **
 **   Description:	Given a path and a module filename, this function    **
 **			checks to find the modulefile.			     **
 ** 									     **
 **   Notes:		This function is RECURSIVE			     **
 **									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	Tcl_Interp	*interp		According Tcl Interp.**
 **			char		*path		Path to start seeking**
 **			char		*prefix		Module name prefix   **
 **			char		*modulename	Name of the module   **
 ** 									     **
 **   Result:		char*	NULL	Any failure( parameters, alloc)	     **
 **				else	Pointer to an allocated buffer con-  **
 **					taining the complete module file path**
 ** 									     **
 **   Attached Globals:	-						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static	char	*GetModuleName(	Tcl_Interp	*interp,
			     	char		*path,
				char		*prefix,
			     	char		*modulename)
{
    char	 *fullpath = NULL;	/** Buffer for creating path names   **/
    char	 *Result = NULL;	/** Our return value		     **/
    uvec	 *filelist = NULL;	/** Buffer for a list of possible    **/
					/** module files		     **/
    int		  numlist,		/** Size of this list		     **/
		  i, slen, is_def;
    is_Result	  fstate;		/** file stat			     **/
    char	 *s, *t;		/** Private string buffer	     **/
    char	 *mod, *ver;		/** Pointer to module and version    **/
    char	 *mod1, *ver1;		/** Temp pointer		     **/
    
    /**
     **  Split the modulename into module and version. Use a private buffer
     **  for this
     **/
    if(!(s = stringer(NULL, 0,  modulename, NULL))) {
	ErrorLogger( ERR_ALLOC, LOC, NULL);
	goto unwind0;
    }
    slen = strlen( s) + 1;
    mod = s;
    if( (ver = strrchr( mod, *psep)) )
	*ver++ = '\0';
    /**
     **  Allocate a buffer for full pathname building
     **/
    if(!(fullpath = stringer(NULL, MOD_BUFSIZE, NULL))) {
	if( OK != ErrorLogger( ERR_STRING, LOC, NULL)) {
	    goto unwind1;
	}
    }
    /**
     **  Check whether $path/$prefix/$modulename is a directory.
     **/
    if( prefix) {
	if(!stringer(fullpath, MOD_BUFSIZE,
	path,psep,prefix,psep,modulename, NULL))
	    goto unwind1;
    } else {
	if(!stringer(fullpath, MOD_BUFSIZE, path,psep,modulename, NULL))
	    goto unwind1;
    }
    if(is_("dir", fullpath)) {
	/**
	 ** So the full modulename is $modulename/default.  Recurse on that.
	 **/
	if(!(t = stringer(NULL, 0, modulename, psep, _(em_default), NULL)))
	    goto unwind1;
	Result = GetModuleName( interp, path, prefix, t);
	null_free((void *) &t);
	null_free((void *) &fullpath);
	null_free((void *) &s);
	return( Result);
    }

    /**
     **  Check whether $path/$prefix/$mod is a directory
     **/
    if( prefix) {
	if(!stringer(fullpath, MOD_BUFSIZE, path,psep,prefix,psep,mod, NULL))
	    goto unwind1;
    } else {
	if(!stringer(fullpath, MOD_BUFSIZE, path,psep,mod, NULL))
	    goto unwind1;
    }
    is_def = !strcmp( mod, _(em_default));

    if(is_def || (fstate = is_("what",fullpath))) {
	/**
	 **  If it is a directory
	 **/
    	if(!is_def && (fstate == IS_DIR)) {
	    /**
	     **  Source the ".modulerc" file if it exists
	     **  For compatibility source the .version file, too
	     **/
	    if( prefix) {
		if(!stringer(modfil_buf, MOD_BUFSIZE, prefix,psep,mod, NULL))
		    goto unwind2;
	    } else {
		if(!stringer(modfil_buf, MOD_BUFSIZE,mod, NULL))
		    goto unwind2;
	    }

	    if(!stringer(fullpath, MOD_BUFSIZE, path,psep,modfil_buf, NULL))
		goto unwind2;
	    g_current_module = modfil_buf;

	    if(	TCL_ERROR == SourceRC(interp,fullpath,modulerc_file,Mod_Load)
	    ||	TCL_ERROR == SourceVers(interp,fullpath,modfil_buf,Mod_Load)) {
		/* flags = save_flags; */
		    goto unwind2;
	    }
	    /**
	     **  After sourcing the RC files, we have to look up versions again
	     **/
	    if( VersionLookup( modulename, &mod1, &ver1)) {
		int len = strlen( mod1) + strlen( ver1) + 2;
		/**
		 **  Maybe we have to enlarge s
		 **/
		if( len > slen) {
		    null_free((void *) &s);
		    if(!(s = stringer( NULL, len, NULL))) {
			ErrorLogger( ERR_STRING, LOC, NULL);
			goto unwind2;
		    }
		    slen = len;
		}
		/**
		 **  Print the new module/version in the buffer
		 **/
		if(!stringer( s, len, mod1,psep, ver1, NULL)) {
		    ErrorLogger( ERR_STRING, LOC, NULL);
		    goto unwind2;
		}
		mod = s;
		if( (ver = strchr( s, *psep)) )
		    *ver++ = '\0';
	    }
	    /**
	     **  recursively delve into subdirectories (until ver == NULL).
	     **/
	    if( ver) {
		int len = strlen( mod) + 1;

		if( prefix)
		    len += strlen( prefix) +1;
		/**
		 **  Build the new prefix
		 **/
		if(!(t = stringer(NULL, len, NULL))) {
		    ErrorLogger( ERR_STRING, LOC, NULL);
		    goto unwind2;
		}

		if( prefix) {
		    if(!stringer(t,len,prefix,psep,mod, NULL)){
			ErrorLogger( ERR_STRING, LOC, NULL);
			goto unwindt;
		    }
		} else {
		    if(!stringer(t, len, mod, NULL)){
			ErrorLogger( ERR_STRING, LOC, NULL);
			goto unwindt;
		    }
		}
		/**
		 **  This is the recursion
		 **/
		Result = GetModuleName( interp, path, t, ver);

		/**
		 **  Free our temporary prefix buffer
		 **/
		null_free((void *) &t);
		if (0) {	/* an error occurred */
unwindt:
		    null_free((void *) &t);
		    goto unwind2;
		}
	    } 
	} else {     /** if( $path/$prefix/$mod is a directory) **/
	    /**
	     **  Now 'mod' should be either a file or the word 'default'
	     **  In case of default get the file with the highest version number
	     **  in the current directory
	     **/
	    if( is_def) {
		if( !prefix)
		    prefix = ".";
		if(!(filelist = SortedDirList( path, prefix,&numlist)))
		    goto unwind1;

		prefix = (char *) NULL;
		/**
		 **  Select the first one on the list which is either a
		 **  modulefile or another directory. We start at the highest
		 **  lexicographical name in the directory since the filelist
		 **  is reverse sorted.
		 **  If it's a directory, we delve into it.
		 **/
		for( i=0; i<numlist && Result == NULL; i++) {
			char	 *filename;
		    /**
		     **  Build the full path name and check if it is a
		     **  directory. If it is, recursively try to find there what
		     **  we're seeking for
		     **/
			filename = uvec_vector(filelist)[i];
		    if(!stringer(fullpath,MOD_BUFSIZE,path,psep,filename,NULL))
			    goto unwind2;

		    if(is_("dir",fullpath)) {
			Result = GetModuleName( interp, path, prefix, filename);
		    } else {
			/**
			 **  Otherwise check the file for a magic cookie ...
			 **/
			if( check_magic( fullpath, MODULES_MAGIC_COOKIE, 
			    MODULES_MAGIC_COOKIE_LENGTH)) 
			    Result = filename;
		    } /** if( !stat) **/
		} /** for **/
	    } else {  /** default **/
		/**
		 **  If mod names a file, we have to check wheter it exists and
		 **  is a valid module file
		 **/
		if( check_magic( fullpath, MODULES_MAGIC_COOKIE, 
		    MODULES_MAGIC_COOKIE_LENGTH)) 
		    Result = mod;
		else {
		    ErrorLogger( ERR_MAGIC, LOC, fullpath, NULL);
		    Result = NULL;
		}
	    } /** if( mod is a filename) **/
	    /**
	     **  Build the full filename (using prefix and Result) if
	     **  Result is defined
	     **/
	    if( Result) {
		int len = strlen( Result) + 1;

		if( prefix)
		    len += strlen( prefix) + 1;

		if(!(t = stringer(NULL, len, NULL))) {
		   ErrorLogger( ERR_STRING, LOC, NULL);
		   goto unwind2;
		}
		if( prefix) {
		    if(!stringer(t,len, prefix,psep,Result,NULL))
			goto unwindt2;
		} else {
		    if(!stringer(t,len, Result,NULL))
			goto unwindt2;
		}
		Result = t;
		if (0) {	/* an error occurred */
unwindt2:
		    null_free((void *) &t);
		    goto unwind2;
		}
	    } 
	} /** mod is a file **/
    } /** mod exists **/
    /**
     **  Free up temporary values and return what we've found
     **/
    null_free((void*) &fullpath);
    null_free((void*) &s);
    FreeList( &filelist);
    
    return( Result);			/** -------- EXIT (SUCCESS) -------> **/

unwind2:
    null_free((void *) &fullpath);
unwind1:
    null_free((void *) &s);
unwind0:
    return(NULL);			/** -------- EXIT (FAILURE) -------> **/

} /** End of 'GetModuleName' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		SourceRC					     **
 ** 									     **
 **   Description:	Source the passed RC file			     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	Tcl_Interp	*interp		Tcl interpreter	     **
 **			char		*path		Path to be used      **
 **			char		*name		Name of the RC file  **
 **			Mod_Act		action		Load or Unload	     **
 **									     **
 **   Result:		int		TCL_OK		Success		     **
 **					TCL_ERROR	Failure		     **
 **									     **
 **   Attached Globals:	g_flags		These are set up accordingly before  **
 **					this function is called in order to  **
 **					control everything		     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

int SourceRC(
	Tcl_Interp * interp,
	const char *path,
	const char *name,
	const Mod_Act action
) {
	char           *buffer;		/** for full path/name		     **/
	int             save_flags,	/** cache g_flags		     **/
	                Result = TCL_OK;

    /**
     **  If there's a problem with the input parameters it means, that
     **  we do not have to source anything
     **  Only a valid TCL interpreter should be there
     **/
	if (!path || !name)
		return (TCL_OK);

	if (!interp)
		return (TCL_ERROR);

	if (action != Mod_Load && action != Mod_Unload)
		return (TCL_ERROR);

    /**
     **  Build the full name of the RC file
     **/
	if (!(buffer = stringer(NULL, 0, path, psep, name, NULL)))
		if (OK != ErrorLogger(ERR_STRING, LOC, NULL))
			goto unwind0;
    /**
     **  Check whether the RC file exists and has the magic cookie inside
     **/
	if (is_("file",buffer)) {
		if (check_magic(buffer, MODULES_MAGIC_COOKIE,
				MODULES_MAGIC_COOKIE_LENGTH)) {
	    /**
	     **  Set the flags to '(un)load only'. This prevents from
	     **  accidentally printing something
	     **/
			save_flags = g_flags;
			if (action == Mod_Load)
				g_flags = M_LOAD;
			else
				g_flags = M_REMOVE;
	    /**
	     **  Source now
	     **/
			if (TCL_ERROR == Execute_TclFile(interp, buffer))
				if (OK !=
				    ErrorLogger(ERR_SOURCE, LOC, buffer, NULL))
					Result = TCL_ERROR;

			g_flags = save_flags;

		} else {
			/* Not an error ... just warn of invalid magic cookie */
			ErrorLogger(ERR_MAGIC, LOC, buffer, NULL);
		}
	} /** file presumably not found **/
    /**
     **  Free resources and return result
     **/
	null_free((void *)&buffer);

	return (Result);

unwind1:
	null_free((void *)&buffer);
unwind0:
	return (TCL_ERROR);

} /** End of 'SourceRC' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		SourceVers					     **
 ** 									     **
 **   Description:	Source the '.version' file			     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	Tcl_Interp	*interp		Tcl interpreter	     **
 **			char		*path		Path to be used      **
 **			char		*name		Name of the module   **
 **			Mod_Act		action		Load or Unload	     **
 **									     **
 **   Result:		int		TCL_OK		Success		     **
 **					TCL_ERROR	Failure		     **
 **									     **
 **   Attached Globals:	g_flags		These are set up accordingly before  **
 **					this function is called in order to  **
 **					control everything		     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

int SourceVers(
	Tcl_Interp * interp,
	const char *path,
	const char *name,
	const Mod_Act action
) {
	char           *buffer,		/** for full path/name		     **/
		       *version,	/** default version		     **/
		       *mod, *ver;	/** module & version		     **/
	const char     *modname;	/** ptr module part of name	     **/
	int             save_flags,	/** cache g_flags		     **/
	                Result = TCL_OK;

    /**
     **  If there's a problem with the input parameters it means, that
     **  we do not have to source anything
     **  Only a valid TCL interpreter should be here
     **/
	if (!path || !name)
		return (TCL_OK);
	if (!interp)
		return (TCL_ERROR);
    /**
     **  Build the full name of the RC file and check whether it exists and
     **  has the magic cookie inside
     **/
	if (!(buffer = stringer(NULL, 0, path, psep, version_file, NULL)))
		if (OK != ErrorLogger(ERR_STRING, LOC, NULL))
			return (TCL_ERROR);
	if (is_("file",buffer)) {
		if (
#if VERSION_MAGIC != 0
			   check_magic(buffer, MODULES_MAGIC_COOKIE,
				       MODULES_MAGIC_COOKIE_LENGTH)
#else
			   1
#endif
		    ) {
			save_flags = g_flags;
			if (action == Mod_Load)
				g_flags = M_LOAD;
			else
				g_flags = M_REMOVE;

			if (TCL_ERROR !=
			    (Result = Execute_TclFile(interp, buffer))
			    && (version =
				(char *)Tcl_GetVar(interp, "ModulesVersion",
						   0))) {
				char	       *new_argv[4];
				int             objc = 3;
				Tcl_Obj       **objv;
		/**
		 **  The version has been specified within the
		 **  '.version' file via the ModulesVersion variable
		 **/
		/* for deep modulefile dirs ... just use lowest part */
				if (!(modname = (char *) strrchr(name,*psep))) {
					modname = name;
				} else {
					modname++;
				}
				null_free((void *)&buffer);
				if (!(buffer = stringer(NULL, 0,
					modname, psep, version, NULL)))
					if (OK !=
					    ErrorLogger(ERR_STRING, LOC, NULL))
						return (TCL_ERROR);

				new_argv[0] = "module-version";
				new_argv[1] = buffer;
				new_argv[2] = _(em_default);
				new_argv[3] = NULL;
				Tcl_ArgvToObjv(&objc, &objv, 3, new_argv);
		/**
		 **  Define the default version
		 **/
				if (TCL_OK != cmdModuleVersion((ClientData) 0,
				    (Tcl_Interp *) NULL, objc, objv))
					Result = TCL_ERROR;
				Tcl_FreeObjv(&objv);
		/**
		 **  No need for the set variable (only accepted here)
		 **/
				(void) Tcl_UnsetVar(interp,"ModulesVersion",0);
			} /** if( Execute...) **/
			g_flags = save_flags;
		} else
			ErrorLogger(ERR_MAGIC, LOC, buffer, NULL);
	} /** if( is_ file ) **/
    /**
     ** free buffer memory
     **/
	null_free((void *)&buffer);
    /**
     **  Result determines if this was successful
     **/
	return (Result);

} /** End of 'SourceVers' **/

