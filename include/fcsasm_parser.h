#ifndef FCSASM_PARSER_H
#define FCSASM_PARSER_H

void parse_line(const char* line);

// Главная функция парсера и генератора кода
int fcsasm_parse_and_generate(const char* src, unsigned char* out, int outsize);

#endif // FCSASM_PARSER_H 