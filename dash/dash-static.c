/* /mnt/files.nfs/share/project/libtsm/dash/dash-static.c */
#include "dash.h"

#define STATIC_TEX_SIZE 64
#define STATIC_GLYPH_ROWS 4
#define STATIC_GLYPH_COLS 4
#define STATIC_GLYPH_SIZE (STATIC_TEX_SIZE / STATIC_GLYPH_ROWS)

typedef struct {
    GLuint texture;
    TexCoord *texcoords;
    int num_quads;
} StaticBg;

static void static_bg_update(void *state, int width, int height);
static void static_bg_draw(void *state);
static void static_bg_cleanup(void *state);

/*
 * Initialize Static Background
 */
void
static_bg_init(int width, int height)
{
    StaticBg *bg;
    unsigned char *data;
    int i;

    /* Handle re-initialization (resize) */
    if (app.bg_state) {
        if (app.bg_cleanup) {
            app.bg_cleanup(app.bg_state);
        } else {
            free(app.bg_state);
        }
        app.bg_state = NULL;
    }

    bg = calloc(1, sizeof(StaticBg));
    
    /* Create texture with random noise */
    data = malloc(STATIC_TEX_SIZE * STATIC_TEX_SIZE);
    for (i = 0; i < STATIC_TEX_SIZE * STATIC_TEX_SIZE; i++) {
        data[i] = rand() % 256;
    }

    glGenTextures(1, &bg->texture);
    glBindTexture(GL_TEXTURE_2D, bg->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 
                 STATIC_TEX_SIZE, STATIC_TEX_SIZE, 
                 0, GL_ALPHA, GL_UNSIGNED_BYTE, data);
    free(data);

    /* Allocate texture coordinates array matching the vertex buffer size */
    bg->num_quads = app.vertex_arrays.num_quads;
    bg->texcoords = malloc(bg->num_quads * 4 * sizeof(TexCoord));

    app.bg_state = bg;
    app.bg_update = static_bg_update;
    app.bg_draw = static_bg_draw;
    app.bg_cleanup = static_bg_cleanup;
}

static void
static_bg_cleanup(void *state)
{
    StaticBg *bg = (StaticBg*)state;
    if (bg) {
        if (bg->texture) glDeleteTextures(1, &bg->texture);
        if (bg->texcoords) free(bg->texcoords);
        free(bg);
    }
}

static void
static_bg_update(void *state, int width, int height)
{
    StaticBg *bg = (StaticBg*)state;
    int i;
    float glyph_uv_w = 1.0f / STATIC_GLYPH_COLS;
    float glyph_uv_h = 1.0f / STATIC_GLYPH_ROWS;

    /* Reallocate if screen size changed */
    if (bg->num_quads != app.vertex_arrays.num_quads) {
        bg->num_quads = app.vertex_arrays.num_quads;
        free(bg->texcoords);
        bg->texcoords = malloc(bg->num_quads * 4 * sizeof(TexCoord));
    }

    if (!bg->texcoords) return;

    /* Randomize texture coordinates for each cell */
    for (i = 0; i < bg->num_quads; i++) {
        int glyph_idx = rand() % (STATIC_GLYPH_ROWS * STATIC_GLYPH_COLS);
        int col = glyph_idx % STATIC_GLYPH_COLS;
        int row = glyph_idx / STATIC_GLYPH_COLS;

        float u0 = col * glyph_uv_w;
        float v0 = row * glyph_uv_h;
        float u1 = u0 + glyph_uv_w;
        float v1 = v0 + glyph_uv_h;

        /* Random flip */
        if (rand() % 2) { float t = u0; u0 = u1; u1 = t; }
        if (rand() % 2) { float t = v0; v0 = v1; v1 = t; }

        bg->texcoords[i*4 + 0].u = u0;
        bg->texcoords[i*4 + 0].v = v0;
        bg->texcoords[i*4 + 1].u = u1;
        bg->texcoords[i*4 + 1].v = v0;
        bg->texcoords[i*4 + 2].u = u1;
        bg->texcoords[i*4 + 2].v = v1;
        bg->texcoords[i*4 + 3].u = u0;
        bg->texcoords[i*4 + 3].v = v1;
    }
}

static void
static_bg_draw(void *state)
{
    StaticBg *bg = (StaticBg*)state;
    float br = app.settings.brightness;

    if (!bg->texcoords) return;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, bg->texture);
    
    /* Save client state to restore later */
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);

    /* Use existing vertex array but our new texcoords */
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY); /* We'll use constant color but array is enabled globally */
    
    /* We must disable color array to use constant color, or fill a color array.
       Since app.vertex_arrays.fg_colors is enabled in setup_vertex_arrays, 
       we should disable the array client state to use glColor4f */
    glDisableClientState(GL_COLOR_ARRAY);

    glVertexPointer(2, GL_FLOAT, 0, app.vertex_arrays.vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, bg->texcoords);
    
    glColor4f(1.0f, 1.0f, 1.0f, 0.05f * br); /* Faint static */

    glDrawArrays(GL_QUADS, 0, bg->num_quads * 4);

    /* Restore state */
    glPopClientAttrib();
    
    /* Restore the global pointers that dash.c expects */
    glVertexPointer(2, GL_FLOAT, 0, app.vertex_arrays.vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, app.vertex_arrays.texcoords);
    glColorPointer(4, GL_FLOAT, 0, app.vertex_arrays.fg_colors);
    glEnableClientState(GL_COLOR_ARRAY);
}
