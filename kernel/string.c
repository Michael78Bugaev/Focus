#include <string.h>
#include <vga.h>
#include <idt.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <keyboard.h>
#include <stdarg.h>

int getch_scancode = 0;
int getch_flag = 0;
int getchcode;

int capsOn = false, capsLock = false, extended = false;
int shiftOn = 0;

void getch_handler(struct InterruptRegisters *regs) {
    uint8_t code = inb(0x60);
    uint8_t scanCode = code & 0x7F;
    uint8_t press = code & 0x80;
    
    // Обработка Shift
    if (scanCode == 0x2A || scanCode == 0x36) { // Shift
        if (!press) {
            shiftOn = 1; 
            capsOn = 1; 
            capsLock = 1;
        }
        else {
            shiftOn = 0;
            capsOn = 0;
            capsLock = 0;
        }
        return;
    }
    
    if (press == 0) {
        switch (scanCode) {
            case 0x01: // Escape
                getch_scancode = 0x1B;  // Возвращаем ASCII код ESC
                getch_flag = 1;
                break;
            case 0x1C: // Enter
                getch_scancode = 0x0D;  // Возвращаем ASCII код Enter
                getch_flag = 1;
            break;
            case 0x0E: // Backspace
                getch_scancode = 0x08;  // Возвращаем ASCII код Backspace
                getch_flag = 1;
            break;
            case 0x48: // Up Arrow
                getch_scancode = 0x80;
                getch_flag = 1;
            break;
            case 0x50: // Down Arrow
                getch_scancode = 0x81;
                getch_flag = 1;
            break;
            case 0x4B: // Left Arrow
                getch_scancode = 0x82;
                getch_flag = 1;
            break;
            case 0x4D: // Right Arrow
                getch_scancode = 0x83;
                getch_flag = 1;
            break;
            case 0x47: // Home
                getch_scancode = 0x84;
                getch_flag = 1;
            break;
            case 0x4F: // End
                getch_scancode = 0x85;
                getch_flag = 1;
            break;
            case 0x49: // Page Up
                getch_scancode = 0x86;
                getch_flag = 1;
            break;
            case 0x51: // Page Down
                getch_scancode = 0x87;
                getch_flag = 1;
            break;
            case 0x53: // Delete
                getch_scancode = 0x88;
                getch_flag = 1;
            break;
        default:
                getch_scancode = scanCode;
                getch_flag = 1;
                break;
            }
    }
}

char *strcpy(char *dest, const char *src) {
    char *ptr = dest;

    while ((*ptr++ = *src++) != '\0') {
        ; // Do nothing, just iterate through the strings
    }

    return dest;
}
int strlen(char s[]) {
    int i = 0;
    while (s[i] != '\0') {
        ++i;
    }
    return i;
}
char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;

    // Copy up to n characters from src to dest
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    // If we copied fewer than n characters, pad the rest of dest with null bytes
    for (; i < n; i++) {
        dest[i] = '\0';
    }

    return dest;
}
void strncat(char *s, char c)
{
  int len = strlen(s);
  s[len] = c;
  s[len + 1] = '\0';
}

char *strcat(char *s, char *append)
{
	char *save = s;

	for (; *s; ++s);
	while (*s++ = *append++);
	return(save);
}

// compare string
int strcmp(char s1[], char s2[]) {
    int i;
    for (i = 0; s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') return 0;
    }
    return s1[i] - s2[i];
}

char *mem_set(char *dest, int val)
{
  unsigned char *ptr = dest;

  size_t len = strlen(dest);

  while (len-- > 0)
    *ptr++ = val;
  return dest;
}
void join(char s[], char n) {
    int len = strlen(s);
    s[len] = n;
    s[len + 1] = '\0';
}
void memcp(char *source, char *dest, int nbytes) {
    int i;
    for (i = 0; i < nbytes; i++) {
        *(dest + i) = *(source + i);
    }
}

void clearString(char *string)
{

  for (int i = strlen(string); i = 0; i--)
  {

    string[i] = 0x00;
  }
}

void reverse(char s[]) {
    int c, i, j;
    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

void intToString(int n, char str[]) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    if (sign < 0) str[i++] = '-';
    str[i] = '\0';

    reverse(str);
}
int startsWith(char s1[], char s2[]) {
    int i;
    for (i = 0; s2[i] != '\0'; i++) {
        if (s1[i] != s2[i]) return 0;
    }
    return 1;
}

void strnone(char *str)
{
    for (int i = 0; i < strlen(str); i++)
    {
        str[i] = 0;
    }
    
}

char **splitString(const char *str, int *count) {
     // Сначала подсчитаем количество слов
     int n = 0;
     const char *ptr = str;
     while (*ptr) {
         // Пропускаем пробелы
         while (*ptr == ' ') {
             ptr++;
         }
         if (*ptr) {
             n++; // Нашли слово
             // Пропускаем само слово
             while (*ptr && *ptr != ' ') {
                 ptr++;
             }
         }
     }

     // Выделяем память для массива строк
     char **result = malloc(n * sizeof(char *));
     if (!result) {
         return NULL; // Ошибка выделения памяти
     }

     // Заполняем массив словами
     int index = 0;
     ptr = str;
     while (*ptr) {
         // Пропускаем пробелы
         while (*ptr == ' ') {
             ptr++;
         }
         if (*ptr) {
             const char *start = ptr;
             // Находим конец слова
             while (*ptr && *ptr != ' ') {
                 ptr++;
             }
             // Выделяем память для слова и копируем его
             int length = ptr - start;
             result[index] = malloc((length + 1) * sizeof(char));
             if (!result[index]) {
                 // Освобождаем ранее выделенную память в случае ошибки
                 for (int j = 0; j < index; j++) {
                     mfree(result[j]);
                 }
                 mfree(result);
                 return NULL; // Ошибка выделения памяти
             }
             strncpy(result[index], start, length);
             result[index][length] = '\0'; // Завершаем строку нулевым символом
             index++;
         }
     }

     *count = n; // Возвращаем количество найденных слов
     return result;
}

size_t strnlen(const char *s, size_t maxlen) {
    size_t len;
    for (len = 0; len < maxlen; len++) {
        if (s[len] == '\0') {
            break;
        }
    }
    return len;
}

int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    
    return 0;
}

int atoi(const char *str) {
    int result = 0;
    int sign = 1;
    int i = 0;

    // Handle whitespace
    while (str[i] == ' ' || str[i] == '\t' || str[i] == '\n') {
        i++;
    }

    // Handle sign
    if (str[i] == '-' || str[i] == '+') {
        sign = (str[i] == '-') ? -1 : 1;
        i++;
    }

    // Process digits
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        i++;
    }

    return sign * result;
}

int to_integer(const char* str) {
    return atoi(str);
}

#define MAX_HISTORY 1024
#define MAX_COMMAND_LENGTH 1024

void *memmove(void *dest, const void *src, size_t n) {
    // Приводим указатели к типу char* для работы с байтами
    char *d = (char *)dest;
    const char *s = (const char *)src;

    // Если области памяти не перекрываются, просто копируем
    if (d < s || d >= s + n) {
        // Копируем от начала до конца
        while (n--) {
            *d++ = *s++;
        }
    } else {
        // Если области перекрываются, копируем с конца
        d += n;
        s += n;
        while (n--) {
            *(--d) = *(--s);
        }
    }

    return dest; // Возвращаем указатель на целевую область
}

char *strdup(const char *str) {
    int len = strlen(str);
    char *dup = (char *)malloc((len + 1) * sizeof(char));

    if (dup) {
        strcpy(dup, str);
    }

    return dup;
}
int isalpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

char* tostr(int value) {
    // Handle negative numbers
    bool is_negative = false;
    if (value < 0) {
        is_negative = true;
        value = -value; // Make value positive for processing
    }

    // Calculate the number of digits
    int temp = value;
    int num_digits = 0;
    do {
        num_digits++;
        temp /= 10;
    } while (temp > 0);
     // Allocate memory for the string
     char* str = (char*)malloc(num_digits + is_negative + 1); // +1 for null terminator
    if (str == NULL) {
        return NULL; // Handle memory allocation failure
    }

    // Fill the string with digits in reverse order
    str[num_digits + is_negative] = '\0'; // Null-terminate the string
    for (int i = num_digits - 1; i >= 0; i--) {
        str[i + is_negative] = (value % 10) + '0'; // Convert digit to character
        value /= 10;
    }

    // Add the negative sign if needed
    if (is_negative) {
        str[0] = '-';
    }

    return str;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    // Если длина для сравнения равна 0, строки считаются равными
    if (n == 0)
        return 0;

    // Сравниваем посимвольно до n символов
    while (n-- > 0) {
        // Если текущие символы не равны, возвращаем их разницу
        if (*s1 != *s2) {
            return *(unsigned char *)s1 - *(unsigned char *)s2;
        }

        // Если достигнут конец одной из строк, прекращаем сравнение
        if (*s1 == '\0') {
            return 0;
        }

        // Переходим к следующим символам
        s1++;
        s2++;
    }

    // Если все символы до n равны
    return 0;
}

char* strtok(char* str, const char* delimiters) {
    static char* last_token = NULL;
    
    // Если str не NULL, начинаем новый процесс токенизации
    // Если NULL, продолжаем с последней позиции
    if (str != NULL) {
        last_token = str;
    } else if (last_token == NULL) {
        return NULL;
    }
    
    // Пропускаем начальные разделители
    while (*last_token != '\0' && strchr(delimiters, *last_token) != NULL) {
        last_token++;
    }
    
    // Если достигнут конец строки, возвращаем NULL
    if (*last_token == '\0') {
        last_token = NULL;
        return NULL;
    }
    
    // Начало текущего токена
    char* token_start = last_token;
    
    // Ищем конец текущего токена
    while (*last_token != '\0' && strchr(delimiters, *last_token) == NULL) {
        last_token++;
    }
    
    // Если найден разделитель, заменяем его на '\0' и сохраняем позицию
    if (*last_token != '\0') {
        *last_token = '\0';
        last_token++;
    } else {
        // Если достигнут конец строки, сбрасываем указатель
        last_token = NULL;
    }
    
    return token_start;
}

// Вспомогательная функция strchr (если её нет)
char* strchr(const char* str, int character) {
    while (*str != '\0') {
        if (*str == character) {
            return (char*)str;
        }
        str++;
    }
    if (character == '\0') {
        return (char*)str;
    }
    return NULL;
}

char tolower(char s1) {
  if (s1 >= 64 && s1 <= 90) {
    s1 += 32;
  }

  return s1;
}

int istrncmp(const char *str1, const char *str2, int n) {
  unsigned char u1, u2;

  while (n--) {
    u1 = (unsigned char)*str1++;
    u2 = (unsigned char)*str2++;

    if (u1 != u2 && tolower(u1) != tolower(u2)) {
      return u1 - u2;
    }

    if (u1 == 0) {
      return 0;
    }
  }

  return 0;
}

char *strrchr(const char *str, int character) {
    const char *last_occurrence = NULL; // Указатель на последнее вхождение символа
    while (*str) {
        if (*str == (char)character) {
            last_occurrence = str; // Обновляем указатель, если символ найден
        }
        str++; // Переход к следующему символу
    }
    return (char *)last_occurrence; // Возвращаем последнее вхождение или NULL
}

void remove_null_chars(char *str) {
    char *src = str; // Указатель на исходную строку
    char *dst = str; // Указатель на место для записи результата

    while (*src) { // Пока не достигнут конец строки
        if (*src != '\0') { // Если текущий символ не '\0'
            *dst++ = *src; // Копируем его в результирующую строку
        }
        src++; // Переходим к следующему символу
    }
    *dst = '\0'; // Завершаем результирующую строку нулевым символом
}

void itoa(int value, char* str, int base) {
    int i = 0;
    int is_negative = 0;

    // Handle 0 explicitly
    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    // Handle negative numbers
    if (value < 0 && base == 10) {
        is_negative = 1;
        value = -value;
    }

    // Process each digit
    while (value != 0) {
        int rem = value % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'A' : rem + '0'; // Convert to character
        value = value / base;
    }

    // If number is negative, append '-'
    if (is_negative) {
        str[i++] = '-';
    }

    str[i] = '\0';

    // Reverse the string
    reverse(str);
}

char **split(const char *str, int *count, const char delimeter) {
    // Сначала подсчитаем количество слов
    int n = 0;
    const char *ptr = str;
    while (*ptr) {
        // Пропускаем пробелы
        while (*ptr == delimeter) {
            ptr++;
        }
        if (*ptr) {
            n++; // Нашли слово
            // Пропускаем само слово
            while (*ptr && *ptr != delimeter) {
                ptr++;
            }
        }
    }

    // Выделяем память для массива строк
    char **result = malloc(n * sizeof(char *));
    if (!result) {
        return NULL; // Ошибка выделения памяти
    }

    // Заполняем массив словами
    int index = 0;
    ptr = str;
    while (*ptr) {
        // Пропускаем пробелы
        while (*ptr == delimeter) {
            ptr++;
        }
        if (*ptr) {
            const char *start = ptr;
            // Находим конец слова
            while (*ptr && *ptr != delimeter) {
                ptr++;
            }
            // Выделяем память для слова и копируем его
            int length = ptr - start;
            result[index] = malloc((length + 1) * sizeof(char));
            if (!result[index]) {
                // Освобождаем ранее выделенную память в случае ошибки
                for (int j = 0; j < index; j++) {
                    mfree(result[j]);
                }
                mfree(result);
                return NULL; // Ошибка выделения памяти
            }
            strncpy(result[index], start, length);
            result[index][length] = '\0'; // Завершаем строку нулевым символом
            index++;
        }
    }

    *count = n; // Возвращаем количество найденных слов
    return result;
}

bool contain(const char *str, char contain)
{
    int i = 0;
    while (str[i] != '\0') {
        if (strcmp(str[i], contain) == 0) return true;
    } 
    return false;  
}

void memset(void *dest, char val, uint32_t count)
{
    char *temp = (char*) dest;
    for (; count != 0; count --){
        *temp++ = val;
    }
}

void	memcpy(uint8_t *src, uint8_t *dest, uint32_t bytes)
{
	uint32_t i;

	i = 0;
	while (i < bytes)
	{
		dest[i] = src[i];
		i++;
	}
}

int kgetch() {
    getch_flag = 0;
    while (getch_flag != 1) {
        irq_install_handler(1, &getch_handler);
    }

    // Спецклавиши — возвращаем напрямую
    if (getch_scancode == 0x1B || getch_scancode == 0x0D || getch_scancode == 0x08 ||
        (getch_scancode >= 0x80 && getch_scancode <= 0x88)) {
        return getch_scancode;
    }

    // Обычные буквы/цифры
    if (shiftOn || capsOn || capsLock) {
            return get_acsii_high(getch_scancode);
    } else {
            return get_acsii_low(getch_scancode);
        }
}

char toupper(char c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}

int ksnprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    int written = 0;
    char* dest = str;
    const char* src = format;
    
    while (*src && written < size - 1) {
        if (*src == '%') {
            src++;
            switch (*src) {
                case 's': {
                    const char* s = va_arg(args, const char*);
                    while (*s && written < size - 1) {
                        *dest++ = *s++;
                        written++;
                    }
                    break;
                }
                case 'd': {
                    int num = va_arg(args, int);
                    char num_str[32];
                    int num_len = 0;
                    int is_neg = 0;
                    
                    if (num < 0) {
                        is_neg = 1;
                        num = -num;
                    }
                    
                    if (num == 0) {
                        num_str[num_len++] = '0';
                    } else {
                        while (num > 0) {
                            num_str[num_len++] = '0' + (num % 10);
                            num /= 10;
                        }
                    }
                    
                    if (is_neg && written < size - 1) {
                        *dest++ = '-';
                        written++;
                    }
                    
                    while (num_len > 0 && written < size - 1) {
                        *dest++ = num_str[--num_len];
                        written++;
                    }
                    break;
                }
                case 'x': {
                    unsigned int num = va_arg(args, unsigned int);
                    char hex_str[32];
                    int hex_len = 0;
                    
                    if (num == 0) {
                        hex_str[hex_len++] = '0';
                    } else {
                        while (num > 0) {
                            char digit = num % 16;
                            hex_str[hex_len++] = (digit < 10) ? '0' + digit : 'a' + (digit - 10);
                            num /= 16;
                        }
                    }
                    
                    while (hex_len > 0 && written < size - 1) {
                        *dest++ = hex_str[--hex_len];
                        written++;
                    }
                    break;
                }
                case '%': {
                    if (written < size - 1) {
                        *dest++ = '%';
                        written++;
                    }
                    break;
                }
                default:
                    if (written < size - 1) {
                        *dest++ = *src;
                        written++;
                    }
                    break;
            }
        } else {
            *dest++ = *src;
            written++;
        }
        src++;
    }
    
    *dest = '\0';
    va_end(args);
    return written;
}