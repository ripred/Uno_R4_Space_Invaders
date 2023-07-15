/*
 * Arduino R4 WiFi Space Invaders
 * 
 * version 1.0 July, 2023 ++trent m. wyatt
 * 
 */

#include <Arduino.h>
#include "Arduino_LED_Matrix.h"
#include <memory.h>
#include <vector>
#include <list>

using std::vector;
using std::list;

#define   MAX_Y   8
#define   MAX_X   12

#define   FIRE_PIN    A0
#define   LEFT_PIN    A1
#define   RIGHT_PIN   A2

int invader_dir = 1;

uint8_t grid[MAX_Y][MAX_X] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

ArduinoLEDMatrix matrix;

void set(int x, int y) {
    if (x < 0 || x >= MAX_X || y < 0 || y >= MAX_Y) return;
    grid[y][x] = 1;
}

void reset(int x, int y) {
    if (x < 0 || x >= MAX_X || y < 0 || y >= MAX_Y) return;
    grid[y][x] = 0;
}

struct point_t {
    int x, y;

    point_t() : x(0), y(0) {}
    point_t(int _x, int _y) : x(_x), y(_y) {}
};

typedef char bitmap_t[2][3];

class sprite_t : public point_t {
    protected:
    bitmap_t  sprites[3];   // sprites to sequence through
    int       num_sprites;  // how many sprites we have
    int       cur;          // which sprite is the current displayed sprite

    public:
    sprite_t() 
    {
        num_sprites = 0;
        cur = 0;
    }

    sprite_t(int _x, int _y) : point_t(_x, _y) 
    {
        num_sprites = 0;
        cur = 0;
    }

    int width() const { 
        return 3; 
    }

    int height() const { 
        return 2; 
    }

    void set() const {
        for (int row = 0; row < height(); row++) {
            for (int col = 0; col < width(); col++) {
                if (0 != sprites[cur][row][col]) ::set(col + x,row + y);
            }
        }
    }
    
    void reset() const {
        for (int row = 0; row < height(); row++) {
            for (int col = 0; col < width(); col++) {
                if (0 != sprites[cur][row][col]) ::reset(col+x,row+y);
            }
        }
    }
    
    int get(int const col, int const row) const {
        if (col < 0 || col >= width() || row < 0 || row >= height()) return 0;
        return sprites[cur][row][col];
    }

    void add_sprite(bitmap_t &bm) {
        if (num_sprites < int(sizeof(sprites)/(sizeof(*sprites)))) {
            memcpy(sprites[num_sprites++], bm, sizeof(bitmap_t));
        }
    }

    void next_frame(int const frame = -1) {
        if (0 == num_sprites) return;

        if (-1 == frame) {
            ++cur %= num_sprites;
        }
        else {
            cur = frame % num_sprites;
        }
    }

    bool collide(sprite_t &ref) const {
        if ((x >= ref.x) && (x < (ref.x+3)) && (y >= ref.y) && (y < ref.y + 2)) {
            return true;
        }
        return false;
    }

    // required to be implemented by all sub-classes:
    virtual void init() = 0;

}; // sprite_t


// our base
struct base_t : public sprite_t {
    void init() {
        bitmap_t data = {
            { 0, 1, 0 },
            { 1, 1, 1 }
        };

        add_sprite(data);
    }

    base_t() {
        init();
    }

    base_t(int _x, int _y) : sprite_t(_x, _y) {
        init();
    }

}; // base_t


// an invader
struct invader_t : public sprite_t {
    void init() {
        bitmap_t data1 = {
            { 1, 1, 1 },
            { 1, 0, 0 }
        };
        bitmap_t data2 = {
            { 1, 1, 1 },
            { 0, 1, 0 }
        };
        bitmap_t data3 = {
            { 1, 1, 1 },
            { 0, 0, 1 }
        };
    
        add_sprite(data1);
        add_sprite(data2);
        add_sprite(data3);
    }

    invader_t() {
        init();
    }

    invader_t(int _x, int _y) : sprite_t(_x, _y) {
        init();
    }

}; // invader_t


// a block
struct block_t : public sprite_t {
    void init() {
        bitmap_t data = {
            { 1, 1, 1 },
            { 0, 0, 0 }
        };

        add_sprite(data);
    }

    block_t() {
        init();
    }

    block_t(int _x, int _y) : sprite_t(_x, _y) {
        init();
    }

}; // block_t


// a shot
struct shot_t : public sprite_t {
    int dx, dy;

    void init() {
        bitmap_t data = {
            { 1, 0, 0 },
            { 0, 0, 0 }
        };

        add_sprite(data);
        dx = 0;
        dy = 0;
    }

    shot_t() {
        init();
    }

    shot_t(int _x, int _y) : sprite_t(_x, _y) {
        init();
    }

}; // shot_t


//  ================================================================================
vector<invader_t> invaders;
vector<block_t> blocks;
vector<shot_t> shots;
base_t base;

uint32_t last_invader_move = 0;
uint32_t last_shot_move = 0;

uint32_t invader_move_time = 1000;
uint32_t shot_move_time = 75;

void dbg(struct sprite_t &sprite) {
    for (int row = 0; row < sprite.height(); row++) {
        for (int col = 0; col < sprite.width(); col++) {
            Serial.write(sprite.get(col,row) ? "* " : "  ");
        }
        Serial.write('\n');
    }
}

void render() {
    memset(grid, 0, sizeof(grid));
    base.set();
    base.next_frame();
    for (invader_t &invader : invaders) {
        invader.set();
        invader.next_frame();
    }
    for (block_t &block : blocks) {
        block.set();
        block.next_frame();
    }
    for (shot_t &shot :shots) {
        shot.set();
        shot.next_frame();
    }
    matrix.renderBitmap(grid, MAX_Y, MAX_X);
}

void move_invaders() {
    if (millis() >= last_invader_move + invader_move_time) {
        last_invader_move = millis();
        int dy = 0;
        for (auto &invader : invaders) {
            if ((invader.x + invader_dir) < 0 || 
                (invader.x + invader_dir) >= (MAX_X - 2)) 
            {
                invader_dir *= -1;
                if (1 == invader_dir) {
                    dy = 1;
                }
                break;
            }
        }
    
        for (auto &invader : invaders) {
            invader.x += invader_dir;
            invader.y += dy;

            if (invader.y >= 4) {
                new_game();
                break;
            }
            else {
                if (random(1, 10) > 6) {
                    shoot(invader.x + 1, invader.y + 2, 0, 1);
                }
            }
        }
    }
}

void move_shots() {
    list<shot_t> next;

    if (millis() >= last_shot_move + shot_move_time) {
        last_shot_move = millis();
        for (auto &shot : shots) {
            shot.x += shot.dx;
            shot.y += shot.dy;
            if (shot.x >= 0 && 
                shot.x < MAX_X && 
                shot.y >= 0 && 
                shot.y <= MAX_Y)
            {
                next.push_back(shot);
            }
        }

        shots.clear();
        for (auto &shot : next) {
            shots.push_back(shot);
        }
    }
}

void shoot(int x, int y, int dx, int dy) {
    shot_t shot;
    shot.x = x;
    shot.y = y;
    shot.dx = dx;
    shot.dy = dy;
    shots.push_back(shot);
}


void new_game() {
    base.x = 5;
    base.y = 6;

    invaders.clear();
    for (int col = 0; col < 3; col++) {
        invader_t invader;
        invader.x = col * 4;
        invader.y = 0;
        invaders.push_back(invader);
    }
    
    blocks.clear();
    for (int col = 0; col < 3; col++) {
        block_t block;
        block.x = col * 4;
        block.y = 4;
        blocks.push_back(block);
    }

    shots.clear();
}



void setup() {
    Serial.begin(115200);
    while (!Serial) { }

    Serial.println("Arduino R4 WiFi Space Invaders");

    new_game();

    Serial.println("invader_t");
    dbg(invaders[0]);

    Serial.println("block_t");
    dbg(blocks[0]);

    Serial.println("shot_t");
    dbg(shots[0]);

    Serial.println("base_t");
    dbg(base);

    matrix.begin();

    pinMode(A3, INPUT);
    randomSeed(analogRead(A3));

    last_invader_move = millis();
    last_shot_move = millis();

    pinMode(FIRE_PIN, INPUT_PULLUP);
    pinMode(LEFT_PIN, INPUT_PULLUP);
    pinMode(RIGHT_PIN, INPUT_PULLUP);

    render();
}


void check_block_collisions() {
    list<block_t> next_blocks;

    for (block_t &block : blocks) {
        vector<shot_t> next_shots;
        bool keep = true;
        for (shot_t &shot : shots) {
            if (shot.collide(block)) {
                keep = false;
                break;
            }
            else {
                next_shots.push_back(shot);
            }
        }

        if (keep) {
            next_blocks.push_back(block);
        }
        shots = next_shots;
    }

    blocks.clear();
    for (auto &block : next_blocks) {
        blocks.push_back(block);
    }
}

void check_invader_collisions() {
    list<invader_t> next_invaders;

    for (invader_t &invader : invaders) {
        vector<shot_t> next_shots;
        bool keep = true;
        for (shot_t &shot : shots) {
            if (shot.collide(invader)) {
                keep = false;
                break;
            }
            else {
                next_shots.push_back(shot);
            }
        }

        if (keep) {
            next_invaders.push_back(invader);
        }
        shots = next_shots;
    }

    invaders.clear();
    for (auto &invader : next_invaders) {
        invaders.push_back(invader);
    }

    if (invaders.empty()) {
        new_game();
    }
}

void check_base_collisions() {
    for (shot_t &shot : shots) {
        if (shot.collide(base)) {
            for (int i = 0; i < 8; i++) {
                base.set();
                matrix.renderBitmap(grid, MAX_Y, MAX_X);
                delay(100);
                base.reset();
                matrix.renderBitmap(grid, MAX_Y, MAX_X);
                delay(100);
            }
            new_game();
        }
    }
}


void loop() {
    if (!digitalRead(FIRE_PIN)) {
        shoot(base.x + 1, base.y, 0, -1);
    }

    if (!digitalRead(LEFT_PIN)) {
        if (base.x >= 1) { base.x--; }
    }

    if (!digitalRead(RIGHT_PIN)) {
        if (base.x < (MAX_X - 3)) { base.x++; }
    }

    render();
    move_invaders();
    move_shots();

    check_block_collisions();
    check_invader_collisions();
    check_base_collisions();
    delay( 1000.0 / 12.0);
}
