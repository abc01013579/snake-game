/*
 * snake_game.c -- terminal port of snake_game.py, same rules:
 *
 *     30x24 grid, snake starts at length 3, speed increases by 1 every
 *     5 points eaten (capped), game over on hitting a wall or your own
 *     body. Arrow keys or WASD to steer, 'p' to pause, 'r' to restart
 *     after game over, 'q' to quit.
 *
 * No graphics library -- this uses raw terminal mode (POSIX termios)
 * plus plain ANSI escape codes to draw, the same technique the small,
 * well-known "kilo" text editor (antirez/kilo, ~1000 lines of C) uses
 * to read keys and redraw a screen without ncurses. Worth a read later
 * if this style of terminal control is interesting on its own.
 *
 * compile with: gcc -Wall -Wextra snake_game.c -o snake_game
 * run with:     ./snake_game
 * needs a terminal at least 30 columns x 26 rows -- maximize the window
 * if the bottom of the board looks cut off.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <sys/select.h>

#define GRID_WIDTH         30
#define GRID_HEIGHT           24
#define STARTING_LENGTH        3
#define BASE_SPEED              8   /* moves/sec, matches the .py version */
#define SPEED_INCREMENT_EVERY   5   /* foods eaten before speed goes up */
#define MAX_SPEED              20
#define MAXLEN (GRID_WIDTH * GRID_HEIGHT)

typedef struct { int x, y; } Point;

static Point body[MAXLEN];
static int head_idx, length;
static int dir_x, dir_y, pending_dir_x, pending_dir_y;
static int occupied[GRID_HEIGHT][GRID_WIDTH];
static Point food;
static int score, high_score;
static int game_over, paused, quit_requested;

static struct termios orig_termios;

/* ---- terminal raw mode: read keys the instant they're pressed, with
 * no line-buffering and no local echo, and always restore the user's
 * normal terminal settings on exit (even on Ctrl+C-free abnormal paths,
 * via atexit) ---- */

static void disable_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf("\033[?25h");   /* show cursor again */
    fflush(stdout);
}

static void enable_raw_mode(void)
{
    struct termios raw;

    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf("\033[?25l");   /* hide cursor while playing */
}

/* wait up to timeout_ms for a key, without blocking past it */
static int key_ready(long timeout_ms)
{
    fd_set fds;
    struct timeval tv;

    if (timeout_ms < 0)
        timeout_ms = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

/* ---- game state ---- */

static int current_speed(int s)
{
    int speed = BASE_SPEED + s / SPEED_INCREMENT_EVERY;
    return speed > MAX_SPEED ? MAX_SPEED : speed;
}

static void place_food(void)
{
    int x, y;

    do {
        x = rand() % GRID_WIDTH;
        y = rand() % GRID_HEIGHT;
    } while (occupied[y][x]);
    food.x = x;
    food.y = y;
}

static void reset_game(void)
{
    int cx = GRID_WIDTH / 2, cy = GRID_HEIGHT / 2, i;

    memset(occupied, 0, sizeof(occupied));
    length = STARTING_LENGTH;
    head_idx = STARTING_LENGTH - 1;
    for (i = 0; i < STARTING_LENGTH; i++) {
        body[i].x = cx - (STARTING_LENGTH - 1 - i);
        body[i].y = cy;
        occupied[cy][body[i].x] = 1;
    }
    dir_x = pending_dir_x = 1;
    dir_y = pending_dir_y = 0;
    score = 0;
    game_over = 0;
    paused = 0;
    place_food();
}

static void set_direction(int dx, int dy)
{
    if (dx == -dir_x && dy == -dir_y)   /* no reversing straight into self */
        return;
    pending_dir_x = dx;
    pending_dir_y = dy;
}

static void handle_input(void)
{
    char c, seq0, seq1;

    while (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == 27) {   /* start of an arrow-key escape sequence */
            if (read(STDIN_FILENO, &seq0, 1) != 1)
                continue;
            if (read(STDIN_FILENO, &seq1, 1) != 1)
                continue;
            if (seq0 == '[' && !game_over) {
                switch (seq1) {
                case 'A': set_direction(0, -1); break;
                case 'B': set_direction(0, 1);  break;
                case 'C': set_direction(1, 0);  break;
                case 'D': set_direction(-1, 0); break;
                }
            }
            continue;
        }
        switch (c) {
        case 'w': case 'W': if (!game_over) set_direction(0, -1); break;
        case 's': case 'S': if (!game_over) set_direction(0, 1);  break;
        case 'a': case 'A': if (!game_over) set_direction(-1, 0); break;
        case 'd': case 'D': if (!game_over) set_direction(1, 0);  break;
        case 'p': case 'P': if (!game_over) paused = !paused;     break;
        case 'r': case 'R': if (game_over) reset_game();          break;
        case 'q': case 'Q': quit_requested = 1;                   break;
        }
    }
}

static void step_snake(void)
{
    Point newhead, tail;
    int eating;

    dir_x = pending_dir_x;
    dir_y = pending_dir_y;
    newhead.x = body[head_idx].x + dir_x;
    newhead.y = body[head_idx].y + dir_y;

    if (newhead.x < 0 || newhead.x >= GRID_WIDTH ||
        newhead.y < 0 || newhead.y >= GRID_HEIGHT) {
        game_over = 1;
        if (score > high_score) high_score = score;
        return;
    }

    eating = (newhead.x == food.x && newhead.y == food.y);
    tail = body[(head_idx - length + 1 + MAXLEN) % MAXLEN];

    /* moving onto your own about-to-be-vacated tail cell is fine (same
       rule the .py version gets for free from list insert/pop order) */
    if (occupied[newhead.y][newhead.x] &&
        !(!eating && newhead.x == tail.x && newhead.y == tail.y)) {
        game_over = 1;
        if (score > high_score) high_score = score;
        return;
    }

    if (!eating)
        occupied[tail.y][tail.x] = 0;

    head_idx = (head_idx + 1) % MAXLEN;
    body[head_idx] = newhead;
    occupied[newhead.y][newhead.x] = 1;

    if (eating) {
        length++;
        score++;
        if (score > high_score) high_score = score;
        place_food();
    }
}

/* Prints each line straight to stdout rather than assembling one big
 * fixed-size buffer first -- an earlier version did the latter, sized
 * by hand, and got the size wrong by 26 bytes (a real stack-smashing
 * crash, caught by the compiler's stack protector during testing).
 * Printing line-by-line has no buffer to size in the first place. */
static void render(void)
{
    char row[GRID_WIDTH + 1];
    int x, y;

    fputs("\033[H", stdout);
    printf("Score: %-4d  High: %-4d\033[K\n", score, high_score);
    for (y = 0; y < GRID_HEIGHT; y++) {
        for (x = 0; x < GRID_WIDTH; x++) {
            if (x == food.x && y == food.y)
                row[x] = '*';
            else if (occupied[y][x])
                row[x] = (x == body[head_idx].x && y == body[head_idx].y) ? 'O' : 'o';
            else
                row[x] = '.';
        }
        row[GRID_WIDTH] = '\0';
        fputs(row, stdout);
        fputs("\033[K\n", stdout);
    }
    if (paused)
        fputs("-- PAUSED -- press p to resume, q to quit\033[K\n", stdout);
    else if (game_over)
        fputs("-- GAME OVER -- press r to restart, q to quit\033[K\n", stdout);
    else
        fputs("\033[K\n", stdout);
    fflush(stdout);
}

static long ms_between(struct timespec *a, struct timespec *b)
{
    return (b->tv_sec - a->tv_sec) * 1000 + (b->tv_nsec - a->tv_nsec) / 1000000;
}

int main(void)
{
    struct timespec next_tick, now;
    long remaining;

    srand((unsigned) time(NULL));
    enable_raw_mode();
    printf("\033[2J");   /* clear once at startup */
    reset_game();
    render();

    clock_gettime(CLOCK_MONOTONIC, &next_tick);
    next_tick.tv_nsec += (1000L / current_speed(score)) * 1000000L;
    while (next_tick.tv_nsec >= 1000000000L) {
        next_tick.tv_nsec -= 1000000000L;
        next_tick.tv_sec++;
    }

    while (!quit_requested) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        remaining = ms_between(&now, &next_tick);
        if (key_ready(remaining)) {
            handle_input();
            if (quit_requested)
                break;
            if (game_over || paused)
                render();   /* reflect restart/pause immediately */
            continue;
        }

        if (!paused && !game_over)
            step_snake();
        render();

        clock_gettime(CLOCK_MONOTONIC, &next_tick);
        next_tick.tv_nsec += (1000L / current_speed(score)) * 1000000L;
        while (next_tick.tv_nsec >= 1000000000L) {
            next_tick.tv_nsec -= 1000000000L;
            next_tick.tv_sec++;
        }
    }

    printf("\033[2J\033[H");
    printf("Final score: %d  (high: %d)\n", score, high_score);
    return 0;
}
