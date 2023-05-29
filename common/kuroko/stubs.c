/**
 * @file stubs.c
 * @brief Implementations of libc functions needed under EFI and BIOS.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <lib/print.h>
#include <lib/term.h>
void print_(char *str) {
	FOR_TERM(flanterm_write(TERM, str, strlen(str)));
}

void abort(void) {print_("ABORT\n"); while(1); }
void exit(int status) {print_("EXIT\n"); while(1); }

char * strchr(const char * s, int c) {
	while (*s && *s != c) s++;
	return *s == c ? (char*)s : NULL;
}

char * strrchr(const char * s, int c) {
	size_t l = strlen(s);
	for (size_t i = 0; i < l; ++i) {
		if (s[l-i-1] == c) {
			return (char*)&s[l-i-1];
		}
	}
	return NULL;
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
	if (!nmemb) return;
	if (!size) return;
	char * tmp = malloc(size);
	for (size_t i = 0; i < nmemb-1; ++i) {
		for (size_t j = 0; j < nmemb-1; ++j) {
			void * left = (char *)base + size * j;
			void * right = (char *)base + size * (j + 1);
			if (compar(left,right) > 0) {
				memcpy(tmp, right, size);
				memcpy(right, left, size);
				memcpy(left, tmp, size);
			}
		}
	}
	free(tmp);
}

static int isspace(int c) {
	return c == ' ';
}

static int is_valid(int base, char c) {
	if (c < '0') return 0;
	if (base <= 10) {
		return c < ('0' + base);
	}

	if (c >= 'a' && c < 'a' + (base - 10)) return 1;
	if (c >= 'A' && c < 'A' + (base - 10)) return 1;
	if (c >= '0' && c <= '9') return 1;
	return 0;
}

static int convert_digit(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'z') {
		return c - 'a' + 0xa;
	}
	if (c >= 'A' && c <= 'Z') {
		return c - 'A' + 0xa;
	}
	return 0;
}

#define LONG_MAX 2147483647
#define LONG_LONG_MAX 0x7FFFffffFFFFffffULL

#define strtox(max, type) \
	if (base < 0 || base == 1 || base > 36) { \
		return max; \
	} \
	while (*nptr && isspace(*nptr)) nptr++; \
	int sign = 1; \
	if (*nptr == '-') { \
		sign = -1; \
		nptr++; \
	} else if (*nptr == '+') { \
		nptr++; \
	} \
	if (base == 16) { \
		if (*nptr == '0') { \
			nptr++; \
			if (*nptr == 'x') { \
				nptr++; \
			} \
		} \
	} \
	if (base == 0) { \
		if (*nptr == '0') { \
			base = 8; \
			nptr++; \
			if (*nptr == 'x') { \
				base = 16; \
				nptr++; \
			} \
		} else { \
			base = 10; \
		} \
	} \
	type val = 0; \
	while (is_valid(base, *nptr)) { \
		val *= base; \
		val += convert_digit(*nptr); \
		nptr++; \
	} \
	if (endptr) { \
		*endptr = (char *)nptr; \
	} \
	if (sign == -1) { \
		return -val; \
	} else { \
		return val; \
	}

long int strtol(const char *nptr, char **endptr, int base) {
	strtox(LONG_MAX, unsigned long int);
}

long long int strtoll(const char *nptr, char **endptr, int base) {
	strtox(LONG_LONG_MAX, unsigned long long int);
}

#define ULONG_MAX ((unsigned long)(-1))
unsigned long int strtoul(const char *nptr, char **endptr, int base) {
	strtox(ULONG_MAX, unsigned long int);
}

int atoi(const char * c) {
	return strtol(c,NULL,10);
}

FILE * stdout = NULL;
FILE * stderr = NULL;
FILE * stdin  = NULL;

int fputc(int c, FILE * stream) {
	if (stream == stdout) {
		char tmp[2] = {c,0};
		print_(tmp);
	}
	return c;
}

int fputs(const char * s, FILE * stream) {
	while (*s) {
		fputc(*s,stream);
		s++;
	}
	return 0;
}

int puts(const char * s) {
	fputs(s,stdout);
	fputc('\n',stdout);
	return 0;
}

#define OUT(c) do { callback(userData, (c)); written++; } while (0)
static size_t print_dec(unsigned long long value, unsigned int width, int (*callback)(void*,char), void * userData, int fill_zero, int align_right, int precision) {
	size_t written = 0;
	unsigned long long n_width = 1;
	unsigned long long i = 9;
	if (precision == -1) precision = 1;

	if (value == 0) {
		n_width = 0;
	} else {
		unsigned long long val = value;
		while (val >= 10UL) {
			val /= 10UL;
			n_width++;
		}
	}

	if (n_width < (unsigned long long)precision) n_width = precision;

	int printed = 0;
	if (align_right) {
		while (n_width + printed < width) {
			OUT(fill_zero ? '0' : ' ');
			printed += 1;
		}

		i = n_width;
		char tmp[100];
		while (i > 0) {
			unsigned long long n = value / 10;
			long long r = value % 10;
			tmp[i - 1] = r + '0';
			i--;
			value = n;
		}
		while (i < n_width) {
			OUT(tmp[i]);
			i++;
		}
	} else {
		i = n_width;
		char tmp[100];
		while (i > 0) {
			unsigned long long n = value / 10;
			long long r = value % 10;
			tmp[i - 1] = r + '0';
			i--;
			value = n;
			printed++;
		}
		while (i < n_width) {
			OUT(tmp[i]);
			i++;
		}
		while (printed < (long long)width) {
			OUT(fill_zero ? '0' : ' ');
			printed += 1;
		}
	}

	return written;
}

/*
 * Hexadecimal to string
 */
static size_t print_hex(unsigned long long value, unsigned int width, int (*callback)(void*,char), void* userData, int fill_zero, int alt, int caps, int align) {
	size_t written = 0;
	int i = width;

	unsigned long long n_width = 1;
	unsigned long long j = 0x0F;
	while (value > j && j < UINT64_MAX) {
		n_width += 1;
		j *= 0x10;
		j += 0x0F;
	}

	if (!fill_zero && align == 1) {
		while (i > (long long)n_width + 2*!!alt) {
			OUT(' ');
			i--;
		}
	}

	if (alt) {
		OUT('0');
		OUT(caps ? 'X' : 'x');
	}

	if (fill_zero && align == 1) {
		while (i > (long long)n_width + 2*!!alt) {
			OUT('0');
			i--;
		}
	}

	i = (long long)n_width;
	while (i-- > 0) {
		char c = (caps ? "0123456789ABCDEF" : "0123456789abcdef")[(value>>(i*4))&0xF];
		OUT(c);
	}

	if (align == 0) {
		i = width;
		while (i > (long long)n_width + 2*!!alt) {
			OUT(' ');
			i--;
		}
	}

	return written;
}

/*
 * vasprintf()
 */
size_t xvasprintf(int (*callback)(void *, char), void * userData, const char * fmt, va_list args) {
	char * s;
	int precision = -1;
	size_t written = 0;
	for (const char *f = fmt; *f; f++) {
		if (*f != '%') {
			OUT(*f);
			continue;
		}
		++f;
		unsigned int arg_width = 0;
		int align = 1; /* right */
		int fill_zero = 0;
		int big = 0;
		int alt = 0;
		int always_sign = 0;
		while (1) {
			if (*f == '-') {
				align = 0;
				++f;
			} else if (*f == '#') {
				alt = 1;
				++f;
			} else if (*f == '*') {
				arg_width = (char)va_arg(args, int);
				++f;
			} else if (*f == '0') {
				fill_zero = 1;
				++f;
			} else if (*f == '+') {
				always_sign = 1;
				++f;
			} else if (*f == ' ') {
				always_sign = 2;
				++f;
			} else {
				break;
			}
		}
		while (*f >= '0' && *f <= '9') {
			arg_width *= 10;
			arg_width += *f - '0';
			++f;
		}
		if (*f == '.') {
			++f;
			precision = 0;
			if (*f == '*') {
				precision = (int)va_arg(args, int);
				++f;
			} else  {
				while (*f >= '0' && *f <= '9') {
					precision *= 10;
					precision += *f - '0';
					++f;
				}
			}
		}
		if (*f == 'l') {
			big = 1;
			++f;
			if (*f == 'l') {
				big = 2;
				++f;
			}
		}
		if (*f == 'j') {
			big = (sizeof(uintmax_t) == sizeof(unsigned long long) ? 2 :
				   sizeof(uintmax_t) == sizeof(unsigned long) ? 1 : 0);
			++f;
		}
		if (*f == 'z') {
			big = (sizeof(size_t) == sizeof(unsigned long long) ? 2 :
				   sizeof(size_t) == sizeof(unsigned long) ? 1 : 0);
			++f;
		}
		if (*f == 't') {
			big = (sizeof(ptrdiff_t) == sizeof(unsigned long long) ? 2 :
				   sizeof(ptrdiff_t) == sizeof(unsigned long) ? 1 : 0);
			++f;
		}
		/* fmt[i] == '%' */
		switch (*f) {
			case 's': /* String pointer -> String */
				{
					size_t count = 0;
					if (big) {
						return written;
					} else {
						s = (char *)va_arg(args, char *);
						if (s == NULL) {
							s = "(null)";
						}
						if (precision >= 0) {
							while (*s && precision > 0) {
								OUT(*s++);
								count++;
								precision--;
								if (arg_width && count == arg_width) break;
							}
						} else {
							while (*s) {
								OUT(*s++);
								count++;
								if (arg_width && count == arg_width) break;
							}
						}
					}
					while (count < arg_width) {
						OUT(' ');
						count++;
					}
				}
				break;
			case 'c': /* Single character */
				OUT((char)va_arg(args,int));
				break;
			case 'p':
				alt = 1;
				if (sizeof(void*) == sizeof(long long)) big = 2;
				 /* fallthrough */
			case 'X':
			case 'x': /* Hexadecimal number */
				{
					unsigned long long val;
					if (big == 2) {
						val = (unsigned long long)va_arg(args, unsigned long long);
					} else if (big == 1) {
						val = (unsigned long)va_arg(args, unsigned long);
					} else {
						val = (unsigned int)va_arg(args, unsigned int);
					}
					written += print_hex(val, arg_width, callback, userData, fill_zero, alt, !(*f & 32), align);
				}
				break;
			case 'i':
			case 'd': /* Decimal number */
				{
					long long val;
					if (big == 2) {
						val = (long long)va_arg(args, long long);
					} else if (big == 1) {
						val = (long)va_arg(args, long);
					} else {
						val = (int)va_arg(args, int);
					}
					if (val < 0) {
						OUT('-');
						val = -val;
					} else if (always_sign) {
						OUT(always_sign == 2 ? ' ' : '+');
					}
					written += print_dec(val, arg_width, callback, userData, fill_zero, align, precision);
				}
				break;
			case 'u': /* Unsigned ecimal number */
				{
					unsigned long long val;
					if (big == 2) {
						val = (unsigned long long)va_arg(args, unsigned long long);
					} else if (big == 1) {
						val = (unsigned long)va_arg(args, unsigned long);
					} else {
						val = (unsigned int)va_arg(args, unsigned int);
					}
					written += print_dec(val, arg_width, callback, userData, fill_zero, align, precision);
				}
				break;
			case '%': /* Escape */
				OUT('%');
				break;
			default: /* Nothing at all, just dump it */
				OUT(*f);
				break;
		}
	}
	return written;
}

struct CBData {
	char * str;
	size_t size;
	size_t written;
};

static int cb_sprintf(void * user, char c) {
	struct CBData * data = user;
	if (data->size > data->written + 1) {
		data->str[data->written] = c;
		data->written++;
		if (data->written < data->size) {
			data->str[data->written] = '\0';
		}
	}
	return 0;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
	struct CBData data = {str,size,0};
	int out = xvasprintf(cb_sprintf, &data, format, ap);
	cb_sprintf(&data, '\0');
	return out;
}

int snprintf(char * str, size_t size, const char * format, ...) {
	struct CBData data = {str,size,0};
	va_list args;
	va_start(args, format);
	int out = xvasprintf(cb_sprintf, &data, format, args);
	va_end(args);
	cb_sprintf(&data, '\0');
	return out;
}

static int cb_fprintf(void * user, char c) {
	fputc(c,(FILE*)user);
	return 0;
}

int fprintf(FILE *stream, const char * fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int out = xvasprintf(cb_fprintf, stream, fmt, args);
	va_end(args);
	return out;
}

char * strdup(const char * src) {
	char * out = malloc(strlen(src)+1);
	char * c = out;
	while (*src) {
		*c = *src;
		c++; src++;
	}
	*c = 0;
	return out;
}

char *strstr(const char *haystack, const char *needle) {
	size_t s = strlen(needle);
	const char * end = haystack + strlen(haystack);

	while (haystack + s <= end) {
		if (!memcmp(haystack,needle,s)) return (char*)haystack;
		haystack++;
	}

	return NULL;
}

char * strcat(char *dest, const char *src) {
	char * end = dest;
	while (*end != '\0') {
		++end;
	}
	while (*src) {
		*end = *src;
		end++;
		src++;
	}
	*end = '\0';
	return dest;
}

void ___chkstk_ms(void) { }

size_t fwrite(void *ptr, size_t size, size_t nmemb, FILE * stream) {
	for (size_t i = 0; i < size * nmemb; ++i) {
		fputc(((char*)ptr)[i], stream);
	}
	return size * nmemb;
}

void fflush(FILE * f) {
	/* do nothing */
}

