#include "dash.h"

/* Background system */
typedef struct {
    float x, y;
    float speed;
    float brightness;
    float r, g, b;
    int type; /* 0=pixel, 1=cross */
} Star;

typedef struct {
    int active;
    float x, y;
    float speed;
    float tail_len;
} ShootingStar;

typedef struct {
    Star stars[128];
    ShootingStar shooter;
    struct timeval last_update;
} NightBg;

static void night_bg_update(void *state, int width, int height);
static void night_bg_draw(void *state);

/*
 * Initialize Night Background
 */
void
night_bg_init(int width, int height)
{
    NightBg *bg;
    int i;

    if (app.bg_state) {
        free(app.bg_state);
    }

    bg = calloc(1, sizeof(NightBg));

    gettimeofday(&bg->last_update, NULL);

    for (i = 0; i < 128; i++) {
        bg->stars[i].x = (float)(rand() % (width > 0 ? width : 1));
        bg->stars[i].y = (float)(rand() % (height > 0 ? height : 1));
        bg->stars[i].speed = 5.0f + (float)(rand() % 10);
        bg->stars[i].brightness = 0.1f + ((float)(rand() % 40) / 100.0f);
        bg->stars[i].type = rand() % 2;

        /* Base color with slight tint */
        bg->stars[i].r = 0.8f;
        bg->stars[i].g = 0.8f;
        bg->stars[i].b = 0.8f;

        switch (rand() % 6) {
        case 0: bg->stars[i].r = 1.0f; break; /* Red tint */
        case 1: bg->stars[i].g = 1.0f; break; /* Green tint */
        case 2: bg->stars[i].b = 1.0f; break; /* Blue tint */
        }
    }

    app.bg_state = bg;
    app.bg_update = night_bg_update;
    app.bg_draw = night_bg_draw;
}

/*
 * Update Night Background
 */
static void
night_bg_update(void *state, int width, int height)
{
    NightBg *bg = (NightBg*)state;
    struct timeval tv;
    float dt;
    int i;

    gettimeofday(&tv, NULL);
    dt = (float)(tv.tv_sec - bg->last_update.tv_sec) +
         (float)(tv.tv_usec - bg->last_update.tv_usec) / 1000000.0f;
    bg->last_update = tv;

    if (dt > 0.1f) dt = 0.1f;

    /* Update stars */
    for (i = 0; i < 128; i++) {
        bg->stars[i].x += bg->stars[i].speed * dt;
        if (bg->stars[i].x > width) {
            bg->stars[i].x = 0;
            bg->stars[i].y = (float)(rand() % (height > 0 ? height : 1));
        }
    }

    /* Update shooting star */
    if (bg->shooter.active) {
        float move = bg->shooter.speed * dt;
        bg->shooter.x += move;
        bg->shooter.y += move;

        if (bg->shooter.x > width || bg->shooter.y > height) {
            bg->shooter.active = 0;
        }
    } else {
        if ((rand() % 1000) < 5) { /* ~0.5% chance per update */
            bg->shooter.active = 1;
            bg->shooter.x = (float)(rand() % (width > 0 ? width : 1));
            bg->shooter.y = 0;
            bg->shooter.speed = 300.0f + (float)(rand() % 200);
            bg->shooter.tail_len = 50.0f + (float)(rand() % 50);
        }
    }
}

/*
 * Draw Night Background
 */
static void
night_bg_draw(void *state)
{
    NightBg *bg = (NightBg*)state;
    int i;
    float br = app.settings.brightness;

    glDisable(GL_TEXTURE_2D);

    glBegin(GL_POINTS);
    for (i = 0; i < 128; i++) {
        float b = bg->stars[i].brightness;
        glColor4f(bg->stars[i].r * br, bg->stars[i].g * br, bg->stars[i].b * br, b);
        glVertex2f(bg->stars[i].x, bg->stars[i].y);

        if (bg->stars[i].type == 1) {
            glVertex2f(bg->stars[i].x + 1, bg->stars[i].y);
            glVertex2f(bg->stars[i].x - 1, bg->stars[i].y);
            glVertex2f(bg->stars[i].x, bg->stars[i].y + 1);
            glVertex2f(bg->stars[i].x, bg->stars[i].y - 1);
        }
    }
    glEnd();

    if (bg->shooter.active) {
        glBegin(GL_LINES);
        glColor4f(1.0f * br, 1.0f * br, 0.8f * br, 0.8f);
        glVertex2f(bg->shooter.x, bg->shooter.y);
        glColor4f(1.0f * br, 1.0f * br, 0.8f * br, 0.0f);
        glVertex2f(bg->shooter.x - bg->shooter.tail_len,
                   bg->shooter.y - bg->shooter.tail_len);
        glEnd();

        /* Beefier Head (Cross) */
        glBegin(GL_POINTS);
        glColor4f(1.0f * br, 1.0f * br, 1.0f * br, 1.0f);
        glVertex2f(bg->shooter.x + 1, bg->shooter.y);
        glVertex2f(bg->shooter.x - 1, bg->shooter.y);
        glVertex2f(bg->shooter.x, bg->shooter.y + 1);
        glVertex2f(bg->shooter.x, bg->shooter.y - 1);
        glEnd();
    }

    glEnable(GL_TEXTURE_2D);
}
