// FOCUS Graphical Double buffering snake game
#include <stdint.h>
#include <vbe_terminal.h>
#include <string.h>
#include <ports.h>
#include <keyboard.h>

/* External drawing primitives from vbe_api */
extern void vapi_draw_rect(int x, int y, int w, int h, uint32_t color);
extern void vapi_draw_text(int x, int y, const char *text, uint32_t color);

/* Optional sleep helper from PIT timer (ms). */
extern void apic_timer_sleep(uint32_t ms);

#define CELL_SIZE   10      /* pixels per cell */
#define GRID_COLS   64      /* 640px / 10 = 64 */
#define GRID_ROWS   48      /* 480px / 10 = 48 */
#define MAX_LENGTH  (GRID_COLS*GRID_ROWS)

typedef struct { int x, y; } Point;

static Point snake[MAX_LENGTH];
static int snake_len;

static Point food;

static enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } dir;

/* Very small LCG RNG */
static uint32_t rng_state = 0x12345678;
static uint32_t rand32(void) { rng_state = rng_state * 1664525 + 1013904223; return rng_state; }

/* Place food at random empty cell */
static void place_food(void)
{
    while (1) {
        int x = rand32() % GRID_COLS;
        int y = rand32() % GRID_ROWS;
        int collide = 0;
        for (int i = 0; i < snake_len; i++) if (snake[i].x == x && snake[i].y == y) { collide = 1; break; }
        if (!collide) { food.x = x; food.y = y; return; }
    }
}

static void draw_board(void)
{
    /* Clear screen */
    kclear();

    /* Draw snake */
    for (int i = 0; i < snake_len; i++) {
        int px = snake[i].x * CELL_SIZE;
        int py = snake[i].y * CELL_SIZE;
        vapi_draw_rect(px, py, CELL_SIZE, CELL_SIZE, 0x0A); /* green */
    }

    /* Draw food */
    vapi_draw_rect(food.x * CELL_SIZE, food.y * CELL_SIZE, CELL_SIZE, CELL_SIZE, 0x0C); /* red */

    /* Swap to screen */
    vbe_swap();
}

void snake_main(void)
{
    /* Seed RNG from PIT ticks if available */
    extern uint32_t ticks; rng_state += ticks;

    /* Initialize snake starting position */
    snake_len = 3;
    snake[0] = (Point){ GRID_COLS/2, GRID_ROWS/2 };
    snake[1] = (Point){ GRID_COLS/2 - 1, GRID_ROWS/2 };
    snake[2] = (Point){ GRID_COLS/2 - 2, GRID_ROWS/2 };
    dir = DIR_RIGHT;

    place_food();

    int speed_ms = 120; /* initial speed */
    int game_over = 0;

    while (!game_over) {
        /* Handle keyboard (non-blocking) */
        if (get_key()) {
            uint8_t sc = get_key();
            switch (sc) {
                case KEY_UP:    if (dir != DIR_DOWN)  dir = DIR_UP;    break;
                case KEY_DOWN:  if (dir != DIR_UP)    dir = DIR_DOWN;  break;
                case KEY_LEFT:  if (dir != DIR_RIGHT) dir = DIR_LEFT;  break;
                case KEY_RIGHT: if (dir != DIR_LEFT)  dir = DIR_RIGHT; break;
                case KEY_ESC:   game_over = 1; continue;
            }
        }

        /* Move snake: shift body */
        for (int i = snake_len - 1; i > 0; i--) snake[i] = snake[i-1];

        /* Update head */
        switch (dir) {
            case DIR_UP:    snake[0].y--; break;
            case DIR_DOWN:  snake[0].y++; break;
            case DIR_LEFT:  snake[0].x--; break;
            case DIR_RIGHT: snake[0].x++; break;
        }

        /* Wrap around edges */
        if (snake[0].x < 0) snake[0].x = GRID_COLS - 1;
        if (snake[0].x >= GRID_COLS) snake[0].x = 0;
        if (snake[0].y < 0) snake[0].y = GRID_ROWS - 1;
        if (snake[0].y >= GRID_ROWS) snake[0].y = 0;

        /* Self-collision detection */
        for (int i = 1; i < snake_len; i++) {
            if (snake[i].x == snake[0].x && snake[i].y == snake[0].y) { game_over = 1; break; }
        }
        if (game_over) break;

        /* Food eaten? */
        if (snake[0].x == food.x && snake[0].y == food.y) {
            if (snake_len < MAX_LENGTH) snake_len++;
            place_food();
            if (speed_ms > 40) speed_ms -= 4; /* accelerate */
        }

        draw_board();

        /* Delay */
        //apic_timer_sleep(speed_ms);
    }

    vapi_draw_text(10, 10, "Game Over!", 0x0F);

    get_key();

    /* Small pause so player sees message */
    apic_timer_sleep(500);

    /* Restore text console */
    kclear();
    //shell_execute("sh");
}

/* Unused placeholder kept for symmetry */
void draw_snake_head() {}