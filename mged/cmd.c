/*
 *			C M D . C
 *
 * Functions -
 *	cmdline		Process commands typed on the keyboard
 *	parse_line	Parse command line into argument vector
 *	f_press		hook for displays with no buttons
 *	f_summary	do directory summary
 *	mged_cmd		Check arg counts, run a command
 *
 *  Authors -
 *	Michael John Muuss
 *	Charles M. Kennedy
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "./sedit.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"
#include "./dm.h"

extern void	perror();
extern int	atoi(), execl(), fork(), nice(), wait();
extern long	time();
extern void	sync();

#define	MAXARGS		2000	/* Maximum number of args per line */
int	maxargs = MAXARGS;	/* For dir.c */
int	inpara;			/* parameter input from keyboard */
int	numargs;		/* number of args */
char	*cmd_args[MAXARGS+2];	/* array of pointers to args */

extern int	cmd_glob();

static void	f_help(), f_fhelp(), f_param(), f_comm();
void	mged_cmd();
void	f_center(), f_press(), f_view(), f_blast();
void	f_edit(), f_evedit(), f_delobj(), f_hideline();
void	f_debug(), f_regdebug(), f_debuglib(), f_debugmem();
void	f_name(), f_copy(), f_instance();
void	f_copy_inv(), f_killall(), f_killtree();
void	f_region(), f_itemair(), f_mater(), f_kill(), f_list(), f_cat();
void	f_zap(), f_group(), f_mirror(), f_extrude();
void	f_rm(), f_arbdef(), f_quit();
void	f_edcomb(), f_status(), f_vrot();
void	f_refresh(), f_fix(), f_rt(), f_rrt();
void	f_saveview(), f_savekey();
void	f_make(), f_attach(), f_release();
void	f_tedit(), f_memprint();
void	f_mirface(), f_units(), f_title();
void	f_rot_obj(), f_tr_obj(), f_sc_obj();
void	f_analyze(), f_sed();
void	f_ill(), f_knob(), f_tops(), f_summary();
void	f_prcolor(), f_color(), f_edcolor(), f_3ptarb(), f_rfarb(), f_which_id();
void	f_plot(), f_area(), f_find(), f_edgedir();
void	f_regdef(), f_aeview(), f_in(), f_tables(), f_edcodes(), f_dup(), f_concat();
void	f_rmats(),f_prefix(), f_keep(), f_tree(), f_inside(), f_mvall(), f_amtrack();
void	f_tabobj(), f_pathsum(), f_copyeval(), f_push(), f_facedef(), f_eqn();
void	f_overlay(), f_rtcheck(), f_comb();
void	f_preview();
void	f_ev(), f_debugnmg();
void	f_tol();
void	f_debugdir();

static struct funtab {
	char *ft_name;
	char *ft_parms;
	char *ft_comment;
	void (*ft_func)();
	int ft_min;
	int ft_max;
} funtab[] = {

"?", "", "summary of available commands",
	f_fhelp,0,MAXARGS,
"%", "", "escape to interactive shell",
	f_comm,1,1,
"3ptarb", "", "makes arb given 3 pts, 2 coord of 4th pt, and thickness",
	f_3ptarb, 1, 27,
"ae", "azim elev", "set view using az and elev angles",
	f_aeview, 3, 3,
"analyze", "[arbname]", "analyze faces of ARB",
	f_analyze,1,MAXARGS,
"arb", "name rot fb", "make arb8, rotation + fallback",
	f_arbdef,4,4,
"area", "[endpoint_tolerance]", "calculate presented area of view",
	f_area, 1, 2,
"attach", "[device]", "attach to a display processor, or NU",
	f_attach,1,2,
"B", "<objects>", "clear screen, edit objects",
	f_blast,2,MAXARGS,
"cat", "<objects>", "list attributes (brief)",
	f_cat,2,MAXARGS,
"center", "x y z", "set view center",
	f_center, 4,4,
"color", "low high r g b str", "make color entry",
	f_color, 7, 7,
"comb", "comb_name <operation solid>", "create or extend combination w/booleans",
	f_comb,4,MAXARGS,
"concat", "file [prefix]", "concatenate 'file' onto end of present database",
	f_concat, 2, 3,
"copyeval", "", "copys an 'evaluated' path solid",
	f_copyeval, 1, 27,
"cp", "from to", "copy [duplicate] object",
	f_copy,3,3,
"cpi", "from to", "copy cylinder and position at end of original cylinder",
	f_copy_inv,3,3,
"d", "<objects>", "delete list of objects",
	f_delobj,2,MAXARGS,
"debugdir", "", "Print in-memory directory, for debugging",
	f_debugdir, 1, 1,
"debuglib", "[hex_code]", "Show/set debugging bit vector for librt",
	f_debuglib,1,2,
"debugmem", "", "Print librt memory use map",
	f_debugmem, 1, 1,
"debugnmg", "[hex code]", "Show/set debugging bit vector for NMG",
	f_debugnmg,1,2,
"dup", "file [prefix]", "check for dup names in 'file'",
	f_dup, 2, 3,
"E", "<objects>", "evaluated edit of objects",
	f_evedit,2,MAXARGS,
"e", "<objects>", "edit objects",
	f_edit,2,MAXARGS,
"edcodes", "object(s)", "edit region ident codes",
	f_edcodes, 2, MAXARGS,
"edcolor", "", "text edit color table",
	f_edcolor, 1, 1,
"edcomb", "combname Regionflag regionid air los [GIFTmater]", "edit combination record info",
	f_edcomb,6,7,
"edgedir", "[delta_x delta_y delta_z]|[rot fb]", "define direction of ARB edge being moved",
	f_edgedir, 3, 4,
"ev",	"[-w] [-n] [-P#] <objects>", "evaluate objects via NMG tessellation",
	f_ev, 2, MAXARGS,
"eqn", "A B C", "planar equation coefficients",
	f_eqn, 4, 4,
"extrude", "#### distance", "extrude dist from face",
	f_extrude,3,3,
"facedef", "####", "define new face for an arb",
	f_facedef, 2, MAXARGS,
"find", "<objects>", "find all references to objects",
	f_find, 1, MAXARGS,
"fix", "", "fix display after hardware error",
	f_fix,1,1,
"g", "groupname <objects>", "group objects",
	f_group,3,MAXARGS,
#ifdef HIDELINE
"H", "plotfile [step_size %epsilon]", "produce hidden-line unix-plot",
	f_hideline,2,4,
#endif
"help", "[commands]", "give usage message for given commands",
	f_help,0,MAXARGS,
"i", "obj combination [operation]", "add instance of obj to comb",
	f_instance,3,4,
"idents", "file object(s)", "make ascii summary of region idents",
	f_tables, 3, MAXARGS,
"ill", "name", "illuminate object",
	f_ill,2,2,
"in", "", "keyboard entry of solids",
	f_in, 1, MAXARGS,
"inside", "", "finds inside solid per specified thicknesses",
	f_inside, 1, MAXARGS,
"item", "region item [air]", "change item # or air code",
	f_itemair,3,4,
"keep", "keep_file object(s)", "save named objects in specified file",
	f_keep, 3, MAXARGS,
"kill", "<objects>", "delete objects from file",
	f_kill,2,MAXARGS,
"killall", "object[s]", "kill object[s] and all references",
	f_killall, 2, MAXARGS,
"killtree", "object[s]", "kill complete tree[s] - BE CAREFUL",
	f_killtree, 2, MAXARGS,
"knob", "id [val]", "emulate knob twist",
	f_knob,2,3,
"l", "<objects>", "list attributes (verbose)",
	f_list,2,MAXARGS,
"listeval", "", "lists 'evaluated' path solids",
	f_pathsum, 1, 27,
"ls", "", "table of contents",
	dir_print,1,MAXARGS,
"make", "name <arb8|sph|ellg|tor|tgc>", "create a primitive",
	f_make,3,3,
"mater", "comb [material]", "assign/delete material to combination",
	f_mater,2,3,
"memprint", "", "print memory maps",
	f_memprint, 1, 1,
"mirface", "#### axis", "mirror an ARB face",
	f_mirface,3,3,
"mirror", "old new axis", "Arb mirror ??",
	f_mirror,4,4,
"mv", "old new", "rename object",
	f_name,3,3,
"mvall", "oldname newname", "rename object everywhere",
	f_mvall, 3, 3,
"overlay", "file.plot [name]", "Read UNIX-Plot as named overlay",
	f_overlay, 2, 3,
"p", "dx [dy dz]", "set parameters",
	f_param,2,4,
"paths", "pattern", "lists all paths matching input path",
	f_pathsum, 2, MAXARGS,
"plot", "[-float] [-zclip] [-2d] [-grid] [out_file] [|filter]", "make UNIX-plot of view",
	f_plot, 2, MAXARGS,
"prcolor", "", "print color&material table",
	f_prcolor, 1, 1,
"prefix", "new_prefix object(s)", "prefix each occurrence of object name(s)",
	f_prefix, 3, MAXARGS,
"preview", "preview rt_script", "preview new style RT animation script",
	f_preview, 2, 2,
"press", "button_label", "emulate button press",
	f_press,2,MAXARGS,
"push", "object[s]", "pushes object's path transformations to solids",
	f_push, 2, MAXARGS,
"q", "", "quit",
	f_quit,1,1,
"r", "region <operation solid>", "create or extend a Region combination",
	f_region,4,MAXARGS,
"refresh", "", "send new control list",
	f_refresh, 1,1,
"regdebug", "", "toggle register print",
	f_regdebug, 1,2,
"regdef", "item [air] [los] [GIFTmaterial]", "change next region default codes",
	f_regdef, 2, 5,
"regions", "file object(s)", "make ascii summary of regions",
	f_tables, 3, MAXARGS,
"release", "", "release current display processor [attach NU]",
	f_release,1,1,
"rfarb", "", "makes arb given point, 2 coord of 3 pts, rot, fb, thickness",
	f_rfarb, 1, 27,
"rm", "comb <members>", "remove members from comb",
	f_rm,3,MAXARGS,
"rmats", "file", "load views from file (experimental)",
	f_rmats,2,MAXARGS,
"rotobj", "xdeg ydeg zdeg", "rotate object being edited",
	f_rot_obj, 4, 4,
"rrt", "prog [options]", "invoke prog with view",
	f_rrt,2,MAXARGS,
"rt", "[options]", "do raytrace of view",
	f_rt,1,MAXARGS,
"rtcheck", "[options]", "check for overlaps in current view",
	f_rtcheck,1,MAXARGS,
"savekey", "file [time]", "save keyframe in file (experimental)",
	f_savekey,2,MAXARGS,
"saveview", "file [args]", "save view in file for RT",
	f_saveview,2,MAXARGS,
"scale", "factor", "scale object by factor",
	f_sc_obj,2,2,
"sed", "solid", "solid-edit named solid",
	f_sed,2,2,
"size", "size", "set view size",
	f_view, 2,2,
"solids", "file object(s)", "make ascii summary of solid parameters",
	f_tables, 3, MAXARGS,
"status", "", "get view status",
	f_status, 1,1,
"summary", "[s r g]", "count/list solid/reg/groups",
	f_summary,1,2,
"sync",	"",	"forces UNIX sync",
	sync, 1, 1,
"t", "", "table of contents",
	dir_print,1,MAXARGS,
"tab", "object[s]", "tabulates objects as stored in database",
	f_tabobj, 2, MAXARGS,
"ted", "", "text edit a solid's parameters",
	f_tedit,1,1,
"title", "string", "change the title",
	f_title,2,MAXARGS,
"tol", "[abs #]|[rel #]", "show/set absolute or relative tolerance for tessellation",
	f_tol, 1, 3,
"tops", "", "find all top level objects",
	f_tops,1,1,
"track", "<parameters>", "adds tracks to database",
	f_amtrack, 1, 27,
"translate", "x y z", "trans object to x,y, z",
	f_tr_obj,4,4,
"tree",	"object(s)", "print out a tree of all members of an object",
	f_tree, 2, MAXARGS,
"units", "<mm|cm|m|in|ft>", "change units",
	f_units,2,2,
"vrot", "xdeg ydeg zdeg", "rotate viewpoint",
	f_vrot,4,4,
"whichid", "ident(s)", "lists all regions with given ident code",
	f_which_id, 2, MAXARGS,
"x", "lvl", "print solid table & vector list",
	f_debug, 1,2,
"Z", "", "zap all objects off screen",
	f_zap,1,1,
};
#define NFUNC	( (sizeof(funtab)) / (sizeof(struct funtab)) )

/*
 *			C M D L I N E
 *
 * This routine is called to process a user's command, as typed
 * on the standard input.  Once the
 * main loop of the editor is entered, this routine will be called
 * to process commands which have been typed in completely.
 * Return value non-zero means to print a prompt.  This is needed
 * when non-blocking I/O is used instead of select.
 */
int
cmdline(line)
char	*line;
{
	int	i;

	i = parse_line(line);
	if( i == 0 ) {
		mged_cmd( numargs, cmd_args );
		return 1;
	}
	if( i < 0 )
		return 0;

	return 1;
}

/*
 *			P A R S E _ L I N E
 *
 * Parse commandline into argument vector
 * Returns nonzero value if input is to be ignored
 * Returns less than zero if there is no input to read.
 */
int
parse_line(line)
char	*line;
{
	register char *lp;
	register char *lp1;

	numargs = 0;
	lp = &line[0];

	/* Delete leading white space */
	while( (*lp == ' ') || (*lp == '\t'))
		lp++;

	cmd_args[numargs] = lp;

	if( *lp == '\n' )
		return(1);		/* NOP */

	/* Handle "!" shell escape char so the shell can parse the line */
	if( *lp == '!' )  {
		(void)system( ++lp);
		(void)printf("!\n");
		return(1);		/* Don't process command line! */
	}

	/*  Starting with the first non-whitespace, search ahead for the
	 *  first whitespace (or newline) at the end of each command
	 *  element and turn it into a null.  Then while there is more
	 *  turn it into nulls.  Once the next string is spotted (or
	 *  the of the command line) glob it if necessary and prepare
	 *  for the next command element.
	 */
	for( ; *lp != '\0'; lp++ )  {
		if((*lp == ' ') || (*lp == '\t') || (*lp == '\n'))  {
			*lp = '\0';
			lp1 = lp + 1;
			if((*lp1 == ' ') || (*lp1 == '\t') || (*lp1 == '\n'))
				continue;
			/* If not cmd [0], check for regular exp */
			if( numargs > 0 )
				(void)cmd_glob();
			if( numargs++ >= MAXARGS )  {
				(void)printf("More than %d arguments, excess flushed\n", MAXARGS);
				cmd_args[MAXARGS] = (char *)0;
				return(0);
			}
			cmd_args[numargs] = lp1;
		}
		/* Finally, a non-space char */
	}
	/* Null terminate pointer array */
	cmd_args[numargs] = (char *)0;
	return(0);
}

/*
 *			M G E D _ C M D
 *
 *  Check a table for the command, check for the correct
 *  minimum and maximum number of arguments, and pass control
 *  to the proper function.  If the number of arguments is
 *  incorrect, print out a short help message.
 */
void
mged_cmd( argc, argv )
int	argc;
char	**argv;
{
	register struct funtab *ftp;

	if( argc == 0 )  {
		(void)printf("no command entered, type ? for help\n");
		return;
	}

	for( ftp = &funtab[0]; ftp < &funtab[NFUNC]; ftp++ )  {
		if( strcmp( ftp->ft_name, argv[0] ) != 0 )
			continue;
		/* We have a match */
		if( (ftp->ft_min <= argc) &&
		    (argc <= ftp->ft_max) )  {
			/* Input has the right number of args.
		    	 * Call function listed in table, with
		    	 * main(argc, argv) style args
		    	 */
			ftp->ft_func(argc, argv);
			return;
		}
		(void)printf("Usage: %s %s\n", ftp->ft_name, ftp->ft_parms);
		(void)printf("\t(%s)\n", ftp->ft_comment);
		return;
	}
	(void)printf("%s: no such command, type ? for help\n", argv[0] );
}

/* Input parameter editing changes from keyboard */
/* Format: p dx [dy dz]		*/
static void
f_param( argc, argv )
int	argc;
char	**argv;
{
	register int i;

	if( es_edflag <= 0 )  {
		(void)printf("A solid editor option not selected\n");
		return;
	}
	if( es_edflag == PROT ) {
		(void)printf("\"p\" command not defined for this option\n");
		return;
	}

	inpara = 1;
	sedraw++;
	for( i = 1; i < argc; i++ )  {
		es_para[ i - 1 ] = atof( argv[i] );
		if( es_edflag == PSCALE ||
					es_edflag == SSCALE )  {
			if(es_para[0] <= 0.0) {
				(void)printf("ERROR: SCALE FACTOR <= 0\n");
				inpara = 0;
				sedraw = 0;
				return;
			}
		}
	}
	/* check if need to convert to the base unit */
	switch( es_edflag ) {

		case STRANS:
		case PSCALE:
		case EARB:
		case MVFACE:
		case MOVEH:
		case MOVEHH:
		case PTARB:
			/* must convert to base units */
			es_para[0] *= local2base;
			es_para[1] *= local2base;
			es_para[2] *= local2base;

		default:
			return;
	}
}

/* Let the user temporarily escape from the editor */
/* Format: %	*/
static void
f_comm( argc, argv )
int	argc;
char	**argv;
{

	register int pid, rpid;
	int retcode;

	(void)signal( SIGINT, SIG_IGN );
	if ( ( pid = fork()) == 0 )  {
		(void)signal( SIGINT, SIG_DFL );
		(void)execl("/bin/sh","-",(char *)NULL);
		perror("/bin/sh");
		mged_finish( 11 );
	}
	while ((rpid = wait(&retcode)) != pid && rpid != -1)
		;
	(void)signal(SIGINT, cur_sigint);
	(void)printf("!\n");
}

/* Quit and exit gracefully */
/* Format: q	*/
void
f_quit( argc, argv )
int	argc;
char	**argv;
{
	if( state != ST_VIEW )
		button( BE_REJECT );
	quit();			/* Exiting time */
	/* NOTREACHED */
}

/*
 *			H E L P C O M M
 *
 *  Common code for help commands
 */
static void
helpcomm( argc, argv )
int	argc;
char	**argv;
{
	register struct funtab *ftp;
	register int	i;

	/* Help command(s) */
	for( i=1; i<argc; i++ )  {
		for( ftp = &funtab[0]; ftp < &funtab[NFUNC]; ftp++ )  {
			if( strcmp( ftp->ft_name, argv[i] ) != 0 )
				continue;
			(void)printf("Usage: %s %s\n", ftp->ft_name, ftp->ft_parms);
			(void)printf("\t(%s)\n", ftp->ft_comment);
			break;
		}
		if( ftp == &funtab[NFUNC] )
			(void)printf("%s: no such command, type ? for help\n", argv[i] );
	}
}

/*
 *			F _ H E L P
 *
 *  Print a help message, two lines for each command.
 *  Or, help with the indicated commands.
 */
static void
f_help( argc, argv )
int	argc;
char	**argv;
{
	register struct funtab *ftp;
	register int	i;

	if( argc <= 1 )  {
		(void)printf("The following commands are available:\n");
		for( ftp = &funtab[0]; ftp < &funtab[NFUNC]; ftp++ )  {
			(void)printf("%s %s\n", ftp->ft_name, ftp->ft_parms);
			(void)printf("\t(%s)\n", ftp->ft_comment);
		}
		return;
	}
	helpcomm( argc, argv );
}

/*
 *			F _ F H E L P
 *
 *  Print a fast help message;  just tabulate the commands available.
 *  Or, help with the indicated commands.
 */
static void
f_fhelp( argc, argv )
int	argc;
char	**argv;
{
	register struct funtab *ftp;

	if( argc <= 1 )  {
		(void)printf("The following commands are available:\n");
		for( ftp = &funtab[0]; ftp < &funtab[NFUNC]; ftp++ )  {
			col_item(ftp->ft_name);
		}
		col_eol();
		return;
	}
	helpcomm( argc, argv );
}

/* Hook for displays with no buttons */
void
f_press( argc, argv )
int	argc;
char	**argv;
{
	register int i;

	for( i = 1; i < argc; i++ )
		press( argv[i] );
}

void
f_summary( argc, argv )
int	argc;
char	**argv;
{
	register char *cp;
	int flags = 0;

	if( argc <= 1 )  {
		dir_summary(0);
		return;
	}
	cp = argv[1];
	while( *cp )  switch( *cp++ )  {
		case 's':
			flags |= DIR_SOLID;
			break;
		case 'r':
			flags |= DIR_REGION;
			break;
		case 'g':
			flags |= DIR_COMB;
			break;
		default:
			(void)printf("summary:  S R or G are only valid parmaters\n");
			break;
	}
	dir_summary(flags);
}
