#ifndef DASH_H
#define DASH_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeB.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <Xm/ScrollBar.h>
#ifdef __sgi
#include <X11/GLw/GLwMDrawA.h>
#else
#include <GL/GLwMDrawA.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include "libtsm.h"
#include "shl-pty.h"

#define MAX_TABS 10
#define GLYPH_SCALE 4  /* Render glyphs at 4x for better quality */
#define NUM_GLYPHS 256 /* ASCII + extended */
#define BOLD_ALPHA 0.5f
#define BLINK_INTERVAL_MS 200
#define REDRAW_LATENCY_MS 20
#define VERTEX_ADJ 0.0

#ifdef __sgi
#define DEFAULT_REDRAW_INTERVAL 200
#else
#define DEFAULT_REDRAW_INTERVAL 16
#endif

/* Attribute flags */
#define ATTR_BOLD      0x01
#define ATTR_UNDERLINE 0x02
#define ATTR_ITALIC    0x04
#define ATTR_INVERSE   0x08
#define ATTR_BLINK     0x10
#define ATTR_TEXT      0x20

/* Glyph texture coordinates in atlas */
typedef struct {
    float u0, v0;  /* Top-left texture coordinates */
    float u1, v1;  /* Bottom-right texture coordinates */
    int width;     /* Glyph width in pixels */
    int height;    /* Glyph height in pixels */
} GlyphCoords;

/* Vertex structure for readability */
typedef struct {
    GLfloat x, y;
} Vertex2D;

/* Texture coordinate structure */
typedef struct {
    GLfloat u, v;
} TexCoord;

/* Color structure */
typedef struct {
    GLfloat r, g, b, a;
} Color;

/* Font configuration with texture atlas */
typedef struct {
    XFontStruct *font;
    int char_width;      /* Character cell width */
    int char_height;     /* Character cell height */
    int ascent;
    int descent;
    GLuint atlas_texture; /* Single texture containing all glyphs */
    int atlas_width;      /* Texture atlas width */
    int atlas_height;     /* Texture atlas height */
    GlyphCoords glyphs[NUM_GLYPHS]; /* Texture coords for each glyph */
} FontInfo;

/* Vertex arrays for batch rendering (one quad per character cell) */
typedef struct {
    int cols;              /* Terminal columns */
    int rows;              /* Terminal rows */
    int num_quads;         /* Total quads (cols * rows) */
    Vertex2D *vertices;    /* Vertex positions - constant after resize */
    TexCoord *texcoords;   /* Texture coordinates - updated per frame */
    Color *fg_colors;      /* Foreground colors - updated per frame */
    Color *bg_colors;      /* Background colors - updated per frame */
    unsigned char *row_bg_dirty; /* 1 if row has non-default bg */
    int screen_bg_dirty;   /* 1 if any part of screen has non-default bg */
    struct tsm_screen_attr def_attr; /* Default attributes for current frame */
    int cursor_x;          /* Calculated cursor X for current frame */
    int cursor_y;          /* Calculated cursor Y for current frame */
    uint32_t *chars;       /* Character code per cell */
    unsigned char *attrs;  /* Attributes per cell */
    unsigned char *row_attr_usage; /* Attributes used in row */
    int screen_attr_usage; /* Attributes used in screen */
} VertexArrays;

/* Terminal tab structure */
typedef struct {
    Widget tab_button;
    struct tsm_screen *screen;
    struct tsm_vte *vte;
    struct shl_pty *pty;
    XtInputId pty_input_id;
    int active;
    char *title;
    int is_console;
} DashTab;

/* Application settings resource structure */
typedef struct {
    String font_family;
    int font_size;
    String dynamic_background;
    String cursor_color;
    int scrollback_lines;
    Boolean console_mode;
    float brightness;
    String theme;
    String foreground;
    String background;
    String colors[16];
    int redraw_interval;
} AppSettings;

/* Application resources */
typedef struct {
    Widget toplevel;
    Widget main_window;
    Widget menubar;
    Widget tab_bar;
    Widget term_form;
    Widget gl_area;
    Widget scrollbar;
    GLXContext gl_context;
    XtAppContext app_context;
    DashTab *tabs[MAX_TABS];
    int num_tabs;
    int active_tab;
    FontInfo font_info;
    VertexArrays vertex_arrays;
    Display *display;
    Atom atom_clipboard;
    int own_primary;
    int own_clipboard;
    Color cursor_color;
    int sb_cached_max;
    int sb_cached_slider;
    int sb_cached_val;
    int selecting;
    int sel_last_x;
    int sel_last_y;
    char *selection_text;
    XtIntervalId redraw_timer_id;
    int timer_is_fast;
    int width;
    int height;
    AppSettings settings;
    void *bg_state;
    void (*bg_init)(int width, int height);
    void (*bg_update)(void *state, int width, int height);
    void (*bg_draw)(void *state);
    void (*bg_cleanup)(void *state);
    int zoom_level;
    int base_font_size;
    char *osd_text;
    struct timeval osd_start_time;
    int dirty;
} DashApp;

extern DashApp app;

/* Background prototypes */
void night_bg_init(int width, int height);
void sunset_bg_init(int width, int height);
void ocean_bg_init(int width, int height);
void aurora_bg_init(int width, int height);
void embers_bg_init(int width, int height);
void static_bg_init(int width, int height);
int dash_set_theme(struct tsm_vte *vte, const char *theme_name);

#endif /* DASH_H */
