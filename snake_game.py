"""Classic Snake game built with pygame.

Run with: python snake_game.py

Controls:
    Arrow keys / WASD - change direction
    P                  - pause / resume
    R                  - restart after game over
    Esc / close window - quit
"""

import random
import sys

import pygame

CELL_SIZE = 20
GRID_WIDTH = 30
GRID_HEIGHT = 24
WIDTH = CELL_SIZE * GRID_WIDTH
HEIGHT = CELL_SIZE * GRID_HEIGHT
STARTING_LENGTH = 3
BASE_SPEED = 8
SPEED_INCREMENT_EVERY = 5  # foods eaten before speed goes up
MAX_SPEED = 20

BLACK = (15, 15, 15)
WHITE = (240, 240, 240)
GREEN = (60, 200, 90)
DARK_GREEN = (40, 150, 65)
RED = (220, 70, 70)
GRAY = (60, 60, 60)

UP = (0, -1)
DOWN = (0, 1)
LEFT = (-1, 0)
RIGHT = (1, 0)

OPPOSITE = {UP: DOWN, DOWN: UP, LEFT: RIGHT, RIGHT: LEFT}


class Snake:
    def __init__(self):
        self.reset()

    def reset(self):
        cx, cy = GRID_WIDTH // 2, GRID_HEIGHT // 2
        self.body = [(cx - i, cy) for i in range(STARTING_LENGTH)]
        self.direction = RIGHT
        self.pending_direction = RIGHT
        self.grow_pending = 0

    def set_direction(self, new_direction):
        if new_direction == OPPOSITE.get(self.direction):
            return
        self.pending_direction = new_direction

    def head(self):
        return self.body[0]

    def move(self):
        self.direction = self.pending_direction
        hx, hy = self.head()
        dx, dy = self.direction
        new_head = (hx + dx, hy + dy)
        self.body.insert(0, new_head)
        if self.grow_pending > 0:
            self.grow_pending -= 1
        else:
            self.body.pop()

    def grow(self, amount=1):
        self.grow_pending += amount

    def hits_wall(self):
        hx, hy = self.head()
        return not (0 <= hx < GRID_WIDTH and 0 <= hy < GRID_HEIGHT)

    def hits_self(self):
        return self.head() in self.body[1:]


def random_empty_cell(occupied):
    while True:
        cell = (random.randrange(GRID_WIDTH), random.randrange(GRID_HEIGHT))
        if cell not in occupied:
            return cell


def draw_cell(surface, cell, color):
    x, y = cell
    rect = pygame.Rect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE)
    pygame.draw.rect(surface, color, rect)
    pygame.draw.rect(surface, BLACK, rect, 1)


def draw_grid(surface):
    for x in range(0, WIDTH, CELL_SIZE):
        pygame.draw.line(surface, GRAY, (x, 0), (x, HEIGHT))
    for y in range(0, HEIGHT, CELL_SIZE):
        pygame.draw.line(surface, GRAY, (0, y), (WIDTH, y))


def draw_text_center(surface, text, font, color, y_offset=0):
    rendered = font.render(text, True, color)
    rect = rendered.get_rect(center=(WIDTH // 2, HEIGHT // 2 + y_offset))
    surface.blit(rendered, rect)


def current_speed(score):
    speed = BASE_SPEED + score // SPEED_INCREMENT_EVERY
    return min(speed, MAX_SPEED)


def main():
    pygame.init()
    pygame.display.set_caption("Snake")
    screen = pygame.display.set_mode((WIDTH, HEIGHT))
    clock = pygame.time.Clock()
    font = pygame.font.SysFont("consolas", 24)
    big_font = pygame.font.SysFont("consolas", 40, bold=True)

    key_directions = {
        pygame.K_UP: UP,
        pygame.K_w: UP,
        pygame.K_DOWN: DOWN,
        pygame.K_s: DOWN,
        pygame.K_LEFT: LEFT,
        pygame.K_a: LEFT,
        pygame.K_RIGHT: RIGHT,
        pygame.K_d: RIGHT,
    }

    snake = Snake()
    food = random_empty_cell(set(snake.body))
    score = 0
    high_score = 0
    paused = False
    game_over = False

    while True:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    pygame.quit()
                    sys.exit()
                elif event.key in key_directions and not game_over:
                    snake.set_direction(key_directions[event.key])
                elif event.key == pygame.K_p and not game_over:
                    paused = not paused
                elif event.key == pygame.K_r and game_over:
                    snake.reset()
                    food = random_empty_cell(set(snake.body))
                    score = 0
                    game_over = False
                    paused = False

        if not paused and not game_over:
            snake.move()

            if snake.hits_wall() or snake.hits_self():
                game_over = True
                high_score = max(high_score, score)
            elif snake.head() == food:
                snake.grow()
                score += 1
                high_score = max(high_score, score)
                food = random_empty_cell(set(snake.body))

        screen.fill(BLACK)
        draw_grid(screen)
        draw_cell(screen, food, RED)
        for i, segment in enumerate(snake.body):
            draw_cell(screen, segment, GREEN if i == 0 else DARK_GREEN)

        score_surface = font.render(f"Score: {score}  High: {high_score}", True, WHITE)
        screen.blit(score_surface, (8, 6))

        if paused and not game_over:
            draw_text_center(screen, "PAUSED", big_font, WHITE)
        if game_over:
            draw_text_center(screen, "GAME OVER", big_font, RED, y_offset=-20)
            draw_text_center(screen, "Press R to restart", font, WHITE, y_offset=20)

        pygame.display.flip()
        clock.tick(current_speed(score) if not paused and not game_over else 10)


if __name__ == "__main__":
    main()
