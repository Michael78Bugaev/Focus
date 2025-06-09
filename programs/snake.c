#include <stdint.h>
#include <string.h>
#include <vga.h>
#include <vga_draw.h>
#include <keyboard.h>
#include <ports.h>
#include <pit.h>

#define EASY 0
#define MEDIUM 1
#define HARD 2
#define MAX_SNAKE_LEN 1000

// Global difficulty setting (0=Easy,1=Medium,2=Hard)
static int snake_difficulty = MEDIUM;

typedef struct { int x, y; } Point;

// Simple LCG for random numbers
static uint32_t rand_seed = 123456789;
static uint32_t lcg_rand(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return rand_seed;
}
static int random_in(int max) {
    return (lcg_rand() >> 16) % max;
}

// Settings menu: choose difficulty
static void snake_settings(void) {
    int selection = snake_difficulty;
    int cx = MAX_COLS / 2;
    int cy = MAX_ROWS / 2;
    while (1) {
        kclear();
        api_draw_text(cx - 4, cy - 2, "Settings", 0x0F);
        const char* labels[3] = {"Easy", "Medium", "Hard"};
        for (int i = 0; i < 3; i++) {
            int attr = (i == selection) ? 0x1F : 0x0F;
            api_draw_text(cx - 10 + i*10, cy, labels[i], attr);
        }
        int sc = get_key();
        if (sc == KEY_LEFT && selection > 0) selection--;
        else if (sc == KEY_RIGHT && selection < 2) selection++;
        else if (sc == KEY_ENTER) { snake_difficulty = selection; break; }
        else if (sc == KEY_ESC) { kclear(); return; }
    }
}

// add speeds and non-blocking input
static int speeds[3] = {300, 200, 100};
static int get_key_nb(void) {
    if (inb(0x64) & 1) return inb(0x60) & 0x7F;
    return 0;
}

// Main snake game loop
static void snake_play(void) {
    int bx = 0, by = 0;
    int bw = MAX_COLS;
    int bh = MAX_ROWS - 1;  // leave last row for UI
    int ix = bx + 1, iy = by + 1;
    int iw = bw - 2, ih = bh - 2;

    kclear();
    // draw ascii border
    for (int i = 0; i < 80; i++) {
        api_draw_text(i, 0, "°", 0x0E);
    }
    for (int x = bx; x < bx + bw; x++) {
        api_draw_text(x, by + bh - 1, "°", 0x0E);
        api_draw_text(x, by + bh - 1, "°", 0x0E);
    }
    for (int y = by; y < by + bh; y++) {
        api_draw_text(bx, y, "°", 0x0E);
        api_draw_text(bx + bw - 1, y, "°", 0x0E);
    }

    // initialize snake
    static Point snake[MAX_SNAKE_LEN];
    int snake_len = 3;
    int dirX = 1, dirY = 0;
    snake[0].x = ix + iw / 2;
    snake[0].y = iy + ih / 2;
    for (int i = 1; i < snake_len; i++) {
        snake[i].x = snake[0].x - i;
        snake[i].y = snake[0].y;
    }
    int score = 0;

    // place initial food
    Point food;
    food.x = ix + random_in(iw);
    food.y = iy + random_in(ih);
    api_draw_text(food.x, food.y, "*", 0x0C);

    // draw initial snake
    for (int i = 0; i < snake_len; i++) {
        api_draw_text(snake[i].x, snake[i].y, (i == 0 ? "O" : "o"), 0x0A);
    }

    // game loop
    while (1) {
        int sc = get_key_nb();
        if (sc == KEY_LEFT && dirX == 0)      { dirX = -1; dirY = 0; }
        else if (sc == KEY_RIGHT && dirX == 0){ dirX = 1;  dirY = 0; }
        else if (sc == KEY_UP && dirY == 0)   { dirX = 0;  dirY = -1; }
        else if (sc == KEY_DOWN && dirY == 0) { dirX = 0;  dirY = 1; }
        else if (sc == KEY_R) { return; } // restart

        // compute new head
        Point newHead = { snake[0].x + dirX, snake[0].y + dirY };
        // wall collision
        if (newHead.x <= bx || newHead.x >= bx + bw - 1 ||
            newHead.y <= by || newHead.y >= by + bh - 1) break;
        // self collision
        for (int i = 0; i < snake_len; i++) {
            if (snake[i].x == newHead.x && snake[i].y == newHead.y) goto GAME_OVER;
        }
        // eat food
        int ate = (newHead.x == food.x && newHead.y == food.y);
        if (ate) {
            if (snake_len < MAX_SNAKE_LEN) snake_len++;
            score++;
            // new food
            do {
                food.x = ix + random_in(iw);
                food.y = iy + random_in(ih);
                int coll = 0;
                for (int j = 0; j < snake_len; j++) {
                    if (snake[j].x == food.x && snake[j].y == food.y) { coll = 1; break; }
                }
                if (!coll) break;
            } while (1);
            api_draw_text(food.x, food.y, "*", 0x0C);
        } else {
            // erase tail
            Point tail = snake[snake_len - 1];
            api_draw_text(tail.x, tail.y, " ", 0x0F);
        }
        // move snake
        for (int i = snake_len - 1; i > 0; i--) {
            snake[i] = snake[i - 1];
        }
        snake[0] = newHead;
        api_draw_text(snake[0].x, snake[0].y, "O", 0x0A);

        // draw UI
        char sbuf[12]; char label[32];
        itoa(score, sbuf, 10);
        strcpy(label, "Score:"); strcat(label, sbuf);
        api_draw_text(2, MAX_ROWS - 1, label, 0x0F);
        api_draw_text(bw/2 - 10, MAX_ROWS - 1, "Move: arrows R-restart", 0x0F);

        // delay
        pit_sleep(speeds[snake_difficulty]);
    }
GAME_OVER:
    api_draw_text((bx + bw/2) - 5, by + bh/2, "Game Over", 0x0C);
    api_draw_text((bx + bw/2) - 10, by + bh/2 + 1, "   Press any key...", 0x0F);
    get_key();
}

// Entry point called from shell
void snake_main(void) {
    while (1) {
        int selection = 0;
        int cx = MAX_COLS / 2;
        int cy = MAX_ROWS / 2;
        // Main menu
        while (1) {
            kclear();
            api_draw_text(cx - 5, cy - 2, "Snake", 0x0F);
            api_draw_text(cx - 10, cy, "Play",    (selection == 0 ? 0x1F : 0x0F));
            api_draw_text(cx + 2,  cy, "Settings",(selection == 1 ? 0x1F : 0x0F));
            int sc = get_key();
            if (sc == KEY_LEFT && selection > 0) selection--;
            else if (sc == KEY_RIGHT && selection < 1) selection++;
            else if (sc == KEY_ENTER) break;
            else if (sc == KEY_ESC) return;
        }
        if (selection == 1) snake_settings();
        else snake_play();
    }
}
