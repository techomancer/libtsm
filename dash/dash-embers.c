#include "dash.h"

#define MAX_EMBERS 150

typedef struct {
    float x, y;
    float speed_y;
    float drift_amp;
    float drift_speed;
    float drift_phase;
    float life;      /* 1.0 = full, 0.0 = dead */
    float decay;
    float r, g, b;
} Ember;

typedef struct {
    Ember embers[MAX_EMBERS];
    struct timeval last_update;
} EmbersBg;

static void embers_bg_update(void *state, int width, int height);
static void embers_bg_draw(void *state);

static void
spawn_ember(Ember *e, int width, int height, int initial)
{
    e->x = (float)(rand() % (width > 0 ? width : 1));
    if (initial) {
        e->y = (float)(rand() % (height > 0 ? height : 1));
        e->life = (float)(rand() % 100) / 100.0f;
    } else {
        e->y = (float)height + (float)(rand() % 20);
        e->life = 1.0f;
    }
    
    e->speed_y = 20.0f + (float)(rand() % 40); /* 20-60 px/sec upwards */
    e->drift_amp = 5.0f + (float)(rand() % 15);
    e->drift_speed = 1.0f + (float)(rand() % 20) / 10.0f;
    e->drift_phase = (float)(rand() % 628) / 100.0f;
    e->decay = 0.1f + (float)(rand() % 30) / 100.0f; /* Life decay per second */

    /* Warm colors: Red to Yellow */
    /* R: 1.0, G: 0.2-0.8, B: 0.0-0.2 */
    e->r = 1.0f;
    e->g = 0.2f + (float)(rand() % 60) / 100.0f;
    e->b = (float)(rand() % 20) / 100.0f;
}

void
embers_bg_init(int width, int height)
{
    EmbersBg *bg;
    int i;

    if (app.bg_state) {
        free(app.bg_state);
    }

    bg = calloc(1, sizeof(EmbersBg));
    gettimeofday(&bg->last_update, NULL);

    for (i = 0; i < MAX_EMBERS; i++) {
        spawn_ember(&bg->embers[i], width, height, 1);
    }

    app.bg_state = bg;
    app.bg_update = embers_bg_update;
    app.bg_draw = embers_bg_draw;
}

static void
embers_bg_update(void *state, int width, int height)
{
    EmbersBg *bg = (EmbersBg*)state;
    struct timeval tv;
    float dt;
    int i;

    gettimeofday(&tv, NULL);
    dt = (float)(tv.tv_sec - bg->last_update.tv_sec) +
         (float)(tv.tv_usec - bg->last_update.tv_usec) / 1000000.0f;
    bg->last_update = tv;

    if (dt > 0.1f) dt = 0.1f;

    for (i = 0; i < MAX_EMBERS; i++) {
        Ember *e = &bg->embers[i];
        e->y -= e->speed_y * dt;
        e->life -= e->decay * dt;
        e->drift_phase += e->drift_speed * dt;

        if (e->y < -10.0f || e->life <= 0.0f) {
            spawn_ember(e, width, height, 0);
        }
    }
}

static void
embers_bg_draw(void *state)
{
    EmbersBg *bg = (EmbersBg*)state;
    int i;
    float br = app.settings.brightness;

    glDisable(GL_TEXTURE_2D);
    glShadeModel(GL_SMOOTH);
    glBegin(GL_LINES);
    for (i = 0; i < MAX_EMBERS; i++) {
        Ember *e = &bg->embers[i];
        float x = e->x + sinf(e->drift_phase) * e->drift_amp;

        /* Calculate velocity to orient tail */
        float vx = e->drift_amp * e->drift_speed * cosf(e->drift_phase);
        float vy = -e->speed_y;
        float scale = 0.15f;

        glColor4f(e->r * br, e->g * br, e->b * br, e->life);
        glVertex2f(x, e->y);
        glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
        glVertex2f(x - vx * scale, e->y - vy * scale);
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
}
