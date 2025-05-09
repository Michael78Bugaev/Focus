#ifndef FOCUSOS_CTYPE_H
#define FOCUSOS_CTYPE_H

static inline int isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static inline int isdigit(int c) {
    return c >= '0' && c <= '9';
}

static inline int isalpha(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static inline int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

static inline int toupper(int c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}

static inline int tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

#endif // FOCUSOS_CTYPE_H