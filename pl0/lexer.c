/*
 * program	= block "." .
 * block	= [ "const" ident "=" number { "," ident "=" number } ";" ]
 *		  [ "var" ident { "," ident } ";" ]
 *		  { "procedure" ident ";" block ";" } statement .
 * statement	= [ ident ":=" expression
 *		  | "call" ident
 *		  | "begin" statement { ";" statement } "end"
 *		  | "if" condition "then" statement
 *		  | "while" condition "do" statement ] .
 * condition	= "odd" expression
 *		| expression ( "=" | "#" | "<" | ">" ) expression .
 * expression	= [ "+" | "-" ] term { ( "+" | "-" ) term } .
 * term		= factor { ( "*" | "/" ) factor } .
 * factor	= ident
 *		| number
 *		| "(" expression ")" .
 */

#include <stdio.h>    /* fprintf, stderr */
#include <stdlib.h>   /* malloc, free, exit, strtol */
#include <stdarg.h>   /* varargs */
#include <fcntl.h>    /* open, O_RDONLY  */
#include <sys/stat.h> /* struct stat, fstat */
#include <unistd.h>   /* read, close */
#include <string.h>   /* strrchr, strcmp, strcpy */
#include <ctype.h>    /* isalpha, isdigit, isspace */
#include <errno.h>    /* errno */


typedef enum {
	IDENT,
	NUMBER,
	CONST, VAR,
	PROCEDURE, CALL,
	BEGIN, END,
	IF, THEN,
	WHILE, DO,
	ODD,
	ASSIGN,
	EQUAL, HASH, LT, GT,
	PLUS, MINUS, MULTIPLY, DIVIDE,
	LPAREN, RPAREN,
	DOT, COMMA, SEMICOLON
} token_type;

typedef struct {
	char *value;
	token_type type;
} token;


static size_t line = 1;  /* current line number */
static char *source_buf; /* buffer with source code */
static char *anchor;     /* points to beginning of source buf memory, always */


static void die(const char *fmt, ...)
{
	fprintf(stderr, "pl0c: error: %lu: ", line);

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");
	exit(1);
}

const char* stringify(token_type t)
{
	switch(t) {
	case IDENT: return "IDENT";
	case NUMBER: return "NUMBER";
	case CONST: return "CONST";
	case VAR: return "VAR";
	case PROCEDURE: return "PROCEDURE";
	case CALL: return "CALL";
	case BEGIN: return "BEGIN";
	case END: return "END";
	case IF: return "IF";
	case THEN: return "THEN";
	case WHILE: return "WHILE";
	case DO: return "DO";
	case ODD: return "ODD";
	case ASSIGN: return "ASSIGN";
	case EQUAL: return "EQUAL";
	case HASH: return "HASH";
	case LT: return "LESS-THAN";
	case GT: return "GREATER-THAN";
	case PLUS: return "PLUS";
	case MINUS: return "MINUS";
	case MULTIPLY: return "MULTIPLY";
	case DIVIDE: return "DIVIDE";
	case LPAREN: return "LEFT-PAREN";
	case RPAREN: return "RIGHT-PAREN";
	case DOT: return "DOT";
	case COMMA: return "COMMA";
	case SEMICOLON: return "SEMICOLON";
	default: return "UNKNOWN";
	}
}

static void readfile(const char *file)
{
	char *dotp = strrchr(file, '.');
	if(dotp == NULL || strcmp(dotp, ".pl0") != 0) {
		die("File must end in '.pl0'");
	}

	int fd = open(file, O_RDONLY);
	if(!fd) {
		die("Unable to open file '%s'", file);
	}

	struct stat st;
	if(fstat(fd, &st) == -1) {
		die("Unable to get file size");
	}

	if((source_buf = malloc(st.st_size + 1)) == NULL) {
		die("Unable to malloc buffer for file content");
	}

	if(read(fd, source_buf, st.st_size) != st.st_size) {
		die("Unable to read file");
	}
	source_buf[st.st_size] = '\0';

	anchor = source_buf; /* used to free() at end of program */
	close(fd);
}

/* { ... } */
static void comment()
{
	int c;
	while((c = *source_buf++) != '}') {
		if(c == '\0') {
			die("Unterminated comment");
		}
		if(c == '\n') {
			line++;
		}
	}
}

char* get_token(char *startp, char *endp)
{
	size_t len = endp - startp;
	char *token = malloc(len + 1);
	if(token == NULL) {
		die("Malloc failed");
	}
	for(size_t i = 0; i < len; ++i) {
		token[i] = *startp++;
	}
	token[len] = '\0';
	return token;
}

token make_token(char *value, token_type type)
{
	token t = {.value = value, .type = type};
	return t;
}

static token ident()
{
	/* determine start/end of ident */
	char *startp = source_buf;
	while(isalnum(*source_buf) || *source_buf == '_') {
		source_buf++;
	}

	char *ident = get_token(startp, source_buf);

	/* catch any reserved keywords */
	token_type type;
	if      (!strcmp(ident, "const"))     type = CONST;
	else if (!strcmp(ident, "var"))       type = VAR;
	else if (!strcmp(ident, "procedure")) type = PROCEDURE;
	else if (!strcmp(ident, "call"))      type = CALL;
	else if (!strcmp(ident, "begin"))     type = BEGIN;
	else if (!strcmp(ident, "end"))       type = END;
	else if (!strcmp(ident, "if"))        type = IF;
	else if (!strcmp(ident, "then"))      type = THEN;
	else if (!strcmp(ident, "while"))     type = WHILE;
	else if (!strcmp(ident, "do"))        type = DO;
	else if (!strcmp(ident, "odd"))       type = ODD;
	else                                  type = IDENT;

	return make_token(ident, type);
}

static token number()
{
	/* determine start/end of number */
	char *startp = source_buf;
	while(isdigit(*source_buf)) {
		source_buf++;
	}

	char *token = get_token(startp, source_buf);

	/* ensure number is valid */
	errno = 0;
	int base = 10;
	(void) strtol(token, NULL, base);
	if(errno != 0) {
		die("Invalid number");
	}

	return make_token(token, NUMBER);
}

static token lex()
{
start:
	while(isspace(*source_buf)) {
		if (*source_buf++ == '\n') {
			line++;
		}
	}

	if(isalpha(*source_buf) || *source_buf == '_') {
		return ident();
	}

	if(isdigit(*source_buf)) {
		return number();
	}

	token_type type;
	char *value = malloc(3); /* := is longest symbol */
	if(value == NULL) {
		die ("Malloc failed");
	}

	switch(*source_buf) {
	case '=':
		type = ASSIGN;
		source_buf++;
		break;
	case '#':
		type = HASH;
		source_buf++;
		break;
	case '<':
		type = LT;
		source_buf++;
		break;
	case '>':
		type = GT;
		source_buf++;
		break;
	case '+':
		type = PLUS;
		source_buf++;
		break;
	case '-':
		type = MINUS;
		source_buf++;
		break;
	case '*':
		type = MULTIPLY;
		source_buf++;
		break;
	case '/':
		type = DIVIDE;
		source_buf++;
		break;
	case '(':
		type = LPAREN;
		source_buf++;
		break;
	case ')':
		type = RPAREN;
		source_buf++;
		break;
	case '.':
		type = DOT;
		source_buf++;
		break;
	case ',':
		type = COMMA;
		source_buf++;
		break;
	case ';':
		type = SEMICOLON;
		source_buf++;
		break;

	case ':':
		if(*(++source_buf) != '=') {
			die("Unkown token: '%c'", *source_buf);
		}
		type = ASSIGN;
		source_buf++;
		strcpy(value, ":=\0");
		return make_token(value, ASSIGN);

	case '{':
		comment();
		goto start;

	case '\0': type = DOT; break;
	default:
		die("Unkown token '%c'", *source_buf);
		exit(1); /* not reached... just to silence warning */
	}

	value[0] = *source_buf;
	value[1] = '\0';

	return make_token(value, type);
}



void parse()
{
	token t = {0};
	while(t.type != DOT) {
		t = lex();
		printf("%lu:\t%s, %s\n", line, stringify(t.type), t.value);
		free(t.value);
	}
	printf("done\n");
}

int main(int argc, char **argv)
{
	if(argc != 2) {
		fprintf(stderr, "Usage: pl0c <file>\n");
		return 1;
	}

	readfile(argv[1]);
	parse();
	//free(source_buf);
	free(anchor);

	return 0;
}
