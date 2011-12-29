/*
* A Pandokia log file writer for FCTX
*
* See the fctx runner documentation for examples.
*
* BUG: the pipe handling deadlocks if the test writes too much to
* the pipe.  You can reproduce this bug with the JUnit/XML reporter.
* Maybe fix it to use tmp files.
*/

/*
* Guard against multiple inclusion; also, use this to skip if FCT has
* pandokia support built in someday
*/
#ifndef FCT_PANDOKIA_LOGGER
#define FCT_PANDOKIA_LOGGER


/*
* Get fct.h if the user has not already included it.  This means you
* can use a newer fctx by including it before pandokia_fct.h, but you
* can have the default provided by pandokia.
*/
#ifndef FCT_BGN
#include "fct.h"
#endif

/*
* Check for one or more compatible versions of fctx
*/
#if ( FCT_VERSION_MAJOR == 1 ) && ( FCT_VERSION_MINOR == 6 )
#define PANDOKIA_FCTX_INTERFACE_OK
#endif

/*
* warn if not a known version, but continue anyway
*/
#ifndef PANDOKIA_FCTX_INTERFACE_OK
#warning "FCTX version is not one that pandokia.h is tested with."
#endif


#include <sys/time.h>

/*
* each logger has to have one of these.
*/
struct _pandokia_logger
{
	_fct_logger_head;
	char *pdk_log_name;
	FILE *pdk_log;
	char *pdk_prefix;
};


/*
* happens before the start of the test:
*/
void
pandokia_test_start (fct_logger_i *li, fct_logger_evt_t const *e)
{
	struct timeval t;
	struct _pandokia_logger *l = (struct _pandokia_logger *)li;
	/* log test name */
	fprintf (l->pdk_log, "name=%s%s\n", l->pdk_prefix, fct_test__name (e->test));
	/* log start time */
	gettimeofday (&t, NULL);
	fprintf (l->pdk_log, "start_time=%ld.%06d\n", (long) t.tv_sec,
	   (int) t.tv_usec);
	/* begin capturing stdout/stderr  */
	FCT_SWITCH_STDOUT_TO_BUFFER ();
	FCT_SWITCH_STDERR_TO_BUFFER ();
	/*
	* Flush log file so we have some clues if this test core dumps.
	*/
	fflush (l->pdk_log);
}

/*
* happens after the end of a test:
*/
void
pandokia_test_end (fct_logger_i * li, fct_logger_evt_t const *e)
{
	struct _pandokia_logger *l = (struct _pandokia_logger *)li;
	struct timeval end_time;
	int read_length, x, c;
	char std_buffer[16384];

	/*
	* the end time of the test
	*/
	gettimeofday (&end_time, NULL);
	fprintf (l->pdk_log, "end_time=%ld.%06d\n", (long) end_time.tv_sec, (int) end_time.tv_usec);

	/*
	* fctx only seems to understand pass/fail as test status; hard to do more in C.
	*/
	fprintf (l->pdk_log, "status=%c\n", fct_test__is_pass (e->test) ? 'P' : 'F');

	/*
	* stop capturing stdout/stderr, gather it into the log
	*/
	FCT_SWITCH_STDOUT_TO_STDOUT ();
	FCT_SWITCH_STDERR_TO_STDERR ();

	fprintf (l->pdk_log, "log:\n.");
	while ((read_length = _fct_read (fct_stdout_pipe[0], std_buffer, sizeof (std_buffer))) > 0)
		{
		for (x = 0; x < read_length; x++)
			{
			c = std_buffer[x];
			fputc (c, l->pdk_log);
			if (c == '\n')
				fputc ('.', l->pdk_log);
			}
		}
	fprintf (l->pdk_log, "\n\n");

	/*
	* end of record
	*/
	fprintf (l->pdk_log, "END\n\n");

	/*
	* Flush log file so it is consistent in case the next test core dumps.
	*/
	fflush (l->pdk_log);
}

/*
* invoked when a test is skipped
*/
void
pandokia_skip (fct_logger_i * li, fct_logger_evt_t const *e)
{
	struct _pandokia_logger *l = (struct _pandokia_logger *)li;
	fprintf (l->pdk_log, "name=%s%s\nstatus=D\nEND\n\n", 
		l->pdk_prefix, fct_test__name (e->test));
}

/*
* creates/initializes a logger record - once before any test runs
*/

fct_logger_i *
pandokia_logger (void)
{
	struct _pandokia_logger *l = 
		(struct _pandokia_logger *) calloc (1, sizeof ( struct _pandokia_logger));
	if (l == NULL)
		return NULL;
	fct_logger__init ((fct_logger_i *) l);
	l->vtable.on_test_skip = pandokia_skip;
	l->vtable.on_test_start = pandokia_test_start;
	l->vtable.on_test_end = pandokia_test_end;
	/*
	* do we need to do this?  I say just abandon the allocated memory at the end.
	* l->vtable.on_delete = fct_pandokia_logger__on_delete;
	*/

	/* name of pandokia log file */
	l->pdk_log_name = getenv("PDK_LOG");
	if (! l->pdk_log_name )
		l->pdk_log_name="PDK_LOG";

	/* open pandokia log file */
	l->pdk_log = fopen(l->pdk_log_name,"a");
	fprintf(l->pdk_log,"\n\n");

	/* collect prefix to put on test names */
	l->pdk_prefix = getenv("PDK_PREFIX");
	if (! l->pdk_prefix)
		l->pdk_prefix="";

	return (fct_logger_i *) l;
}

/*
* If we are running in Pandokia, arrange for the default logger to be
* the pandokia logger.  We do this by taking over slot 0 in the
* FCT_LOGGER_TYPES table.  So, a user who has to use the "maker" test
* runner does not have to remember to say "./a.out --logger pdk" -- they
* just say "./a.out".
*
* We also implement a standard custom logger so that "a.out --logger pdk"
* works when the user asks for it explicitly from outside pdkrun.
*
*/

static void
pandokia_intercept_logger ()
{
	char *s;
	/*
	* presence of PDK_FILE is a proxy for running from pandokia.
	*/
	s = getenv("PDK_FILE");
	if (!s)
		return;

	// FCT_LOGGER_TYPES[0].name = "pdk";
	FCT_LOGGER_TYPES[0].logger_new_fn = (fct_logger_new_fn) pandokia_logger;
	FCT_LOGGER_TYPES[0].desc = "Pandokia log files";
}


/*
* Here how you add a custom logger taken from the fctx examples.
*/
static fct_logger_types_t custlogs[] =
{
    {
        "pdk", (fct_logger_new_fn)pandokia_logger,
        "write pandokia log files"
    },
    {NULL, NULL, NULL}
};


/*
* Tests that use custom loggers must use a modified replacement for
* FCT_BGN/FCT_END.  It installs the custom logger so that fctx can
* know about it.
*/
#define CL_FCT_BGN() FCT_BGN() {  fctlog_install(custlogs); pandokia_intercept_logger();

#define CL_FCT_END() } FCT_END()

#endif