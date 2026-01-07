/* /mnt/files.nfs/share/project/libtsm/dash/dash-themes.c */
#include "dash.h"

/* Helper to define colors */
#define C(r, g, b) { r, g, b }

static uint8_t active_palette[18][3];

/* Standard VGA */
static const uint8_t palette_vga[18][3] = {
    C(0x00, 0x00, 0x00), C(0xAA, 0x00, 0x00), C(0x00, 0xAA, 0x00), C(0xAA, 0x55, 0x00),
    C(0x00, 0x00, 0xAA), C(0xAA, 0x00, 0xAA), C(0x00, 0xAA, 0xAA), C(0xAA, 0xAA, 0xAA),
    C(0x55, 0x55, 0x55), C(0xFF, 0x55, 0x55), C(0x55, 0xFF, 0x55), C(0xFF, 0xFF, 0x55),
    C(0x55, 0x55, 0xFF), C(0xFF, 0x55, 0xFF), C(0x55, 0xFF, 0xFF), C(0xFF, 0xFF, 0xFF),
    C(0xAA, 0xAA, 0xAA), /* FG */
    C(0x00, 0x00, 0x00)  /* BG */
};

/* Desaturated VGA (Solarized-ish) */
static const uint8_t palette_desaturated[18][3] = {
    C(0x20, 0x20, 0x20), /* Black */
    C(0x90, 0x30, 0x30), /* Red */
    C(0x30, 0x90, 0x30), /* Green */
    C(0x90, 0x70, 0x20), /* Yellow */
    C(0x30, 0x30, 0x90), /* Blue */
    C(0x90, 0x30, 0x90), /* Magenta */
    C(0x30, 0x90, 0x90), /* Cyan */
    C(0xA0, 0xA0, 0xA0), /* White */
    C(0x60, 0x60, 0x60), /* Br Black */
    C(0xD0, 0x60, 0x60), /* Br Red */
    C(0x60, 0xD0, 0x60), /* Br Green */
    C(0xD0, 0xD0, 0x60), /* Br Yellow */
    C(0x60, 0x60, 0xD0), /* Br Blue */
    C(0xD0, 0x60, 0xD0), /* Br Magenta */
    C(0x60, 0xD0, 0xD0), /* Br Cyan */
    C(0xE0, 0xE0, 0xE0), /* Br White */
    C(0xA0, 0xA0, 0xA0), /* FG */
    C(0x20, 0x20, 0x20)  /* BG */
};

static void
dash_update_colors(struct tsm_vte *vte)
{
    XColor color;
    Colormap cmap = DefaultColormap(app.display, DefaultScreen(app.display));
    int i;

    /* Apply color overrides from resources */
    for (i = 0; i < 16; i++) {
        if (app.settings.colors[i]) {
            if (XParseColor(app.display, cmap, app.settings.colors[i], &color)) {
                active_palette[i][0] = color.red >> 8;
                active_palette[i][1] = color.green >> 8;
                active_palette[i][2] = color.blue >> 8;
            }
        }
    }

    if (app.settings.foreground) {
        if (XParseColor(app.display, cmap, app.settings.foreground, &color)) {
            active_palette[16][0] = color.red >> 8;
            active_palette[16][1] = color.green >> 8;
            active_palette[16][2] = color.blue >> 8;
        }
    }

    if (app.settings.background) {
        if (XParseColor(app.display, cmap, app.settings.background, &color)) {
            active_palette[17][0] = color.red >> 8;
            active_palette[17][1] = color.green >> 8;
            active_palette[17][2] = color.blue >> 8;
        }
    }

    tsm_vte_set_custom_palette(vte, active_palette);
    tsm_vte_set_palette(vte, "custom");
}

int dash_set_theme(struct tsm_vte *vte, const char *theme_name)
{
    if (!vte || !theme_name) return -1;

    if (strcmp(theme_name, "vga-green") == 0) {
        memcpy(active_palette, palette_vga, sizeof(active_palette));
        /* Set FG to Bright Green */
        active_palette[16][0] = 0x55;
        active_palette[16][1] = 0xFF;
        active_palette[16][2] = 0x55;
    } else if (strcmp(theme_name, "solarized") == 0) {
        memcpy(active_palette, palette_desaturated, sizeof(active_palette));
    } else if (strcmp(theme_name, "solarized-light") == 0) {
        memcpy(active_palette, palette_desaturated, sizeof(active_palette));
        /* Swap FG/BG (approx) */
        /* BG = White (index 15) */
        active_palette[17][0] = 0xE0;
        active_palette[17][1] = 0xE0;
        active_palette[17][2] = 0xE0;
        /* FG = Black (index 0) */
        active_palette[16][0] = 0x20;
        active_palette[16][1] = 0x20;
        active_palette[16][2] = 0x20;
    } else if (strcmp(theme_name, "vga") == 0) {
        memcpy(active_palette, palette_vga, sizeof(active_palette));
    } else {
        /* Fallback to libtsm built-in themes (legacy, vga, etc) */
        /* But we want to support overrides, so we load VGA as base */
        memcpy(active_palette, palette_vga, sizeof(active_palette));
    }

    /* Apply overrides from settings */
    dash_update_colors(vte);

    return 0;
}
