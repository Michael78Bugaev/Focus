// Консольная змейка для FocusOS
// Управление: W/A/S/D, Q — выход

extern void kputchar(char c, unsigned char color);
extern void kprint(const char* s);
extern int kgetch();
extern void pit_sleep(int ms);

#define WIDTH  20
#define HEIGHT 16
#define MAXLEN (WIDTH*HEIGHT)

struct Point { int x, y; };
struct Point snake[MAXLEN];
int snake_len, dir, food_x, food_y, score, game_over;

void cls() { kprint("\x1B[2J\x1B[H"); }

void draw() {
    cls();
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int is_snake = 0;
            for (int i = 0; i < snake_len; i++)
                if (snake[i].x == x && snake[i].y == y) {
                    kputchar(i==0?'O':'o', 0x0A); is_snake = 1; break;
                }
            if (!is_snake) kputchar((x==food_x&&y==food_y)?'*':' ', (x==food_x&&y==food_y)?0x0C:0x07);
        }
        kputchar('\n', 0x07);
    }
    kprintf("Score: %d\n", score);
    kprint("WASD — управление, Q — выход\n");
}

int is_collision(int x, int y) {
    if (x<0||x>=WIDTH||y<0||y>=HEIGHT) return 1;
    for (int i=0;i<snake_len;i++) if (snake[i].x==x&&snake[i].y==y) return 1;
    return 0;
}

void place_food() {
    int fx, fy, ok;
    do {
        fx = (snake[0].x*31 + snake[0].y*17 + score*13 + 7) % WIDTH;
        fy = (snake[0].y*23 + snake[0].x*11 + score*19 + 3) % HEIGHT;
        ok = 1;
        for (int i=0;i<snake_len;i++) if (snake[i].x==fx&&snake[i].y==fy) ok=0;
        if (ok) { food_x=fx; food_y=fy; return; }
        score++;
    } while (1);
}

void move() {
    int nx=snake[0].x, ny=snake[0].y;
    if (dir==0) ny--;
    if (dir==1) ny++;
    if (dir==2) nx--;
    if (dir==3) nx++;
    if (is_collision(nx,ny)) { game_over=1; return; }
    for (int i=snake_len;i>0;i--) snake[i]=snake[i-1];
    snake[0].x=nx; snake[0].y=ny;
    if (nx==food_x&&ny==food_y) { snake_len++; score++; if (snake_len<MAXLEN) place_food(); }
}

int main() {
    snake_len=3; dir=3; score=0; game_over=0;
    snake[0].x=WIDTH/2; snake[0].y=HEIGHT/2;
    snake[1].x=WIDTH/2-1; snake[1].y=HEIGHT/2;
    snake[2].x=WIDTH/2-2; snake[2].y=HEIGHT/2;
    place_food();
    draw();
    while (!game_over) {
        int key=0;
        for (int t=0;t<10;t++) { if ((key=kgetch())) break; pit_sleep(10); }
        if (key=='w'||key=='W') { if (dir!=1) dir=0; }
        if (key=='s'||key=='S') { if (dir!=0) dir=1; }
        if (key=='a'||key=='A') { if (dir!=3) dir=2; }
        if (key=='d'||key=='D') { if (dir!=2) dir=3; }
        if (key=='q'||key=='Q') { game_over=1; break; }
        move();
        draw();
        pit_sleep(80);
    }
    kprint("Game over!\n");
    return 0;
}
