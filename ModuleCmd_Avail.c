/*****
 ** ** Module Header ******************************************************* **
 ** 									     **
 **   Modules Revision 3.0						     **
 **   Providing a flexible user environment				     **
 ** 									     **
 **   File:		Modulate_Avail.c				     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Authors:	John Furlan, jlf@behere.com				     **
 **		Jens Hamisch, jens@Strawberry.COM			     **
 **		R.K. Owen, <rk@owen.sj.ca.us> or <rkowen@nersc.gov>	     **
 ** 									     **
 **   Description: 	This module command prints out the modulefiles that  **
 **			are available in the directories listed in the	     **
 **			MODULEPATH environment variable.		     **
 ** 									     **
 **   Exports:		ModuleCmd_Avail					     **
 **			print_aligned_files				     **
 **			check_dir					     **
 **			get_dir						     **
 **			dirlst_to_list					     **
 **			delete_dirlst					     **
 **			delete_cache_list				     **
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

static char Id[] = "@(#)$Id: ModuleCmd_Avail.c,v 1.20.2.1 2009/09/14 22:08:48 rkowen Exp $";
static void *UseId[] = { &UseId, Id };

/** ************************************************************************ **/
/** 				      HEADERS				     **/
/** ************************************************************************ **/

#include <time.h>
#include "modules_def.h"

/** ************************************************************************ **/
/** 				  LOCAL DATATYPES			     **/
/** ************************************************************************ **/

/**
 **  Structure for a linked list that stores directories to be listed.
 **/

typedef struct _subdir_node {
  fi_ent*              sd_dir;
  struct _subdir_node* sd_next;
} sd_node;

/** ************************************************************************ **/
/** 				     CONSTANTS				     **/
/** ************************************************************************ **/

/**
 **  I tried having a test for isgraph() in the configuration file,
 **  but it fails on AIX. This is the best I could come up with...
 **/

#if !defined(isgraph) && defined(_P) && defined(_N)
#  define isgraph(c)	((_ctype_+1)[c]&(_P|_U|_L|_N))
#endif

#define DIREST  50

/** ************************************************************************ **/
/**				      MACROS				     **/
/** ************************************************************************ **/

/** not applicable **/

/** ************************************************************************ **/
/** 				    LOCAL DATA				     **/
/** ************************************************************************ **/

static	char	 buffer[MOD_BUFSIZE];
static	char	 buf[ LINELENGTH];
static	char	 module_name[] = __FILE__;
static	char	 short_format[] = "%s";
static	char	 short_format_part[] = "%s/%s";
static	char	 short_format_full[] = "%s/%s(%s)";
static	char	 long_format[] = "%-39.39s  %-10.10s  %17s\n";
 	char	 long_header[] = "\
- Package -----------------------------+- Versions -+- Last mod. ------\n";

/**
 **  Terse file list buffer
 **/

#define	FILE_LIST_SEGM_SIZE 100
static	char	  _file_list_buffer[ 200];
static	char	**_file_list_ptr = (char **) NULL;
static	int	  _file_list_cnt = 0;
static	int	  _file_list_wr_ndx = 0;
static	int	  _file_list_rd_ndx = 0;

/** ************************************************************************ **/
/**				    PROTOTYPES				     **/
/** ************************************************************************ **/

static	int	  print_dir( Tcl_Interp*, char*, char*);
static	void	  print_spaced_file( char*, int, int, int);
static	char	 *mkdirnm( char*, char*);
static	int	  fi_ent_cmp( const void*, const void*);
static	void	  _init_file_list(void);
static	void	  _add_file_list( char *name);
static	char	 *_get_file_list(void);
static	char	 *_pick_file_list( int ndx);
static	void	  print_terse_files( int terminal_width, int len, char *header,
			int numbered);

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		ModuleCmd_Avail					     **
 ** 									     **
 **   Description:	Execution of the 'module avail' command		     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	Tcl_Interp	*interp		Current Tcl Interpr. **
 **			char		*argv[]		Arguments to the     **
 **							command		     **
 ** 									     **
 **   Result:		int	TCL_OK		Successful operation	     **
 **				TCL_ERROR	Any failure		     **
 ** 									     **
 **   Attached Globals:	g_specified_module	The module name from the     **
 **						command line.		     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

int ModuleCmd_Avail(
	Tcl_Interp * interp,
	int argc,
	char *argv[]
) {
	char          **dirname;
	uvec           *modpath;
	int             Result = -TCL_ERROR;

    /**
     **  Load the MODULEPATH and split it into a list of paths. Assume success
     **  if no list to output ...
     **/
	if (!(modpath = ModulePathList()))
		goto unwind0;
	if (!uvec_number(modpath)) {
		Result = TCL_OK;
		goto success0;
	}
    /**
     **  If we're given a full-path, then we'll just check that directory.
     **  Otherwise, we'll check every directory in MODULESPATH.
     **/
	if (argc > 0 && **argv == *psep) {
		while (argc--) {
	    /**
	     ** Set the name of the module specified on the command line
	     **/
			g_specified_module = *argv;

			if (!check_dir(*argv)) {
				if (OK != ErrorLogger(ERR_PARAM, LOC, NULL)) {
					Result = TCL_ERROR;
					/** --- EXIT PROCEDURE (FAILURE) --> **/
				}
			} else {
				print_dir(interp, *argv, NULL);
			}
			argv++;
		}
	} else {
	/**
	 **  We're not given a full path. Tokenize the module path string and
	 **  print the contents of each directory specified (if it exists ;-)
	 **/
		if (sw_format & SW_LONG)
			fprintf(stderr,"%s", _(long_header));

	/**
	 **  If a module category is specified check whether it is part
	 **  of the directory we're scanning at the moment.
	 **/
		if (argc > 0) {	/* show sub directory */
			while (argc--) {
		/**
		 ** Set the name of the module specified on the command line
		 **/
				g_specified_module = *argv;

				dirname = uvec_vector(modpath);
				while (dirname && *dirname) {
		    /**
		     **  Print the category
		     **/
					if (check_dir(*dirname))
					    print_dir(interp,*dirname,*argv);
					dirname++;
				}
				argv++;
			}
	/**
	 **  Otherwise, if there's no category given, descend the current
	 **  directory and print its contents.
	 **/
		} else {
			dirname = uvec_vector(modpath);
			while (dirname && *dirname) {
				if (check_dir(*dirname))
					print_dir(interp, *dirname, NULL);
				dirname++;
			}
		} /** argc **/
	} /** if( no full path name given) **/
    /**
     **  Free up what has been allocated and exit from this procedure
     **/
	/* if got here via this path ... it must have been OK */
	if (Result < 0)
		Result = TCL_OK;

	FreeList(&modpath);

unwind0:
	/* if Result is negative here ... must have been an unwind */
	if (Result < 0)
		Result = -Result;

success0:
	return (Result);	/** --- EXIT PROCEDURE (FAILURE/SUCCESS) --> **/

} /** End of 'ModuleCmd_Avail' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		check_dir					     **
 ** 									     **
 **   Description:	Open and close the passed directory in order to check**
 **			if it does exist and is readable		     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	char	*dirname	Name of the directory to be  **
 **						checked			     **
 ** 									     **
 **   Result:		int	0	Not a directory or unreadable	     **
 **				1	OK				     **
 ** 									     **
 **   Attached Globals:	-						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

int check_dir(	char	*dirname)
{
    DIR*   dirp;

    if( !(dirp = opendir( dirname))) 
	return( 0);

    if( -1 == closedir( dirp))
	if( OK != ErrorLogger( ERR_CLOSEDIR, LOC, dirname, NULL))
	    return( 0);

    return( 1);

} /** End of 'check_dir' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		print_dir					     **
 ** 									     **
 **   Description:	Print all files beyond the passed directory	     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	char	*dir		Directory to be scanned	     **
 **			char	*module		A selcted module name or NULL**
 ** 									     **
 **   Result:		int	TCL_OK		Successful operation	     **
 ** 									     **
 **   Attached Globals:	-						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static int print_dir(
	Tcl_Interp * interp,
	char *dir,
	char *module
) {
	fi_ent         *dirlst_head = NULL;
					/** Directory list base pointer	     **/
	int             count = 0,	/** Number of elements in the top    **/
					/** level directory list	     **/
			tcount = 0,	/** Total number of files to print   **/
			start = 0,
			dirlen;
	char          **cache_list = NULL;
	char           *selection, *s;

    /**
     **  Print the directory name
     **/
	if ((sw_format & (SW_PARSE | SW_TERSE | SW_LONG))
	    && !(sw_format & (SW_HUMAN | SW_LIST))) {
		fprintf(stderr, "%s:\n", dir);
	}

	if (dir)
		dirlen = strlen(dir) + 1;
	else
		dirlen = 0;

    /**
     **  If the is a module selection given, build the whole selected path
     **/

	if (module) {
		if (dir) {
			if ((char *)NULL == (selection = stringer(NULL, 0,
					  dir, psep, module, NULL))) {
				ErrorLogger(ERR_STRING, LOC, NULL);
				goto unwind0;	/** ---- EXIT (FAILURE) ---> **/
			}
		} else
			selection = module;
	} else
		selection = (char *)NULL;

	if (!cache_list) {
	/**
	 **  Normal reading of the files
	 **/
		if (NULL == (dirlst_head = get_dir(dir, NULL, &count, &tcount)))
			if (OK != ErrorLogger(ERR_READDIR, LOC, dir, NULL))
				goto unwind1;
		if (NULL == (cache_list =
		     (char **)module_malloc(tcount * sizeof(char **))))
			if (OK != ErrorLogger(ERR_ALLOC, LOC, NULL))
				goto unwind1;
		(void)memset(cache_list, 0, tcount * sizeof(char **));

		start = 0;
		dirlst_to_list(cache_list, dirlst_head, count, &start, dir,
			       selection);
	}
    /**
     **  In case of any selection, we have to force all .modulrc's and .versions
     **  on the path
     **/
	if (dir) {
		s = dir;
		while (s) {
			if ((s = strchr(s, *psep)))
				*s = '\0';
			else
				break;

			SourceRC(interp, dir, modulerc_file);
			SourceVers(interp, dir, module);

			if (s)
				*s++ = *psep;
		}
	/**
	 **  Finally source the rc files in the directory itself
	 **/
		SourceRC(interp, dir, modulerc_file);
		SourceVers(interp, dir, module);
	}

	if (dir && selection)
		null_free((void *)&selection);
    /**
     **  Print and remove the cache list
     **/
	delete_dirlst(dirlst_head, count);
	print_aligned_files(interp, dir, dir, cache_list, tcount,
			    (sw_format & SW_LIST ? 1 : -1));
	delete_cache_list(cache_list, start);

	if (sw_format & SW_LONG)
		fprintf(stderr, "\n");
	return (TCL_OK);		/** ------- EXIT (SUCCESS) --------> **/

unwind1:
	if (dir && selection)
		null_free((void *)&selection);
unwind0:
	return (TCL_ERROR);		/** ------- EXIT (FAILURE) --------> **/

} /** End of 'print_dir' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		get_dir						     **
 ** 									     **
 **   Description:	Read in the passed directory and save every interes- **
 **			ting item in the directory list			     **
 **			skipping known version control directories:	     **
 **				CVS RCS .git .svn			     **
 **			unless they contain .version files		     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	char	*dir		Directory to be read	     **
 **			char	*prefix		Directory prefix (path)	     **
 **			int	*listcount	Buffer to store the number of**
 **						elements in the current      **
 **						directory list		     **
 **			int 	*total_count	Buffer for the total number  **
 **						of files read		     **
 ** 									     **
 **   Result:		fi_ent*		NULL	Failure			     **
 **					else	Directory list base pointer  **
 **			*listcount		Number of elements in the    **
 **						top level directory list     **
 **			*total_count		Total number of files read   **
 ** 									     **
 **   Attached Globals:	-						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

fi_ent	*get_dir(	char	*dir,
			char	*prefix,
			int	*listcount,
			int	*total_count)
{
    struct dirent	*dp;		/** Directory read pointer	     **/
    DIR			*dirptr;	/** Directory handle		     **/
    fi_ent		*dirlst_head,	/** Directory list pointers. Head,   **/
    			*dirlst_cur,	/** current			     **/
    			*dirlst_last;	/** and last			     **/
    char		*dirname;	/** Expanded directory path name     **/
    char		*tmp;		
    int			 count = 0;

    /**
     **  Open the desired directiory
     **/
    if( !(dirptr = opendir( dir))) {
#if 0
	/* if you can't open a directory ... is that really an error? */
	if( OK != ErrorLogger( ERR_OPENDIR, LOC, dir, NULL))
#endif
	    return( NULL);	/** ----------- EXIT (FAILURE) ------------> **/
    }

    /**
     **  Allocate memory for reading in the directory
     **/

    if( NULL == (dirlst_cur = dirlst_head = (fi_ent*) module_calloc( DIREST,
	sizeof( fi_ent)))) {
	if( OK != ErrorLogger( ERR_ALLOC, LOC, NULL)) {
	    if( -1 == closedir( dirptr))
		ErrorLogger( ERR_CLOSEDIR, LOC, dir, NULL);
	    goto unwind0;
	}
    }
    dirlst_last = dirlst_head + DIREST;
  
    /**
     **  Read in the  contents of the directory. Ignore dotfiles
     **		and version directories.
     **/

    for( count = 0,  dp = readdir( dirptr); dp != NULL; dp = readdir( dirptr)) {
        if( *dp->d_name == '.') continue;

	/**
	 **  Conditionally double up the space allocated for reading the direc-
	 **  tory
	 **/

        if(dirlst_cur == dirlst_last) {
            if( NULL == (dirlst_head = (fi_ent*) module_realloc(
		(char*) dirlst_head, (count<<1) * sizeof( fi_ent)))) 
		if( OK != ErrorLogger( ERR_ALLOC, LOC, NULL)) 
		    goto unwind0;
            dirlst_cur = dirlst_head + count;
            dirlst_last = dirlst_head + (count<<1);
	}

	/**
	 **  Build the complete path name and get information about the file
	 **/

        if( !( dirname = mkdirnm( dir, dp->d_name)))
	    if( OK != ErrorLogger( ERR_DIRNAME, LOC, NULL)) {
		if( -1 == closedir( dirptr))
		    ErrorLogger( ERR_CLOSEDIR, LOC, dir, NULL);
		goto unwind1;
	    }

        if( stat( dirname, &(dirlst_cur->fi_stats)) < 0)
	    if( OK != ErrorLogger( ERR_DIRNOTFOUND, LOC, dirname, NULL)) {
		if( -1 == closedir( dirptr))
		    ErrorLogger( ERR_CLOSEDIR, LOC, dir, NULL);
		goto unwind1;
	    }

	/**
	 **  If it is a directory, recursively delve into it ..
	 **/

        if(dirlst_cur->fi_stats.st_mode & S_IFDIR) {
            char* np;
            char* ndir;
            int   tmpcount = 0;

	    /**
	     **  Build the new base points for the recursion
	     **/

            if( !( tmp = mkdirnm( prefix, dp->d_name))) {
		if( OK != ErrorLogger( ERR_DIRNAME, LOC, NULL)) {
		    if( -1 == closedir( dirptr))
			ErrorLogger( ERR_CLOSEDIR, LOC, dir, NULL);
		    goto unwind1;
		}
	    } else {
		if( NULL == (np = stringer(NULL,0, tmp, NULL)))
		    if( OK != ErrorLogger( ERR_ALLOC, LOC, NULL))
			goto unwind1;
	    }

            if( !( tmp = mkdirnm( dir, dp->d_name))) {
		if( OK != ErrorLogger( ERR_DIRNAME, LOC, NULL)) {
		    if( -1 == closedir( dirptr))
			ErrorLogger( ERR_CLOSEDIR, LOC, dir, NULL);
		    goto unwind1;
		}
	    } else {
		if( NULL == (ndir = stringer(NULL,0, tmp, NULL)))
		    if( OK != ErrorLogger( ERR_ALLOC, LOC, NULL)) 
			goto unwind1;
	    }

	    /**
	     **  What if it's a known version control directory
	     **  check if it has a .version file
	     **/
	    if (!strcmp("CVS",dp->d_name)
	    ||  !strcmp("RCS",dp->d_name)
	    ||  !strcmp(".git",dp->d_name)
	    ||  !strcmp(".svn",dp->d_name)) {
    		FILE	*fi;
		if( (char *) NULL == stringer(buffer, MOD_BUFSIZE,
		    tmp, "/.version", NULL))
		    if( OK != ErrorLogger( ERR_STRING, LOC, NULL))
			goto unwind1;
		if( NULL == (fi = fopen( buffer, "r"))) {
			/* does not have a .version file */
			continue;
		} else {
			/* has a .version file ... assume to be module dir */
			fclose(fi);
		}
	    }

	    /**
	     **  The recursion itself ...
	     **/

            dirlst_cur->fi_subdir = get_dir( ndir,np,&dirlst_cur->fi_listcount,
		&tmpcount);

            /**
             **  Add the number of real modulefiles (i.e. not subdirs and 
             **  not non-modulefiles) to our total number of modulefiles 
             **  contained in the structure.
             **/

            *total_count += tmpcount;

            /**
             **  This means that it's an empty directory so the prefix is
	     **  never used
             **/

            if( !dirlst_cur->fi_listcount)
		null_free((void *) &np);
            null_free((void *) &ndir);

	/**
	 **  if it is not a directory check the magic cookie of the file. Only
	 **  files with the modules magic cookie will be accepted. Also tem-
	 **  porary files are to be ignored.
	 **/

        } else if( dp->d_name[NLENGTH(dp)-1] == '~' ||
	    !check_magic( dirname, MODULES_MAGIC_COOKIE,
		MODULES_MAGIC_COOKIE_LENGTH)) {
	    continue;
	} else {
	    dirlst_cur->fi_subdir = NULL;
	}

	/**
	 **  Put the name of the file on the directory list
	 **/

	dirlst_cur->fi_prefix = prefix;
	if( NULL == (dirlst_cur->fi_name = stringer(NULL,0, dp->d_name, NULL)))
	    if( OK != ErrorLogger( ERR_ALLOC, LOC, NULL)) 
		goto unwind1;

	/**
	 **  Count even the number of elements in the current list as the
	 **  total number of elements read in so far.
	 **  Increment the list index to be prepared for the next entry.
	 **/

	count++;
	(*total_count)++;
	dirlst_cur++;

    } /** for **/

    /**
     **  Now sort alphabetically what has been read
     **/

    if( count > 1)
	qsort( dirlst_head, count, sizeof(fi_ent), fi_ent_cmp);

    /**
     **  Close the directory, set up return values
     **/

    if( -1 == closedir( dirptr))
	if( OK != ErrorLogger( ERR_CLOSEDIR, LOC, dir, NULL))
	    goto unwind1;

    *listcount = count;
    return( dirlst_head); 	/** ----------- EXIT (SUCCESS) ------------> **/

unwind1:
    null_free((void *) &dirlst_cur);

unwind0:
    return( NULL);	 	/** ----------- EXIT (FAILURE) ------------> **/
} /** End of 'get_dir' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		dirlst_to_list					     **
 ** 									     **
 **   Description:	Transform the passed nested directory list into a    **
 **			flat list of strings				     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	char	**list		List to be created	     **
 **			fi_ent	 *dirlst_head	Head of the directory list   **
 **						to be transformed	     **
 **			int	  count		Number of elements in the    **
 **						directory list		     **
 **			int	 *beginning	Index of the element in List **
 **						to start appending the file- **
 **						names at.		     **
 **			char	 *path		prepend pathname to list     **
 **			char	 *module	A search pattern	     **
 **									     **
 **   Result:		-						     **
 ** 									     **
 **   Attached Globals:	-						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

void	dirlst_to_list(	char	**list,
			fi_ent	 *dirlst_head,
			int	  count,
			int	 *beginning,
			char	 *path,
			char	 *module)
{
    fi_ent	*dirlst_cur;	
    int 	 i;
    char	*ptr;
    int		 mlen;

    /**
     **  If there's any selection given, figure out its length
     **/

    if( module)
	mlen = strlen( module);

    /**
     **  Put all files in the directory list at the end of the passed list
     **  of character arrays
     **/
    for( i=0, dirlst_cur=dirlst_head;
	 i<count && dirlst_cur;
         i++, dirlst_cur++) {

        if( dirlst_cur->fi_prefix) {

	    if( path) {
		if( (char *) NULL == stringer(buf, MOD_BUFSIZE,
		    path,psep, dirlst_cur->fi_prefix,psep, dirlst_cur->fi_name,
		    NULL))
			return;
	    } else {
		if( (char *) NULL == stringer(buf, MOD_BUFSIZE,
		    dirlst_cur->fi_prefix,psep, dirlst_cur->fi_name, NULL))
			return;
	    }

	    ptr = buf;
        } else {
	    if( path) {
		if( (char *) NULL == stringer(buf, MOD_BUFSIZE,
		    path,psep, dirlst_cur->fi_name, NULL))
			return;
		ptr = buf;

	    } else
		ptr = dirlst_cur->fi_name;
        }

	/**
	 **  Check whether this is part of the selected modules ...
	 **/

	if( !module || !strncmp( module, buf, mlen)) {

	    /**
	     **  Put this guy on the list
	     **/

	    if( NULL == (list[(*beginning)++] = stringer(NULL,0, ptr, NULL))) {
		if( OK != ErrorLogger( ERR_ALLOC, LOC, NULL)) {
		    while( i--) 
			null_free((void *) list + (--(*beginning)));
		    return;		/** ------- EXIT (FAILURE) --------> **/
		}
	    }
	}

	/**
	 **  recursively descend to subdirectories
	 **/

        if( dirlst_cur->fi_subdir)
            dirlst_to_list( list, dirlst_cur->fi_subdir,
		dirlst_cur->fi_listcount, beginning, path, module);
    } /** for **/

} /** end of 'dirlst_to_list' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		delete_dirlst					     **
 ** 									     **
 **   Description:	Delete an entire directory list including all sub-   **
 **			directory lists					     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	fi_ent	*dirlst_head	Head of the list to be re-   **
 **						moved			     **
 ** 									     **
 **   Result:		-						     **
 ** 									     **
 **   Attached Globals:	-						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

void	delete_dirlst(	fi_ent	*dirlst_head,
			int	 count)
{
    fi_ent	*dirlst_cur;
    int 	 i;

    if( !dirlst_head)
	return;

    /**
     **  Free all filenames stored in the list
     **/

    for( i=0, dirlst_cur=dirlst_head;
	 i<count && dirlst_cur;
         i++, dirlst_cur++) {

        null_free((void *) &(dirlst_cur->fi_name));

	/**
	 **  Recursivle decend to subdirectories
	 **/

        if( dirlst_cur->fi_subdir) 
            delete_dirlst( dirlst_cur->fi_subdir, dirlst_cur->fi_listcount);
    } /** for **/

    /**
     **  Remove the entire list
     **/

    if( dirlst_head->fi_prefix)
	null_free((void *) &(dirlst_head->fi_prefix));
    null_free((void *) &dirlst_head);

} /** End of 'delete_dirlst' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		delete_cache_list				     **
 ** 									     **
 **   Description:	Remove an entire list of allocated strings and free  **
 **			up the used memory				     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	char	**list		List of filenames to be print**
 **			int	  tcount	Size ofd the list in elements**
 ** 									     **
 **   Result:		-						     **
 ** 									     **
 **   Attached Globals:	-						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

void delete_cache_list(	char	**list,
			    int	  tcount)
{
    int i;

    for( i=0; i<tcount; i++)
        null_free((void *) (list + i));
    
    null_free((void *)&list);

} /** End of 'delete_cache_list' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		print_aligned_files				     **
 ** 									     **
 **   Description:	Print out the filenames passed in a sorted array     **
 **			column by column taking care of the order being re-  **
 **			flected to the single columns			     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	char	**list		List of filenames to print   **
 **			char	 *path		common path		     **
 **			char	 *header	List header		     **
 **			int	  tcount	Size of the list in elements **
 **			int	  numbered	Controls printing of numbers **
 ** 						set to -1 for none	     **
 ** 									     **
 **   Result:		-						     **
 ** 									     **
 **   Attached Globals:	g_current_module	The module which is handled  **
 **						by the current command	     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

void print_aligned_files(
	Tcl_Interp * interp,
	char *path,
	char *header,
	char **list,
	int tcount,
	int numbered
) {
	struct stat     stats;
	struct tm      *tm;
	char           *symbols, *module, *release;
	char            buffer[20];
	char            modulefile[MOD_BUFSIZE];
	char            modulename[MOD_BUFSIZE];
	char           *timestr;
	char           *s;
	int             t;
	int             terminal_width = 80;
	int             maxlen = 0;
	uvec           *modpath;

    /**
     **  In case of terse, human output we need to obtain the size of 
     **  the tty
     **/
	if (sw_format & (SW_HUMAN | SW_LONG)) {
		struct winsize  window_size;
		int             fd_err = fileno(stderr);

		if (isatty(fd_err))
			if (ioctl(fd_err, TIOCGWINSZ, &window_size) != -1)
				terminal_width = (window_size.ws_col == 0) ?
				    80 : window_size.ws_col;
	}

	if (!path) {
    /**
     **  For listing loaded modules ...
     **  Load the MODULEPATH and split it into a list of paths.
     **/
		if (!(modpath = ModulePathList())
		    && (!uvec_number(modpath)))
			if (OK != ErrorLogger(ERR_MODULE_PATH, LOC, NULL))
				return;
	}
    /**
     **  Scan all entries of the passed list
     **/
	_init_file_list();
	while (list && tcount-- && *list) {
	/**
	 ** find module[/version] in filename
	 **/
		if ((g_current_module = s = strrchr(*list, *psep))) {
			*s = 0;
			g_current_module++;
			if (TCL_ERROR ==
			    Locate_ModuleFile(interp, g_current_module,
					      modulename, modulefile)) {
				g_current_module = strrchr(*list, *psep);
				g_current_module++;
			}
			*s = *psep;
		}
		if (!stat(*list, &stats)) {
	    /**
	     **  If the file is a directory, try to source the .modulerc
	     **  file and skip to the next file
	     **/
			if (S_ISDIR(stats.st_mode)) {
				SourceRC(interp, *list, modulerc_file);
				SourceVers(interp, *list, g_current_module);
				g_current_module = (char *)NULL;
				list++;
				continue;
			}
	    /**
	     **  For listing loaded modules ...
	     **/
			if (!path) {
				size_t          pathlen, maxPrefixLength = 0;
				char          **dirname = uvec_vector(modpath);
		    /**
		     ** try to find the longest prefix from the module path
		     **/
				while (dirname && *dirname) {
					pathlen = strlen(*dirname);
					if (!strncmp(*list,*dirname,pathlen)
					&& (pathlen > maxPrefixLength)) {
						/* partial match */
						maxPrefixLength = pathlen;
					}
					dirname++;
				}
		    /**
		     ** Skip over *psep
		     **/
				if (maxPrefixLength > 0)
					maxPrefixLength += 1;

				module = stringer(NULL, 0,
						*list + maxPrefixLength, NULL);
			} else {
				t = strlen(path);
				if (*(*list + t) == *psep)
					t++;
				module = stringer(NULL, 0, *list + t, NULL);
			}
			if ((char *)NULL == module) {
				if (OK != ErrorLogger(ERR_ALLOC, LOC, NULL))
					break;
				else
					continue;
			}
	    /**
	     **  Expand the symbols and get the version of the module
	     **/
			if ((char *)NULL == (symbols = ExpandVersions(module)))
				symbols = "";

			if ((sw_format & SW_LONG)
			    || (char *)NULL == (release = strchr(module, *psep)))
				release = "";	/* no release info */
			else
				*release++ = '\0';
	    /**
	     **  Long or short format
	     **/
			if (sw_format & (SW_TERSE | SW_PARSE | SW_HUMAN)) {
				/** short format **/
				int             tmp_len;

				if (sw_format & SW_PARSE) {
					sprintf(_file_list_buffer,
						short_format_full, module,
						release, symbols);
				} else { /* assume a human readable format */
					if (*symbols) {
						sprintf(_file_list_buffer,
							short_format_full,
							module, release,
							symbols);
					} else {
						if (*release) {
							sprintf
							    (_file_list_buffer,
							     short_format_part,
							     module, release);
						} else {
							sprintf
							    (_file_list_buffer,
							     short_format,
							     module);
						}
					}
				}
				_add_file_list(_file_list_buffer);

				tmp_len = strlen(_file_list_buffer);
				if (tmp_len > maxlen)
					maxlen = tmp_len;

			} else if (sw_format & SW_LONG) {
				/** long format **/
		/**
		 **  Get the time of last modification
		 **/
				if ((struct tm *)NULL !=
				    (tm = gmtime(&stats.st_mtime))) {
					sprintf(buffer,
						"%04d/%02d/%02d %2d:%02d:%02d",
						1900 + tm->tm_year,
						tm->tm_mon + 1, tm->tm_mday,
						tm->tm_hour, tm->tm_min,
						tm->tm_sec);
					timestr = buffer;

				} else
					timestr = "";
		/**
		 **  Now print and free what we've allocated
		 **/
				fprintf(stderr, long_format, module, symbols,
					timestr);
			}
			null_free((void *)&module);
		} /** if( !stat) **/
		list++;
	} /** while **/
    /**
     **  In case of terse output we have to flush our buffer
     **/
	if (!(sw_format & SW_LONG)) {
		if (_file_list_wr_ndx > 0)
			print_terse_files(terminal_width, maxlen, header,
					  numbered);
	}
	if (!path)
		FreeList(&modpath);

} /** End of 'print_aligned_files' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		print_terse_files				     **
 ** 									     **
 **   Description:	Print out the filenames in the _file_list array in   **
 **			case of terse output 				     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	int	  terminal_width	Terminal size	     **
 **			int	  len			max. filename length **
 **			char	 *header		header to print	     **
 **			int	  number		value to start number**
 ** 							use -1 for none	     **
 ** 									     **
 **   Result:		-						     **
 ** 									     **
 **   Attached Globals:	-						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static	void	print_terse_files(  int terminal_width,
				    int len,
				    char *header,
				    int numbered)
{
    char	*module;
    char	*moduleright;

    /**
     **  Print human readable lists
     **/

    len += (numbered != -1 ? 6 : 1);
    if( sw_format & SW_HUMAN ) {
	int columns = (terminal_width - 1) / len;
	int col_ndx, row_ndx;
	int rows;
        int mod_ndx;
        
	/**
	 **  Print the header line
	 **/

	if( header) {
	    int lin_len = terminal_width - strlen( header) - 2;
	    int i;

	    fprintf( stderr, "\n");

	    if( lin_len >= 2) 
		for( i = 0; i < lin_len / 2; i++)
		    fprintf( stderr, "-");

	    fprintf( stderr, " %s ", header);

	    if( lin_len >= 2)
		for( i = 0; i < (lin_len+1) / 2; i++)
		    fprintf( stderr, "-");

	    fprintf( stderr, "\n");
	}

	/**
	 **  Print the columns
	 **/

        
	if( !columns)
	    columns = 1;
	rows = (_file_list_wr_ndx + columns - 1) / columns;

        for( row_ndx = 0; row_ndx < rows; row_ndx++) {
          for( col_ndx = 0; col_ndx < columns; col_ndx++) { 
            
            mod_ndx = row_ndx + col_ndx * rows;
            
            if( (module = _pick_file_list( mod_ndx)) ) {
              moduleright = _pick_file_list( row_ndx + (col_ndx+1)* rows);

              print_spaced_file( module, len,
                                 ( (col_ndx == columns - 1)
                                   || (moduleright == (char *) NULL)
                                   ? 0 : 1 ),
                                  ( (numbered == -1) ? numbered : ++mod_ndx) );
		}
	    }
	    fprintf( stderr, "\n");
	}
    }

    /**
     **  Print parseable lists
     **/

    else {
	while( (module = _get_file_list()) ) {
	    fprintf( stderr, "%s\n", module);
	}
    }
}

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		_add_file_list   				     **
 **			_init_file_list					     **
 **			_get_file_list					     **
 **			_pick_file_list					     **
 ** 									     **
 **   Description:	File list functions for terse module display mode    **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	char	*name		Name to be stored	     **
 ** 									     **
 **   Result:		-						     **
 ** 									     **
 **   Attached Globals:	-						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static	void _init_file_list()
{
    if( _file_list_ptr && !_file_list_cnt) {
	null_free((void *) &_file_list_ptr);
	_file_list_cnt = 0;
    }

    _file_list_wr_ndx = 0;
    _file_list_rd_ndx = 0;
}

static	void _add_file_list( char *name)
{
    /**
     **  Parameter check
     **/

    if( !name || !*name)
	return;

    /**
     **  Reallocate if the current array is to small
     **/

    if( _file_list_cnt <= _file_list_wr_ndx) {
	_file_list_cnt += FILE_LIST_SEGM_SIZE;

	if( !_file_list_ptr) 
	    _file_list_ptr =
		(char **) module_malloc(_file_list_cnt * sizeof(char *));
	else
	    _file_list_ptr = (char **) module_realloc( _file_list_ptr,
		_file_list_cnt * sizeof(char *));

    }

    /**
     **  Save the passed name, if the allocation succeeded
     **/

    if( !_file_list_ptr) {
	ErrorLogger( ERR_ALLOC, LOC, NULL);
	_file_list_cnt = 0;
	_file_list_wr_ndx = 0;
	_file_list_rd_ndx = 0;

    } else {
	_file_list_ptr[ _file_list_wr_ndx++] = stringer(NULL,0, name, NULL);
    }
}

static	char *_get_file_list()
{
    return((_file_list_rd_ndx < _file_list_wr_ndx) ?
	 _file_list_ptr[ _file_list_rd_ndx++] : (char *) NULL);
}

static	char *_pick_file_list( int ndx)
{
    return((ndx < _file_list_wr_ndx) ? _file_list_ptr[ ndx] : (char *) NULL);
}

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		print_spaced_file				     **
 ** 									     **
 **   Description:	Print out the passed filename and fill the output    **
 **			area up to the passed number of characters	     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	char	*name		Name to be printed	     **
 **			int	 maxwidth	With of the output field to  **
 **						be filled up		     **
 **			int	 space		Boolean value controlling if **
 **						the output area should be    **
 **						filled up with spaces or not **
 **			int	 number		value to start number list   **
 ** 						use -1 for none		     **
 ** 									     **
 **   Result:		-						     **
 ** 									     **
 **   Attached Globals:	-						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static	void print_spaced_file(	char	*name,
				int	 maxwidth,
				int	 space,
				int	 number)
{
    int filelen;		/** Length of the filename to print	     **/

    chk4spch( name);		/** turn any weird characters into ? marks   **/

    /**
     **  Print the name and calculate its length
     **/

    filelen = strlen( name);
    if( -1 != number) {
	fprintf( stderr, "%3d) ", number);
	filelen += 5;
    }

    fprintf(stderr, "%s",  name);

    /**
     **  Conditionally fill the output area with spaces
     **/

    if( space) {
	putc(' ', stderr);
	while( ++filelen < maxwidth)
	    putc(' ', stderr);
    }

} /** end of 'print_spaced_file' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		mkdirnm						     **
 ** 									     **
 **   Description:	Build a full pathname out of the passed directory    **
 **			and file					     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	char	*dir		The directory to be used     **
 **			char	*file		The filename w/o path	     **
 ** 									     **
 **   Result:		char*	NULL		Compound filename to long    **
 **				else		Pointer to the full path     **
 ** 									     **
 **   Attached Globals:	-						     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static	char *mkdirnm(	char	*dir,
			char	*file)
{
    static char  dirbuf[ MOD_BUFSIZE];	/** Buffer for path creation	     **/

    /**
     **  If only a file is given, or the file is in the current directory
     **  return just the file.
     **/
    if( dir == NULL || *dir == '\0' || !strcmp(dir,"."))
	return( strcpy( dirbuf, file));

    /**
     **  Check whether the full path fits into the buffer
     **/
    if( (int) ( strlen( dir) + 1 + strlen( file) + 1 ) > MOD_BUFSIZE) {
	if( OK != ErrorLogger( ERR_NAMETOLONG, LOC, dir, file, NULL))
	    return( NULL);
    }

    /**
     **  Copy directory and file into the buffer taking care that there will
     **  be no double slash ...
     **/

    strcpy( dirbuf, dir);
    if( dir[ strlen( dir) - 1] != *psep && file[0] != *psep)
	strcat( dirbuf, psep);
    return( strcat( dirbuf, file));

} /** End of 'mkdirnm' **/

/*++++
 ** ** Function-Header ***************************************************** **
 ** 									     **
 **   Function:		fi_ent_cmp					     **
 ** 									     **
 **   Description:	compares two file entry structures		     **
 **			Different cmdline arguments (i.e. -u, -c, -t, -z)    **
 **			will change what value is compared. As a default,    **
 **			the name is used.				     **
 **									     **
 **   Notes:		This procedure is used as comparison function for    **
 **			qsort()					 	     **
 ** 									     **
 **   First Edition:	1991/10/23					     **
 ** 									     **
 **   Parameters:	const void *fi1		First file entry	     **
 **			const void *fi2		Second one to compare	     **
 ** 									     **
 **   Result:		int	1 	fi2 > fi1			     **
 **    				-1	fi2 < fi1			     **
 **	  			0	fi2 == fi1			     **
 ** 									     **
 **   Attached Globals:							     **
 ** 									     **
 ** ************************************************************************ **
 ++++*/

static	int fi_ent_cmp(	const void	*fi1,
			const void	*fi2)
{

#ifdef DEF_COLLATE_BY_NUMBER
  return colcomp( ((fi_ent*)fi1)->fi_name, ((fi_ent*)fi2)->fi_name);
#else
  return strcmp( ((fi_ent*)fi1)->fi_name, ((fi_ent*)fi2)->fi_name);
#endif
}
