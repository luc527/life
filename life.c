#include <assert.h>
#include <ncurses.h>
#include <curses.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    int x;
    int y;
} coords;

#define MAX(a,b) ((a)>(b)?(a):(b))

// ==============================

void close(int code)
{
    endwin();
    exit(code);
}

void* calloc_or_fail(size_t nmemb, size_t size)
{
    void* p = calloc(nmemb, size);
    if (p == NULL) {
        printf("calloc failed.\n");
        close(EXIT_FAILURE);
    }
    return p;
}

// ==============================

void zoom_out_mode();
void print_status_line();
void iterate();

// ==============================

int world_height;
int world_width;

int** world;
int** aux;

int screen_height;
int screen_width;

int status_line;
int scroll_offset;

coords screen_in_world;
coords user_in_world;
coords user_in_screen;

unsigned long steps;
bool paused;
bool iterate_one_step;

// ==============================

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    halfdelay(1);

    world_height = MAX(36, LINES);
    world_width  = MAX(200, COLS);

    screen_height = LINES - 1;  // Last line reserved for status bar
    screen_width  = COLS;

    scroll_offset = 3;
    status_line = LINES - 1;

    screen_in_world.y = 0;
    screen_in_world.x = 0;

    user_in_world.y = 0;
    user_in_world.x = 0;

    user_in_screen.y = 0; // Used with scroll_offset to determine when to scroll
    user_in_screen.x = 0;


    world = calloc_or_fail(world_height, sizeof(*world));
    for (int i = 0; i < world_height; i++)
        world[i] = calloc_or_fail(world_width, sizeof(*world[i]));

    // Auxiliary array for iterating
    aux = calloc_or_fail(world_height, sizeof(*aux));
    for (int i = 0; i < world_height; i++)
        aux[i] = calloc_or_fail(world_width, sizeof(*aux[i]));

    steps = 0;
    paused = true;
    iterate_one_step = false;

    while (true)
    {
        chtype ch = getch();
        switch (ch) {
            case 'w':
                user_in_world.y--;
                user_in_screen.y--;
                if (user_in_world.y < 0) {  // Wrap around the world
                    user_in_world.y = world_height - 1;
                } 
                if (user_in_screen.y < scroll_offset) {
                    // Move the screen down with the user, so the user stays in the same screen position
                    screen_in_world.y--;
                    if (screen_in_world.y < 0) {  // The screen can also wrap
                        screen_in_world.y = world_height - 1;
                    }
                    user_in_screen.y++;  // Stay in the same position by cancelling the previous decrement
                }
                // All the following cases follow the same pattern
                break;
            case 'a':
                user_in_world.x--;
                user_in_screen.x--;
                if (user_in_world.x < 0) {
                    user_in_world.x = world_width  - 1;
                } 
                if (user_in_screen.x < scroll_offset) {
                    screen_in_world.x--;
                    if (screen_in_world.x < 0) {
                        screen_in_world.x = world_width - 1;
                    }
                    user_in_screen.x++;
                }
                break;
            case 's':
                user_in_world.y++;
                user_in_screen.y++;
                if (user_in_world.y >= world_height) {
                    user_in_world.y = 0;
                } 
                if (screen_height - user_in_screen.y <= scroll_offset) {
                    screen_in_world.y++;
                    if (screen_in_world.y >= world_height) {
                        screen_in_world.y = 0;
                    }
                    user_in_screen.y--;
                }
                break;
            case 'd':
                user_in_world.x++;
                user_in_screen.x++;
                if (user_in_world.x >= world_width) {
                    user_in_world.x = 0;
                } 
                if (screen_width - user_in_screen.x <= scroll_offset) {
                    screen_in_world.x++;
                    if (screen_in_world.x >= world_height) {
                        screen_in_world.x = 0;
                    }
                    user_in_screen.x--;
                }
                break;
            case ' ':
                if (paused)
                    world[user_in_world.y][user_in_world.x] = !world[user_in_world.y][user_in_world.x];
            break;
            case 'p':
                paused = !paused;
                break;
            case 'i':
                iterate_one_step = true;
                break;
            case 'z':
                zoom_out_mode();
                break;
            case 'q':
                close(EXIT_SUCCESS);
        }

        //
        // Draw
        // 
        erase();
        print_status_line();
        move(0, 0);
        for (int i = 0; i < screen_height; i++) {
            int y = (screen_in_world.y + i) % world_height;
            for (int j = 0; j < screen_width; j++) {
                int x = (screen_in_world.x + j) % world_width;
                assert(y >= 0 && y < world_height);
                assert(x >= 0 && x < world_width);
                chtype cell = ' ';
                if (user_in_world.y == y && user_in_world.x == x)
                    attron(A_REVERSE);
                if (world[y][x])
                    cell = '#';
                addch(cell);
                attroff(A_REVERSE);
            }
        }

        iterate();
    }
}

void print_status_line()
{
        move(status_line, 0);
        clrtoeol();
        move(status_line, 0);
        printw("(%d, %d) [%d, %d] %s %lu", user_in_world.x,   user_in_world.y,
                                           user_in_screen.x, user_in_screen.y, 
                                           paused ? "PAUSED" : "",
                                           steps);
}

void iterate()
{
    if (!paused || iterate_one_step) {
        steps++;
        for (int y = 0; y < world_height; y++) {
            for (int x = 0; x < world_width; x++) {

                int neighbors = 0;
                for (int i = -1; i <= 1; i++) {
                    for (int j = -1; j <= 1; j++) {
                        if (i == 0 && j == 0)
                            continue;

                        int ii = (y + i) % world_height;
                        if (ii < 0) ii += world_height;

                        int jj = (x + j) % world_width;
                        if (jj < 0) jj += world_width;

                        assert(ii >= 0 && ii < world_height);
                        assert(jj >= 0 && jj < world_width);

                        if (world[ii][jj])
                            neighbors++;
                    }
                }

                aux[y][x] = world[y][x];
                if (world[y][x]) {
                    if (neighbors < 2 || neighbors > 3)
                        aux[y][x] = 0;
                } else {
                    if (neighbors == 3)
                        aux[y][x] = 1;
                }
            }
        }
        for (int y = 0; y < world_height; y++) {
            for (int x = 0; x < world_width; x++) {
                world[y][x] = aux[y][x];
            }
        }
        iterate_one_step = false;
    }
}

// ==============================

inline
int safe_world_get(int y, int x)
{
    return (y >= 0 && y < world_height && x >= 0 && x < world_width) ? world[y][x] : 0;
}

wchar_t braille[64] = {L'⠀',L'⠁',L'⠂',L'⠃',L'⠄',L'⠅',L'⠆',L'⠇',L'⠈',L'⠉',L'⠊',L'⠋',L'⠌',L'⠍',L'⠎',L'⠏',
                       L'⠐',L'⠑',L'⠒',L'⠓',L'⠔',L'⠕',L'⠖',L'⠗',L'⠘',L'⠙',L'⠚',L'⠛',L'⠜',L'⠝',L'⠞',L'⠟',
                       L'⠠',L'⠡',L'⠢',L'⠣',L'⠤',L'⠥',L'⠦',L'⠧',L'⠨',L'⠩',L'⠪',L'⠫',L'⠬',L'⠭',L'⠮',L'⠯',
                       L'⠰',L'⠱',L'⠲',L'⠳',L'⠴',L'⠵',L'⠶',L'⠷',L'⠸',L'⠹',L'⠺',L'⠻',L'⠼',L'⠽',L'⠾',L'⠿'};

wchar_t to_braille(int y, int x, int** world)
{
    // y and x are the coordinates of the upper-left corner of the 2x3
    // sub-matrix of the world we have to make a braille character from.  So we
    // need to add 2 to y and 1 to x to get the other elements. But these may
    // be out-of-bounds of the world, as pictured:
    //----------+
    //          |
    //          *#   (* denotes the [y][x] position)
    //          ##
    //          ##
    //          |
    //----------+
    // What we want is to treat those as empty/dead cells.
    // That's why we use the safe_world_get function.

    // v (x,y)
    // 03
    // 14 -> 543210
    // 25
    unsigned bits = 0;
    bits |= (safe_world_get(y,   x)   & 1) << 0;
    bits |= (safe_world_get(y+1, x)   & 1) << 1;
    bits |= (safe_world_get(y+2, x)   & 1) << 2;
    bits |= (safe_world_get(y,   x+1) & 1) << 3;
    bits |= (safe_world_get(y+1, x+1) & 1) << 4;
    bits |= (safe_world_get(y+2, x+1) & 1) << 5;
    assert(bits >= 0 && bits < 64);
    return braille[bits];
}

void zoom_out_mode() {
    while (true)
    {
        chtype ch = getch();
        switch (ch) {
            case 'w':
                break;
            case 'a':
                break;
            case 's':
                break;
            case 'd':
                break;
            case 'p':
                paused = !paused;
                break;
            case 'i':
                iterate_one_step = true;
                break;
            case 'z':
                return;
            case 'q':
                close(EXIT_SUCCESS);
        }
        erase();
        print_status_line();
        move(0, 0);
        for (int i = 0; i < 3*screen_height; i+=3) {
            int y = (screen_in_world.y + i) % world_height;
            for (int j = 0; j < 2*screen_width; j+=2) {
                int x = (screen_in_world.x + j) % world_width;
                printw("%lc", to_braille(y, x, world));
            }
        }
        iterate();
    }
    // unreachable
}
