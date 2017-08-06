/*
 * Protocol errors
 */
struct file_error	{
	char		*e_code;	/* Standard three letter code */
	char		*e_string;	/* Standard error message string */
};
/*
 * Types of error as reported in error().
 */
#define E_COMMAND	'C'		/* Command error */
#define	E_FATAL		'F'		/* Fatal transfer error */
#define	E_RECOVERABLE	'R'		/* Recoverable transfer error */

#ifdef DEFERROR
#define ERRDEF(x,y) {x,y},
#else
#define ERRDEF(x,y)
#endif
/*
 * Definitions of errors
 * If errstring is not set before calling error(), the string below is used.
 * This macro stuff is gross but necessary.
 */
struct file_error	errors[48]
#ifdef DEFERROR
= {
#endif
	/*	code	message */
	ERRDEF("000",	"No error")
#define ATD	1
	ERRDEF("ATD",	"Incorrect access to directory")
#define ATF	2
	ERRDEF("ATF",	"Incorrect access to file")
#define DND	3
	ERRDEF("DND",	"Dont delete flag set")
#define IOD	4
	ERRDEF("IOD",	"Invalid operation for directory")
#define IOL	5
	ERRDEF("IOL",	"Invalid operation for link")
#define IBS	6
	ERRDEF("IBS",	"Invalid byte size")
#define IWC	7
	ERRDEF("IWC",	"Invalid wildcard")
#define RAD	8
	ERRDEF("RAD",	"Rename across directories")
#define REF	9
	ERRDEF("REF",	"Rename to existing file")
#define WNA	10
	ERRDEF("WNA",	"Wildcard not allowed")
#define ACC	11
	ERRDEF("ACC",	"Access error")
#define BUG	12
	ERRDEF("BUG",	"File system bug")
#define CCD	13
	ERRDEF("CCD",	"Cannot create directory")
#define CCL	14
	ERRDEF("CCL",	"Cannot create link")
#define CDF	15
	ERRDEF("CDF",	"Cannot delete file")
#define CIR	16
	ERRDEF("CIR",	"Circular link")
#define CRF	17
	ERRDEF("CRF",	"Rename failure")
#define CSP	18
	ERRDEF("CSP",	"Change property failure")
#define DAE	19
	ERRDEF("DAE",	"Directory already exists")
#define DAT	20
	ERRDEF("DAT",	"Data error")
#define DEV	21
	ERRDEF("DEV",	"Device not found")
#define DNE	22
	ERRDEF("DNE",	"Directory not empty")
#define DNF	23
	ERRDEF("DNF",	"Directory not found")
#define FAE	24
	ERRDEF("FAE",	"File already exists")
#define FNF	25
	ERRDEF("FNF",	"File not found")
#define FOO	26
	ERRDEF("FOO",	"File open for output")
#define FOR	27
	ERRDEF("FOR",	"Filepos out of range")
#define HNA	28
	ERRDEF("HNA",	"Host not available")
#define ICO	29
	ERRDEF("ICO",	"Inconsistent options")
#define IP	30
	ERRDEF("IP?",	"Invalid password")
#define IPS	31
	ERRDEF("IPS",	"Invalid pathname syntax")
#define IPV	32
	ERRDEF("IPV",	"Invalid property value")
#define LCK	33
	ERRDEF("LCK",	"File locked")
#define LNF	34
	ERRDEF("LNF",	"Link target not found")
#define LIP	35
	ERRDEF("LIP",	"Login problems")
#define MSC	36
	ERRDEF("MSC",	"Misc problems")
#define NAV	37
	ERRDEF("NAV",	"Not available")
#define NER	38
	ERRDEF("NER",	"Not enough resources")
#define NET	39
	ERRDEF("NET",	"Network lossage")
#define NFS	40
	ERRDEF("NFS",	"No file system")
#define NLI	41
	ERRDEF("NLI",	"Not logged in")
#define NMR	42
	ERRDEF("NMR",	"No more room")
#define UKC	43
	ERRDEF("UKC",	"Unknown operation")
#define UKP	44
	ERRDEF("UKP",	"Unknown property")
#define UNK	45
	ERRDEF("UNK",	"Unknown user")
#define UUO	46
	ERRDEF("UUO",	"Unimplemented option")
#define WKF	47
	ERRDEF("WKF",	"Wrong kind of file")
#ifdef DEFERROR
}
#endif
	;

/*
 * Fatal internal error messages
 */
#define NOMEM		"Out of memory"
#define BADSYNTAX	"Bad syntax definition in program"
#define CTLWRITE	"Write error on control connection"
#define BADFHLIST	"Bad file_handle list"
#define FSTAT		"Fstat failed"

/*
 * Our own error messages for error responses,
 */
#define WRITEDIR	"Access denied for modifying directory."
#define SEARCHDIR	"Access denied for searching directory."
#define READDIR		"Access denied for reading directory."
#define WRITEFILE	"Access denied for writing file."
#define READFILE	"Access denied for reading file."
#define PERMFILE	"Access denied on file."
#define PATHNOTDIR	"One of the pathname components is not a directory."
#define MISSDIR		"Directory doesn't exist."
#define MISSBRACK	"Missing ] in wild card syntax"

extern char errtype;			/* Error type if not E_COMMAND */
extern char *errstring;			/* Error message if non-standard */
#define ERRSIZE 100
extern char errbuf[ERRSIZE + 1];	/* Buffer for building error messages */
extern int globerr;			/* Error return from glob() */

void settreeroot(const char *root);
