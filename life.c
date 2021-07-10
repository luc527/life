#include <assert.h>
#include <ncurses.h>
#include <curses.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#define DEFAULT_WORLD_WIDTH  150
#define DEFAULT_WORLD_HEIGHT 40

typedef struct {
    int x;
    int y;
} coords;

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

void interactive_mode();
void quiet_mode();

//
// Variables regarding both modes
//

int world_height;
int world_width;

int** world;
int** aux;  // Auxiliary array for iterating

unsigned long steps;

//
// Variales regarding quiet mode
//

unsigned long max_steps;

//
// Variables regarding interactive mode
//

int screen_height;
int screen_width;

int scroll_offset;
int status_line;

coords screen_in_world;
coords user_in_world;
coords user_in_screen;

bool paused;
bool iterate_one_step;

void print_world() {
    for (int y = 0; y < world_height; y++) {
        for (int x = 0; x < world_width; x++) {
            putchar(world[y][x] ? '#' : ' ');
        }
        putchar('\n');
    }
}


// ==============================

int main(int argc, char **argv)
{
    bool run_interactive_mode = true;

    // TODO display usage when -h is an option
    // FIXME the user_in_screen scrolling stuff is really weird when the world is smaller than the screen.
    //       possible fix (really just guessing): screen_height = MAX(world_height-1, LINES-1);
    //                                            screen_width  = MAX(world_width,  COLS);
    //       another possible fix is to disable the scroll (also just guessing)

    world_height = DEFAULT_WORLD_HEIGHT;
    world_width  = DEFAULT_WORLD_WIDTH;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'w': world_width  = atoi(&argv[i][2]); break;
            case 'h': world_height = atoi(&argv[i][2]); break;
            case 'I':
                run_interactive_mode = false;
                max_steps = atoi(&argv[i][2]);
                break;
            }
        }
        if (world_width  < 5 || world_width  > 1000) world_width  = DEFAULT_WORLD_WIDTH;
        if (world_height < 5 || world_height > 1000) world_height = DEFAULT_WORLD_HEIGHT;
    }

    world = calloc_or_fail(world_height, sizeof(*world));
    for (int i = 0; i < world_height; i++)
        world[i] = calloc_or_fail(world_width, sizeof(*world[i]));

    aux = calloc_or_fail(world_height, sizeof(*aux));
    for (int i = 0; i < world_height; i++)
        aux[i] = calloc_or_fail(world_width, sizeof(*aux[i]));

    steps = 0;

    if (run_interactive_mode) interactive_mode();
    else quiet_mode();

    return 0;
}

void iterate()
{   // {{{
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
}   // }}}

void interactive_mode()
{   // {{{
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    halfdelay(1);

    // Last line reserved for status bar
    screen_height = LINES - 1;  
    screen_width  = COLS;

    scroll_offset = 3;
    status_line = LINES - 1;

    screen_in_world.y = 0;
    screen_in_world.x = 0;

    user_in_world.y = 0;
    user_in_world.x = 0;

    user_in_screen.y = 0;
    user_in_screen.x = 0;

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
                    if (screen_in_world.x >= world_width) {
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
            // TODO add more commands: c (clear), r (random), ...
            // also some for moving faster (like three squares at a time or something)
            case 'q':
                close(EXIT_SUCCESS);
        }

        erase();
        move(status_line, 0);
        clrtoeol();
        move(status_line, 0);
        printw("uiw(%d, %d) uis[%d, %d] siw{%d, %d} %s %lu",
               user_in_world.x, user_in_world.y,
               user_in_screen.x, user_in_screen.y, 
               screen_in_world.x, screen_in_world.y,
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
            iterate();
            iterate_one_step = false;
        }
    }
}   //}}}

void quiet_mode()
{   // {{{
    // Read seed from stdin
    int c, y=0, x=0;
    while ((c = getchar()) != EOF) {
        if (c == '\n') {
            y++;
            if (y >= world_height) break;
            x = 0;
        } else {
            world[y][x] = c != ' ';
            x++;
            if (x >= world_width) {
                y++;
                if (y >= world_height) break;
                x = 0;
            }
        }
    }

    while (steps < max_steps)
        iterate();
    // No need to do steps++, iterate() already does that for us

    // Write final state to stdout
    print_world();
}   // }}}

