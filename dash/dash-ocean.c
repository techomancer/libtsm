#include "dash.h"

#define GRID_COLS 8
#define GRID_ROWS 4
#define WAVE_AMP 16.0f
#define MARGIN WAVE_AMP
#define MAX_BUBBLES 16
#define MAX_FISH 16
#define BUBBLE_ALPHA_CENTER 0.05f
#define BUBBLE_ALPHA_EDGE 0.20f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    float speed;    /* pixels per second */
    float period;   /* cycles per screen width */
    float offset;   /* current phase offset in pixels */
} WaveLine;

typedef struct {
    float base_x;
    float y;
    float speed;
    float wave_phase;
    float wave_amp;
    int size_idx;
    int active;
} Bubble;

typedef struct {
    float x, y;
    float speed;
    float size;
    int active;
} Fish;

typedef struct {
    WaveLine lines[GRID_ROWS + 1];
    struct timeval last_update;
    Bubble bubbles[MAX_BUBBLES];
    float spawn_timer;
    Fish fish[MAX_FISH];
    float fish_spawn_timer;
} OceanBg;

static const Color ocean_colors[3] = {
    {0.1f, 0.1f, 0.3f, 1.0f}, /* Top: Less saturated dark blue */
    {0.0f, 0.0f, 0.4f, 1.0f}, /* Mid: Saturated dark blue */
    {0.0f, 0.0f, 0.0f, 1.0f}  /* Bot: Black */
};

static const float bubble_sizes[] = { 2.5f, 4.0f, 5.5f };

static void ocean_bg_update(void *state, int width, int height);
static void ocean_bg_draw(void *state);

/*
 * Initialize Ocean Background
 */
void
ocean_bg_init(int width, int height)
{
    OceanBg *bg;
    int i;

    if (app.bg_state) {
        free(app.bg_state);
    }

    bg = calloc(1, sizeof(OceanBg));

    gettimeofday(&bg->last_update, NULL);

    /* Initialize wave parameters for each horizontal line */
    for (i = 0; i <= GRID_ROWS; i++) {
        /* Random speed: -50 to 50 pixels/sec */
        bg->lines[i].speed = (float)(rand() % 100 - 50);
        
        /* Period: 1.0 to 2.0 cycles per screen width */
        bg->lines[i].period = 1.0f + (float)(rand() % 100) / 100.0f;
        
        /* Random initial offset */
        bg->lines[i].offset = (float)(rand() % (width > 0 ? width : 1));
    }

    bg->fish_spawn_timer = 5.0f + (float)(rand() % 10);

    app.bg_state = bg;
    app.bg_update = ocean_bg_update;
    app.bg_draw = ocean_bg_draw;
}

/*
 * Update Ocean Background
 */
static void
ocean_bg_update(void *state, int width, int height)
{
    OceanBg *bg = (OceanBg*)state;
    struct timeval tv;
    float dt;
    int i;

    gettimeofday(&tv, NULL);
    dt = (float)(tv.tv_sec - bg->last_update.tv_sec) +
         (float)(tv.tv_usec - bg->last_update.tv_usec) / 1000000.0f;
    bg->last_update = tv;

    if (dt > 0.1f) dt = 0.1f;

    /* Update wave offsets */
    for (i = 0; i <= GRID_ROWS; i++) {
        bg->lines[i].offset += bg->lines[i].speed * dt;
    }

    /* Update bubbles */
    bg->spawn_timer -= dt;
    if (bg->spawn_timer <= 0.0f) {
        int count = 3 + rand() % 5;
        float grp_x = (float)(rand() % (width > 0 ? width : 1));
        
        bg->spawn_timer = 10.0f + (float)(rand() % 500) / 100.0f;
        
        for (i = 0; i < MAX_BUBBLES && count > 0; i++) {
            if (!bg->bubbles[i].active) {
                bg->bubbles[i].active = 1;
                bg->bubbles[i].base_x = grp_x + (float)(rand() % 60 - 30);
                bg->bubbles[i].y = height + MARGIN + (float)(rand() % 50);
                bg->bubbles[i].speed = 8.0f + (float)(rand() % 50) / 10.0f;
                bg->bubbles[i].wave_phase = (float)(rand() % 628) / 100.0f;
                bg->bubbles[i].wave_amp = 2.0f + (float)(rand() % 30) / 10.0f;
                bg->bubbles[i].size_idx = rand() % 3;
                count--;
            }
        }
    }

    for (i = 0; i < MAX_BUBBLES; i++) {
        if (bg->bubbles[i].active) {
            bg->bubbles[i].y -= bg->bubbles[i].speed * dt;
            if (bg->bubbles[i].y < -20.0f) {
                bg->bubbles[i].active = 0;
            }
        }
    }

    /* Update fish */
    bg->fish_spawn_timer -= dt;
    if (bg->fish_spawn_timer <= 0.0f) {
        int is_school = rand() % 2;
        int direction = rand() % 2; /* 0: L->R, 1: R->L */
        float start_y = height * 0.4f + (float)(rand() % (int)(height * 0.5f));
        float base_speed = 30.0f + (float)(rand() % 40);
        if (direction == 1) base_speed = -base_speed;
        
        int count = is_school ? (3 + rand() % 4) : 1;
        float base_size = is_school ? (10.0f + (float)(rand() % 10)) : (30.0f + (float)(rand() % 30));
        
        bg->fish_spawn_timer = 15.0f + (float)(rand() % 20);

        for (i = 0; i < MAX_FISH && count > 0; i++) {
            if (!bg->fish[i].active) {
                bg->fish[i].active = 1;
                bg->fish[i].size = base_size * (0.8f + (float)(rand() % 40) / 100.0f);
                bg->fish[i].speed = base_speed * (0.9f + (float)(rand() % 20) / 100.0f);
                
                float x_offset = (float)(rand() % 100);
                float y_offset = (float)(rand() % 60 - 30);
                
                if (direction == 0) { /* L->R */
                    bg->fish[i].x = -100.0f - x_offset;
                } else { /* R->L */
                    bg->fish[i].x = width + 100.0f + x_offset;
                }
                bg->fish[i].y = start_y + y_offset;
                count--;
            }
        }
    }

    for (i = 0; i < MAX_FISH; i++) {
        if (bg->fish[i].active) {
            bg->fish[i].x += bg->fish[i].speed * dt;
            if ((bg->fish[i].speed > 0 && bg->fish[i].x > width + 150.0f) ||
                (bg->fish[i].speed < 0 && bg->fish[i].x < -150.0f)) {
                bg->fish[i].active = 0;
            }
        }
    }
}

/*
 * Helper to calculate gradient color
 * t: 0.0 (top) to 1.0 (bottom)
 */
static void
get_ocean_color(float t, Color *c)
{
    const Color *c1, *c2;
    float f;

    if (t < 0.5f) {
        c1 = &ocean_colors[0];
        c2 = &ocean_colors[1];
        f = t * 2.0f;
    } else {
        c1 = &ocean_colors[1];
        c2 = &ocean_colors[2];
        f = (t - 0.5f) * 2.0f;
    }

    c->r = (c1->r * (1.0f - f) + c2->r * f) * app.settings.brightness;
    c->g = (c1->g * (1.0f - f) + c2->g * f) * app.settings.brightness;
    c->b = (c1->b * (1.0f - f) + c2->b * f) * app.settings.brightness;
    c->a = 1.0f;
}

/*
 * Draw Ocean Background
 */
static void
ocean_bg_draw(void *state)
{
    OceanBg *bg = (OceanBg*)state;
    int r, c;
    int w = app.width;
    int h = app.height;
    float br = app.settings.brightness;
    
    /* Grid covers -MARGIN to h+MARGIN to ensure screen is covered despite waves */
    float start_y = -MARGIN;
    float total_h = h + 2 * MARGIN;
    float row_h = total_h / GRID_ROWS;
    float col_w = (float)w / GRID_COLS;

    glDisable(GL_TEXTURE_2D);
    glShadeModel(GL_SMOOTH);

    /* Draw grid row by row */
    for (r = 0; r < GRID_ROWS; r++) {
        glBegin(GL_QUAD_STRIP);
        for (c = 0; c <= GRID_COLS; c++) {
            float x = c * col_w;
            
            /* Top vertex of this strip (row r) */
            float y_base_top = start_y + r * row_h;
            float t_top = (float)r / GRID_ROWS;
            float wave_top = WAVE_AMP * sinf(2.0f * M_PI * bg->lines[r].period * 
                                           (x + bg->lines[r].offset) / w);
            float y_top = y_base_top + wave_top;
            
            Color c_top;
            get_ocean_color(t_top, &c_top);
            glColor3fv(&c_top.r);
            glVertex2f(x, y_top);

            /* Bottom vertex of this strip (row r+1) */
            float y_base_bot = start_y + (r + 1) * row_h;
            float t_bot = (float)(r + 1) / GRID_ROWS;
            float wave_bot = WAVE_AMP * sinf(2.0f * M_PI * bg->lines[r+1].period * 
                                           (x + bg->lines[r+1].offset) / w);
            float y_bot = y_base_bot + wave_bot;

            Color c_bot;
            get_ocean_color(t_bot, &c_bot);
            glColor3fv(&c_bot.r);
            glVertex2f(x, y_bot);
        }
        glEnd();
    }

    /* Draw fish */
    for (r = 0; r < MAX_FISH; r++) {
        if (bg->fish[r].active) {
            float x = bg->fish[r].x;
            float y = bg->fish[r].y;
            float s = bg->fish[r].size;
            float dir = (bg->fish[r].speed > 0) ? 1.0f : -1.0f;
            
            glBegin(GL_TRIANGLES);
            
            /* Top Body Triangle (Darker) */
            glColor4f(0.15f * br, 0.15f * br, 0.2f * br, 0.8f);
            glVertex2f(x + s * 0.5f * dir, y); /* Nose */
            glVertex2f(x - s * 0.5f * dir, y); /* TailBase */
            glVertex2f(x, y - s * 0.3f);       /* Top */
            
            /* Bottom Body Triangle (Lighter) */
            glColor4f(0.25f * br, 0.25f * br, 0.3f * br, 0.8f);
            glVertex2f(x + s * 0.5f * dir, y); /* Nose */
            glVertex2f(x - s * 0.5f * dir, y); /* TailBase */
            glVertex2f(x, y + s * 0.3f);       /* Bottom */
            
            /* Tail Triangle */
            glColor4f(0.2f * br, 0.2f * br, 0.25f * br, 0.8f);
            glVertex2f(x - s * 0.5f * dir, y); /* TailBase */
            glVertex2f(x - (s * 0.5f + s * 0.3f) * dir, y - s * 0.25f);
            glVertex2f(x - (s * 0.5f + s * 0.3f) * dir, y + s * 0.25f);
            
            glEnd();
        }
    }

    /* Draw bubbles */
    for (r = 0; r < MAX_BUBBLES; r++) {
        if (bg->bubbles[r].active) {
            float x = bg->bubbles[r].base_x + sinf(bg->bubbles[r].y * 0.05f + bg->bubbles[r].wave_phase) * bg->bubbles[r].wave_amp;
            float y = bg->bubbles[r].y;
            float rad = bubble_sizes[bg->bubbles[r].size_idx];
            int j;

            glBegin(GL_TRIANGLE_FAN);
            glColor4f(1.0f * br, 1.0f * br, 1.0f * br, BUBBLE_ALPHA_CENTER);
            glVertex2f(x, y);
            
            glColor4f(1.0f * br, 1.0f * br, 1.0f * br, BUBBLE_ALPHA_EDGE);
            for (j = 0; j <= 8; j++) {
                float a = j * 2.0f * M_PI / 8.0f;
                glVertex2f(x + cosf(a) * rad, y + sinf(a) * rad);
            }
            glEnd();
        }
    }

    glEnable(GL_TEXTURE_2D);
}
