/*****
 ** ** Module Header ******************************************************* **
 ** 									     **
 **   Modules Revision 3.0						     **
 **   Providing a flexible user environment				     **
 ** 									     **
 **   File:		main.c						     **
 **   First Edition:	91/10/23					     **
 ** 									     **
 **   Authors:	John Furlan, jlf@behere.com				     **
 **		Jens Hamisch, jens@Strawberry.COM			     **
 ** 									     **
 **   Description:	The main routine of Tcl Modules including all of     **
 **			the global data.				     **
 ** 									     **
 **   Exports:		main		Main program			     **
 **			Tcl_AppInit	Tcl Application initialization	     **
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

static char Id[] = "@(#)$Id: main.c,v 1.2.6.1 2005/04/11 17:32:07 rkowen Exp $";
static void *UseId[] = { &UseId, Id };

/** ************************************************************************ **/
/** 				      HEADERS				     **/
/** ************************************************************************ **/

#include "modules_def.h"
#include "getopt.h"

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
/** 				    GLOBAL DATA				     **/
/** ************************************************************************ **/

char	 *g_current_module = NULL;	/** The module which is handled by   **/
					/** the current command		     **/
char	 *specified_module = NULL;	/** The module that was specified    **/
					/** on the command line		     **/
char	**shell_startups;		/** A list off all startup files our **/
					/** invoking shell will source	     **/
char	  shell_name[20];		/** Name of the shell (first para-   **/
					/** meter to modulcmd)		     **/
char	  shell_derelict[20];		/** Shell family (sh, csh)	     **/
int	  g_flags = 0;			/** Control what to do at the moment **/
					/** The posible values are defined in**/
					/** module_def.h		     **/
int	  append_flag = 0;		/** only used by the 'use' command   **/

char	  _default[] = "default";	/** id string for default versions   **/

/**
 **  Name of the rc files
 **  PREFIX points to the location where modules is going to be installed.
 **  It comes from the Makefile
 **/

char	*instpath = PREFIX;

char	*rc_file = RCFILE;
char	*modulerc_file = MODULERCFILE;
char	*version_file = VERSIONFILE;

/**
 **  pointers for regular expression evaluations
 **/
char
    *addRE      = "^(add|load)", 		/** 'module add <file>'	     **/
    *rmRE       = "^(rm|del|era|remov|unload)",	/** 'module unload <file>'   **/
    *swRE       = "^sw",			/** 'module switch <file>'   **/
    *dispRE     = "^(disp|show)", 		/** 'module display <file>'  **/
    *listRE     = "^li", 			/** 'module list <file>'     **/
    *availRE    = "^av",			/** 'module avail <file>'    **/
    *helpRE     = "^(help|\\?)",		/** 'module help <file>'     **/
    *initRE     = "^init",			/** 'module init'	     **/
    *useRE      = "^use",			/** 'module use <file>'	     **/
    *unuseRE    = "^unuse",			/** 'module unuse <file>'    **/
    *updateRE   = "^update",			/** 'module update'	     **/
    *purgeRE    = "^purge",			/** 'module purge'	     **/
    *clearRE    = "^clear",			/** 'module clear'	     **/
    *whatisRE   = "^wh",			/** 'module whatis'	     **/
    *aproposRE  = "^(apr|key)";			/** 'module apropos'	     **/

/**
 **  Hash-Tables for all changes to the environment.
 **  ??? What do we save here, the old or the new setup ???
 **/

Tcl_HashTable	*setenvHashTable;
Tcl_HashTable	*unsetenvHashTable;
Tcl_HashTable	*aliasSetHashTable;
Tcl_HashTable	*aliasUnsetHashTable;
Tcl_HashTable	*markVariableHashTable;
Tcl_HashTable	*markAliasHashTable;

/**
 **  A buffer for reading a single line
 **/

char	*line = NULL;

/**
 **  Flags influenced by the command line switches
 **/

int	sw_force = 0;
int	sw_detach = 0;
int	sw_format = 0;
int	sw_verbose = 0;
int	sw_create = 0;
int	sw_userlvl = UL_ADVANCED;
int	sw_icase = 0;

/** ************************************************************************ **/
/** 				    LOCAL DATA				     **/
/** ************************************************************************ **/

static	char	module_name[] = "main.c";	/** File name of this module **/

#if WITH_DEBUGGING
static	char	_proc_main[] = "main";
#endif
#if WITH_DEBUGGING_INIT
static	char	_proc_Check_Switches[] = "Check_Switches";
static	char	_proc_Tcl_AppInit[] = "Tcl_AppInit";
#endif

/** ************************************************************************ **/
/**				    PROTOTYPES				     **/
/** ************************************************************************ **/

static int	Check_Switches( int *argc, char *argv[]);

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		main						     **
 ** 									     **
 **   Description:	Main program					     **
 ** 									     **
 **   First Edition:	91/10/23					     **
 ** 									     **
 **   Parameters:	int	argc		Number of parameters	     **
 **			char	*argv[]		Command line arguments	     **
 **			char	*environ[]	Pointer to the process' en-  **
 **						vironment.		     **
 **   Result:		int	1		Usage information printed    **
 **				TCL_OK		Successful completion	     **
 **				other		Return value of the module   **
 **			 			subcomand		     **
 ** 									     **
 **   Attached Globals:	*Ptr	 	by InitializeTcl		     **
 **			*HashTable	by InitializeTcl	 	     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

int	main( int argc, char *argv[], char *environ[]) {

    Tcl_Interp	*interp;
    int          return_val = 0;
    char	*rc_name;
    char	*rc_path;

#if WITH_DEBUGGING
    ErrorLogger( NO_ERR_START, LOC, _proc_main, NULL);
#endif
	/**
	 ** check if first argument is --version or -V then output the
	 ** version to stdout.  This is a special circumstance handled
	 ** by the regular options.
	 **/
	if (argc > 1 && *argv[1] == '-') {
		if (!strcmp("-V", argv[1]) || !strcmp("--version", argv[1])) {
		    printf("%s\n", version_string);
		    return 0;
		}
	}
    /**
     **  Initialization. 
     **  Check the command line syntax. There will be no return from the
     **  initialization function in case of invalid command line arguments.
     **/

    if( TCL_OK != Initialize_Tcl( &interp, argc, argv, environ))
	exit( 1);

    if( TCL_OK != Setup_Environment( interp))
	exit( 1);

    /**
     **  Check for command line switches
     **/

    if( TCL_OK != Check_Switches( &argc, argv))
	exit( 1);

    /**
     **  Figure out, which global RC file to use. This depends on the environ-
     **  ment variable 'MODULERCFILE', which can be set to one of the following:
     **
     **		<filename>	-->	PREFIX/etc/<filename>
     **		<dir>/		-->	<dir>/RC_FILE
     **		<dir>/<file>	-->	<dir>/<file>
     **  Use xgetenv to expand 1 level of env.vars.
     **/

    if((rc_name = xgetenv( "MODULERCFILE"))) {
	/* found something in MODULERCFILE */
	if((char *) NULL == (rc_path = strdup(rc_name))) {
	    if( OK != ErrorLogger( ERR_ALLOC, LOC, NULL))
		exit( 1);
	    else {
		free(rc_name);
		rc_name = NULL;
	    }
	} else {
	    free(rc_name);
	    if((char *) NULL == (rc_name = strrchr( rc_path, '/'))) {
		rc_name = rc_path;
		rc_path = instpath;
	    } else
		*rc_name++ = '\0';
	    
	    if( !*rc_name) {
		rc_name = rc_file;
	    }
	}

    } else {
	rc_path = instpath;
	free(rc_name);
	rc_name = rc_file;
    }

    /**
     **  Finaly we have to change PREFIX -> PREFIX/etc
     **/

    if( rc_path == instpath) {
	if((char *) NULL == (rc_path = malloc( strlen( instpath) + 5))) {
	    if( OK != ErrorLogger( ERR_ALLOC, LOC, NULL))
		exit( 1);
	    else
		rc_path = NULL;
	
	} else {
	    /* sprintf( rc_path, "%s/etc", instpath); */
	    strcpy( rc_path, instpath);
	    strcat( rc_path, "/etc");
	}
    }

    /**
     **  Source the global and the user defined RC file
     **/

    g_current_module = (char *) NULL;

    if( TCL_ERROR == SourceRC( interp, rc_path, rc_name) ||
	TCL_ERROR == SourceRC( interp, getenv( "HOME"), modulerc_file))
	exit( 1);

    if( rc_path)
	free( rc_path);

    /**
     **  Invocation of the module command as specified in the command line
     **/

    g_flags = 0;
    return_val = cmdModule((ClientData) 0, interp, (argc - 1), (argv + 1));

    /**
     **  If we were doing some operation that has already flushed its output,
     **  then we don't need to re-flush the output here.
     **
     **  Also, if we've had an error here, then the whole modulecmd failed
     **  and not just the values for a single modulefile.  So, we'll pass in
     **  a NULL here to indicate that any error message should say that
     **  absolutely NO changes were made to the environment.
     **/

    if( TCL_OK == return_val) {
	Output_Modulefile_Changes( interp);
#ifdef HAS_X11LIBS
	xresourceFinish( 1);
#endif
    } else {
	Unwind_Modulefile_Changes( interp, NULL);
#ifdef HAS_X11LIBS
	xresourceFinish( 0);
#endif
    }

    /**
     **  Finally clean up. Delete the required hash tables and conditionally
     **  allocated areas.
     **/

    Delete_Global_Hash_Tables();

    if( line)
	free( line);
    if( error_line)
	free( error_line);

    /**
     **  This return value may be evaluated by the calling shell
     **/

#if WITH_DEBUGGING
    ErrorLogger( NO_ERR_END, LOC, _proc_main, NULL);
#endif

    return( return_val);

} /** End of 'main' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		Tcl_AppInit					     **
 ** 									     **
 **   Description:	This is needed if you use shared TCL libraries.	     **
 **			It won't be called, but the linker complains if it   **
 **		   	doesn't exist.					     **
 ** 									     **
 **   First Edition:	91/10/23					     **
 ** 									     **
 **   Parameters:	Tcl_Interp	*interp		Tcl interpreter to   **
 **							be initialized	     **
 **   Result:		int		TCL_OK		Initialization succ. **
 **   Attached Globals:	-						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

int Tcl_AppInit(Tcl_Interp *interp)
{

#if WITH_DEBUGGING_INIT
    ErrorLogger( NO_ERR_START, LOC, _proc_Tcl_AppInit, NULL);
#endif

    return( TCL_OK);

} /** End of 'Tcl_AppInit' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		Check_Switches					     **
 ** 									     **
 **   Description:	Check for command line switches and set internal     **
 **			control variable according to them. Command line     **
 **			switches are defined to appear between the shell and **
 **			the module command. They begin on a dash and may     **
 **			appear in long and short format. 		     **
 **									     **
 **			The following switches are defined:		     **
 **									     **
 **			    --force, -f		Force prerequired actions    **
 **			    --terse, -t		Terse, parseable messages    **
 **			    --human, -h		Human readable form          **
 **			    --long, -l		Long messages		     **
 **			    --verbose, -v	Verbose mode on		     **
 **			    --silent, -s	Verbose mode off	     **
 **			    --create, -c	Create a cache while execu-  **
 **						ting the command	     **
 **			    --userlvl, -u	Change the user level	     **
 **			    --icase, -i		Ignore case of modulefile    **
 **						names			     **
 **			    --version, -V	Report version only	     **
 ** 									     **
 **   First Edition:	95/12/20					     **
 ** 									     **
 **   Parameters:	int	*argc		Number of parameters	     **
 **			char	*argv[]		Command line arguments	     **
 **									     **
 **   Result:		int	TCL_OK		Successful completion	     **
 **				TCL_ERROR	Unknown switch found	     **
 **			*argc, *argv		All switches are removed     **
 **						from the argv vector	     **
 ** 									     **
 **   Attached Globals:	sw_force		--force, -f		     **
 **			sw_format		-t, -l, -h, -p               **
 **			sw_verbose		--verbose, -v, --silent, -s  **
 **			sw_create		--create, -c		     **
 **			sw_userlvl		--userlvl, -u		     **
 **			sw_icase		--icase, -i		     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static int	Check_Switches( int *argc, char *argv[])
{

    int c;

    /**
     **  These are the options we do provide
     **/

    const struct option longopts[] = {
	{ "force", no_argument, NULL, 'f' },
	{ "terse", no_argument, NULL, 't' },
	{ "long", no_argument, NULL, 'l' },
	{ "human", no_argument, NULL, 'h' },
	{ "parseable", no_argument, NULL, 'p' },
	{ "parse", no_argument, NULL, 'p' },
	{ "verbose", no_argument, NULL, 'v' },
	{ "silent", no_argument, NULL, 's' },
	{ "create", no_argument, NULL, 'c' },
	{ "icase", no_argument, NULL, 'i' },
	{ "userlvl", required_argument, NULL, 'u'},
	{ "append", no_argument, NULL, 'a' },
	{ "version", no_argument, NULL, 'V' },
	{ NULL, no_argument, NULL, 0 }
    };

#if WITH_DEBUGGING_INIT
    ErrorLogger( NO_ERR_START, LOC, _proc_Check_Switches, NULL);
#endif

    /**
     **  Scan the command line for options defined in the longopt table.
     **  Skip the very first argument, which is the shell to be used
     **/

    if( *argc > 1) {

	while( EOF != (c = getopt_long( *argc-1, &argv[1], "hpftlvsciu:aV",
	    longopts, NULL))) {

	    switch( c) {

		/**
		 **  Force
		 **/

		case 'f':			/* force */
		    sw_force = 1;
		    break;

		/**
		 **  Format of the messages
		 **/

		case 't':			/* terse */
		    sw_format |= (SW_SET | SW_TERSE);
		    sw_format &= ~ SW_LONG;
		    break;

		case 'l':			/* long */
		    sw_format |= (SW_SET | SW_LONG);
		    sw_format &= ~ SW_TERSE;
		    break;

		case 'h':			/* human */
		    sw_format |= (SW_SET | SW_HUMAN);
		    sw_format &= ~ SW_PARSE;
		    break;

		case 'p':			/* parseable */
		    sw_format |= (SW_SET | SW_PARSE);
		    sw_format &= ~ SW_HUMAN;
		    break;

		/**
		 **  Verbosity
		 **/

		case 'v':			/* verbose */
		    sw_verbose = 1;
		    break;

		case 's':			/* silent */
		    sw_detach = 1;
		    break;

		/**
		 **  Caching control
		 **/

		case 'c':			/* create */
		    sw_create = 1;
		    break;

		/**
		 **  Locating
		 **/

		case 'i':			/* icase */
		    sw_icase = 1;
		    break;

		/**
		 **  The user level comes as a string argument to the -u option
		 **/

		case 'u':			/* userlvl */
		    cmdModuleUser_sub( optarg);
		    break;

		/**
		 **  a special purpose flag for 'use' only
		 **/
		case 'a':			/* --append */
		    append_flag = 1;
		    break;

		case 'V':			/* version */
		    fprintf(stderr, "%s\n", version_string);
		    return ~TCL_OK;

		/**
		 **  All remaining options will influence their flags as defined
		 **  in the optlong table above.
		 **/

		case 0:
		    break;

		/**
		 **  Error messages for unknown options will be printed by
		 **  getopt ...
		 **/

		case '?':
		    break;

		/**
		 **  Well, this seems to be an internal error
		 **/

		default:
		    if( OK != ErrorLogger( ERR_GETOPT, LOC, NULL))
			return( TCL_ERROR);	/** --- EXIT (FAILURE) ----> **/
		    break;

	    }  /** switch() **/
	}  /** while() **/
    } /** if( argc) **/

    /**
     **  Special things to be dine for the 'silent' option: Pipe stderr
     **  output to /dev/null
     **/

    if( sw_detach) {
	sw_verbose = 0;
	if (!ttyname(2)) {
	    int temp_fd = open("/dev/null", O_RDWR);

	    close(2);
	    dup2(temp_fd, 2);
	}
    }

    /**
     **  Finally remove all options from the command line stream
     **/

    c = optind - 1;
    if( optind < *argc && c > 0) {
	while( optind < *argc) {
	    argv[ optind-c] = argv[ optind];
	    optind++;
	}
	*argc -= c;
    }

    /**
     **  Exit on success
     **/

#if WITH_DEBUGGING_INIT
    ErrorLogger( NO_ERR_END, LOC, _proc_Check_Switches, NULL);
#endif

    return( TCL_OK);

} /** End of 'Check_Switches' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		dup2						     **
 ** 									     **
 **   Description:	Duplicate file descriptor			     **
 ** 									     **
 **   First Edition:	96/02/08					     **
 ** 									     **
 **   Parameters:	int	old		Old file descriptor	     **
 **			int	new		New file descriptor	     **
 **									     **
 **   Result:		int	-1		any error		     **
 **				other		new file descriptor	     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

#ifndef HAVE_DUP2
int dup2( int old, int new)
{
    int fd;

    close(new);
    fd = dup(old);
    if (fd != new) {
	close( fd);
	errno = EMFILE;
	fd = -1;
    }

    return( fd);
}
#endif

