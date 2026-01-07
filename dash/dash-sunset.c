#include "dash.h"

#define SUNSET_BRIGHTNESS 0.25f
#define GRID_SPEED 0.5f

typedef struct {
    float offset;
    struct timeval last_update;
} SunsetBg;

static void sunset_bg_update(void *state, int width, int height);
static void sunset_bg_draw(void *state);

/*
 * Initialize Sunset Background
 */
void
sunset_bg_init(int width, int height)
{
    SunsetBg *bg;

    if (app.bg_state) {
        free(app.bg_state);
    }

    bg = calloc(1, sizeof(SunsetBg));
    gettimeofday(&bg->last_update, NULL);

    app.bg_state = bg;
    app.bg_update = sunset_bg_update;
    app.bg_draw = sunset_bg_draw;
}

static void
sunset_bg_update(void *state, int width, int height)
{
    SunsetBg *bg = (SunsetBg*)state;
    struct timeval tv;
    float dt;

    gettimeofday(&tv, NULL);
    dt = (float)(tv.tv_sec - bg->last_update.tv_sec) +
         (float)(tv.tv_usec - bg->last_update.tv_usec) / 1000000.0f;
    bg->last_update = tv;

    if (dt > 0.1f) dt = 0.1f;

    /* Move grid towards user */
    bg->offset += GRID_SPEED * dt;
    if (bg->offset > 1.0f) bg->offset -= 1.0f;
}

static void
sunset_bg_draw(void *state)
{
    SunsetBg *bg = (SunsetBg*)state;
    int w = app.width;
    int h = app.height;
    float b = SUNSET_BRIGHTNESS * app.settings.brightness;
    float br = app.settings.brightness;
    int i;

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glShadeModel(GL_SMOOTH);

    glBegin(GL_QUAD_STRIP);

    /* Top: Black */
    glColor3f(0.0f, 0.0f, 0.0f);
    glVertex2i(0, 0);
    glVertex2i(w, 0);

    /* Upper Sky: Dark Blue */
    glColor3f(0.0f, 0.0f, 0.5f * b);
    glVertex2i(0, h * 0.4);
    glVertex2i(w, h * 0.4);

    /* Mid Sky: Magenta */
    glColor3f(0.8f * b, 0.0f, 0.5f * b);
    glVertex2i(0, h * 0.65);
    glVertex2i(w, h * 0.65);

    /* Horizon: Orange */
    glColor3f(0.9f * b, 0.4f * b, 0.0f);
    glVertex2i(0, h * 0.80);
    glVertex2i(w, h * 0.80);

    /* Bottom: Black (Ground) */
    glColor3f(0.0f, 0.0f, 0.0f);
    glVertex2i(0, h);
    glVertex2i(w, h);

    glEnd();

    /* Draw Perspective Grid */
    glEnable(GL_BLEND);
    glBegin(GL_LINES);

    /* Vertical lines (radiating from center horizon) */
    float cx = w * 0.5f;
    float horizon_y = h * 0.80f;
    float spacing = w / 8.0f;
    
    for (i = -10; i <= 10; i++) {
        float x_bottom = cx + i * spacing;
        
        /* Fade out near horizon */
        glColor4f(1.0f * br, 1.0f * br, 1.0f * br, 0.0f);
        glVertex2f(cx, horizon_y);
        glColor4f(1.0f * br, 1.0f * br, 1.0f * br, 0.1f);
        glVertex2f(x_bottom, h);
    }

    /* Horizontal lines (scrolling perspective) */
    float ground_h = h - horizon_y;
    /* z goes from 1.0 (bottom) to infinity (horizon) */
    for (float z = 1.0f - bg->offset; z < 20.0f; z += 1.0f) {
        if (z < 0.1f) continue;
        float y = horizon_y + ground_h / z;
        float alpha = 0.2f / z;
        
        glColor4f(1.0f * br, 1.0f * br, 1.0f * br, alpha);
        glVertex2f(0, y);
        glVertex2f(w, y);
    }
    glEnd();

    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
}
