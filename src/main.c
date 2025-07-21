#include <stdbool.h>

#include "wasm4.h"

#include "sprites.h"

#define P_SIZE 30

#define P_COLL_WIDTH 28
#define P_COLL_HEIGHT 15

#define P_Y_MARGIN 30

#define CANON_Y_OFFSET 11

#define CANON_X_OFFSET 10

#define EXPLO_SIZE 30
#define BAR_SIZE 5

struct player {
    uint8_t id;
    const uint8_t *pad;
    uint8_t x, y;
    int8_t xdir;
    uint8_t speed;
    uint8_t life;
    uint8_t drawflags;
    struct {
        uint8_t speed;
        uint8_t rate;    // timeout reset value
        uint8_t timeout;
        int8_t  dir;
        uint8_t damage;
        uint8_t y[SCREEN_SIZE];
        uint8_t x[SCREEN_SIZE];
        bool fire;
    } bullet;
} p[2], template = {
    .x = SCREEN_SIZE/2,
    .life = 64,
    .speed = 1,
    .bullet.speed = 1,
    .bullet.rate  = 30,
    .bullet.damage = 3,
};

enum gamestate {START, PLAY, END} state = START;

uint8_t winner = 0;
uint8_t t = 0;

int global_delay = 0;

uint8_t player2color(struct player p) {
    return p.id ? 4 : 3;
}

void reset_players() {
    p[0] = p[1] = template;

    p[0].id = 0;
    p[0].pad = GAMEPAD1;
    p[0].y   = P_Y_MARGIN;
    p[0].bullet.dir = 1;
    p[0].drawflags = BLIT_FLIP_Y;

    p[1].id = 1;
    p[1].pad = GAMEPAD2;
    p[1].y   = SCREEN_SIZE - P_Y_MARGIN;
    p[1].bullet.dir = -1;
}

void reset_palette(){
    PALETTE[0] = 0x0;
    PALETTE[1] = 0xffffff;
    PALETTE[2] = 0x0000ff;
    PALETTE[3] = 0xff0000;
}

void set_sprite_colors(struct player p) {
    *DRAW_COLORS = (uint16_t)(player2color(p) << 12) | 0x20; 
}


bool collision(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
    return (x1 + w1/2 >= x2 - w2/2 && x1 - w1/2 <= x2 + w2/2 &&
            y1 + h1/2 >= y2 - h2/2 && y1 - h1/2 <= y2 + h2/2);
}

void damage(int id, uint8_t damage) {
    if (p[id].life > damage) {
        p[id].life -= damage;
    } else {
        p[id].life = 0;
        winner = (uint8_t) (1-id);
        state = END;
    }
}

int next_free(uint8_t *array, int length, int start) {
    while (start<length && array[start] != 0)
        start++;
    if (start >= length)
        start = -1;

    return start;
}

void start() {
    reset_players();
    reset_palette();
    state = START;
}

void update_players() {

    for (int i=0 ; i<2 ; i++){
        if (*p[i].pad & BUTTON_LEFT) {
            p[i].x -= p[i].speed;
            if (p[i].x - P_COLL_WIDTH/2 < 0)
                p[i].x = P_COLL_WIDTH/2;
            if (p[i].xdir < 2)
                p[i].xdir++;
        } else if (*p[i].pad & BUTTON_RIGHT) {
            p[i].x += p[i].speed;
            if (p[i].x + P_COLL_WIDTH/2 > SCREEN_SIZE)
                p[i].x = SCREEN_SIZE - P_COLL_WIDTH/2;
            if (p[i].xdir > -2)
                p[i].xdir--;
        } else {
            p[i].xdir = 0;
        }
        if (*p[i].pad & BUTTON_1 && !p[i].bullet.timeout) {
            p[i].bullet.timeout = p[i].bullet.rate; 

            int bi = 0;

            for (int j = 0 ; j<2 ; j++) {
                bi = next_free(p[i].bullet.y, sizeof(p[i].bullet.y), bi);
                if (bi >= 0) {
                    p[i].bullet.y[bi] = (uint8_t) (p[i].y + CANON_Y_OFFSET/2 * p[i].bullet.dir);
                    p[i].bullet.x[bi] = (uint8_t) (p[i].x - CANON_X_OFFSET + CANON_X_OFFSET*2*j);
                }
            }
             
        }

        if (p[i].bullet.timeout)
            p[i].bullet.timeout--;

        for (int j = 0 ; j < SCREEN_SIZE ; j++) {
            uint8_t *bx = &p[i].bullet.x[j], *by = &p[i].bullet.y[j];
            if (*by) {
                if (*by < 0 || *by > SCREEN_SIZE) {
                    *by = 0;
                } else {
                    if (collision(p[1-i].x, p[1-i].y, P_COLL_WIDTH, P_COLL_HEIGHT,
                                  *bx, *by, bulletWidth, bulletHeight)) {
                        damage(1-i, p[i].bullet.damage);
        		*DRAW_COLORS = player2color(p[i]);
                        oval(*bx-EXPLO_SIZE/2, *by-EXPLO_SIZE/2, EXPLO_SIZE, EXPLO_SIZE);
                        *by = 0;

                    } else {
                        *by += p[i].bullet.speed * p[i].bullet.dir;
                        set_sprite_colors(p[i]);
                        blit(bullet,
                             *bx-bulletWidth/2, *by - bulletHeight/2,
                             bulletWidth, bulletHeight,
                             bulletFlags|(p[i].bullet.dir>0?0:BLIT_FLIP_Y));
                    }
                }
            }               
        }
    } 
}

void draw_life(struct player p) {
    uint8_t x = 0;
    uint8_t y = (p.id == 0) ? 0 : SCREEN_SIZE - BAR_SIZE; 

    *DRAW_COLORS = player2color(p);
    
    rect(0, y, p.life, BAR_SIZE);
}

void blit_ship(struct player p, const uint8_t* sprite, int sprite_idx, uint32_t stride, uint32_t flags) {
    blitSub(sprite, 
            p.x - P_SIZE/2, p.y - P_SIZE/2,
            P_SIZE, P_SIZE,
            (uint32_t)(P_SIZE*sprite_idx), 0,
            stride,
            flags | p.drawflags);
    
}

void draw_ship(struct player p) {

    uint8_t xflip = 0;

    int sprite_idx = p.xdir;

    if (sprite_idx < 0) {
        xflip = BLIT_FLIP_X;
        sprite_idx = -sprite_idx;
    }
    
    *DRAW_COLORS = 1;

    blit_ship(p, ship_shadow, sprite_idx, ship_shadowWidth, ship_shadowFlags | xflip);

    set_sprite_colors(p);
    blit_ship(p, ship, sprite_idx, shipWidth, shipFlags | xflip);
}

void draw_players() {
    for (int i=0 ; i<2 ; i++){
        draw_life(p[i]);
        draw_ship(p[i]);
    } 
}

void update_start() {
    *DRAW_COLORS = 2 + t/4%3;
    text("Ready?", 80-24, 80-4);
    draw_players();

    if (*p[0].pad & BUTTON_1 && *p[1].pad & BUTTON_1) {
        state = PLAY;
    }
}

void update_play() {
    *DRAW_COLORS = 2 + t/4%3;
    text("Fight!", 80-24, 80-4);
    update_players();
    draw_players();
}

void update_end() {
    *DRAW_COLORS = t/4%2 ? winner+3 : 2;
    text("Player", 80-24, 80-12);
    text(winner == 0 ? "1" : "2", 80-4, 80);
    text("wins!", 80-20, 80+12);
    draw_players();

    if (global_delay == 0 && *p[0].pad & BUTTON_1 && *p[1].pad & BUTTON_1) {
        start();
    }
}

void update () {
    t++;

    switch (state) {
        case START:
            update_start();
            break;
        case PLAY:
            update_play();
            break;
        case END:
            update_end();
            break;
    }
}
