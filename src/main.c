#include <stdbool.h>

#include "wasm4.h"

#include "sprites.h"

#define P_SIZE 30
#define P_Y_MARGIN 30
#define EXPLO_SIZE 30
#define BAR_SIZE 5

const uint8_t smiley[] = {
    0b11000011,
    0b10000001,
    0b00100100,
    0b00100100,
    0b00000000,
    0b00100100,
    0b10011001,
    0b11000011,
};


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

bool collision(struct player p, int x, int y, int w, int h) {
    return (p.x + P_SIZE >= x && p.x <= x + w &&
            p.y + P_SIZE >= y && p.y <= y + h);
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

void start() {
    reset_players();
    reset_palette();
    state = START;
}

void update_players() {

    for (int i=0 ; i<2 ; i++){
        if (*p[i].pad & BUTTON_LEFT) {
            p[i].x -= p[i].speed;
            if (p[i].x - P_SIZE/2 < 0)
                p[i].x = P_SIZE/2;
            if (p[i].xdir < 2)
                p[i].xdir++;
        } else if (*p[i].pad & BUTTON_RIGHT) {
            p[i].x += p[i].speed;
            if (p[i].x + P_SIZE/2 > SCREEN_SIZE)
                p[i].x = SCREEN_SIZE - P_SIZE/2;
            if (p[i].xdir > -2)
                p[i].xdir--;
        } else {
            p[i].xdir = 0;
        }
        if (*p[i].pad & BUTTON_1 && !p[i].bullet.timeout) {
            p[i].bullet.fire = true;
            p[i].bullet.timeout = p[i].bullet.rate; 
        }

        if (p[i].bullet.timeout)
            p[i].bullet.timeout--;

        *DRAW_COLORS = p[i].id + 3;
        for (int j = 0 ; j < SCREEN_SIZE ; j++) {
            uint8_t *bx = &p[i].bullet.x[j], *by = &p[i].bullet.y[j];
            if (*by) {
                if (*by < 0 || *by > SCREEN_SIZE) {
                    *by = 0;
                } else {
                    if (collision(p[1-i], *bx, *by, 1, p[i].bullet.speed)) {
                        damage(1-i, p[i].bullet.damage);
                        oval(*bx-EXPLO_SIZE/2, *by-EXPLO_SIZE/2, EXPLO_SIZE, EXPLO_SIZE);
                        *by = 0;

                    } else {
                        *by += p[i].bullet.speed * p[i].bullet.dir;
                        rect(*bx, *by, 1, p[i].bullet.speed);
                    }
                }
                
            } else if (p[i].bullet.fire) {
                p[i].bullet.fire = false;
                *by = (uint8_t) (p[i].y + P_SIZE/2 * p[i].bullet.dir);
                *bx = p[i].x;
            }
        }
    } 
}

void draw_life(struct player p) {
    uint8_t x = 0;
    uint8_t y = (p.id == 0) ? 0 : SCREEN_SIZE - BAR_SIZE; 

    *DRAW_COLORS = p.id + 3;
    
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
/*
    blitSub(ship_shadow, 
            p.x - P_SIZE/2, p.y - P_SIZE/2,
            P_SIZE, P_SIZE,
            0, 0, ship_shadowWidth,
            BLIT_1BPP | xflip | p.drawflags);
  */   
    *DRAW_COLORS = (p.id ? 0x4000 : 0x3000) | 0x20;
    blit_ship(p, ship, sprite_idx, shipWidth, shipFlags | xflip);
    
    /*blitSub(ship, 
            p.x - P_SIZE/2, p.y - P_SIZE/2,
            P_SIZE, P_SIZE,
            P_SIZE*sprite_idx, 0, shipWidth,
            shipFlags | xflip | p.drawflags);*/
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
