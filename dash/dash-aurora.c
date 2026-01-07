#include "dash.h"

#define NUM_LAYERS 3
#define POINTS_PER_LAYER 40
#define AURORA_ALPHA 0.15f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    float speed;
    float offset;
    float height;
    float y_base;
    float period;
    float r, g, b;
} AuroraLayer;

typedef struct {
    AuroraLayer layers[NUM_LAYERS];
    struct timeval last_update;
} AuroraBg;

static void aurora_bg_update(void *state, int width, int height);
static void aurora_bg_draw(void *state);

/*
 * Initialize Aurora Background
 */
void
aurora_bg_init(int width, int height)
{
    AuroraBg *bg;

    if (app.bg_state) {
        free(app.bg_state);
    }

    bg = calloc(1, sizeof(AuroraBg));
    gettimeofday(&bg->last_update, NULL);

    /* Layer 1: Green/Teal */
    bg->layers[0].r = 0.0f; bg->layers[0].g = 0.8f; bg->layers[0].b = 0.6f;
    bg->layers[0].y_base = height * 0.2f;
    bg->layers[0].height = height * 0.4f;
    bg->layers[0].period = 1.0f;
    bg->layers[0].speed = 10.0f;
    bg->layers[0].offset = (float)(rand() % 100);

    /* Layer 2: Purple/Blue */
    bg->layers[1].r = 0.4f; bg->layers[1].g = 0.0f; bg->layers[1].b = 0.8f;
    bg->layers[1].y_base = height * 0.3f;
    bg->layers[1].height = height * 0.35f;
    bg->layers[1].period = 1.5f;
    bg->layers[1].speed = -8.0f;
    bg->layers[1].offset = (float)(rand() % 100);

    /* Layer 3: Deep Blue */
    bg->layers[2].r = 0.0f; bg->layers[2].g = 0.2f; bg->layers[2].b = 0.9f;
    bg->layers[2].y_base = height * 0.4f;
    bg->layers[2].height = height * 0.3f;
    bg->layers[2].period = 2.0f;
    bg->layers[2].speed = 5.0f;
    bg->layers[2].offset = (float)(rand() % 100);

    app.bg_state = bg;
    app.bg_update = aurora_bg_update;
    app.bg_draw = aurora_bg_draw;
}

static void
aurora_bg_update(void *state, int width, int height)
{
    AuroraBg *bg = (AuroraBg*)state;
    struct timeval tv;
    float dt;
    int i;

    gettimeofday(&tv, NULL);
    dt = (float)(tv.tv_sec - bg->last_update.tv_sec) +
         (float)(tv.tv_usec - bg->last_update.tv_usec) / 1000000.0f;
    bg->last_update = tv;

    if (dt > 0.1f) dt = 0.1f;

    for (i = 0; i < NUM_LAYERS; i++) {
        bg->layers[i].offset += bg->layers[i].speed * dt;
    }
}

static void
aurora_bg_draw(void *state)
{
    AuroraBg *bg = (AuroraBg*)state;
    int i, j;
    int w = app.width;
    float br = app.settings.brightness;

    glDisable(GL_TEXTURE_2D);
    glShadeModel(GL_SMOOTH);

    for (i = 0; i < NUM_LAYERS; i++) {
        AuroraLayer *l = &bg->layers[i];
        glBegin(GL_QUAD_STRIP);
        for (j = 0; j <= POINTS_PER_LAYER; j++) {
            float x = (float)j * w / POINTS_PER_LAYER;
            float wave = sinf(x * 0.01f * l->period + l->offset * 0.05f) * 30.0f;
            float y = l->y_base + wave;

            glColor4f(l->r * br, l->g * br, l->b * br, 0.0f);
            glVertex2f(x, y);

            glColor4f(l->r * br, l->g * br, l->b * br, AURORA_ALPHA);
            glVertex2f(x, y + l->height);
        }
        glEnd();
    }

    glEnable(GL_TEXTURE_2D);
}
