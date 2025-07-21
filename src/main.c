#include "wasm4.h"

#define P_WIDTH 8

#define P_Y_MARGIN 4

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
    const uint8_t *pad;
    uint8_t x, y;
    uint8_t speed;
    uint8_t drawflags;
} p[2], template = {
    .x = SCREEN_SIZE/2,
    .speed = 1,
};


void reset_players() {
    p[0] = p[1] = template;

    p[0].pad = GAMEPAD1;
    p[0].y   = P_Y_MARGIN;
    
    p[1].pad = GAMEPAD2;
    p[1].y   = SCREEN_SIZE - P_Y_MARGIN;
    p[1].drawflags = BLIT_FLIP_Y;
}

void start() {
    reset_players();
}

void update () {
    *DRAW_COLORS = 2;
    text("Fight!", 80-24, 80-4);


    for (int i=0 ; i<2 ; i++){
        if (*p[i].pad & BUTTON_LEFT) {
            p[i].x -= p[i].speed;
        } 
        if (*p[i].pad & BUTTON_RIGHT) {
            p[i].x += p[i].speed;
        }
    }

    for (int i=0 ; i<2 ; i++){
        blit(smiley, p[i].x - P_WIDTH/2, p[i].y - P_WIDTH/2, 8, 8, BLIT_1BPP|p[i].drawflags);
    }
}
