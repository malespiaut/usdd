#include <stdbool.h>
#include <math.h>

#include "wasm4.h"

#include "sprites.h"

#define P_SIZE 30

#define P_COLL_WIDTH 28
#define P_COLL_HEIGHT 15

#define P_Y_MARGIN 22

#define CANON_Y_OFFSET 15

#define CANON_X_OFFSET 10

#define EXPLO_SIZE 30

#define MAX_BLASTS 32
#define BLAST_TIME_SCALE 3

#define BAR_SIZE 8
#define BAR_GAP 2


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

#define MAX_ITEMS 32
#define ITEM_SIZE 8
#define NUM_ITEM_TYPES 5 // 5th item (arc) not implemented

#define HEAL_ADD 7

#define MIN_FIRERATE 30

#define SPEED_INC 0.2f
#define MAX_BULLET_SPEED 3.0f

#define start_button_Frames 6
#define start_button_X (40-3)
#define start_button_Y (100-3)
#define start_button_Interval 8
#define start_button_Time_scale 4

#define START_DELAY 60
#define END_DELAY 120

#define end_nameX 42
#define end_nameY 30


#define MAX_UPGRADE 10
#define MAX_ARC MAX_UPGRADE
#define MAX_FIRERATE MAX_UPGRADE
#define MAX_LASER MAX_UPGRADE
#define MAX_SPEED MAX_UPGRADE

#define SHOOT_LASER_FRAMES 11
#define SHOOT_LASER_ORDER 0
#define SHOOT_ARC_FRAMES 14
#define SHOOT_ARC_ORDER 1
#define SHOOT_BULLETS_FRAMES 6
#define SHOOT_BULLETS_ORDER 2

#define LASER_CHARGE 188
#define LASER_UNLOAD 42
#define LASER_TIMEOUT (60 + LASER_UNLOAD)
#define LASER_CHARGE_FRAMES 11
#define LASER_FRAMES 5
#define LASER_BEAM_WIDTH (laserWidth/LASER_FRAMES)
#define LASER_BEAM_DMG_WIDTH 9
#define LASER_OFFSET (P_Y_MARGIN+6)
#define LASER_DMG 1


const char* digits[] = {
    "0", "1", "2", "3", "4",
    "5", "6", "7", "8", "9",
};

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



struct item {
    bool active;
    uint8_t x, y;
    enum itemtype {HEAL, FIRERATE, LASER, SPEED, ARC} type;
} items[MAX_ITEMS];

const uint8_t * item_sprite[] = {
    [HEAL] = heal,
    [FIRERATE] = upgrade_firerate,
    [LASER] = upgrade_laser,
    [SPEED] = upgrade_speed,
    [ARC] = upgrade_arc, // not implemented
};

struct player {
    uint8_t id;
    const uint8_t *pad;
    uint8_t x, y;
    int8_t xdir;
    uint8_t speed;
    uint8_t life;
    uint8_t drawflags;
    uint8_t upgrades[NUM_ITEM_TYPES];
    struct {
        float speed;
        uint8_t rate;    // timeout reset value
        uint8_t timeout;
        int8_t  dir;
        uint8_t damage;
        float y[SCREEN_SIZE];
        uint8_t x[SCREEN_SIZE];
        bool fire;
    } bullet; 
    struct {
        uint8_t charge;
        uint8_t unload;
        uint8_t timeout;
    } laser;
} p[2], template = {
    .x = SCREEN_SIZE/2,
    .life = 64,
    .speed = 1.0f,
    .bullet.speed = 1,
    .bullet.rate  = 60,
    .bullet.damage = 3,
    //.upgrades[LASER] = 1,
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
uint32_t t = 0;

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

void start_palette(){
    PALETTE[0] = 0x0;
    PALETTE[1] = 0xffffff;
    PALETTE[2] = 0xe92f2f;
    PALETTE[3] = 0xf6c515;
}

void play_palette(){
    PALETTE[0] = 0x0;
    PALETTE[1] = 0xffffff;
    PALETTE[2] = 0x0000ff;
    PALETTE[3] = 0xff0000;
}

void end_palette(){
    PALETTE[0] = 0x0;
    PALETTE[1] = 0xffffff;
    PALETTE[2] = 0x7d7d7d;
    PALETTE[3] = (winner == 0) ? 0x0000ff : 0xff0000;
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

void reset_items(){
    for (int i = 0 ; i<MAX_ITEMS ; i++) {
        items[i] = (struct item){0};
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
        global_delay = END_DELAY;
        end_palette();
        state = END;
    }
}

int next_free(float *array, int length, int start) {
    while (start<length && array[start] != 0)
        start++;
    if (start >= length)
        start = -1;

    return start;
}

void start() {
    reset_players();
    start_palette();
    reset_asteroids();
    reset_blasts();
    reset_items();
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
    tone(60 | (290 << 16), 0x02080808, 0x2020, TONE_NOISE);
    
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

void spawn_item(uint8_t x, uint8_t y) {
    for(int i = 0; i<MAX_ITEMS; i++) {
        if (!items[i].active){
            items[i].active = true;
            items[i].x = x;
            items[i].y = y;

            //items[i].type = (enum itemtype)(rand()%NUM_ITEM_TYPES);
            // last item (arc) not implemented
            items[i].type = (enum itemtype)(rand()%NUM_ITEM_TYPES-1);
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
        uint8_t x = (uint8_t)asteroids[ai].x;
        uint8_t y = (uint8_t)asteroids[ai].y;

        if (size > 0) {
            for(int i=0 ; i<2 ; i++) {
                spawn_asteroid(x, y, 0, (int8_t)(1-2*pi), (enum astsize) size-1);
            }
        } else {
            spawn_item(x, y);
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

void get_item(int pi, enum itemtype type) {
    switch (type) {
        case HEAL:
            p[pi].life += HEAL_ADD;
            break;
        case FIRERATE:
            //if(p[pi].bullet.rate > MIN_FIRERATE) {
            if (p[pi].upgrades[FIRERATE] <= MAX_FIRERATE) {
                p[pi].bullet.rate -= 3;
                p[pi].upgrades[FIRERATE]++;
            } 
            break;
        case LASER:
            if (p[pi].upgrades[LASER] <= MAX_LASER) {
                p[pi].upgrades[LASER]++;
            }
            break;
        case SPEED:
            //if(p[pi].bullet.speed < MAX_BULLET_SPEED) {
            if (p[pi].upgrades[SPEED] <= MAX_SPEED) {
                p[pi].bullet.speed += SPEED_INC; 
                p[pi].upgrades[SPEED]++;
            }
            break;
        case ARC: // not implmented
            if (p[pi].upgrades[ARC] <= MAX_ARC) {
                p[pi].upgrades[ARC]++;
            }
            break;
    } 
}

bool check_bullet_collision(int pi, int bi) {
    uint8_t *bx = &p[pi].bullet.x[bi];
    float *by = &p[pi].bullet.y[bi];
    if (collision(p[1-pi].x, p[1-pi].y, P_COLL_WIDTH, P_COLL_HEIGHT,
                  *bx, (uint8_t)*by, bulletWidth, bulletHeight)) {
        damage(1-pi, p[pi].bullet.damage);

        return true;
    }

    for(int i = 0; i<MAX_ASTEROIDS ; i++) {
        if (asteroids[i].active) {
            if (astero_collision(
                asteroids[i].x, asteroids[i].y,
                astero_size[asteroids[i].size]/2.0f,
                *bx, (uint8_t)*by,
                bulletWidth/2.0f
            )) {
                astero_damage(i, pi, p[pi].bullet.damage);
                return true;
            }
        }
    }

    for(int i = 0; i<MAX_ITEMS ; i++) {
        if (items[i].active) {
            if (collision(items[i].x, items[i].y, ITEM_SIZE, ITEM_SIZE,
                          *bx, (uint8_t)*by, bulletWidth, bulletHeight)) {
                items[i].active = false;
                get_item(pi, items[i].type);
                return true;
            }
        }

    }
    
    return false; 
}

void check_laser_collision(int pi) {
    uint8_t x = (uint8_t)p[pi].x;
    if (collision(p[1-pi].x, p[1-pi].y, P_COLL_WIDTH, P_COLL_HEIGHT,
                  x, SCREEN_SIZE/2, LASER_BEAM_DMG_WIDTH, SCREEN_SIZE)) {
        damage(1-pi, LASER_DMG);
    }

    float laser_min_x = x - LASER_BEAM_DMG_WIDTH/2;
    float laser_max_x = x + LASER_BEAM_DMG_WIDTH/2;

    for(int i = 0; i<MAX_ASTEROIDS ; i++) {
        if (asteroids[i].active) {
            float aster_min_x = asteroids[i].x - astero_size[asteroids[i].size]/2.0f;
            float aster_max_x = asteroids[i].x + astero_size[asteroids[i].size]/2.0f;
            if (laser_min_x <= aster_max_x && laser_max_x >= aster_min_x) {
                astero_damage(i, pi, LASER_DMG);
            }
        }
    }

    for(int i = 0; i<MAX_ITEMS ; i++) {
        if (items[i].active) {
            if (collision(items[i].x, items[i].y, ITEM_SIZE, ITEM_SIZE,
                          x, SCREEN_SIZE/2, LASER_BEAM_DMG_WIDTH, SCREEN_SIZE)) {
                items[i].active = false;
                get_item(pi, items[i].type);
            }
        }

    } 
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

            tone(890 | (180 << 16), 0x0a00, 70, TONE_MODE3);

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


        if (*p[i].pad & BUTTON_2 && !p[i].laser.timeout && p[i].upgrades[LASER] > 0) {
            if (!p[i].laser.charge) {
                //tone(150 | (410 << 16), 0x383d3314, 30, TONE_NOISE); //laser charge
                tone(150 | (410 << 16), 0x383d3314, 60, TONE_TRIANGLE); //laser charge
            }

            p[i].laser.charge++;

            if (p[i].laser.charge == LASER_CHARGE) {
                p[i].upgrades[LASER]--;
                tone(750 | (60 << 16), 0x0a0f0f, 100, TONE_PULSE2); //laser shoot
                p[i].laser.charge = 0;
                p[i].laser.timeout = LASER_TIMEOUT;
                p[i].laser.unload = LASER_UNLOAD;
            }
        } else {
            p[i].laser.charge = 0; // charge released
        }

        
        if (p[i].laser.unload) {
            p[i].laser.unload--;
            if(t%2 == 0) { // was OP without
            	check_laser_collision(i);
            }
        }

        if (p[i].laser.timeout)
            p[i].laser.timeout--;


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
        
        for(int ii = 0; ii<MAX_ITEMS ; ii++) {
            if (items[ii].active) {
                if (collision(items[ii].x, items[ii].y, ITEM_SIZE, ITEM_SIZE,
                              p[i].x, p[i].y, P_COLL_WIDTH, P_COLL_HEIGHT)) {
                    items[ii].active = false;
                    get_item(i, items[ii].type);
                }
            }
        }
        
        for (int j = 0 ; j < SCREEN_SIZE ; j++) {
            uint8_t *bx = &p[i].bullet.x[j];
            float *by = &p[i].bullet.y[j];
            if (*by != 0) {
                if (*by < 0 || *by > SCREEN_SIZE) {
                    *by = 0;
                } else {
                    if (check_bullet_collision(i,j)) {
        		*DRAW_COLORS = player2color(p[i]);

        		spawn_blast(*bx, (uint8_t)*by, i);
                        //oval(*bx-EXPLO_SIZE/2, *by-EXPLO_SIZE/2, EXPLO_SIZE, EXPLO_SIZE);
                        *by = 0;

                    } else {
                        *by += p[i].bullet.speed * p[i].bullet.dir;
                        set_sprite_colors(p[i]);
                        blit(bullet,
                             *bx-bulletWidth/2, (uint8_t)*by - bulletHeight/2,
                             bulletWidth, bulletHeight,
                             bulletFlags|(p[i].bullet.dir>0?0:BLIT_FLIP_Y));
                    }
                }
            }               
        }
    } 
}

void update_items() {
    for (int i=0; i<MAX_ITEMS ; i++) {
        if (items[i].active) { 
            *DRAW_COLORS = (uint16_t)((2 + (t/4+items[i].type)%3)<<4);
            blit(item_sprite[items[i].type],
                 items[i].x - ITEM_SIZE/2,
                 items[i].y - ITEM_SIZE/2,
                 ITEM_SIZE, ITEM_SIZE,
                 BLIT_1BPP);
        }
    }
}

void draw_bar(struct player p) {
    uint8_t x = 0;
    uint8_t y = (p.id == 0) ? 0 : SCREEN_SIZE - BAR_SIZE; 

    *DRAW_COLORS = player2color(p);
    
    rect(0, y, p.life, BAR_SIZE);

    x = p.life + BAR_GAP*2;
    
    for (int i=1; i < NUM_ITEM_TYPES ; i++) {
        int ups = p.upgrades[i];
        if (ups > 0) {
            *DRAW_COLORS = 0x20 | player2color(p);
            blit(item_sprite[(enum itemtype) i], x, y,
                 ITEM_SIZE, ITEM_SIZE, BLIT_1BPP);
            x += ITEM_SIZE + BAR_GAP;


            *DRAW_COLORS = 0x2;
            if(ups == MAX_UPGRADE) {
                text("MAX", x, y);
                x += FONT_SIZE*3 + BAR_GAP*2;
            } else {
                text(digits[ups], x, y);
                x += FONT_SIZE + BAR_GAP*2;
            }
            
        }
    }
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


    int bullet_frame = (p.bullet.rate - p.bullet.timeout)/2;
    if (bullet_frame < SHOOT_BULLETS_FRAMES) {
        blitSub(shoot_bullet_anim, 
                p.x - P_SIZE/2, p.y - P_SIZE/2,
                P_SIZE, P_SIZE,
                (uint32_t)(P_SIZE*bullet_frame),
                (sprite_idx>0 ? 0: 1) * P_SIZE,
                shoot_bullet_animWidth,
                shoot_bullet_animFlags | xflip | p.drawflags); 
    }

    if (p.laser.charge) {
        int laser_charge_frame = (p.laser.charge*LASER_CHARGE_FRAMES) / LASER_CHARGE;
        blitSub(shoot_laser_anim,
                p.x - P_SIZE/2, p.y - P_SIZE/2,
                P_SIZE, P_SIZE,
                (uint32_t)(P_SIZE*laser_charge_frame),
                (sprite_idx>0 ? 1: 0) * P_SIZE,
                shoot_laser_animWidth,
                shoot_laser_animFlags | xflip | p.drawflags);
    }

    if (p.laser.unload) {
        int laser_frame = p.laser.unload % LASER_FRAMES;
        blitSub(laser,
                p.x - LASER_BEAM_WIDTH/2, LASER_OFFSET * p.bullet.dir,
                LASER_BEAM_WIDTH, laserHeight,
                (uint32_t)(LASER_BEAM_WIDTH * laser_frame), 0,
                laserWidth,
                laserFlags | xflip | p.drawflags);
        
    }
}

void draw_players() {
    for (int i=0 ; i<2 ; i++){
        draw_bar(p[i]);
        draw_ship(p[i]);
    } 
}

void draw_bg() {

    float pi_t = (float)(t*M_PI*2/1024);

    int32_t margin = bgWidth - SCREEN_SIZE;
    
    int32_t x = (int32_t)(sinf(pi_t) * (float)margin/2) - margin/2;
    int32_t y = (int32_t)(cosf(pi_t) * (float)margin/2) - margin/2;

    blit(bg, x, y, bgWidth, bgHeight, bgFlags);
    //blit(bg, y, x, bgWidth, bgHeight, bgFlags | BLIT_FLIP_Y);
}

void update_start() {
    //draw_bg();

    *DRAW_COLORS = 0x4320;

    blit(startscreen, 0, 0, startscreenWidth, startscreenHeight, startscreenFlags);
    
    uint32_t button_width = start_buttonWidth/start_button_Frames;
    uint32_t button_frame = start_button_Frames -1;

    if (t/start_button_Time_scale % (start_button_Frames*start_button_Interval) < start_button_Frames) {
        button_frame = t/start_button_Time_scale % start_button_Frames;
    }

    blitSub(start_button,
            start_button_X, start_button_Y,
            button_width, start_buttonHeight,
            button_width * button_frame, 0,
            start_buttonWidth,
            start_buttonFlags);

    //*DRAW_COLORS = 2 + t/4%3;
    //text("Ready?", 80-24, 80-4);

    if (global_delay > 0) {
        global_delay--;
        return;
    }
    
    bool p1_ready = *p[0].pad & BUTTON_1;
    bool p2_ready = *p[1].pad & BUTTON_1;

    *DRAW_COLORS = 3 + t/6%2;

    if (p1_ready) {
        text("P1 ready", SCREEN_SIZE/2-4*FONT_SIZE, SCREEN_SIZE-FONT_SIZE);
    }
    if (p2_ready) {
        text("P2 ready", SCREEN_SIZE/2-4*FONT_SIZE, SCREEN_SIZE-FONT_SIZE);
    }

    bool button_clicked = false;

    if (*MOUSE_BUTTONS & MOUSE_LEFT) {
    	uint32_t mx = (uint32_t) *MOUSE_X;
    	uint32_t my = (uint32_t) *MOUSE_Y;

    	button_clicked = mx > start_button_X &&
    		         mx < start_button_X + button_width &&
    		         my > start_button_Y &&
    		         my < start_button_Y + start_buttonHeight;
    }
    

    if ((p1_ready && p2_ready) || button_clicked) {
        //tone(262, 60 | (30 << 8), 100, TONE_PULSE1);

        //tone(60 | (290 << 16), 0x020f0f08, 100, TONE_PULSE1);
        play_palette();
        state = PLAY;
    }
}

void update_play() {

    *DRAW_COLORS = 0x20;
    draw_bg();
    
    update_players();
    update_asteroids();
    draw_players();
    update_blasts();
    update_items();
}

void draw_end_name(bool randoffset) {
    blitSub(end_name,
            end_nameX + (randoffset ? rand()%5-2 : 0),
            end_nameY + (randoffset ? rand()%5-2 : 0),
            end_nameWidth, end_nameHeight/2,
            0, (1-winner)*end_nameHeight/2, end_nameWidth, end_nameFlags);
}

void update_end() {
   
    *DRAW_COLORS = 0x4320;

    if(*p[winner].pad & BUTTON_1) {
        *DRAW_COLORS = 0x40;
        draw_bg();
        if (t/4%2) {
            *DRAW_COLORS = 0x30;
            draw_end_name(true);
            *DRAW_COLORS = 0x4420;
        } else {
            *DRAW_COLORS = 0x4320;
        }
        if (rand()%20 == 0) {
            spawn_blast((uint8_t)(rand()%SCREEN_SIZE), (uint8_t)(rand()%SCREEN_SIZE), winner);
        }
    }

    blit(end_screen, 0, 0, end_screenWidth, end_screenHeight, end_screenFlags);

    *DRAW_COLORS = 0x40;
    draw_end_name(false);

    *DRAW_COLORS = 0x4320;
    update_blasts();

    if (global_delay > 0) {
        global_delay--;
    } else if (*MOUSE_BUTTONS & MOUSE_LEFT ||
               (*p[0].pad & BUTTON_1 && *p[1].pad & BUTTON_1)) {
        global_delay = START_DELAY;
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
