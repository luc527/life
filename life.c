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

int main(int argc, char **argv)
{
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    halfdelay(1);

    int world_height = MAX(36, LINES);
    int world_width  = MAX(200, COLS);

    int screen_height = LINES - 1;  // Last line reserved for status bar
    int screen_width  = COLS;

    int scroll_offset = 3;
    int status_line = LINES - 1;

    coords screen_in_world = {0, 0};
    coords user_in_world   = {0, 0};
    coords user_in_screen  = {0, 0}; // Used with scroll_offset to determine when to scroll

    int** world = calloc_or_fail(world_height, sizeof(*world));
    for (int i = 0; i < world_height; i++)
        world[i] = calloc_or_fail(world_width, sizeof(*world[i]));

    // Auxiliary array for iterating
    int** aux = calloc_or_fail(world_height, sizeof(*aux));
    for (int i = 0; i < world_height; i++)
        aux[i] = calloc_or_fail(world_width, sizeof(*aux[i]));

    unsigned long steps = 0;
    bool paused = true;
    bool iterate_one_step = false;

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
            case 'q':
                close(EXIT_SUCCESS);
        }

        //
        // Draw
        // 
        erase();
        move(status_line, 0);
        clrtoeol();
        move(status_line, 0);
        printw("(%d, %d) [%d, %d] %s %lu", user_in_world.x,   user_in_world.y,
                                           user_in_screen.x, user_in_screen.y, 
                                           paused ? "PAUSED" : "",
                                           steps);
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
}

