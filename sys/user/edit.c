#include <stdint.h>
#include <vga.h>
#include <fat32.h>
#include <string.h>
#include <kernel.h>

#define EDITOR_ROWS 22
#define EDITOR_COLS 78
#define EDITOR_START_ROW 1
#define EDITOR_START_COL 1
#define MAX_FILE_SIZE 4096
#define MAX_LINE_LENGTH 256
#define MAX_LINES 1000

// Структура для хранения строки текста
typedef struct {
    char text[MAX_LINE_LENGTH];
    int length;
} Line;

// Глобальные переменные редактора
static Line lines[MAX_LINES];
static int num_lines = 0;
static int cursor_x = 0;
static int cursor_y = 0;
static char filename[32] = {0};
static int modified = 0;
static int scroll_offset = 0;  // Смещение для прокрутки

// Функции для работы с текстом
void insert_char(int x, int y, char c) {
    if (y >= num_lines) {
        if (num_lines < MAX_LINES) {
            lines[y].length = 0;
            num_lines++;
        } else return;
    }
    
    if (x >= MAX_LINE_LENGTH - 1) return;
    
    // Сдвигаем текст вправо
    for (int i = lines[y].length; i > x; i--) {
        lines[y].text[i] = lines[y].text[i-1];
    }
    
    lines[y].text[x] = c;
    if (x >= lines[y].length) lines[y].length = x + 1;
    lines[y].text[lines[y].length] = 0;
    modified = 1;
}

void delete_char(int x, int y) {
    if (y >= num_lines || x >= lines[y].length) return;
    
    // Сдвигаем текст влево
    for (int i = x; i < lines[y].length - 1; i++) {
        lines[y].text[i] = lines[y].text[i+1];
    }
    
    lines[y].length--;
    lines[y].text[lines[y].length] = 0;
    modified = 1;
}

void split_line(int x, int y) {
    if (y >= num_lines || num_lines >= MAX_LINES - 1) return;
    
    // Сдвигаем все строки вниз
    for (int i = num_lines; i > y + 1; i--) {
        lines[i] = lines[i-1];
    }
    
    // Копируем оставшийся текст в новую строку
    int new_line_len = lines[y].length - x;
    if (new_line_len > 0) {
        memcpy(lines[y+1].text, &lines[y].text[x], new_line_len);
        lines[y+1].length = new_line_len;
        lines[y+1].text[new_line_len] = 0;
    } else {
        lines[y+1].length = 0;
        lines[y+1].text[0] = 0;
    }
    
    // Обрезаем текущую строку
    lines[y].length = x;
    lines[y].text[x] = 0;
    
    num_lines++;
    modified = 1;
}

// Функции отображения
void draw_menu() {
    set_cursor_x(0); set_cursor_y(0);
    kprintf("<(70)> File   Edit   Help");
    for (int i = 20; i < MAX_COLS; i++) kprintf("<(70)> ");
}

void draw_status(const char* msg) {
    set_cursor_x(0); set_cursor_y(MAX_ROWS+1);
    char status[80];
    ksnprintf(status, sizeof(status), "%s %s", filename, modified ? "[Modified]" : "");
    kprintf("<(70)> %s", status);
    for (int i = strlen(status)+2; i < MAX_COLS; i++) kprintf("<(70)> ");
}

// Обновляем только измененную строку
void update_line(int row) {
    set_cursor_x(EDITOR_START_COL); 
    set_cursor_y(EDITOR_START_ROW + row);
    
    int line_idx = row + scroll_offset;
    if (line_idx < num_lines) {
        for (int col = 0; col < EDITOR_COLS; col++) {
            char c = (col < lines[line_idx].length) ? lines[line_idx].text[col] : ' ';
            kputchar(c, GRAY_ON_BLACK);
        }
    } else {
        for (int col = 0; col < EDITOR_COLS; col++) {
            kputchar(' ', GRAY_ON_BLACK);
        }
    }
}

// Обновляем только курсор
void update_cursor() {
    set_cursor_x(EDITOR_START_COL + cursor_x);
    set_cursor_y(EDITOR_START_ROW + cursor_y);
}

// Функции работы с файлами
void load_file(const char* fname) {
    uint32_t cl = 0;
    if (fat32_resolve_path(0, fname, &cl) == 0) {
        char buffer[MAX_FILE_SIZE];
        int size = fat32_read_file(0, cl, (uint8_t*)buffer, MAX_FILE_SIZE-1);
        if (size > 0) {
            buffer[size] = 0;
            num_lines = 0;
            char* line = strtok(buffer, "\n");
            while (line && num_lines < MAX_LINES) {
                strncpy(lines[num_lines].text, line, MAX_LINE_LENGTH-1);
                lines[num_lines].length = strlen(line);
                lines[num_lines].text[lines[num_lines].length] = 0;
                num_lines++;
                line = strtok(NULL, "\n");
            }
            modified = 0;
            draw_status("File loaded");
        }
    } else {
        num_lines = 1;
        lines[0].length = 0;
        lines[0].text[0] = 0;
        modified = 0;
        draw_status("New file");
    }
}

void save_file(const char* fname) {
    char buffer[MAX_FILE_SIZE];
    int pos = 0;
    
    for (int i = 0; i < num_lines; i++) {
        if (pos + lines[i].length + 1 >= MAX_FILE_SIZE) break;
        memcpy(buffer + pos, lines[i].text, lines[i].length);
        pos += lines[i].length;
        buffer[pos++] = '\n';
    }
    
    int res = fat32_write_file(0, fname, (const uint8_t*)buffer, pos);
    if (res >= 0) {
        modified = 0;
        draw_status("File saved");
    } else {
        draw_status("Save error");
    }
}

// Основная функция редактора
void editor_main(const char* fname) {
    kclear();
    strncpy(filename, fname, sizeof(filename)-1);
    draw_menu();
    load_file(fname);
    
    // Начальная отрисовка текста
    for (int row = 0; row < EDITOR_ROWS; row++) {
        update_line(row);
    }
    update_cursor();
    
    int running = 1;
    while (running) {
        int ch = kgetch();
        int old_cursor_y = cursor_y;
        
        switch(ch) {
            case 27: // ESC
                running = 0;
                break;
                
            case 19: // Ctrl+S
                save_file(filename);
                break;
                
            case 15: // Ctrl+O
                draw_status("Open not implemented");
                break;
                
            case 8: // Backspace
                if (cursor_x > 0) {
                    delete_char(cursor_x - 1, cursor_y);
                    cursor_x--;
                    update_line(cursor_y);
                } else if (cursor_y > 0) {
                    cursor_x = lines[cursor_y-1].length;
                    // Объединяем строки
                    strcat(lines[cursor_y-1].text, lines[cursor_y].text);
                    lines[cursor_y-1].length = strlen(lines[cursor_y-1].text);
                    // Сдвигаем все строки вверх
                    for (int i = cursor_y; i < num_lines-1; i++) {
                        lines[i] = lines[i+1];
                    }
                    num_lines--;
                    cursor_y--;
                    // Обновляем все строки от курсора до конца
                    for (int i = cursor_y; i < EDITOR_ROWS; i++) {
                        update_line(i);
                    }
                }
                break;
                
            case 13: // Enter
                split_line(cursor_x, cursor_y);
                cursor_x = 0;
                cursor_y++;
                // Обновляем все строки от курсора до конца
                for (int i = cursor_y-1; i < EDITOR_ROWS; i++) {
                    update_line(i);
                }
                break;
                
            case 0x80: // Up Arrow
                if (cursor_y > 0) {
                    cursor_y--;
                    if (cursor_x > lines[cursor_y].length) {
                        cursor_x = lines[cursor_y].length;
                    }
                }
                break;
                
            case 0x81: // Down Arrow
                if (cursor_y < num_lines - 1) {
                    cursor_y++;
                    if (cursor_x > lines[cursor_y].length) {
                        cursor_x = lines[cursor_y].length;
                    }
                }
                break;
                
            case 0x82: // Left Arrow
                if (cursor_x > 0) {
                    cursor_x--;
                } else if (cursor_y > 0) {
                    cursor_y--;
                    cursor_x = lines[cursor_y].length;
                }
                break;
                
            case 0x83: // Right Arrow
                if (cursor_x < lines[cursor_y].length) {
                    cursor_x++;
                } else if (cursor_y < num_lines - 1) {
                    cursor_y++;
                    cursor_x = 0;
                }
                break;
                
            case 0x84: // Home
                cursor_x = 0;
                break;
                
            case 0x85: // End
                cursor_x = lines[cursor_y].length;
                break;
                
            case 0x86: // Page Up
                cursor_y = (cursor_y > EDITOR_ROWS) ? cursor_y - EDITOR_ROWS : 0;
                if (cursor_x > lines[cursor_y].length) {
                    cursor_x = lines[cursor_y].length;
                }
                break;
                
            case 0x87: // Page Down
                cursor_y = (cursor_y + EDITOR_ROWS < num_lines) ? cursor_y + EDITOR_ROWS : num_lines - 1;
                if (cursor_x > lines[cursor_y].length) {
                    cursor_x = lines[cursor_y].length;
                }
                break;
                
            case 0x88: // Delete
                if (cursor_x < lines[cursor_y].length) {
                    delete_char(cursor_x, cursor_y);
                    update_line(cursor_y);
                } else if (cursor_y < num_lines - 1) {
                    // Объединяем текущую строку со следующей
                    strcat(lines[cursor_y].text, lines[cursor_y+1].text);
                    lines[cursor_y].length = strlen(lines[cursor_y].text);
                    // Сдвигаем все строки вверх
                    for (int i = cursor_y + 1; i < num_lines - 1; i++) {
                        lines[i] = lines[i+1];
                    }
                    num_lines--;
                    // Обновляем все строки от курсора до конца
                    for (int i = cursor_y; i < EDITOR_ROWS; i++) {
                        update_line(i);
                    }
                }
                break;
                
            default:
                if (ch >= 32 && ch <= 126) {
                    insert_char(cursor_x, cursor_y, ch);
                    cursor_x++;
                    update_line(cursor_y);
                }
                break;
        }
        
        // Ограничиваем курсор
        if (cursor_y >= num_lines) cursor_y = num_lines - 1;
        if (cursor_y < 0) cursor_y = 0;
        if (cursor_x > lines[cursor_y].length) cursor_x = lines[cursor_y].length;
        if (cursor_x < 0) cursor_x = 0;
        
        // Обновляем курсор только если его позиция изменилась
        if (old_cursor_y != cursor_y) {
            update_cursor();
        }
    }
    
    kclear();
} 
