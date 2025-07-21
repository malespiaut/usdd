#include <stdbool.h>

#include "wasm4.h"

#include "sprites.h"

#define P_SIZE 30

#define P_COLL_WIDTH 28
#define P_COLL_HEIGHT 15

#define P_Y_MARGIN 20

#define CANON_Y_OFFSET 11

#define CANON_X_OFFSET 10

#define EXPLO_SIZE 30

#define MAX_BLASTS 32
#define BLAST_TIME_SCALE 3

#define BAR_SIZE 5


#define ASTERO_TIMER_MIN 30
#define ASTERO_TIMER_MASK 0x7f

#define ASTERO_SPEED_BASE 0.3f
#define ASTERO_SIZE_BOOST 0.3f
#define ASTERO_SPEED_VAR  0.2f
#define ASTERO_RESOLUTION 256

#define ASTERO_OUT_MARGIN 10
#define ASTERO_LIFE 3

#define ASTERO_DAMAGE_SCALE 3

#define MAX_ASTEROIDS 64

/* rand() implem from C std */

static unsigned long int nextrand = 1;

int rand(void) // RAND_MAX assumed to be 32767
{
    nextrand = nextrand * 1103515245 + 12345;
    return (unsigned int)(nextrand/65536) % 32768;
}

void srand(unsigned int seed)
{
    nextrand = seed;
}



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
    .bullet.rate  = 60,
    .bullet.damage = 3,
};

struct asteroid {
    bool active;
    uint8_t side;
    uint32_t rand_offset;
    uint8_t flip_flags;
    uint8_t life;
    float x, y, dx, dy;
    enum astsize {MINI, SMALL, FULLSIZE} size;
} asteroids[MAX_ASTEROIDS];

uint8_t astero_size[] = {
    [MINI] = 5,
    [SMALL] = 10,
    [FULLSIZE] = 20    
};

struct blast {
    uint8_t frame; // 1 frame offset (0=inactive)
    int author;
    uint8_t x, y;
} blasts[MAX_BLASTS];

enum gamestate {START, PLAY, END} state = START;

uint8_t winner = 0;
uint8_t t = 0;

int global_delay = 0;

int asteroid_spawn_delay = 0;

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

void reset_asteroids(){
    for (int i = 0 ; i<MAX_ASTEROIDS ; i++) {
        asteroids[i] = (struct asteroid){0};
    }
}
void reset_blasts(){
    for (int i = 0 ; i<MAX_BLASTS ; i++) {
        blasts[i] = (struct blast){0};
    }
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
    reset_asteroids();
    reset_blasts();
    state = START;
}

float rand_ast_var() {
    return ((float)(rand() % ASTERO_RESOLUTION - ASTERO_RESOLUTION/2)*ASTERO_SPEED_VAR*2)/ASTERO_RESOLUTION;
}

void spawn_asteroid(uint8_t x, uint8_t y, int8_t xdir, int8_t ydir, enum astsize size){
    for(int i = 0; i<MAX_ASTEROIDS ; i++) {
        if (!asteroids[i].active) {
            asteroids[i].active = true;
            asteroids[i].size = size;
            asteroids[i].x = x;
            asteroids[i].y = y;

            asteroids[i].life = ASTERO_LIFE * (1 + (uint8_t)size);
            
            float xvar = rand_ast_var();
            float yvar = rand_ast_var();

            asteroids[i].dx = xvar + xdir * (ASTERO_SPEED_BASE + (float)(2-size) * ASTERO_SIZE_BOOST);
            asteroids[i].dy = yvar + ydir * (ASTERO_SPEED_BASE + (float)(2-size) * ASTERO_SIZE_BOOST);

            asteroids[i].rand_offset = (uint32_t)rand();

            asteroids[i].flip_flags = (uint8_t)(rand()& (BLIT_FLIP_X|BLIT_FLIP_Y));
            
            return;
        }
    }
}

void spawn_blast(uint8_t x, uint8_t y, int author) {
    for(int i = 0; i<MAX_BLASTS; i++) {
        if (blasts[i].frame == 0){
            blasts[i].x = x;
            blasts[i].y = y;
            blasts[i].author = author;
            blasts[i].frame = (uint8_t)(blastWidth/blastHeight + 1)*BLAST_TIME_SCALE;
            return;
        }
    }
}

void show_asteroid(struct asteroid a) {
    uint8_t size = astero_size[a.size];
    const uint8_t* data = asteroid;
    uint32_t stride = asteroidWidth;

    if (a.size == SMALL) {
        data = small_asteroid;
        stride = small_asteroidWidth; 
    } else if (a.size == MINI) {
        data = mini_asteroid;
        stride = mini_asteroidWidth;
    }

    uint32_t frame = ((t>>4)+a.rand_offset) % (stride / size);

    *DRAW_COLORS = 0x21;

    blitSub(data,
            (int32_t) a.x - size/2,
            (int32_t) a.y - size/2,
            size, size,
            size * frame, 0,
            stride,
            BLIT_2BPP|a.flip_flags);
}

bool astero_collision(float x1, float y1, float r1, float x2, float y2, float r2) {
    float dx = x1-x2;
    float dy = y1-y2;
    float dist = r1+r2;

    return dx*dx + dy*dy < dist*dist;
}

void astero_damage(int ai, int pi, uint8_t damage) {
    if (asteroids[ai].life > damage) {
        asteroids[ai].life -= damage;
    } else {
        asteroids[ai].active = false;

        spawn_blast((uint8_t)asteroids[ai].x, (uint8_t)asteroids[ai].y, pi);
        
        int size = (int)asteroids[ai].size;

        if (size > 0) {
            for(int i=0 ; i<2 ; i++) {
                uint8_t x = (uint8_t)asteroids[ai].x;
                uint8_t y = (uint8_t)asteroids[ai].y;
                spawn_asteroid(x, y, 0, (int8_t)(1-2*pi), (enum astsize) size-1);
            }
        } else {
            //SPAWN BONUS
        }
    }
}


void update_asteroids() {
    if (asteroid_spawn_delay <= 0) {
        asteroid_spawn_delay = ASTERO_TIMER_MIN + rand()&ASTERO_TIMER_MASK;
        int8_t xdir = rand()%2 ? -1:1;
        spawn_asteroid(xdir < 0 ? SCREEN_SIZE : 0, SCREEN_SIZE/2, xdir, 0, (enum astsize)(rand()%2+1));
    } else {
        asteroid_spawn_delay--;
    }

    for(int i = 0; i<MAX_ASTEROIDS ; i++) {
        if (asteroids[i].active) {
            asteroids[i].x += asteroids[i].dx;
            asteroids[i].y += asteroids[i].dy;

            if (asteroids[i].x < -ASTERO_OUT_MARGIN ||
                asteroids[i].x > SCREEN_SIZE + ASTERO_OUT_MARGIN ||
                asteroids[i].y < -ASTERO_OUT_MARGIN ||
                asteroids[i].y > SCREEN_SIZE + ASTERO_OUT_MARGIN) {
                    asteroids[i].active = false;
            }

            show_asteroid(asteroids[i]);
        }
    }
}

void update_blasts() {
    for(int i = 0; i<MAX_BLASTS; i++) {
        if (blasts[i].frame != 0){
            blasts[i].frame--;
            set_sprite_colors(p[blasts[i].author]);
            blitSub(blast,
                    blasts[i].x - blastHeight/2, blasts[i].y - blastHeight/2,
                    blastHeight, blastHeight,
                    blastHeight*(blasts[i].frame/BLAST_TIME_SCALE), 0,
                    blastWidth, blastFlags);
        }
    }
}

bool check_bullet_collision(int pi, int bi) {
    uint8_t *bx = &p[pi].bullet.x[bi], *by = &p[pi].bullet.y[bi];
    if (collision(p[1-pi].x, p[1-pi].y, P_COLL_WIDTH, P_COLL_HEIGHT,
                  *bx, *by, bulletWidth, bulletHeight)) {
        damage(1-pi, p[pi].bullet.damage);
        return true;
    }

    for(int i = 0; i<MAX_ASTEROIDS ; i++) {
        if (asteroids[i].active) {
            if (astero_collision(
                asteroids[i].x, asteroids[i].y,
                astero_size[asteroids[i].size]/2.0f,
                *bx, *by,
                bulletWidth/2.0f
            )) {
                astero_damage(i, pi, p[pi].bullet.damage);
                return true;
            }
        }
    }
    
    
    return false; 
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

        for(int ai = 0; ai<MAX_ASTEROIDS ; ai++) {
            if (asteroids[ai].active) {
                if (astero_collision(
                    asteroids[ai].x, asteroids[ai].y,
                    astero_size[asteroids[ai].size]/2.0f,
                    p[i].x,
                    p[i].y,
                    P_COLL_WIDTH/2
                )) {
                    astero_damage(ai, i, asteroids[ai].life);
                    damage(i, ASTERO_DAMAGE_SCALE * ((uint8_t)asteroids[ai].size+1));
                }
            }
        }
        
        for (int j = 0 ; j < SCREEN_SIZE ; j++) {
            uint8_t *bx = &p[i].bullet.x[j], *by = &p[i].bullet.y[j];
            if (*by) {
                if (*by < 0 || *by > SCREEN_SIZE) {
                    *by = 0;
                } else {
                    if (check_bullet_collision(i,j)) {
        		*DRAW_COLORS = player2color(p[i]);

        		spawn_blast(*bx, *by, i);
                        //oval(*bx-EXPLO_SIZE/2, *by-EXPLO_SIZE/2, EXPLO_SIZE, EXPLO_SIZE);
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
    update_asteroids();
    update_blasts();
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
    rand();

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
