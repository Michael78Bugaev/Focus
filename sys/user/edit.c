#include <stdint.h>
#ifndef VGA_MEM
#define VGA_MEM    ((uint16_t*)0xB8000)
#endif
#ifndef VGA_WIDTH
#define VGA_WIDTH  80
#endif
#include <vga.h>
#include <fat32.h>
#include <string.h>
#include <stdarg.h>
#include <vga_draw.h>

#define EDITOR_ROWS 22
#define EDITOR_COLS 79
#define EDITOR_START_ROW 1
#define EDITOR_START_COL 0
#define MAX_FILE_SIZE 4096
#define MAX_LINE_LENGTH 256
#define MAX_LINES 100000

static int horizontal_offset = 0; // ?????????????? ?????????
static int scroll_offset = 0;  // ???????????? ?????????

// Функция проверки наличия нажатой клавиши
int kbhit(void) {
    return kgetch() != 0;
}


// Структура для хранения строки текста
typedef struct {
    char text[MAX_LINE_LENGTH];
    int length;
} Line;

// --- ?????????? ?????????? ---
static Line lines[MAX_LINES];
static int num_lines = 0;
static int cursor_x = 0;
static int cursor_y = 0;
static char filename[32] = {0};
static int modified = 0;

// Цвета для меню
#define MENU_BG 0x70   // чёрный на светло-сером
#define MENU_SEL 0x07  // белый на чёрном
#define MENU_HILITE 0x07 // выделение File

// --- Сохраняем и восстанавливаем позицию VGA-курcора ---
uint16_t saved_cursor_x = 0, saved_cursor_y = 0;
void save_vga_cursor() {
    saved_cursor_x = get_cursor_x() / 2;
    saved_cursor_y = get_cursor_y();
}
void restore_vga_cursor() {
    set_cursor_x(EDITOR_START_COL + cursor_x);
    set_cursor_y(EDITOR_START_ROW + cursor_y);
}

static uint16_t screen_start;
static int screen_width;
static int screen_height;

static void move_cursor(int dx, int dy) {
    cursor_x += dx;
    cursor_y += dy;
    if (cursor_x < 0) cursor_x = 0;
    if (cursor_y < 0) cursor_y = 0;
    if (cursor_y >= num_lines) cursor_y = num_lines - 1;
    if (cursor_x > lines[cursor_y].length) cursor_x = lines[cursor_y].length;
    if (cursor_x >= screen_width) cursor_x = screen_width - 1;
}

static void draw_line(int line_num, const char* line) {
    uint16_t pos = screen_start + line_num * screen_width;
    int i;
    for (i = 0; line[i] && i < screen_width; i++) {
        write(line[i], WHITE_ON_BLACK, pos + i);
    }
    for (; i < screen_width; i++) {
        write(' ', WHITE_ON_BLACK, pos + i);
    }
}

static void draw_buffer() {
    uint16_t old_cursor = get_cursor();
    for (int i = 0; i < screen_height; i++) {
        if (i < num_lines) {
            draw_line(i, lines[i].text);
        } else {
            draw_line(i, "");
        }
    }
    set_cursor(old_cursor);
}

// Функции для работы с текстом
void insert_char(int x, int y, char c) {
    if (y >= num_lines) {
        if (num_lines < MAX_LINES) {
            lines[y].length = 0;
            num_lines++;
        } else return;
    }
    
    if (x >= MAX_LINE_LENGTH - 1) return;
    
    // ???????????: ??????????? ????? ?????? ???????, ????? ?? ?????? ????????? ??????
    if (lines[y].length < MAX_LINE_LENGTH - 1) {
        lines[y].length++;
    }
    // ???????? ??????? ??????
    for (int i = lines[y].length - 1; i > x; i--) {
        lines[y].text[i] = lines[y].text[i-1];
    }
    lines[y].text[x] = c;
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

int split_line(int x, int y) {
    if (y < 0 || y >= num_lines || num_lines >= MAX_LINES) return 0;
    if (x == 0) {
        // ???????? ?????? ?????? ????? ???????
        for (int i = num_lines; i > y; i--) {
            lines[i] = lines[i-1];
        }
        lines[y].length = 0;
        lines[y].text[0] = 0;
        num_lines++;
        modified = 1;
        return 1;
    }
    // ??????????? ?????????? ??????
    for (int i = num_lines; i > y + 1; i--) {
        lines[i] = lines[i-1];
    }
    int new_line_len = lines[y].length - x;
    if (new_line_len > 0) {
        strncpy(lines[y+1].text, &lines[y].text[x], new_line_len);
        lines[y+1].length = new_line_len;
        lines[y+1].text[new_line_len] = 0;
    } else {
        lines[y+1].length = 0;
        lines[y+1].text[0] = 0;
    }
    lines[y].length = x;
    lines[y].text[x] = 0;
    num_lines++;
    modified = 1;
    return 1;
}

// Функции отображения
void draw_menu() {
    // ???????? ??????? ?????? ?????? ????
    api_draw_rectangle(EDITOR_COLS, 1, 0, 0, MENU_BG);
    set_cursor_x(0); set_cursor_y(0);
    kprintf("<(70)> File   Edit   Help");
    // ??????? ??????? ??????? ??????
    char posbuf[32];
    ksnprintf(posbuf, sizeof(posbuf), "POSX: %d       POSY:%d", cursor_x + 1, cursor_y + 1);
    int pos_col = EDITOR_COLS - 1 - (int)strlen(posbuf);
    if (pos_col < 20) pos_col = 20; // ????? ?? ??????? ?? ????
    set_cursor_x(pos_col);
    set_cursor_y(0);
    kprintf("<(70)>%s", posbuf);
}

void draw_status(const char* msg) {
    // ???????? ?????? ?????? ?????? ????
    api_draw_rectangle(EDITOR_COLS, 1, 0, MAX_ROWS-1, MENU_BG);
    set_cursor_x(0); set_cursor_y(MAX_ROWS-1);
    char status[80];
    ksnprintf(status, sizeof(status), "%s %s", filename, modified ? "[Modified]" : "");
    kprintf("<(70)> %s", status);
}

void ensure_cursor_visible() {
    // ?????????????? ?????????
    if (cursor_x < horizontal_offset) {
        horizontal_offset = cursor_x;
    }
    if (cursor_x >= horizontal_offset + EDITOR_COLS) {
        horizontal_offset = cursor_x - EDITOR_COLS + 1;
    }
    // ???????????? ?????????
    if (cursor_y < scroll_offset) {
        scroll_offset = cursor_y;
    }
    if (cursor_y >= scroll_offset + EDITOR_ROWS) {
        scroll_offset = cursor_y - EDITOR_ROWS + 1;
    }
}

// Обновляем только измененную строку
void update_line(int row) {
    int line_idx = row + scroll_offset;
    if (line_idx < num_lines) {
        // ??????? ????? ?????? ???????? ? VGA
        api_draw_text(EDITOR_START_COL, EDITOR_START_ROW + row, lines[line_idx].text, GRAY_ON_BLACK);
        // ???? ?????? ?????? EDITOR_COLS ? ???????? ??????? ?????????
        int len = lines[line_idx].length;
        if (len < EDITOR_COLS) {
            // ???????? ??????? ?????? ?????????
            for (int i = len; i < EDITOR_COLS; i++) {
                api_draw_text(EDITOR_START_COL + i, EDITOR_START_ROW + row, " ", GRAY_ON_BLACK);
            }
        }
    } else {
        // ?????? ?????? ? ???????? ??? ?????? ?????????
        api_draw_rectangle(EDITOR_COLS, 1, EDITOR_START_COL, EDITOR_START_ROW + row, GRAY_ON_BLACK);
    }
}

void update_screen() {
    for (int row = 0; row < EDITOR_ROWS; row++) {
        update_line(row);
    }
    // ?????????? ?????????? ?????? ?????? ????? ???? ?????????
    update_cursor();
}

// Обновляем только курсор
void update_cursor() {
    set_cursor_x(EDITOR_START_COL + cursor_x - horizontal_offset);
    set_cursor_y(EDITOR_START_ROW + cursor_y - scroll_offset);
}

// ??????? ??? ?????????? ?????? POSX ? POSY ? ??????? ??????
void update_cursor_pos_panel() {
    char posbuf[32];
    ksnprintf(posbuf, sizeof(posbuf), "POSX: %d       POSY:%d", cursor_x + 1, cursor_y + 1);
    int pos_col = EDITOR_COLS - 1 - (int)strlen(posbuf);
    if (pos_col < 20) pos_col = 20;
    // ?????? ?????? ? VGA ??? ??????????? ??????????? ???????
    int vga_offset = 0 + pos_col; // ?????? 0, ??????? pos_col
    for (int i = 0; posbuf[i] && (pos_col + i) < VGA_WIDTH; i++) {
        VGA_MEM[vga_offset + i] = (MENU_BG << 8) | posbuf[i];
    }
}

// --- Окно ошибки ---
int show_error_box(const char* message, int allow_cancel) {
    int win_width = 40;
    int win_height = 5;
    int win_row = 8;
    int win_col = (MAX_COLS - win_width) / 2;
    // ?????? ????
    save_vga_cursor();
    for (int r = 0; r < win_height; r++) {
        set_cursor_x(win_col);
        set_cursor_y(win_row + r);
        for (int c = 0; c < win_width; c++) {
            char ch = ' ';
            if (r == 0 || r == win_height - 1) ch = '-';
            if (c == 0 || c == win_width - 1) ch = '|';
            if ((r == 0 || r == win_height - 1) && (c == 0 || c == win_width - 1)) ch = '+';
            kputchar(ch, 0x70);
        }
    }
    restore_vga_cursor();
    
    // ?????????
    set_cursor_x(win_col + 2);
    set_cursor_y(win_row + 2);
    kprintf("<(70)>%s", message);
    
    // ??????
    int btn_ok_col = win_col + win_width/2 - 6;
    int btn_cancel_col = win_col + win_width/2 + 2;
    set_cursor_y(win_row + win_height - 2);
    set_cursor_x(btn_ok_col); kprintf("<(70)>[ Ok ]");
    if (allow_cancel) { set_cursor_x(btn_cancel_col); kprintf("<(70)>[Cancel]"); }
    // Навигация
    int selected = 0;
    while (1) {
        set_cursor_y(win_row + win_height - 2);
        set_cursor_x(btn_ok_col); kprintf(selected == 0 ? "<(07)>[ Ok ]" : "<(70)>[ Ok ]");
        if (allow_cancel) {
            set_cursor_x(btn_cancel_col); kprintf(selected == 1 ? "<(07)>[Cancel]" : "<(70)>[Cancel]");
        }
        int ch = kgetch();
        if (allow_cancel && (ch == 0x83 || ch == 9)) selected = 1 - selected; // Tab или Right/Left
        if (ch == 13) {
            draw_menu();
            update_screen();
            return selected; // Enter: 0=Ok, 1=Cancel
        }
        if (ch == 27) {
            draw_menu();
            update_screen();
            return 1; // ESC = Cancel
        }
    }
}

// --- ?????????????? ???? ? ???????? ??????? ? ?????????????? ---
void info_window(const char* fmt, ...) {
    int win_width = 40;
    int win_height = 5;
    int win_row = 8;
    int win_col = (MAX_COLS - win_width) / 2;
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    ksnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    
    // ????????? ?? ??????
    char* lines[8];
    int nlines = 0;
    char* p = msg;
    while (*p && nlines < 8) {
        lines[nlines++] = p;
        char* nl = strchr(p, '\n');
        if (nl) {
            *nl = 0;
            p = nl + 1;
        } else {
            break;
        }
    }
    
    // ?????? ????
    save_vga_cursor();
    for (int r = 0; r < win_height; r++) {
        set_cursor_x(win_col);
        set_cursor_y(win_row + r);
        for (int c = 0; c < win_width; c++) {
            char ch = ' ';
            if (r == 0 || r == win_height - 1) ch = '-';
            if (c == 0 || c == win_width - 1) ch = '|';
            if ((r == 0 || r == win_height - 1) && (c == 0 || c == win_width - 1)) ch = '+';
            kputchar(ch, 0x70);
        }
    }
    restore_vga_cursor();
    
    // ?????????? ? ??????? ?????? ??????
    save_vga_cursor();
    for (int i = 0; i < nlines; i++) {
        int msglen = strlen(lines[i]);
        int msg_col = win_col + (win_width - msglen) / 2;
        set_cursor_x(msg_col);
        set_cursor_y(win_row + 1 + i);
        kprintf("<(70)>%s", lines[i]);
    }
    restore_vga_cursor();
    
    // ?????? Ok ? ?????? ????????? (blink-??? VGA)
    int btn_ok_col = win_col + (win_width - 6) / 2; // ?????????? ?????? [ Ok ]
    int blink = 0;
    while (1) {
        set_cursor_y(win_row + win_height - 2);
        set_cursor_x(btn_ok_col);
        if (blink) {
            kprintf("<(07)>[ Ok ]"); // blink
            set_cursor_x(btn_ok_col);
        } else {
            kprintf("<(70)>[ Ok ]");    // ??????? ????
            set_cursor_x(btn_ok_col);
        }
        for (volatile int d = 0; d < 1000000; d++); // ??????? ????????
        blink = !blink;
        if (kbhit()) {
            int ch = kgetch();
            if (ch == 13 || ch == 27) break;
        }
    }
    draw_menu();
    update_screen();
}

// --- ???? ????? ?????? (??????????? ??????????????) ---
int input_window(char* out, int maxlen, const char* prompt) {
    int win_width = 60;
    int win_height = 5;
    int win_row = 8;
    int win_col = (MAX_COLS - win_width) / 2;
    // ?????? ????
    save_vga_cursor();
    for (int r = 0; r < win_height; r++) {
        set_cursor_x(win_col);
        set_cursor_y(win_row + r);
        for (int c = 0; c < win_width; c++) {
            char ch = ' ';
            if (r == 0 || r == win_height - 1) ch = '?';
            if (c == 0 || c == win_width - 1) ch = '�';
            if ((r == 0 || r == win_height - 1) && (c == 0 || c == win_width - 1)) ch = '�';
            kputchar(ch, 0x70);
        }
    }
    restore_vga_cursor();
    
    // ?????????
    save_vga_cursor();
    int promptlen = strlen(prompt);
    int prompt_col = win_col + (win_width - promptlen) / 2;
    set_cursor_x(prompt_col);
    set_cursor_y(win_row + 1);
    kprintf("<(70)>%s", prompt);
    restore_vga_cursor();
    
    // ???? ?????
    int input_col = win_col + 4;
    int input_row = win_row + 3;
    set_cursor_x(input_col);
    set_cursor_y(input_row);
    int pos = 0;
    out[0] = 0;
    while (1) {
        // ?????????? ????
        set_cursor_x(input_col);
        set_cursor_y(input_row);
        for (int i = 0; i < maxlen - 1; i++) {
            char ch = (i < pos) ? out[i] : ' ';
            kputchar(ch, 0x70);
        }
        set_cursor_x(input_col + pos);
        set_cursor_y(input_row);
        int ch = kgetch();
        if (ch == 13) { // Enter
            out[pos] = 0;
            draw_menu();
            update_screen();
            return 1;
        } else if (ch == 27) { // Esc
            out[0] = 0;
            draw_menu();
            update_screen();
            return 0;
        } else if (ch == 8) { // Backspace
            if (pos > 0) pos--;
            out[pos] = 0;
        } else if (ch >= 32 && ch <= 126 && pos < maxlen - 1) {
            out[pos++] = (char)ch;
            out[pos] = 0;
        }
    }
}

// ??????? ????????????? ?????? ?????????
void editor_screen_init(const char* status_msg) {
    draw_menu();
    update_screen();
    draw_status(status_msg);
    update_cursor();
}

// Функции работы с файлами
void load_file(const char* fname) {
    char fatname[12];
    memset(fatname, ' ', 11);
    fatname[11] = 0;
    int clen = strlen(fname);
    int dot = -1;
    for (int i = 0; i < clen; i++) if (fname[i] == '.') { dot = i; break; }
    if (dot == -1) {
        for (int i = 0; i < clen && i < 8; i++) fatname[i] = toupper(fname[i]);
    } else {
        for (int i = 0; i < dot && i < 8; i++) fatname[i] = toupper(fname[i]);
        for (int i = dot + 1, j = 8; i < clen && j < 11; i++, j++) fatname[j] = toupper(fname[i]);
    }
    fat32_dir_entry_t entries[32];
    int n = fat32_read_dir(0, current_dir_cluster, entries, 32);
    int found = 0;
    uint32_t cl = 0;
    for (int i = 0; i < n; i++) {
        if (strncmp(entries[i].name, fatname, 11) == 0) {
            if (entries[i].attr & 0x10) {
                show_error_box("Cannot open directory as file!", 0);
                return;
            }
            cl = ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
            found = 1;
            break;
        }
    }
    if (found) {
        char buffer[MAX_FILE_SIZE];
        int size = fat32_read_file(0, cl, (uint8_t*)buffer, MAX_FILE_SIZE-1);
        if (size > 0) {
            buffer[size] = 0;
            num_lines = 0;
            int i = 0, line_start = 0;
            while (i <= size && num_lines < MAX_LINES) {
                if (buffer[i] == '\n' || buffer[i] == 0) {
                    int len = i - line_start;
                    if (len > MAX_LINE_LENGTH-1) len = MAX_LINE_LENGTH-1;
                    if (len > 0) {
                        strncpy(lines[num_lines].text, &buffer[line_start], len);
                        lines[num_lines].text[len] = 0;
                        lines[num_lines].length = len;
                    } else {
                        lines[num_lines].text[0] = 0;
                        lines[num_lines].length = 0;
                    }
                    num_lines++;
                    line_start = i + 1;
                }
                i++;
            }
            // ???? ????????? ?????? ?? ??????, ????????? ?????? ??????
            if (num_lines < MAX_LINES && num_lines > 0 && lines[num_lines-1].length != 0) {
                lines[num_lines].length = 0;
                lines[num_lines].text[0] = 0;
                //num_lines++;
            }
            // ???? ???? ??????, ????????? ???? ?????? ??????
            if (num_lines == 0) {
                lines[0].length = 0;
                lines[0].text[0] = 0;
                num_lines = 1;
            }
            modified = 0;
            editor_screen_init("File loaded");
            return;
        }
    } else {
        // ????? ????
        num_lines = 1;
        lines[0].length = 0;
        lines[0].text[0] = 0;
        modified = 0;
        editor_screen_init("New file");
        cursor_x = 0;
        cursor_y = 0;
        scroll_offset = 0;
    }
}

void save_file(const char* fname) {
    // Проверяем, не директория ли это
    fat32_dir_entry_t entries[32];
    int n = fat32_read_dir(0, current_dir_cluster, entries, 32);
    for (int i = 0; i < n; i++) {
        char fatname[12];
        strncpy(fatname, fname, 11); fatname[11] = 0;
        if (strncmp(entries[i].name, fatname, 11) == 0 && (entries[i].attr & 0x10)) {
            show_error_box("Cannot save to a directory!", 0);
            return;
        }
    }
    char buffer[MAX_FILE_SIZE];
    int pos = 0;
    // ????? ????????? ???????? ?????? ??????
    int last_nonempty = num_lines - 1;
    while (last_nonempty > 0 && lines[last_nonempty].length == 0) {
        last_nonempty--;
    }
    for (int i = 0; i <= last_nonempty; i++) {
        if (pos + lines[i].length + 1 >= MAX_FILE_SIZE) break;
        strncpy(buffer + pos, lines[i].text, lines[i].length);
        pos += lines[i].length;
        buffer[pos++] = '\n';
    }
    int res = fat32_write_file(0, fname, (const uint8_t*)buffer, pos);
    if (res >= 0) {
        modified = 0;
        info_window("File saved");
    } else {
        show_error_box("Save error!", 1);
        draw_status("Save error");
    }
}

// Нарисовать прямоугольник меню
void draw_menu_box(int menu_row, int menu_col, int menu_width, int menu_height) {
    // ???????? ??????? ???? ??????
    api_draw_rectangle(menu_width, menu_height, menu_col, menu_row, MENU_BG);
}

void draw_menu_bar_highlighted() {
    set_cursor_x(1); set_cursor_y(0);
    // Выделяем слово File
    kputchar('F', MENU_HILITE);
    kputchar('i', MENU_HILITE);
    kputchar('l', MENU_HILITE);
    kputchar('e', MENU_HILITE);
    kputchar(' ', MENU_BG);
    // Остальные пункты обычным цветом
    kprintf("<(70)>  Edit   Help");
    for (int i = 21; i < MAX_COLS; i++) kprintf("<(70)> ");
}

void draw_file_menu(int sel, const char** items, int n_items, int menu_row, int menu_col, int menu_width) {
    for (int i = 0; i < n_items; i++) {
        set_cursor_x(menu_col);
        set_cursor_y(menu_row + i);
        int color = (i == sel) ? MENU_SEL : MENU_BG;
        char buf[32];
        int len = strlen(items[i]);
        strncpy(buf, items[i], sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;
        for (int j = len; j < menu_width && j < (int)sizeof(buf) - 1; j++) {
            buf[j] = ' ';
            buf[j + 1] = 0;
        }
        for (int j = 0; buf[j]; j++) kputchar(buf[j], color);
    }
}

int show_file_menu() {
    const char* items[] = {"Save", "Exit", "About"};
    const int n_items = 3;
    int selected = 0;
    int menu_row = 1, menu_col = 0;
    int menu_width = 12;

    draw_menu_bar_highlighted();
    draw_menu_box(menu_row, menu_col, menu_width, n_items);
    draw_file_menu(selected, items, n_items, menu_row, menu_col, menu_width);
    save_vga_cursor();
    update_cursor();
    restore_vga_cursor();

    while (1) {
        int ch = kgetch();
        if (ch == 0x80) { // Up
            if (selected > 0) selected--;
            save_vga_cursor();
            draw_file_menu(selected, items, n_items, menu_row, menu_col, menu_width);
            restore_vga_cursor();
        } else if (ch == 0x81) { // Down
            if (selected < n_items - 1) selected++;
            save_vga_cursor();
            draw_file_menu(selected, items, n_items, menu_row, menu_col, menu_width);
            restore_vga_cursor();
        } else if (ch == 13) { // Enter
            // ????? ?????? ?? ???? ???????? ??????? ???? ??????
            clear_menu_box(menu_row, menu_col, menu_width, n_items);
            return selected + 1;
        } else if (ch == 27) {
            clear_menu_box(menu_row, menu_col, menu_width, n_items);
            return 0;
        }
    }
}

void clear_menu_box(int menu_row, int menu_col, int menu_width, int menu_height) {
    // ?????? ???????? ??????? ?????? ????
    api_draw_rectangle(menu_width, menu_height, menu_col, menu_row, MENU_BG);
}

// ???????????? ???????????? ??????? ??? ????????????? ???????????? ???????
void editor_main(const char* fname) {
    kclear();
    strncpy(filename, fname, sizeof(filename)-1);
    load_file(fname);
    int running = 1;
    bool esc = false;
    while (running) {
        int ch = kgetch();
        
        // ????? ??????????? ??????? ????????? ? clamp cursor_y
        if (cursor_y > num_lines - 1) cursor_y = num_lines - 1;
        if (cursor_y < 0) cursor_y = 0;
        // ???? ?????? ???????? ?? ?????????????? ?????? ? ??????? ????? ?????? ??????
        if (cursor_y == num_lines && num_lines < MAX_LINES) {
            lines[num_lines].length = 0;
            lines[num_lines].text[0] = 0;
            num_lines++;
        }
        
        switch(ch) {
            case 27: { // ESC
                int menu_action = show_file_menu();
                draw_menu();
                if (menu_action == 1) save_file(filename);
                else if (menu_action == 2) { running = 0; kclear(); esc = true;}
                else if (menu_action == 3) {
                    info_window("FOCUS Text Editor v0.3\n   Created by Michael Bugaev");
                    update_screen();
                }
                break;
            }
                
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
                    update_screen();
                } else if (cursor_y > 0) {
                    cursor_x = lines[cursor_y-1].length;
                    strcat(lines[cursor_y-1].text, lines[cursor_y].text);
                    lines[cursor_y-1].length = strlen(lines[cursor_y-1].text);
                    for (int i = cursor_y; i < num_lines-1; i++) {
                        lines[i] = lines[i+1];
                    }
                    num_lines--;
                    cursor_y--;
                    for (int i = cursor_y; i < EDITOR_ROWS; i++) {
                        update_line(i);
                    }
                }
                break;
                
            case 13: // Enter
                if (cursor_y == num_lines - 1) {
                    // ???????? ????? ?????? ?????? ? ?????
                    if (num_lines < MAX_LINES) {
                        lines[num_lines].length = 0;
                        lines[num_lines].text[0] = 0;
                        num_lines++;
                        cursor_y = num_lines - 1;
                        cursor_x = 0;
                    }
                } else {
                    int did_split = split_line(cursor_x, cursor_y);
                    if (did_split) {
                        cursor_y++;
                        cursor_x = 0;
                    }
                }
                int start = cursor_y > 0 ? cursor_y - 1 : 0;
                for (int i = start; i < EDITOR_ROWS; i++) {
                    update_line(i);
                }
                ensure_cursor_visible();
                update_screen();
                update_cursor();
                break;
                
            case 0x80: // Up Arrow
                if (cursor_y > 0) {
                    cursor_y--;
                    ensure_cursor_visible();
                    update_screen();
                    update_cursor();
                }
                break;
                
            case 0x81: // Down Arrow
                if (cursor_y < num_lines - 1) {
                    cursor_y++;
                    ensure_cursor_visible();
                    update_screen();
                    update_cursor();
                }
                break;
                
            case 0x82: // Left Arrow
                if (cursor_x > 0) {
                    cursor_x--;
                    ensure_cursor_visible();
                    update_screen();
                    update_cursor();
                } else if (cursor_y > 0) {
                    cursor_y--;
                    cursor_x = lines[cursor_y].length;
                    ensure_cursor_visible();
                    update_screen();
                    update_cursor();
                }
                break;
                
            case 0x83: // Right Arrow
                if (cursor_x < lines[cursor_y].length) {
                    cursor_x++;
                    ensure_cursor_visible();
                    update_screen();
                    update_cursor();
                } else if (cursor_y < num_lines - 1) {
                    cursor_y++;
                    cursor_x = 0;
                    ensure_cursor_visible();
                    update_screen();
                    update_cursor();
                }
                break;
                
            default:
                if (ch >= 32 && ch <= 126) {
                    // ????? ???????? ??????? ? ???? ?????? ?? ?????????????? ??????, ??????? ??????
                    if (cursor_y == num_lines && num_lines < MAX_LINES) {
                        lines[num_lines].length = 0;
                        lines[num_lines].text[0] = 0;
                        num_lines++;
                    }
                    insert_char(cursor_x, cursor_y, ch);
                    cursor_x++;
                    ensure_cursor_visible();
                    update_screen();
                    update_cursor();
                }
                break;
        }
        // ????? ?????? ???????? ? clamp cursor_y
        if (cursor_y > num_lines - 1) cursor_y = num_lines - 1;
        if (cursor_y < 0) cursor_y = 0;
        if (cursor_x > lines[cursor_y].length) cursor_x = lines[cursor_y].length;
        if (cursor_x < 0) cursor_x = 0;
        update_cursor_pos_panel();
    }
    if (esc) { kclear(); return; }
    kclear();
    draw_menu();
    for (int row = 0; row < EDITOR_ROWS; row++) update_line(row);
    update_cursor();
} 
