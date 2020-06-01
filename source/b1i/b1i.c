/*
 BASIC1 interpreter
 Copyright (c) 2020 Nikolay Pletnev
 MIT license

 b1i.c: command line BASIC interpreter main file
*/


#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>

#ifdef B1_FEATURE_UNICODE_UCS2
#include <wchar.h>
#endif

#include "b1feat.h"
#include "b1int.h"
#include "b1err.h"

#include "version.h"
#include "gitrev.h"


#ifdef B1_FEATURE_UNICODE_UCS2
#define FPUTS(STR, STREAM) \
do { \
	size_t _slen = strlen(STR); \
	wchar_t *_wstr = malloc(sizeof(wchar_t) * (_slen + 1)); \
	mbstowcs(_wstr, STR, _slen); \
	_wstr[_slen] = L'\0'; \
	fputws(_wstr, STREAM); \
	free(_wstr); \
} while(0)
#define FPRINTF(STREAM, FMT, ...) \
do { \
	size_t _slen = strlen(FMT); \
	wchar_t *_wstr = malloc(sizeof(wchar_t) * (_slen + 1)); \
	mbstowcs(_wstr, FMT, _slen); \
	_wstr[_slen] = L'\0'; \
	fwprintf(STREAM, _wstr, ##__VA_ARGS__); \
	free(_wstr); \
} while(0)
#define PRINTF_TYPESPEC_STRING "%ls"
#else
#define FPUTS(STR, STREAM) fputs(STR, STREAM)
#define FPRINTF(STREAM, FMT, ...) fprintf(STREAM, FMT, ##__VA_ARGS__)
#define PRINTF_TYPESPEC_STRING "%s"
#endif


extern B1_T_ERROR b1_ex_prg_set_prog_file(const char *prog_file);


static const char *version = B1_INT_VERSION;

static const char *err_msgs[] =
{
	"invalid token",
	"program line too long",
	"invalid line number",
	"invalid statement",
	"invalid argument",
	"expression too long",
	"missing comma or bracket",
	"unbalanced brackets",
	"wrong argument count",
	"expression evaluation temporary stack overflow",
	"unknown syntax error",
	"wrong argument type",
	"not enough memory",
	"invalid memory block descriptor",
	"buffer too small",
	"string too long",
	"too many open brackets",
	"unknown identifier",
	"wrong subscript count",
	"type mismatch",
	"subscript out of range",
	"identifier already in use",
	"integer divide by zero",
	"nested IF statement not allowed",
	"ELSE without IF",
	"line number not found",
	"statement stack overflow",
	"statement stack underflow",
	"can't use the reserved word in this context",
	"not a variable",
	"environment fatal error",
	"unexpected RETURN statement",
	"unexpected end of program",
	"the end of DATA block reached",
	"NEXT without FOR",
	"",
	"FOR without NEXT",
	"can't use subscripted variable as FOR loop control variable",
	"invalid number",
	"numeric overflow",
	"too many DEF statements",
	"user functions stack overflow",
	"end of file",
};


static void b1_print_error(uint8_t err_code, int print_line_cnt, int print_err_desc)
{
	FPRINTF(stderr, "error: %d", (int)err_code);
	
	if(print_line_cnt)
	{
		FPRINTF(stderr, " at line %d", (int)b1_int_curr_prog_line_cnt);
	}

	if(print_err_desc && err_code >= B1_RES_FIRSTERRCODE && err_code <= B1_RES_LASTERRCODE)
	{
		FPRINTF(stderr, " (" PRINTF_TYPESPEC_STRING ")", err_msgs[err_code - B1_RES_FIRSTERRCODE]);
	}

	FPUTS("\n", stderr);
}

static void b1_print_version(FILE *fstr)
{
	FPUTS("BASIC1 interpreter\n", fstr);
	FPUTS("MIT license\n", fstr);
	FPUTS("Version: ", fstr);
	FPUTS(version, fstr);
#ifdef B1_GIT_REVISION
	FPUTS(" (", fstr);
	FPUTS(B1_GIT_REVISION, fstr);
	FPUTS(")", fstr);
#endif
	FPUTS("\n", fstr);
}

int main(int argc, char **argv)
{
	B1_T_ERROR err;
	int i;
	int retcode = 0;
	int print_time = 0;
	clock_t start, end;
	int print_err_desc = 0;
	int print_version = 0;
#ifdef B1_FEATURE_LOCALES
	int get_locale = 0, ss, se;
	char *locale = NULL;
#endif

	if(argc <= 1)
	{
		b1_print_version(stderr);
		FPUTS("error: missing file name\n", stderr);
		FPUTS("usage: ", stderr);
		FPUTS(B1_PROJECT_NAME, stderr);
		FPUTS(" [options] filename\n", stderr);
		FPUTS("options:\n", stderr);
		FPUTS("-d or /d - print error description\n", stderr);
		FPUTS("-e or /e - echo input\n", stderr);
#ifdef B1_FEATURE_LOCALES
		FPUTS("-l <locale_name> or /l <locale_name> - set program locale\n", stderr);
#endif
		FPUTS("-t or /t - print program execution time\n", stderr);
		FPUTS("-v or /v - show interpreter version\n", stderr);
		return -1;
	}

	// options
	for(i = 1; i < argc; i++)
	{
#ifdef B1_FEATURE_LOCALES
		if(get_locale)
		{
			get_locale = 0;

			if(argv[i][0] != '-' && argv[i][0] != '/')
			{
				ss = 0;
				se = (int)strlen(argv[i]) - 1;
				
				if(argv[i][0] == '\"' || argv[i][0] == '\'')
				{
					ss++;
				}

				if(argv[i][se] == '\"' || argv[i][se] == '\'')
				{
					se--;
				}

				if(ss <= se)
				{
					locale = (char *)malloc(se - ss + 2);
					strncpy(locale, argv[i] + ss, se - ss + 1);
					locale[se - ss + 1] = 0;
					continue;
				}
			}
		}
#endif

		// echo on
		if((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'E' || argv[i][1] == 'e') &&
			argv[i][2] == 0)
		{
			b1_int_input_echo = 1;
			continue;
		}

#ifdef B1_FEATURE_LOCALES
		// locale
		if((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'L' || argv[i][1] == 'l') &&
			argv[i][2] == 0)
		{
			get_locale = 1;
			continue;
		}
#endif

		// print program execution time
		if((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'T' || argv[i][1] == 't') &&
			argv[i][2] == 0)
		{
			print_time = 1;
			continue;
		}

		// print error description
		if((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'D' || argv[i][1] == 'd') &&
			argv[i][2] == 0)
		{
			print_err_desc = 1;
			continue;
		}

		// print interpreter version
		if((argv[i][0] == '-' || argv[i][0] == '/') &&
			(argv[i][1] == 'V' || argv[i][1] == 'v') &&
			argv[i][2] == 0)
		{
			print_version = 1;
			break;
		}
	}

#ifdef B1_FEATURE_LOCALES
	// set locale for toupper, tolower, strcoll, towupper, towlower and wcscoll functions to work properly
	setlocale(LC_ALL, locale == NULL ? "" : locale);
	if(locale != NULL)
	{
		free(locale);
	}
#endif

	// set numeric properties from C locale for sprintf to use dot as decimal delimiter
	setlocale(LC_NUMERIC, "C");

	if(print_version)
	{
		// just print version and stop executing
		b1_print_version(stdout);
		return 0;
	}

	err = b1_ex_prg_set_prog_file(argv[argc - 1]);
	if(err != B1_RES_OK)
	{
		b1_print_error(err, 0, print_err_desc);
		return -2;
	}

	start = clock();

	err = b1_int_reset();
	if(err != B1_RES_OK)
	{
		b1_print_error(err, 0, print_err_desc);
		retcode = -3;
	}
	else
	{
		err = b1_int_prerun();
		if(err != B1_RES_OK)
		{
			b1_print_error(err, 1, print_err_desc);
			retcode = -4;
		}
		else
		{
			err = b1_int_run();
			if(err != B1_RES_OK)
			{
				b1_print_error(err, 1, print_err_desc);
				retcode = -5;
			}
		}
	}

	end = clock();

	b1_ex_prg_set_prog_file(NULL);

	if(print_time && retcode == 0)
	{
		i = (int)(100 * (uint64_t)(end - start) / CLOCKS_PER_SEC);

		FPRINTF(stdout, "The program execution time: %d.%02d s\n", i / 100, i - (i / 100) * 100);
	}

	return retcode;
}
