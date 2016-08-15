/* Array of platform specifiers, starting from
 * https://sourceforge.net/p/predef/wiki/OperatingSystems/ and adding others
 * not covered there that are still of interest (such as Haiku) */
static const char *os_platform_ids[] = {
	"AMIGA",
	"BSD",
	"BSD4_2",
	"BSD4_3",
	"BSD4_4",
	"DGUX",
	"EPLAN9",
	"MSDOS",
	"M_I386",
	"M_XENIX",
	"Macintosh",
	"OS2",
	"SUNOS",
	"SYSV",
	"UTS",
	"VMS",
	"WIN32",
	"WIN64",
	"_AIX",
	"_AIX3",
	"_AIX32",
	"_AIX41",
	"_AIX43",
	"_CRAY",
	"_HPUX_SOURCE",
	"_MSDOS",
	"_OS2",
	"_OSK",
	"_SCO_DS",
	"_SEQUENT_",
	"_SYSTYPE_BSD",
	"_SYSTYPE_SVR4",
	"_UNICOS",
	"_UNIXWARE7",
	"_UWIN",
	"_WIN16",
	"_WIN32",
	"_WIN32_WCE",
	"_WIN64",
	"_WINDU_SOURCE",
	"_WRS_KERNEL",
	"__ANDROID_API__",
	"__ANDROID__",
	"__APPLE__",
	"__BEOS__",
	"__CYGWIN__",
	"__DGUX__",
	"__DOS__",
	"__DragonFly__",
	"__ECOS",
	"__EMX__",
	"__FreeBSD__",
	"__GNU__",
	"__HAIKU__",
	"__HOS_MVS__",
	"__INTEGRITY",
	"__INTERIX",
	"__Lynx__",
	"__MINGW32__",
	"__MINGW64__",
	"__MORPHOS__",
	"__MSDOS__",
	"__MVS__",
	"__NetBSD_Version__",
	"__NetBSD__",
	"__OS2__",
	"__OS400__",
	"__OS9000",
	"__OpenBSD__",
	"__QNXNTO__",
	"__QNX__",
	"__RTP__",
	"__SVR4",
	"__SYLLABLE__",
	"__SYMBIAN32__",
	"__TANDEM",
	"__THW_BLUEGENE__",
	"__TOS_AIX__",
	"__TOS_BGQ__",
	"__TOS_MVS__",
	"__TOS_OS2__",
	"__TOS_WIN__",
	"__VMS",
	"__VOS__",
	"__VXWORKS__",
	"__WIN32__",
	"__WIN64__",
	"__WINDOWS__",
	"__amigaos__",
	"__bg__",
	"__bgq__",
	"__bsdi__",
	"__convex__",
	"__crayx1",
	"__dgux__",
	"__gnu_hurd__",
	"__gnu_linux__",
	"__hiuxmpp",
	"__hpux",
	"__linux",
	"__linux__",
	"__minix",
	"__mpexl",
	"__nucleus__",
	"__osf",
	"__osf__",
	"__palmos__",
	"__sgi",
	"__sun",
	"__svr4__",
	"__sysv__",
	"__ultrix",
	"__ultrix__",
	"__unix",
	"__unix__",
	"__vxworks",
	"_hpux",
	"aegis",
	"apollo",
	"hpux",
	"linux",
	"macintosh",
	"mpeix",
	"pyr",
	"sco",
	"sequent",
	"sgi",
	"sinux",
	"sun",
	"ultrix",
	"unix",
	"vax"
};
