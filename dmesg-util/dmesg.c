/*
 * dmesg.c borrowed from util-linux/sys-utils
 *
 * Copyright (C) 1993 Theodore Ts'o <tytso@athena.mit.edu>
 * Copyright (C) 2011 Karel Zak <kzak@redhat.com>
 *
 * This program comes with ABSOLUTELY NO WARRANTY.
 */
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/klog.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <ctype.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

#include "c.h"
#include "strutils.h"
#include "bitops.h"
#include "optutils.h"
#include "timeutils.h"
#include "monotonic.h"
#include "mangle.h"

#include "dmesg.h"

/*
 * Supported log facilities:
    kern - kernel messages
    user - random user-level messages
    mail - mail system
  daemon - system daemons
    auth - security/authorization messages
  syslog - messages generated internally by syslogd
     lpr - line printer subsystem
    news - network news subsystem

Supported log levels (priorities):
   emerg - 0 system is unusable
   alert - 1 action must be taken immediately
    crit - 2 critical conditions
     err - 3 error conditions
    warn - 4 warning conditions
  notice - 5 normal but significant condition
    info - 6 informational
   debug - 7 debug-level messages
 *
 */

#ifndef XALLOC_EXIT_CODE
# define XALLOC_EXIT_CODE EXIT_FAILURE
#endif

static inline void __err_oom(const char *file, unsigned int line)
{
	err(XALLOC_EXIT_CODE, "%s: %u: cannot allocate memory", file, line);
}

#define err_oom()	__err_oom(__FILE__, __LINE__)

static inline __ul_alloc_size(1)
void *xmalloc(const size_t size)
{
        void *ret = malloc(size);

        if (!ret && size)
                err(XALLOC_EXIT_CODE, "cannot allocate %zu bytes", size);
        return ret;
}

#ifdef HAVE_WIDECHAR

# include <wchar.h>
# include <wctype.h>

#else /* !HAVE_WIDECHAR */

# include <ctype.h>
  /* Fallback for types */
# define wchar_t char
# define wint_t int
# ifndef WEOF
#  define WEOF EOF
# endif

 /* Fallback for input operations */
# define fgetwc fgetc
# define getwc getc
# define getwchar getchar
# define fgetws fgets

  /* Fallback for output operations */
# define fputwc fputc
# define putwc putc
# define putwchar putchar
# define fputws fputs

  /* Fallback for character classification */
# define iswgraph isgraph
# define iswprint isprint
# define iswspace isspace

  /* Fallback for string functions */
# define wcschr strchr
# define wcsdup strdup
# define wcslen strlen
# define wcspbrk strpbrk

# define wcwidth(c) (1)
# define wmemset memset
# define ungetwc ungetc

#endif /* HAVE_WIDECHAR */



/* Close the log.  Currently a NOP. */
#define SYSLOG_ACTION_CLOSE          0
/* Open the log. Currently a NOP. */
#define SYSLOG_ACTION_OPEN           1
/* Read from the log. */
#define SYSLOG_ACTION_READ           2
/* Read all messages remaining in the ring buffer. (allowed for non-root) */
#define SYSLOG_ACTION_READ_ALL       3
/* Read and clear all messages remaining in the ring buffer */
#define SYSLOG_ACTION_READ_CLEAR     4
/* Clear ring buffer. */
#define SYSLOG_ACTION_CLEAR          5
/* Disable printk's to console */
#define SYSLOG_ACTION_CONSOLE_OFF    6
/* Enable printk's to console */
#define SYSLOG_ACTION_CONSOLE_ON     7
/* Set level of messages printed to console */
#define SYSLOG_ACTION_CONSOLE_LEVEL  8
/* Return number of unread characters in the log buffer */
#define SYSLOG_ACTION_SIZE_UNREAD    9
/* Return size of the log buffer */
#define SYSLOG_ACTION_SIZE_BUFFER   10

/*
 * Priority and facility names
 */
struct dmesg_name {
	const char *name;
	const char *help;
};

/*
 * Priority names -- based on sys/syslog.h
 */
static const struct dmesg_name level_names[] =
{
	[LOG_EMERG]   = { "emerg", N_("system is unusable") },
	[LOG_ALERT]   = { "alert", N_("action must be taken immediately") },
	[LOG_CRIT]    = { "crit",  N_("critical conditions") },
	[LOG_ERR]     = { "err",   N_("error conditions") },
	[LOG_WARNING] = { "warn",  N_("warning conditions") },
	[LOG_NOTICE]  = { "notice",N_("normal but significant condition") },
	[LOG_INFO]    = { "info",  N_("informational") },
	[LOG_DEBUG]   = { "debug", N_("debug-level messages") }
};

/*
 * sys/syslog.h uses (f << 3) for all facility codes.
 * We want to use the codes as array indexes, so shift back...
 *
 * Note that libc LOG_FAC() macro returns the base codes, not the
 * shifted code :-)
 */
#define FAC_BASE(f)	((f) >> 3)

static const struct dmesg_name facility_names[] =
{
	[FAC_BASE(LOG_KERN)]     = { "kern",     N_("kernel messages") },
	[FAC_BASE(LOG_USER)]     = { "user",     N_("random user-level messages") },
	[FAC_BASE(LOG_MAIL)]     = { "mail",     N_("mail system") },
	[FAC_BASE(LOG_DAEMON)]   = { "daemon",   N_("system daemons") },
	[FAC_BASE(LOG_AUTH)]     = { "auth",     N_("security/authorization messages") },
	[FAC_BASE(LOG_SYSLOG)]   = { "syslog",   N_("messages generated internally by syslogd") },
	[FAC_BASE(LOG_LPR)]      = { "lpr",      N_("line printer subsystem") },
	[FAC_BASE(LOG_NEWS)]     = { "news",     N_("network news subsystem") },
	[FAC_BASE(LOG_UUCP)]     = { "uucp",     N_("UUCP subsystem") },
	[FAC_BASE(LOG_CRON)]     = { "cron",     N_("clock daemon") },
	[FAC_BASE(LOG_AUTHPRIV)] = { "authpriv", N_("security/authorization messages (private)") },
	[FAC_BASE(LOG_FTP)]      = { "ftp",      N_("FTP daemon") },
};

/* supported methods to read message buffer
 */
enum {
	DMESG_METHOD_KMSG,	/* read messages from /dev/kmsg (default) */
	DMESG_METHOD_SYSLOG,	/* klogctl() buffer */
	DMESG_METHOD_MMAP	/* mmap file with records (see --file) */
};

enum {
	DMESG_TIMEFTM_NONE = 0,
	DMESG_TIMEFTM_CTIME,		/* [ctime] */
	DMESG_TIMEFTM_CTIME_DELTA,	/* [ctime <delta>] */
	DMESG_TIMEFTM_DELTA,		/* [<delta>] */
	DMESG_TIMEFTM_RELTIME,		/* [relative] */
	DMESG_TIMEFTM_TIME,		/* [time] */
	DMESG_TIMEFTM_TIME_DELTA,	/* [time <delta>] */
	DMESG_TIMEFTM_ISO8601		/* 2013-06-13T22:11:00,123456+0100 */
};
#define is_timefmt(c, f) ((c)->time_fmt == (DMESG_TIMEFTM_ ##f))

struct dmesg_control {
	/* bit arrays -- see include/bitops.h */
	char levels[ARRAY_SIZE(level_names) / NBBY + 1];
	char facilities[ARRAY_SIZE(facility_names) / NBBY + 1];

	struct timeval	lasttime;	/* last printed timestamp */
	struct tm	lasttm;		/* last localtime */
	struct timeval	boot_time;	/* system boot time */

	int		action;		/* SYSLOG_ACTION_* */
	int		method;		/* DMESG_METHOD_* */

	size_t		bufsize;	/* size of syslog buffer */

	int		kmsg;		/* /dev/kmsg file descriptor */
	ssize_t		kmsg_first_read;/* initial read() return code */
	char		kmsg_buf[BUFSIZ];/* buffer to read kmsg data */
	char		kmsg_saved[BUFSIZ];/* buffer to save line after fragment */
	ssize_t		kmsg_saved_size;  /* if nonzero, read from kmsg_saved */

	/*
	 * For the --file option we mmap whole file. The unnecessary (already
	 * printed) pages are always unmapped. The result is that we have in
	 * memory only the currently used page(s).
	 */
	char		*filename;
	char		*mmap_buff;
	size_t		pagesize;
	unsigned int	time_fmt;	/* time format */

	unsigned int	follow:1,	/* wait for new messages */
			raw:1,		/* raw mode */
			fltr_lev:1,	/* filter out by levels[] */
			fltr_fac:1,	/* filter out by facilities[] */
			decode:1;	/* use "facility: level: " prefix */
	int		indent;		/* due to timestamps if newline */
	int quiet;
	int severity;
	int count;
	struct timeval after;
};

struct dmesg_record {
	const char	*mesg;
	size_t		mesg_size;
	int		level;
	int		facility;
	struct timeval  tv;
	char		flags;
	const char	*next;		/* buffer with next unparsed record */
	size_t		next_size;	/* size of the next buffer */
};

#define INIT_DMESG_RECORD(_r)  do { \
		(_r)->mesg = NULL; \
		(_r)->mesg_size = 0; \
		(_r)->facility = -1; \
		(_r)->level = -1; \
		(_r)->tv.tv_sec = 0; \
		(_r)->tv.tv_usec = 0; \
	} while (0)

static int read_kmsg(struct dmesg_control *ctl);


static int parse_kmsg_record(struct dmesg_control *ctl,
			     struct dmesg_record *rec,
			     char *buf,
			     size_t sz);




/*
 * LEVEL     ::= <number> | <name>
 *  <number> ::= @len is set:  number in range <0..N>, where N < ARRAY_SIZE(level_names)
 *           ::= @len not set: number in range <1..N>, where N <= ARRAY_SIZE(level_names)
 *  <name>   ::= case-insensitive text
 *
 *  Note that @len argument is not set when parsing "-n <level>" command line
 *  option. The console_level is interpreted as "log level less than the value".
 *
 *  For example "dmesg -n 8" or "dmesg -n debug" enables debug console log
 *  level by klogctl(SYSLOG_ACTION_CONSOLE_LEVEL, NULL, 8). The @str argument
 *  has to be parsed to number in range <1..8>.
 */
static int parse_level(const char *str, size_t len)
{
	int offset = 0;

	if (!str)
		return -1;
	if (!len) {
		len = strlen(str);
		offset = 1;
	}
	errno = 0;

	if (isdigit(*str)) {
		char *end = NULL;
		long x = strtol(str, &end, 10) - offset;

		if (!errno && end && end > str && (size_t) (end - str) == len &&
		    x >= 0 && (size_t) x < ARRAY_SIZE(level_names))
			return x + offset;
	} else {
		size_t i;

		for (i = 0; i < ARRAY_SIZE(level_names); i++) {
			const char *n = level_names[i].name;

			if (strncasecmp(str, n, len) == 0 && *(n + len) == '\0')
				return i + offset;
		}
	}

	if (errno)
		err(EXIT_FAILURE, _("failed to parse level '%s'"), str);

	errx(EXIT_FAILURE, _("unknown level '%s'"), str);
	return -1;
}

/*
 * FACILITY  ::= <number> | <name>
 *  <number> ::= number in range <0..N>, where N < ARRAY_SIZE(facility_names)
 *  <name>   ::= case-insensitive text
 */
static int parse_facility(const char *str, size_t len)
{
	if (!str)
		return -1;
	if (!len)
		len = strlen(str);
	errno = 0;

	if (isdigit(*str)) {
		char *end = NULL;
		long x = strtol(str, &end, 10);

		if (!errno && end && end > str && (size_t) (end - str) == len &&
		    x >= 0 && (size_t) x < ARRAY_SIZE(facility_names))
			return x;
	} else {
		size_t i;

		for (i = 0; i < ARRAY_SIZE(facility_names); i++) {
			const char *n = facility_names[i].name;

			if (strncasecmp(str, n, len) == 0 && *(n + len) == '\0')
				return i;
		}
	}

	if (errno)
		err(EXIT_FAILURE, _("failed to parse facility '%s'"), str);

	errx(EXIT_FAILURE, _("unknown facility '%s'"), str);
	return -1;
}

/*
 * Parses numerical prefix used for all messages in kernel ring buffer.
 *
 * Priorities/facilities are encoded into a single 32-bit quantity, where the
 * bottom 3 bits are the priority (0-7) and the top 28 bits are the facility
 * (0-big number).
 *
 * Note that the number has to end with '>' or ',' char.
 */
static const char *parse_faclev(const char *str, int *fac, int *lev)
{
	long num;
	char *end = NULL;

	if (!str)
		return str;

	errno = 0;
	num = strtol(str, &end, 10);

	if (!errno && end && end > str) {
		*fac = LOG_FAC(num);
		*lev = LOG_PRI(num);

		if (*lev < 0 || (size_t) *lev > ARRAY_SIZE(level_names))
			*lev = -1;
		if (*fac < 0 || (size_t) *fac > ARRAY_SIZE(facility_names))
			*fac = -1;
		return end + 1;		/* skip '<' or ',' */
	}

	return str;
}

/*
 * Parses timestamp from syslog message prefix, expected format:
 *
 *	seconds.microseconds]
 *
 * the ']' is the timestamp field terminator.
 */
static const char *parse_syslog_timestamp(const char *str0, struct timeval *tv)
{
	const char *str = str0;
	char *end = NULL;

	if (!str0)
		return str0;

	errno = 0;
	tv->tv_sec = strtol(str, &end, 10);

	if (!errno && end && *end == '.' && *(end + 1)) {
		str = end + 1;
		end = NULL;
		tv->tv_usec = strtol(str, &end, 10);
	}
	if (errno || !end || end == str || *end != ']')
		return str0;

	return end + 1;	/* skip ']' */
}

/*
 * Parses timestamp from /dev/kmsg, expected formats:
 *
 *	microseconds,
 *	microseconds;
 *
 * the ',' is fields separators and ';' items terminator (for the last item)
 */
static const char *parse_kmsg_timestamp(const char *str0, struct timeval *tv)
{
	const char *str = str0;
	char *end = NULL;
	uint64_t usec;

	if (!str0)
		return str0;

	errno = 0;
	usec = strtoumax(str, &end, 10);

	if (!errno && end && (*end == ';' || *end == ',')) {
		tv->tv_usec = usec % 1000000;
		tv->tv_sec = usec / 1000000;
	} else
		return str0;

	return end + 1;	/* skip separator */
}


static double time_diff(struct timeval *a, struct timeval *b)
{
	return (a->tv_sec - b->tv_sec) + (a->tv_usec - b->tv_usec) / 1E6;
}

static int get_syslog_buffer_size(void)
{
	int n = klogctl(SYSLOG_ACTION_SIZE_BUFFER, NULL, 0);

	return n > 0 ? n : 0;
}

/*
 * Reads messages from regular file by mmap
 */
static ssize_t mmap_file_buffer(struct dmesg_control *ctl, char **buf)
{
	struct stat st;
	int fd;

	if (!ctl->filename)
		return -1;

	fd = open(ctl->filename, O_RDONLY);
	if (fd < 0)
		err(EXIT_FAILURE, _("cannot open %s"), ctl->filename);
	if (fstat(fd, &st))
		err(EXIT_FAILURE, _("stat of %s failed"), ctl->filename);

	*buf = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (*buf == MAP_FAILED)
		err(EXIT_FAILURE, _("cannot mmap: %s"), ctl->filename);
	ctl->mmap_buff = *buf;
	ctl->pagesize = getpagesize();
	close(fd);

	return st.st_size;
}

/*
 * Reads messages from kernel ring buffer by klogctl()
 */
static ssize_t read_syslog_buffer(struct dmesg_control *ctl, char **buf)
{
	size_t sz;
	int rc = -1;

	if (ctl->bufsize) {
		sz = ctl->bufsize + 8;
		*buf = xmalloc(sz * sizeof(char));
		rc = klogctl(ctl->action, *buf, sz);
	} else {
		sz = 16392;
		while (1) {
			*buf = xmalloc(sz * sizeof(char));
			rc = klogctl(SYSLOG_ACTION_READ_ALL, *buf, sz);
			if (rc < 0)
				break;
			if ((size_t) rc != sz || sz > (1 << 28))
				break;
			free(*buf);
			*buf = NULL;
			sz *= 4;
		}

		if (rc > 0 && ctl->action == SYSLOG_ACTION_READ_CLEAR)
			rc = klogctl(SYSLOG_ACTION_READ_CLEAR, *buf, sz);
	}

	return rc;
}

/*
 * Top level function to read messages
 */
static ssize_t read_buffer(struct dmesg_control *ctl, char **buf)
{
	ssize_t n = -1;

	switch (ctl->method) {
	case DMESG_METHOD_MMAP:
		n = mmap_file_buffer(ctl, buf);
		break;
	case DMESG_METHOD_SYSLOG:
		if (!ctl->bufsize)
			ctl->bufsize = get_syslog_buffer_size();

		n = read_syslog_buffer(ctl, buf);
		break;
	case DMESG_METHOD_KMSG:
		/*
		 * Since kernel 3.5.0
		 */
		n = read_kmsg(ctl);
		if (n == 0 && ctl->action == SYSLOG_ACTION_READ_CLEAR)
			n = klogctl(SYSLOG_ACTION_CLEAR, NULL, 0);
		break;
	default:
		abort();	/* impossible method -> drop core */
	}

	return n;
}

static int fwrite_hex(const char *buf, size_t size, FILE *out)
{
	size_t i;

	for (i = 0; i < size; i++) {
		int rc = fprintf(out, "\\x%02hhx", buf[i]);
		if (rc < 0)
			return rc;
	}
	return 0;
}

/*
 * Prints to 'out' and non-printable chars are replaced with \x<hex> sequences.
 */
static void safe_fwrite(const char *buf, size_t size, int indent, FILE *out)
{
	size_t i;
#ifdef HAVE_WIDECHAR
	mbstate_t s;
	memset(&s, 0, sizeof (s));
#endif
	for (i = 0; i < size; i++) {
		const char *p = buf + i;
		int rc, hex = 0;
		size_t len;

#ifdef HAVE_WIDECHAR
		wchar_t wc;
		len = mbrtowc(&wc, p, size - i, &s);

		if (len == 0)				/* L'\0' */
			return;

		if (len == (size_t)-1 || len == (size_t)-2) {		/* invalid sequence */
			memset(&s, 0, sizeof (s));
			len = hex = 1;
		} else if (len > 1 && !iswprint(wc)) {	/* non-printable multibyte */
			hex = 1;
		}
		i += len - 1;
#else
		len = 1;
		if (!isprint((unsigned char) *p) &&
		    !isspace((unsigned char) *p))        /* non-printable */
			hex = 1;
#endif
		if (hex)
			rc = fwrite_hex(p, len, out);
		else if (*p == '\n' && *(p + 1) && indent) {
		        rc = fwrite(p, 1, len, out) != len;
			if (fprintf(out, "%*s", indent, "") != indent)
				rc |= 1;
		}
		else
			rc = fwrite(p, 1, len, out) != len;
		if (rc != 0) {
			if (errno != EPIPE)
				err(EXIT_FAILURE, _("write failed"));
			exit(EXIT_SUCCESS);
		}
	}
}

static const char *skip_item(const char *begin, const char *end, const char *sep)
{
	while (begin < end) {
		int c = *begin++;

		if (c == '\0' || strchr(sep, c))
			break;
	}

	return begin;
}

/*
 * Parses one record from syslog(2) buffer
 */
static int get_next_syslog_record(struct dmesg_control *ctl,
				  struct dmesg_record *rec)
{
	size_t i;
	const char *begin = NULL;

	if (ctl->method != DMESG_METHOD_MMAP &&
	    ctl->method != DMESG_METHOD_SYSLOG)
		return -1;

	if (!rec->next || !rec->next_size)
		return 1;

	INIT_DMESG_RECORD(rec);

	/*
	 * Unmap already printed file data from memory
	 */
	if (ctl->mmap_buff && (size_t) (rec->next - ctl->mmap_buff) > ctl->pagesize) {
		void *x = ctl->mmap_buff;

		ctl->mmap_buff += ctl->pagesize;
		munmap(x, ctl->pagesize);
	}

	for (i = 0; i < rec->next_size; i++) {
		const char *p = rec->next + i;
		const char *end = NULL;

		if (!begin)
			begin = p;
		if (i + 1 == rec->next_size) {
			end = p + 1;
			i++;
		} else if (*p == '\n' && *(p + 1) == '<')
			end = p;

		if (begin && !*begin)
			begin = NULL;	/* zero(s) at the end of the buffer? */
		if (!begin || !end)
			continue;
		if (end <= begin)
			continue;	/* error or empty line? */

		if (*begin == '<') {
			if (ctl->fltr_lev || ctl->fltr_fac || ctl->decode)
				begin = parse_faclev(begin + 1, &rec->facility,
						     &rec->level);
			else
				begin = skip_item(begin, end, ">");
		}

		if (*begin == '[' && (*(begin + 1) == ' ' ||
				      isdigit(*(begin + 1)))) {

			if (!is_timefmt(ctl, NONE))
				begin = parse_syslog_timestamp(begin + 1, &rec->tv);
			else
				begin = skip_item(begin, end, "]");

			if (begin < end && *begin == ' ')
				begin++;
		}

		rec->mesg = begin;
		rec->mesg_size = end - begin;

		rec->next_size -= end - rec->next;
		rec->next = rec->next_size > 0 ? end + 1 : NULL;
		if (rec->next_size > 0)
			rec->next_size--;

		return 0;
	}

	return 1;
}

static int accept_record(struct dmesg_control *ctl, struct dmesg_record *rec)
{
//	if (ctl->fltr_lev && (rec->facility < 0 ||
//			      !isset(ctl->levels, rec->level)))
//		return 0;
//
//	if (ctl->fltr_fac && (rec->facility < 0 ||
//			      !isset(ctl->facilities, rec->facility)))
//		return 0;

	return 1;
}

static void raw_print(struct dmesg_control *ctl, const char *buf, size_t size)
{
	int lastc = '\n';

	if (!ctl->mmap_buff) {
		/*
		 * Print whole ring buffer
		 */
		safe_fwrite(buf, size, 0, stdout);
		lastc = buf[size - 1];
	} else {
		/*
		 * Print file in small chunks to save memory
		 */
		while (size) {
			size_t sz = size > ctl->pagesize ? ctl->pagesize : size;
			char *x = ctl->mmap_buff;

			safe_fwrite(x, sz, 0, stdout);
			lastc = x[sz - 1];
			size -= sz;
			ctl->mmap_buff += sz;
			munmap(x, sz);
		}
	}

	if (lastc != '\n')
		putchar('\n');
}

static struct tm *record_localtime(struct dmesg_control *ctl,
				   struct dmesg_record *rec,
				   struct tm *tm)
{
	time_t t = ctl->boot_time.tv_sec + rec->tv.tv_sec;
	return localtime_r(&t, tm);
}

static char *record_ctime(struct dmesg_control *ctl,
			  struct dmesg_record *rec,
			  char *buf, size_t bufsiz)
{
	struct tm tm;

	record_localtime(ctl, rec, &tm);

	if (strftime(buf, bufsiz, "%a %b %e %H:%M:%S %Y", &tm) == 0)
		*buf = '\0';
	return buf;
}

static char *short_ctime(struct tm *tm, char *buf, size_t bufsiz)
{
	if (strftime(buf, bufsiz, "%b%e %H:%M", tm) == 0)
		*buf = '\0';
	return buf;
}

static char *iso_8601_time(struct dmesg_control *ctl, struct dmesg_record *rec,
			   char *buf, size_t bufsz)
{
	struct timeval tv = {
		.tv_sec = ctl->boot_time.tv_sec + rec->tv.tv_sec,
		.tv_usec = rec->tv.tv_usec
	};

	if (strtimeval_iso(&tv,	ISO_8601_DATE|ISO_8601_TIME|ISO_8601_COMMAUSEC|
				ISO_8601_TIMEZONE,
				buf, bufsz) != 0)
		return NULL;

	return buf;
}

static double record_count_delta(struct dmesg_control *ctl,
				 struct dmesg_record *rec)
{
	double delta = 0;

	if (timerisset(&ctl->lasttime))
		delta = time_diff(&rec->tv, &ctl->lasttime);

	ctl->lasttime = rec->tv;
	return delta;
}

static const char *get_subsys_delimiter(const char *mesg, size_t mesg_size)
{
	const char *p = mesg;
	size_t sz = mesg_size;

	while (sz > 0) {
		const char *d = strnchr(p, sz, ':');
		if (!d)
			return NULL;
		sz -= d - p + 1;
		if (sz) {
			if (isblank(*(d + 1)))
				return d;
			p = d + 1;
		}
	}
	return NULL;
}

static void print_record(struct dmesg_control *ctl,
			 struct dmesg_record *rec)
{
	char buf[256];
	int has_color = 0;
	const char *mesg;
	size_t mesg_size;
	int indent = 0;

	if (!accept_record(ctl, rec))
		return;

	if (!rec->mesg_size) {
		putchar('\n');
		return;
	}

	/*
	 * compose syslog(2) compatible raw output -- used for /dev/kmsg for
	 * backward compatibility with syslog(2) buffers only
	 */
	if (ctl->raw) {
		ctl->indent = printf("%-6s:%-6s(%d:%d) [%5ld.%06ld] ",
				facility_names[rec->facility].name,
				level_names[rec->level].name,
				rec->facility, rec->level,
				(long) rec->tv.tv_sec,
				(long) rec->tv.tv_usec);

		goto mesg;
	}

	// todo: unused

	switch (ctl->time_fmt) {
		double delta;
		struct tm cur;
	case DMESG_TIMEFTM_NONE:
		ctl->indent = 0;
		break;
	case DMESG_TIMEFTM_CTIME:
		ctl->indent = printf("[%s] ", record_ctime(ctl, rec, buf, sizeof(buf)));
		break;
	case DMESG_TIMEFTM_CTIME_DELTA:
		ctl->indent = printf("[%s <%12.06f>] ",
		       		     record_ctime(ctl, rec, buf, sizeof(buf)),
		       		     record_count_delta(ctl, rec));
		break;
	case DMESG_TIMEFTM_DELTA:
		ctl->indent = printf("[<%12.06f>] ", record_count_delta(ctl, rec));
		break;
	case DMESG_TIMEFTM_RELTIME:
		record_localtime(ctl, rec, &cur);
		delta = record_count_delta(ctl, rec);
		if (cur.tm_min != ctl->lasttm.tm_min ||
		    cur.tm_hour != ctl->lasttm.tm_hour ||
		    cur.tm_yday != ctl->lasttm.tm_yday) {
//			dmesg_enable_color(DMESG_COLOR_TIMEBREAK);
			ctl->indent = printf("[%s] ", short_ctime(&cur, buf, sizeof(buf)));
		} else {
			if (delta < 10)
				ctl->indent = printf("[  %+8.06f] ", delta);
			else
				ctl->indent = printf("[ %+9.06f] ", delta);
		}
		ctl->lasttm = cur;
		break;
	case DMESG_TIMEFTM_TIME:
		ctl->indent = printf("[%5ld.%06ld] ",
		               (long)rec->tv.tv_sec, (long)rec->tv.tv_usec);
		break;
	case DMESG_TIMEFTM_TIME_DELTA:
		ctl->indent = printf("[%5ld.%06ld <%12.06f>] ", (long)rec->tv.tv_sec,
		               (long)rec->tv.tv_usec, record_count_delta(ctl, rec));
		break;
	case DMESG_TIMEFTM_ISO8601:
		ctl->indent = printf("%s ", iso_8601_time(ctl, rec, buf, sizeof(buf)));
		break;
	default:
		abort();
	}

	ctl->indent += indent;

mesg:
	mesg = rec->mesg;
	mesg_size = rec->mesg_size;

    safe_fwrite(mesg, mesg_size, ctl->indent, stdout);

	if (*(mesg + mesg_size - 1) != '\n')
		putchar('\n');
}

/*
 * Prints the 'buf' kernel ring buffer; the messages are filtered out according
 * to 'levels' and 'facilities' bitarrays.
 */
static void print_buffer(struct dmesg_control *ctl,
			const char *buf, size_t size)
{
	struct dmesg_record rec = { .next = buf, .next_size = size };

	if (ctl->raw) {
		raw_print(ctl, buf, size);
		return;
	}

	while (get_next_syslog_record(ctl, &rec) == 0)
		print_record(ctl, &rec);
}

/*
 * Read one record from kmsg, automatically concatenating message fragments
 */
static ssize_t read_kmsg_one(struct dmesg_control *ctl)
{
	ssize_t size;
	struct dmesg_record rec = { .flags = 0 };
	char fragment_buf[BUFSIZ] = { 0 };
	ssize_t fragment_offset = 0;

	if (ctl->kmsg_saved_size != 0) {
		size = ctl->kmsg_saved_size;
		memcpy(ctl->kmsg_buf, ctl->kmsg_saved, size);
		ctl->kmsg_saved_size = 0;
		return size;
	}

	/*
	 * kmsg returns EPIPE if record was modified while reading.
	 * Read records until there is one with a flag different from 'c'/'+',
	 * which indicates that a fragment (if it exists) is complete.
	 */
	do {
        /*
         * If there is a fragment in progress, and we're in follow mode,
         * read with a timeout so that if no line is read in 100ms, we can
         * assume that the fragment is the last line in /dev/kmsg and it
         * is completed.
         */
        if (ctl->follow && fragment_offset) {
            struct pollfd pfd = {.fd = ctl->kmsg, .events = POLLIN};
            poll(&pfd, 1, 100);
            /* If 100ms has passed and kmsg has no data to read() */
            if (!(pfd.revents & POLLIN)) {
                memcpy(ctl->kmsg_buf, fragment_buf, fragment_offset);
                return fragment_offset + 1;
            }
        }

		size = read(ctl->kmsg, ctl->kmsg_buf, sizeof(ctl->kmsg_buf) - 1);

		/*
		 * If read() would have blocked and we have a fragment in
		 * progress, assume that it's completed (ie. it was the last line
		 * in the ring buffer) otherwise it won't be displayed until
		 * another non-fragment message is logged.
		 */
		if (errno == EAGAIN && fragment_offset) {
			memcpy(ctl->kmsg_buf, fragment_buf, fragment_offset);
			return fragment_offset + 1;
		}

		if (parse_kmsg_record(ctl, &rec, ctl->kmsg_buf,
				     (size_t) size) == 0) {
			/*
			 * 'c' can indicate a start of a fragment or a
			 * continuation, '+' is used in older kernels to
			 * indicate a continuation.
			 */
			if (rec.flags == 'c' || rec.flags == '+') {
				if (!fragment_offset) {
					memcpy(fragment_buf, ctl->kmsg_buf, size);
					fragment_offset = size - 1;
				} else {
					/*
					 * In case of a buffer overflow, just
					 * truncate the fragment - no one should
					 * be logging this much anyway
					 */
					ssize_t truncate_size = min(
						    fragment_offset + rec.mesg_size,
						    sizeof(fragment_buf));

					memcpy(fragment_buf + fragment_offset,
					       rec.mesg, truncate_size);
					fragment_offset += rec.mesg_size;
				}

			} else if (rec.flags == '-') {
				/*
				 * If there was a fragment being built, move it
				 * into kmsg_buf, but first save a copy of the
				 * current message so that it doesn't get lost.
				 */
				if (fragment_offset) {
					memcpy(ctl->kmsg_saved,
					       ctl->kmsg_buf, size);
					ctl->kmsg_saved_size = size;
					memcpy(ctl->kmsg_buf,
					       fragment_buf, fragment_offset);
					return fragment_offset + 1;
				}
			}
		}
	} while ((size < 0 && errno == EPIPE) ||
		 (rec.flags == 'c' || rec.flags == '+'));

	return size;
}

static int init_kmsg(struct dmesg_control *ctl)
{
	int mode = O_RDONLY;

	if (!ctl->follow)
		mode |= O_NONBLOCK;
	else
		setlinebuf(stdout);

	ctl->kmsg = open("/dev/kmsg", mode);
	if (ctl->kmsg < 0)
		return -1;

	/*
	 * Seek after the last record available at the time
	 * the last SYSLOG_ACTION_CLEAR was issued.
	 *
	 * ... otherwise SYSLOG_ACTION_CLEAR will have no effect for kmsg.
	 */
	lseek(ctl->kmsg, 0, SEEK_DATA);

	/*
	 * Old kernels (<3.5) allow to successfully open /dev/kmsg for
	 * read-only, but read() returns -EINVAL :-(((
	 *
	 * Let's try to read the first record. The record is later processed in
	 * read_kmsg().
	 */
	ctl->kmsg_first_read = read_kmsg_one(ctl);
	if (ctl->kmsg_first_read < 0) {
		close(ctl->kmsg);
		ctl->kmsg = -1;
		return -1;
	}

	return 0;
}

/*
 * /dev/kmsg record format:
 *
 *     faclev,seqnum,timestamp[optional, ...];message\n
 *      TAGNAME=value
 *      ...
 *
 * - fields are separated by ','
 * - last field is terminated by ';'
 *
 */
#define LAST_KMSG_FIELD(s)	(!s || !*s || *(s - 1) == ';')

static int parse_kmsg_record(struct dmesg_control *ctl,
			     struct dmesg_record *rec,
			     char *buf,
			     size_t sz)
{
	const char *p = buf, *end;

	if (sz == 0 || !buf || !*buf)
		return -1;

	end = buf + (sz - 1);
	INIT_DMESG_RECORD(rec);

	while (p < end && isspace(*p))
		p++;

	/* A) priority and facility */
	p = parse_faclev(p, &rec->facility, &rec->level);

	if (LAST_KMSG_FIELD(p))
		goto mesg;

	/* B) sequence number */
	p = skip_item(p, end, ",;");
	if (LAST_KMSG_FIELD(p))
		goto mesg;

	/* C) timestamp */
	p = parse_kmsg_timestamp(p, &rec->tv);

	if (LAST_KMSG_FIELD(p))
		goto mesg;

	/* D) flags */
	rec->flags = *p; // flag is one char
	p = p + 1;
	if (LAST_KMSG_FIELD(p))
		goto mesg;

	/* E) optional fields (ignore) */
	p = skip_item(p, end, ";");

mesg:
	/* F) message text */
	rec->mesg = p;
	p = skip_item(p, end, "\n");

	if (!p)
		return -1;

	rec->mesg_size = p - rec->mesg;

	/*
	 * Kernel escapes non-printable characters, unfortunately kernel
	 * definition of "non-printable" is too strict. On UTF8 console we can
	 * print many chars, so let's decode from kernel.
	 */
	unhexmangle_to_buffer(rec->mesg, (char *) rec->mesg, rec->mesg_size + 1);

	/* G) message tags (ignore) */

	return 0;
}

/*
 * Note that each read() call for /dev/kmsg returns always one record. It means
 * that we don't have to read whole message buffer before the records parsing.
 *
 * So this function does not compose one huge buffer (like read_syslog_buffer())
 * and print_buffer() is unnecessary. All is done in this function.
 *
 * Returns 0 on success, -1 on error.
 */
static int read_kmsg(struct dmesg_control *ctl)
{
	struct dmesg_record rec;
	ssize_t sz;

	if (ctl->method != DMESG_METHOD_KMSG || ctl->kmsg < 0)
		return -1;

	/*
	 * The very first read() call is done in kmsg_init() where we test
	 * /dev/kmsg usability. The return code from the initial read() is
	 * stored in ctl->kmsg_first_read;
	 */
	sz = ctl->kmsg_first_read;

	while (sz > 0) {
		*(ctl->kmsg_buf + sz) = '\0';	/* for debug messages */

		if (parse_kmsg_record(ctl, &rec, ctl->kmsg_buf, (size_t) sz) == 0) {
			// todo: move conditions to accept_record
			if (rec.level <= ctl->severity) { // if message severity is stronger
				if (timercmp(&rec.tv, &ctl->after, >)) { // if message occurred after
					ctl->count++;
					if (!ctl->quiet) {
						print_record(ctl, &rec);
					}
				}
			}
		}

		sz = read_kmsg_one(ctl);
	}

	return 0;
}

static int which_time_format(const char *s)
{
	if (!strcmp(s, "notime"))
		return DMESG_TIMEFTM_NONE;
	if (!strcmp(s, "ctime"))
		return DMESG_TIMEFTM_CTIME;
	if (!strcmp(s, "delta"))
		return DMESG_TIMEFTM_DELTA;
	if (!strcmp(s, "reltime"))
		return DMESG_TIMEFTM_RELTIME;
	if (!strcmp(s, "iso"))
		return DMESG_TIMEFTM_ISO8601;
	errx(EXIT_FAILURE, _("unknown time format: %s"), s);
}

# define dmesg_get_boot_time	get_boot_time

int grep_kernel_messages(int level, struct timeval *after, int verbose) {

	char *buf = NULL;
	int  c = 0;
	int  console_level = 0;
	int  klog_rc = 0;
	int  delta = 0;
	ssize_t n;
	static struct dmesg_control ctl = {
		.filename = NULL,
		.action = SYSLOG_ACTION_READ_ALL,
		.method = DMESG_METHOD_KMSG,
		.kmsg = -1,
		.time_fmt = DMESG_TIMEFTM_TIME,
		.indent = 0,
		.decode = 0,
		.fltr_fac = 0,
		.raw = 1,
		.count = 0,
	};

	enum {
		OPT_TIME_FORMAT = CHAR_MAX + 1,
	};

	ctl.severity = level;
	ctl.quiet = !verbose;
	ctl.after.tv_sec = after ? after->tv_sec : 0;
	ctl.after.tv_usec = after ? after->tv_usec : 0;

	if ((is_timefmt(&ctl, RELTIME) ||
	     is_timefmt(&ctl, CTIME)   ||
	     is_timefmt(&ctl, ISO8601))
	    && dmesg_get_boot_time(&ctl.boot_time) != 0)
		ctl.time_fmt = DMESG_TIMEFTM_NONE;

		if (ctl.method == DMESG_METHOD_KMSG && init_kmsg(&ctl) != 0)
			ctl.method = DMESG_METHOD_SYSLOG;

		n = read_buffer(&ctl, &buf);

		if (n > 0 && !ctl.quiet) {
			print_buffer(&ctl, buf, n);
		}

		if (!ctl.mmap_buff)
			free(buf);

		if (n < 0)
			err(EXIT_FAILURE, _("read kernel buffer failed"));

		if (ctl.kmsg >= 0)
			close(ctl.kmsg);

	return ctl.count;
}

#if DOEXE
int main(int argc, char *argv[]) {
	// show all messages
	struct timeval zero = {0, 0};
	int severity = 7;
	if (argc > 1) {
		severity = atoi(argv[1]);
	}
	if (argc > 2) {
		zero.tv_sec = atoi(argv[2]);
	}
	int cnt = grep_kernel_messages(severity, &zero, 1);
	printf ("\n%d messages of <= %d level after %d sec\n", cnt, severity, zero.tv_sec);
}
#endif
