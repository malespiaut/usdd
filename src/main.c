#include "wasm4.h"
#include <stdbool.h>

#define P_SIZE 8

#define EXPLO_SIZE 30

#define P_Y_MARGIN 12

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


void reset_players() {
    p[0] = p[1] = template;

    p[0].id = 0;
    p[0].pad = GAMEPAD1;
    p[0].y   = P_Y_MARGIN;
    p[0].bullet.dir = 1;

    p[1].id = 1;
    p[1].pad = GAMEPAD2;
    p[1].y   = SCREEN_SIZE - P_Y_MARGIN;
    p[1].drawflags = BLIT_FLIP_Y;
    p[1].bullet.dir = -1;
}

void draw_life(struct player p) {
    uint8_t x = 0;
    uint8_t y = (p.id == 0) ? 0 : SCREEN_SIZE - BAR_SIZE; 

    *DRAW_COLORS = p.id + 3;
    
    rect(0, y, p.life, BAR_SIZE);
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

void start() {
    reset_players();
    reset_palette();
}

void update () {
    *DRAW_COLORS = 2;
    text("Fight!", 80-24, 80-4);


    for (int i=0 ; i<2 ; i++){
        if (*p[i].pad & BUTTON_LEFT) {
            p[i].x -= p[i].speed;
            if (p[i].x - P_SIZE/2 < 0)
                p[i].x = P_SIZE/2;
        } 
        if (*p[i].pad & BUTTON_RIGHT) {
            p[i].x += p[i].speed;
            if (p[i].x + P_SIZE/2 > SCREEN_SIZE)
                p[i].x = SCREEN_SIZE - P_SIZE/2;
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
                        p[1-i].life -= p[i].bullet.damage;
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

    for (int i=0 ; i<2 ; i++){
        draw_life(p[i]);
        blit(smiley, p[i].x - P_SIZE/2, p[i].y - P_SIZE/2, 8, 8, BLIT_1BPP|p[i].drawflags);
    }
}
