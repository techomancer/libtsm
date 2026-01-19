/*
 * dash - Modern IRIX Terminal Emulator
 * Named after a very good cat
 *
 * Built with Motif 1.2 and OpenGL on IRIX
 */

#include "dash.h"

#define ATTR_BOXDRAW 0x40

DashApp app;

/* Callback prototypes */
static void quit_cb(Widget w, XtPointer client_data, XtPointer call_data);
static void new_tab_cb(Widget w, XtPointer client_data, XtPointer call_data);
static void new_console_cb(Widget w, XtPointer client_data, XtPointer call_data);
static void close_tab_cb(Widget w, XtPointer client_data, XtPointer call_data);
static void tab_button_cb(Widget w, XtPointer client_data, XtPointer call_data);
static void bg_menu_cb(Widget w, XtPointer client_data, XtPointer call_data);
static void theme_menu_cb(Widget w, XtPointer client_data, XtPointer call_data);
static void gl_init_cb(Widget w, XtPointer client_data, XtPointer call_data);
static void gl_resize_cb(Widget w, XtPointer client_data, XtPointer call_data);
static void gl_expose_cb(Widget w, XtPointer client_data, XtPointer call_data);
static void input_event_handler(Widget w, XtPointer client_data, XEvent *event, Boolean *cont);
static void scrollbar_cb(Widget w, XtPointer client_data, XtPointer call_data);
static Boolean convert_selection_cb(Widget w, Atom *selection, Atom *target,
                                    Atom *type, XtPointer *value,
                                    unsigned long *length, int *format);
static void lose_selection_cb(Widget w, Atom *selection);
static void paste_selection_cb(Widget w, XtPointer client_data, Atom *selection,
                               Atom *type, XtPointer value,
                               unsigned long *length, int *format);
static void update_scrollbar(void);
static void update_tab_visibility(void);
static void close_tab(int tab_index);
static void pty_input_cb(XtPointer client_data, int *source, XtInputId *id);

/* Forward declarations */
static void switch_to_tab(int tab_index);
static void render_frame(void);
static void redraw_terminal(void);
static int load_font(const char *font_name, int font_size);
static int next_power_of_2(int n);
static void setup_gl_viewport(void);
static void setup_vertex_arrays(int width, int height);
static void free_vertex_arrays(void);
static int build_font_atlas(XFontStruct *scaled_font, int actual_scale);
static void update_zoom(int step);
static void resize_screens(void);
static void show_osd(const char *text);
static void draw_osd(void);

/* TSM callbacks */
static void tsm_log_cb(void *data, const char *file, int line,
                       const char *fn, const char *subs,
                       unsigned int sev, const char *format,
                       va_list args);
static void tsm_write_cb(struct tsm_vte *vte, const char *u8, size_t len,
                         void *data);
static int tsm_draw_cb(struct tsm_screen *screen, uint64_t id,
                       const uint32_t *ch, size_t len, unsigned int cwidth,
                       unsigned int posx, unsigned int posy,
                       const struct tsm_screen_attr *attr,
                       tsm_age_t age, void *data);
static void tsm_osc_cb(struct tsm_vte *vte, const char *u8, size_t len,
                       void *data);

/* PTY callbacks */
static void pty_read_cb(struct shl_pty *pty, void *data,
                        char *u8, size_t len);

/*
 * TSM log callback - print log messages to stderr
 */
static void
tsm_log_cb(void *data, const char *file, int line,
           const char *fn, const char *subs,
           unsigned int sev, const char *format, va_list args)
{
    fprintf(stderr, "dash-tsm: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

/*
 * TSM write callback - write data to PTY
 */
static void
tsm_write_cb(struct tsm_vte *vte, const char *u8, size_t len, void *data)
{
    DashTab *tab = (DashTab *)data;

    if (tab && tab->pty) {
        shl_pty_write(tab->pty, u8, len);
        shl_pty_dispatch(tab->pty);
    }
}

/*
 * TSM draw callback - render a single cell
 */
static int
tsm_draw_cb(struct tsm_screen *screen, uint64_t id,
            const uint32_t *ch, size_t len, unsigned int cwidth,
            unsigned int posx, unsigned int posy,
            const struct tsm_screen_attr *attr,
            tsm_age_t age, void *data)
{
    int i, idx;
    GlyphCoords *glyph;
    Color fg, bg;
    unsigned char c;
    int inverse;
    unsigned char cell_attr = 0;
    int cell_idx;

    /* Ignore wide characters for now - just use first char */
    if (len == 0 || !ch) {
        c = ' ';
    } else {
        /* Map unicode to ASCII for now */
        if (ch[0] < 256) {
            c = (unsigned char)ch[0];
        } else if (ch[0] >= 0x2500 && ch[0] <= 0x257F) {
            c = ' ';
            cell_attr |= ATTR_BOXDRAW;
            cell_attr &= ~(ATTR_BOLD | ATTR_ITALIC | ATTR_UNDERLINE);
        } else {
            c = '?';  /* Replacement character for unmapped unicode */
        }
    }

    /* Calculate vertex array index (4 vertices per quad) */
    cell_idx = posy * app.vertex_arrays.cols + posx;
    idx = cell_idx * 4;

    /* Calculate attributes */
    if (attr->bold)      cell_attr |= ATTR_BOLD;
    if (attr->underline) cell_attr |= ATTR_UNDERLINE;
    if (attr->italic)    cell_attr |= ATTR_ITALIC;
    if (attr->inverse)   cell_attr |= ATTR_INVERSE;
    if (attr->blink)     cell_attr |= ATTR_BLINK;
    if (c != ' ') cell_attr |= ATTR_TEXT;

    /* Store cell data */
    if (cell_attr & ATTR_BOXDRAW) {
        app.vertex_arrays.chars[cell_idx] = ch[0];
    } else {
        app.vertex_arrays.chars[cell_idx] = (uint32_t)c;
    }
    app.vertex_arrays.attrs[cell_idx] = cell_attr;

    /* Update usage flags */
    app.vertex_arrays.row_attr_usage[posy] |= cell_attr;
    app.vertex_arrays.screen_attr_usage |= cell_attr;

    /* If italic, replace with space in main pass (drawn in 3rd pass) */
    if (cell_attr & ATTR_ITALIC) {
        c = ' ';
    }
    
    /* Get glyph texture coordinates */
    glyph = &app.font_info.glyphs[c];

    /* Set texture coordinates for this quad (TL, TR, BR, BL) */
    app.vertex_arrays.texcoords[idx + 0].u = glyph->u0;
    app.vertex_arrays.texcoords[idx + 0].v = glyph->v0;

    app.vertex_arrays.texcoords[idx + 1].u = glyph->u1;
    app.vertex_arrays.texcoords[idx + 1].v = glyph->v0;

    app.vertex_arrays.texcoords[idx + 2].u = glyph->u1;
    app.vertex_arrays.texcoords[idx + 2].v = glyph->v1;

    app.vertex_arrays.texcoords[idx + 3].u = glyph->u0;
    app.vertex_arrays.texcoords[idx + 3].v = glyph->v1;

    /* Convert 8-bit RGB to float */
    fg.r = attr->fr / 255.0f;
    fg.g = attr->fg / 255.0f;
    fg.b = attr->fb / 255.0f;
    fg.a = 1.0f;

    bg.r = attr->br / 255.0f;
    bg.g = attr->bg / 255.0f;
    bg.b = attr->bb / 255.0f;
    bg.a = 1.0f;

    inverse = attr->inverse;

    /* Disable libtsm's cursor inversion so we can render our own */
    if (posx == app.vertex_arrays.cursor_x &&
        posy == app.vertex_arrays.cursor_y &&
        !(tsm_screen_get_flags(screen) & TSM_SCREEN_HIDE_CURSOR)) {
        inverse = !inverse;
    }

    /* Handle inverse video (selection or SGR 7) */
    if (inverse) {
        Color tmp = fg;
        fg = bg;
        bg = tmp;
    }

    /* Check if background matches default */
    /* We must check the effective background color (which might be the foreground if inverted) */
    if ((inverse ? attr->fr : attr->br) != app.vertex_arrays.def_attr.br ||
        (inverse ? attr->fg : attr->bg) != app.vertex_arrays.def_attr.bg ||
        (inverse ? attr->fb : attr->bb) != app.vertex_arrays.def_attr.bb) {
        app.vertex_arrays.row_bg_dirty[posy] = 1;
        app.vertex_arrays.screen_bg_dirty = 1;
    }

    /* Set foreground colors for all 4 vertices */
    for (i = 0; i < 4; i++) {
        app.vertex_arrays.fg_colors[idx + i] = fg;
        app.vertex_arrays.bg_colors[idx + i] = bg;
    }

    return 0;
}

/*
 * Base64 decoding helper
 */
static char *
base64_decode(const char *input, size_t len)
{
    static const int decoding_table[] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
    };
    
    size_t out_len = (len * 3) / 4;
    char *out = malloc(out_len + 1);
    char *p = out;
    int val = 0, valb = -8;

    if (!out) return NULL;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = input[i];
        if (c > 127 || decoding_table[c] == -1) continue;
        
        val = (val << 6) | decoding_table[c];
        valb += 6;
        
        if (valb >= 0) {
            *p++ = (char)((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    *p = 0;
    return out;
}

/*
 * Helper to set clipboard text from OSC 52
 */
static void
set_clipboard_text(const char *text, int primary, int clipboard)
{
    if (app.selection_text) free(app.selection_text);
    app.selection_text = strdup(text);
    
    if (primary) {
        app.own_primary = 1;
        XtOwnSelection(app.toplevel, XA_PRIMARY, CurrentTime,
                       convert_selection_cb, lose_selection_cb, NULL);
    }
    if (clipboard) {
        app.own_clipboard = 1;
        XtOwnSelection(app.toplevel, app.atom_clipboard, CurrentTime,
                       convert_selection_cb, lose_selection_cb, NULL);
    }
}

/*
 * TSM OSC callback - handle escape sequences (like window title)
 */
static void
tsm_osc_cb(struct tsm_vte *vte, const char *val, size_t len, void *data)
{
    DashTab *tab = (DashTab *)data;
    char *title;
    int cmd = 0;
    size_t i = 0;
    size_t j;

    /* Parse command number */
    while (i < len && val[i] >= '0' && val[i] <= '9') {
        cmd = cmd * 10 + (val[i] - '0');
        i++;
    }

    if (i < len && val[i] == ';') {
        i++; /* Skip semicolon */
    } else {
        return; /* Invalid format */
    }

    /* Handle 0 (Icon+Title) and 2 (Title) */
    if (cmd == 0 || cmd == 2) {
        title = malloc(len - i + 1);
        if (!title) return;

        for (j = 0; i < len; i++, j++) {
            title[j] = val[i];
        }
        title[j] = '\0';

        if (tab->title) free(tab->title);
        tab->title = title;

        /* Update tab button label */
        XmString xmstr = XmStringCreateLocalized(title);
        XtVaSetValues(tab->tab_button, XmNlabelString, xmstr, NULL);
        XmStringFree(xmstr);

        /* Update window title if active */
        if (tab->active) {
            char win_title[256];
            snprintf(win_title, sizeof(win_title), "dash: %s", title);
            XtVaSetValues(app.toplevel, XmNtitle, win_title, NULL);
        }
    }
    /* Handle 52 (Clipboard) */
    else if (cmd == 52) {
        int target_clipboard = 0;
        int target_primary = 0;
        
        /* Parse Pc (clipboard parameters) */
        /* Format: 52;Pc;Pd */
        /* Pc can contain 'c' (clipboard), 'p' (primary), 's' (selection/primary) */
        
        size_t start_pc = i;
        while (i < len && val[i] != ';') i++;
        
        /* Check flags in Pc */
        for (size_t k = start_pc; k < i; k++) {
            if (val[k] == 'c') target_clipboard = 1;
            if (val[k] == 'p' || val[k] == 's') target_primary = 1;
        }
        
        /* If no flags, default to clipboard (xterm behavior varies, but this is safe) */
        if (start_pc == i) target_clipboard = 1;

        if (i < len && val[i] == ';') i++; /* Skip semicolon */
        
        /* Remainder is Pd (Base64 data) */
        if (i < len) {
            char *decoded = base64_decode(val + i, len - i);
            if (decoded) {
                /* If data is "?", it's a query, but we only support setting for now */
                if (strcmp(decoded, "?") != 0) {
                    set_clipboard_text(decoded, target_primary, target_clipboard);
                    show_osd("COPIED");
                }
                free(decoded);
            }
        }
    }
}

/*
 * Close a terminal tab
 */
static void
close_tab(int tab_index)
{
    DashTab *tab;
    int i;

    if (tab_index < 0 || tab_index >= app.num_tabs) return;
    tab = app.tabs[tab_index];

    printf("dash: closing tab %d\n", tab_index);

    /* Cleanup resources */
    if (tab->pty_input_id) XtRemoveInput(tab->pty_input_id);
    if (tab->pty) shl_pty_unref(tab->pty);
    if (tab->vte) tsm_vte_unref(tab->vte);
    if (tab->screen) tsm_screen_unref(tab->screen);
    if (tab->tab_button) XtDestroyWidget(tab->tab_button);
    if (tab->title) free(tab->title);
    free(tab);

    /* Shift remaining tabs */
    for (i = tab_index; i < app.num_tabs - 1; i++) {
        app.tabs[i] = app.tabs[i+1];

        /* Update button callback data */
        XtRemoveAllCallbacks(app.tabs[i]->tab_button, XmNactivateCallback);
        XtAddCallback(app.tabs[i]->tab_button, XmNactivateCallback,
            tab_button_cb, (XtPointer)(uintptr_t)i);
    }
    app.tabs[app.num_tabs - 1] = NULL;
    app.num_tabs--;

    /* Handle active tab */
    if (app.num_tabs == 0) {
        exit(0);
    }

    if (app.active_tab == tab_index) {
        /* We closed the active tab */
        int new_active = tab_index;
        if (new_active >= app.num_tabs) new_active = app.num_tabs - 1;
        switch_to_tab(new_active);
    } else if (app.active_tab > tab_index) {
        /* Active tab shifted down */
        app.active_tab--;
    }

    update_tab_visibility();
}

/*
 * PTY read callback - feed data to VTE
 */
static void
pty_read_cb(struct shl_pty *pty, void *data, char *u8, size_t len)
{
    DashTab *tab = (DashTab *)data;

    if (tab && tab->vte) {
        /* Check for Bell */
        if (memchr(u8, '\a', len)) {
            if (tab->active) {
                show_osd("BELL");
            }
        }

        tsm_vte_input(tab->vte, u8, len);
        /* Request redraw */
        if (tab->active && app.gl_area) {
            redraw_terminal(); /* Schedule redraw instead of forcing it */
        }
    }
}

/*
 * X input callback for PTY fd
 */
static void
pty_input_cb(XtPointer client_data, int *source, XtInputId *id)
{
    DashTab *tab = (DashTab *)client_data;
    int r;

    if (tab && tab->pty) {
        r = shl_pty_dispatch(tab->pty);
        if (r != 0 && r != -EAGAIN) {
            int i;
            for (i = 0; i < app.num_tabs; i++) {
                if (app.tabs[i] == tab) {
                    close_tab(i);
                    return;
                }
            }
        }
    }
}

/*
 * Update tab bar visibility based on number of tabs
 */
static void
update_tab_visibility(void)
{
    if (!app.tab_bar || !app.term_form) return;

    if (app.num_tabs > 1) {
        if (!XtIsManaged(app.tab_bar)) {
            XtManageChild(app.tab_bar);
            XtVaSetValues(app.term_form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, app.tab_bar,
                NULL);
        }
    } else {
        if (XtIsManaged(app.tab_bar)) {
            XtUnmanageChild(app.tab_bar);
            XtVaSetValues(app.term_form,
                XmNtopAttachment, XmATTACH_FORM,
                NULL);
        }
    }
}

/*
 * Create the menu bar with File menu
 */
static Widget
create_menubar(Widget parent)
{
    Widget menubar, file_menu, bg_menu, theme_menu, static_item;
    Widget new_item, console_item, close_item, quit_item, aurora_item, embers_item;
    Widget none_item, night_item, sunset_item, ocean_item;
    Widget vga_item, vga_green_item, solarized_item, solarized_light_item, legacy_item;

    /* Create menu bar */
    menubar = XmCreateMenuBar(parent, "menubar", NULL, 0);

    /* Term menu */
    file_menu = XmCreatePulldownMenu(menubar, "termMenu", NULL, 0);

    XtVaCreateManagedWidget("Term",
        xmCascadeButtonWidgetClass, menubar,
        XmNsubMenuId, file_menu,
        NULL);

    /* New Terminal */
    new_item = XtVaCreateManagedWidget("New Terminal",
        xmPushButtonWidgetClass, file_menu,
        NULL);
    XtAddCallback(new_item, XmNactivateCallback, new_tab_cb, NULL);

    /* New Console */
    console_item = XtVaCreateManagedWidget("New Console",
        xmPushButtonWidgetClass, file_menu,
        NULL);
    XtAddCallback(console_item, XmNactivateCallback, new_console_cb, NULL);

    /* Close Tab */
    close_item = XtVaCreateManagedWidget("Close Tab",
        xmPushButtonWidgetClass, file_menu,
        NULL);
    XtAddCallback(close_item, XmNactivateCallback, close_tab_cb, NULL);

    /* Separator */
    XtVaCreateManagedWidget("sep",
        xmSeparatorWidgetClass, file_menu,
        NULL);

    /* Quit */
    quit_item = XtVaCreateManagedWidget("Quit",
        xmPushButtonWidgetClass, file_menu,
        NULL);
    XtAddCallback(quit_item, XmNactivateCallback, quit_cb, NULL);

    /* Theme menu */
    theme_menu = XmCreatePulldownMenu(menubar, "themeMenu", NULL, 0);

    XtVaCreateManagedWidget("Theme",
        xmCascadeButtonWidgetClass, menubar,
        XmNsubMenuId, theme_menu,
        NULL);

    vga_item = XtVaCreateManagedWidget("VGA", xmPushButtonWidgetClass, theme_menu, NULL);
    XtAddCallback(vga_item, XmNactivateCallback, theme_menu_cb, (XtPointer)"vga");

    vga_green_item = XtVaCreateManagedWidget("VGA Green", xmPushButtonWidgetClass, theme_menu, NULL);
    XtAddCallback(vga_green_item, XmNactivateCallback, theme_menu_cb, (XtPointer)"vga-green");

    solarized_item = XtVaCreateManagedWidget("Solarized", xmPushButtonWidgetClass, theme_menu, NULL);
    XtAddCallback(solarized_item, XmNactivateCallback, theme_menu_cb, (XtPointer)"solarized");

    solarized_light_item = XtVaCreateManagedWidget("Solarized Light", xmPushButtonWidgetClass, theme_menu, NULL);
    XtAddCallback(solarized_light_item, XmNactivateCallback, theme_menu_cb, (XtPointer)"solarized-light");

    legacy_item = XtVaCreateManagedWidget("Legacy", xmPushButtonWidgetClass, theme_menu, NULL);
    XtAddCallback(legacy_item, XmNactivateCallback, theme_menu_cb, (XtPointer)"legacy");

    /* Background menu */
    bg_menu = XmCreatePulldownMenu(menubar, "bgMenu", NULL, 0);

    XtVaCreateManagedWidget("Background",
        xmCascadeButtonWidgetClass, menubar,
        XmNsubMenuId, bg_menu,
        NULL);

    none_item = XtVaCreateManagedWidget("None",
        xmPushButtonWidgetClass, bg_menu, NULL);
    XtAddCallback(none_item, XmNactivateCallback, bg_menu_cb, (XtPointer)"none");

    night_item = XtVaCreateManagedWidget("Night",
        xmPushButtonWidgetClass, bg_menu, NULL);
    XtAddCallback(night_item, XmNactivateCallback, bg_menu_cb, (XtPointer)"night");

    sunset_item = XtVaCreateManagedWidget("Sunset",
        xmPushButtonWidgetClass, bg_menu, NULL);
    XtAddCallback(sunset_item, XmNactivateCallback, bg_menu_cb, (XtPointer)"sunset");

    ocean_item = XtVaCreateManagedWidget("Ocean",
        xmPushButtonWidgetClass, bg_menu, NULL);
    XtAddCallback(ocean_item, XmNactivateCallback, bg_menu_cb, (XtPointer)"ocean");

    aurora_item = XtVaCreateManagedWidget("Aurora",
        xmPushButtonWidgetClass, bg_menu, NULL);
    XtAddCallback(aurora_item, XmNactivateCallback, bg_menu_cb, (XtPointer)"aurora");

    embers_item = XtVaCreateManagedWidget("Embers",
        xmPushButtonWidgetClass, bg_menu, NULL);
    XtAddCallback(embers_item, XmNactivateCallback, bg_menu_cb, (XtPointer)"embers");

    static_item = XtVaCreateManagedWidget("Static",
        xmPushButtonWidgetClass, bg_menu, NULL);
    XtAddCallback(static_item, XmNactivateCallback, bg_menu_cb, (XtPointer)"static");

    XtManageChild(menubar);
    return menubar;
}

/*
 * Create a new terminal tab
 */
static int
create_terminal_tab(int console)
{
    DashTab *tab;
    char label[32];
    int tab_index;
    int r;
    pid_t pid;

    if (app.num_tabs >= MAX_TABS) {
        fprintf(stderr, "dash: maximum tabs reached\n");
        return -1;
    }

    tab_index = app.num_tabs;
    app.tabs[tab_index] = calloc(1, sizeof(DashTab));
    if (!app.tabs[tab_index]) {
        return -1;
    }
    tab = app.tabs[tab_index];

    tab->is_console = console;

    /* Create tab button */
    if (console)
        sprintf(label, "Console %d", tab_index + 1);
    else
        sprintf(label, "Terminal %d", tab_index + 1);

    tab->tab_button = XtVaCreateManagedWidget(label,
        xmPushButtonWidgetClass, app.tab_bar,
        XmNmarginTop, 1,
        XmNmarginBottom, 1,
        XmNshadowThickness, 1,
        NULL);
    XtAddCallback(tab->tab_button, XmNactivateCallback,
        tab_button_cb, (XtPointer)(uintptr_t)tab_index);

    /* Initialize terminal data */
    tab->active = 0;
    tab->pty = NULL;
    tab->title = strdup(label);

    /* Create TSM screen */
    r = tsm_screen_new(&tab->screen, tsm_log_cb, tab);
    if (r < 0) {
        fprintf(stderr, "dash: tsm_screen_new failed: %d\n", r);
        return -1;
    }

    /* Set max scrollback */
    tsm_screen_set_max_sb(tab->screen, app.settings.scrollback_lines);

    /* Set screen size */
    tsm_screen_resize(tab->screen, app.vertex_arrays.cols, app.vertex_arrays.rows);

    /* Create TSM VTE */
    r = tsm_vte_new(&tab->vte, tab->screen, tsm_write_cb, tab, tsm_log_cb, tab);
    if (r < 0) {
        fprintf(stderr, "dash: tsm_vte_new failed: %d\n", r);
        tsm_screen_unref(tab->screen);
        return -1;
    }

    if (app.settings.theme) {
        dash_set_theme(tab->vte, app.settings.theme);
    }

    tsm_vte_set_osc_cb(tab->vte, tsm_osc_cb, tab);

    /* Open PTY and spawn shell */
    pid = shl_pty_open(&tab->pty, pty_read_cb, tab,
                       app.vertex_arrays.cols, app.vertex_arrays.rows);
    if (pid < 0) {
        fprintf(stderr, "dash: shl_pty_open failed: %d\n", (int)pid);
        tsm_vte_unref(tab->vte);
        tsm_screen_unref(tab->screen);
        return -1;
    } else if (pid == 0) {
        /* Child process - exec shell */
        if (console) {
#ifdef __sgi
            ioctl(0, TIOCCONS, NULL);
#endif
        }

#ifdef __sgi
        putenv("TERM=dash");
#else
        setenv("TERM", "dash", 1);
#endif

        char *shell = getenv("SHELL");
        if (!shell)
            shell = "/bin/sh";
        execl(shell, shell, NULL);
        exit(1);
    }

    /* Register PTY fd with X event loop */
    tab->pty_input_id = XtAppAddInput(app.app_context,
                                      shl_pty_get_fd(tab->pty),
                                      (XtPointer)XtInputReadMask,
                                      pty_input_cb,
                                      tab);

    app.num_tabs++;

    update_tab_visibility();

    return tab_index;
}

/*
 * Switch to a specific tab
 */
static void
switch_to_tab(int tab_index)
{
    int i;
    char buf[32];

    if (tab_index < 0 || tab_index >= app.num_tabs) {
        return;
    }

    /* Deactivate all tabs */
    for (i = 0; i < app.num_tabs; i++) {
        app.tabs[i]->active = 0;
    }

    /* Activate selected tab */
    app.tabs[tab_index]->active = 1;
    app.active_tab = tab_index;

    /* Update window title */
    if (app.tabs[tab_index]->title) {
        char win_title[256];
        snprintf(win_title, sizeof(win_title), "dash: %s", app.tabs[tab_index]->title);
        XtVaSetValues(app.toplevel, XmNtitle, win_title, NULL);
    }
    snprintf(buf, sizeof(buf), "[%d]", tab_index + 1);
    show_osd(buf);

    /* Redraw the GL area with the new terminal's content */
    redraw_terminal();

    /* Return focus to terminal window */
    XmProcessTraversal(app.gl_area, XmTRAVERSE_CURRENT);
}

/*
 * Callback: Scrollbar value changed (drag or click)
 */
static void
scrollbar_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    XmScrollBarCallbackStruct *cbs = (XmScrollBarCallbackStruct *)call_data;
    DashTab *tab;
    int sb_max, current_pos, target_pos, delta;

    if (app.active_tab < 0 || app.active_tab >= app.num_tabs) return;
    tab = app.tabs[app.active_tab];
    if (!tab->screen) return;

    sb_max = (int)tsm_screen_sb_get_line_count(tab->screen);
    current_pos = (int)tsm_screen_sb_get_line_pos(tab->screen);

    /* Calculate target sb_pos from scrollbar value
     * value = 0 (top) -> sb_pos = 0
     * value = max - slider (bottom) -> sb_pos = sb_max
     */
    target_pos = cbs->value;
    if (target_pos < 0) target_pos = 0;
    if (target_pos > sb_max) target_pos = sb_max;

    delta = target_pos - current_pos;

    if (delta > 0) {
        tsm_screen_sb_down(tab->screen, delta);
    } else if (delta < 0) {
        tsm_screen_sb_up(tab->screen, -delta);
    }

    redraw_terminal();
}

/*
 * Update scrollbar to match TSM screen state
 */
static void
update_scrollbar(void)
{
    DashTab *tab;
    int sb_max, sb_pos, rows;
    int val, slider_size, max_val;
    Arg args[5];
    int n = 0;

    if (!app.scrollbar) return;

    if (app.active_tab < 0 || app.active_tab >= app.num_tabs) return;
    tab = app.tabs[app.active_tab];
    if (!tab->screen) return;

    sb_max = (int)tsm_screen_sb_get_line_count(tab->screen);
    sb_pos = (int)tsm_screen_sb_get_line_pos(tab->screen);
    rows = app.vertex_arrays.rows;
    if (rows < 1) rows = 1;

    slider_size = rows;
    max_val = sb_max + rows;
    val = sb_pos;

    /* Check if update is needed to avoid expensive XtSetValues */
    if (app.sb_cached_max == max_val &&
        app.sb_cached_slider == slider_size &&
        app.sb_cached_val == val) {
        return;
    }

    /* Update cache */
    app.sb_cached_max = max_val;
    app.sb_cached_slider = slider_size;
    app.sb_cached_val = val;

    XtSetArg(args[n], XmNmaximum, max_val); n++;
    XtSetArg(args[n], XmNsliderSize, slider_size); n++;
    XtSetArg(args[n], XmNvalue, val); n++;
    XtSetArg(args[n], XmNpageIncrement, rows > 1 ? rows - 1 : 1); n++;
    XtSetValues(app.scrollbar, args, n);
}

/*
 * Timer callback for cursor blinking (forced refresh)
 */
static void
redraw_timer_cb(XtPointer client_data, XtIntervalId *id)
{
    DashTab *tab;
    int active = 0;

    app.redraw_timer_id = 0;
    render_frame();
    app.dirty = 0;

    /* Check if we need to keep the loop running */
    if (app.bg_update) active = 1;
    if (app.osd_text) active = 1;
    if (app.dirty) active = 1;

    /* Check cursor blink requirement */
    if (app.active_tab >= 0 && app.active_tab < app.num_tabs) {
        tab = app.tabs[app.active_tab];
        if (tab && tab->screen && !(tsm_screen_get_flags(tab->screen) & TSM_SCREEN_HIDE_CURSOR)) {
            active = 1;
        }
    }

    if (active) {
        int interval = app.settings.redraw_interval;
        /* Boost framerate for OSD animations */
        if (app.osd_text && interval > 50) interval = 50;
        
        app.redraw_timer_id = XtAppAddTimeOut(app.app_context, 
                                            interval, 
                                            redraw_timer_cb, NULL);
    }
}

/*
 * Helper to draw box drawing characters
 */
static void
draw_box_char(float x, float y, float w, float h, uint32_t code)
{
    float mx = x + w / 2.0f;
    float my = y + h / 2.0f;
    int up = 0, down = 0, left = 0, right = 0;

    /* Apply vertex adjustment to match grid */
    mx += VERTEX_ADJ;
    my += VERTEX_ADJ;
    x += VERTEX_ADJ;
    y += VERTEX_ADJ;

    switch (code) {
    case 0x2500: left = 1; right = 1; break; /* ─ */
    case 0x2502: up = 1; down = 1; break;    /* │ */
    case 0x250C: right = 1; down = 1; break; /* ┌ */
    case 0x2510: left = 1; down = 1; break;  /* ┐ */
    case 0x2514: right = 1; up = 1; break;   /* └ */
    case 0x2518: left = 1; up = 1; break;    /* ┘ */
    case 0x251C: up = 1; down = 1; right = 1; break; /* ├ */
    case 0x2524: up = 1; down = 1; left = 1; break;  /* ┤ */
    case 0x252C: left = 1; right = 1; down = 1; break; /* ┬ */
    case 0x2534: left = 1; right = 1; up = 1; break;   /* ┴ */
    case 0x253C: left = 1; right = 1; up = 1; down = 1; break; /* ┼ */

    /* Map double lines to single for now */
    case 0x2550: left = 1; right = 1; break;
    case 0x2551: up = 1; down = 1; break;
    case 0x2554: right = 1; down = 1; break;
    case 0x2557: left = 1; down = 1; break;
    case 0x255A: right = 1; up = 1; break;
    case 0x255D: left = 1; up = 1; break;
    case 0x2560: up = 1; down = 1; right = 1; break;
    case 0x2563: up = 1; down = 1; left = 1; break;
    case 0x2566: left = 1; right = 1; down = 1; break;
    case 0x2569: left = 1; right = 1; up = 1; break;
    case 0x256C: left = 1; right = 1; up = 1; down = 1; break;
    }

    /* Draw segments from center */
    if (up)    { glVertex2f(mx, my); glVertex2f(mx, y); }
    if (down)  { glVertex2f(mx, my); glVertex2f(mx, y + h); }
    if (left)  { glVertex2f(mx, my); glVertex2f(x, my); }
    if (right) { glVertex2f(mx, my); glVertex2f(x + w, my); }
}

/*
 * Show OSD message
 */
static void
show_osd(const char *text)
{
    if (app.osd_text) free(app.osd_text);
    app.osd_text = strdup(text);
    gettimeofday(&app.osd_start_time, NULL);
    redraw_terminal();
}

/*
 * Draw OSD overlay
 */
static void
draw_osd(void)
{
    if (!app.osd_text) return;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    float elapsed = (float)(tv.tv_sec - app.osd_start_time.tv_sec) +
                    (float)(tv.tv_usec - app.osd_start_time.tv_usec) / 1000000.0f;
    float alpha = 1.0f;

    if (elapsed > 3.0f) {
        free(app.osd_text);
        app.osd_text = NULL;
        return;
    }
    if (elapsed > 2.0f) {
        alpha = 1.0f - (elapsed - 2.0f);
    }

    /* OSD Settings: 90s CRT TV Style (Green, Large, Blocky) */
    float scale = 3.0f;
    int len = strlen(app.osd_text);
    float w = len * app.font_info.char_width * scale;
    float h = app.font_info.char_height * scale;
    float padding = 20.0f;
    float x = app.width - w - padding * 2.0f;
    float y = padding * 2.0f;

    glDisable(GL_TEXTURE_2D);
    
    /* Draw Background Box (Black) */
    glColor4f(0.0f, 0.0f, 0.0f, 0.8f * alpha);
    glBegin(GL_QUADS);
    glVertex2f(x - padding, y - padding);
    glVertex2f(x + w + padding, y - padding);
    glVertex2f(x + w + padding, y + h + padding);
    glVertex2f(x - padding, y + h + padding);
    glEnd();

    /* Draw Text (Bright Green) */
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, app.font_info.atlas_texture);
    glColor4f(0.0f, 1.0f, 0.0f, 1.0f * alpha);

    glBegin(GL_QUADS);
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)app.osd_text[i];
        GlyphCoords *glyph = &app.font_info.glyphs[c];
        float cx = x + i * app.font_info.char_width * scale;
        
        glTexCoord2f(glyph->u0, glyph->v0); glVertex2f(cx, y);
        glTexCoord2f(glyph->u1, glyph->v0); glVertex2f(cx + app.font_info.char_width * scale, y);
        glTexCoord2f(glyph->u1, glyph->v1); glVertex2f(cx + app.font_info.char_width * scale, y + h);
        glTexCoord2f(glyph->u0, glyph->v1); glVertex2f(cx, y + h);
    }
    glEnd();
}

/*
 * Render the active terminal frame
 */
static void
render_frame(void)
{
    DashTab *tab;
    int i;

    if (!app.gl_area || !app.gl_context) {
        fprintf(stderr, "dash: redraw_terminal called but no GL area or context\n");
        return;
    }

    /* Check if vertex arrays are ready */
    if (!app.vertex_arrays.vertices || !app.font_info.atlas_texture) {
        fprintf(stderr, "dash: vertex arrays or texture not ready\n");
        return;
    }

    /* Get active tab */
    if (app.active_tab < 0 || app.active_tab >= app.num_tabs) {
        fprintf(stderr, "dash: no active tab\n");
        return;
    }

    tab = app.tabs[app.active_tab];
    if (!tab->screen) {
        fprintf(stderr, "dash: active tab has no screen\n");
        return;
    }

    /* Update background animation state */
    if (app.bg_update && app.bg_state) {
        app.bg_update(app.bg_state, app.width, app.height);
    }


    GLwDrawingAreaMakeCurrent(app.gl_area, app.gl_context);

    /* Query default attributes for background color */
    tsm_vte_get_def_attr(tab->vte, &app.vertex_arrays.def_attr);

    /* Calculate cursor position with scrollback adjustment */
    app.vertex_arrays.cursor_x = tsm_screen_get_cursor_x(tab->screen);
    app.vertex_arrays.cursor_y = tsm_screen_get_cursor_y(tab->screen);
    app.vertex_arrays.cursor_y += tsm_screen_sb_get_line_count(tab->screen) - tsm_screen_sb_get_line_pos(tab->screen);

    /* Reset attribute usage flags */
    app.vertex_arrays.screen_attr_usage = 0;
    memset(app.vertex_arrays.row_attr_usage, 0, app.vertex_arrays.rows * sizeof(unsigned char));

    /* Reset dirty flags */
    app.vertex_arrays.screen_bg_dirty = 0;
    memset(app.vertex_arrays.row_bg_dirty, 0, app.vertex_arrays.rows * sizeof(unsigned char));

    /* Clear screen with default background color */
    glClearColor(app.vertex_arrays.def_attr.br / 255.0f,
                 app.vertex_arrays.def_attr.bg / 255.0f,
                 app.vertex_arrays.def_attr.bb / 255.0f,
                 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    /* Draw background effect */
    if (app.bg_draw) {
        app.bg_draw(app.bg_state);
    }

    /* Bind glyph atlas texture */
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, app.font_info.atlas_texture);

    /* Let TSM fill the vertex arrays via tsm_draw_cb.
     * This will also populate row_bg_dirty and screen_bg_dirty. */
    tsm_screen_draw(tab->screen, tsm_draw_cb, NULL);

    /* Pass 1: Draw Backgrounds (only for rows that differ from default) */
    if (app.vertex_arrays.screen_bg_dirty) {
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND); /* Bonus: Turn off blend for background pass */
        glColorPointer(4, GL_FLOAT, 0, app.vertex_arrays.bg_colors);
        for (i = 0; i < app.vertex_arrays.rows; i++) {
            if (app.vertex_arrays.row_bg_dirty[i]) {
                glDrawArrays(GL_QUADS, i * app.vertex_arrays.cols * 4, app.vertex_arrays.cols * 4);
            }
        }
        glEnable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
    }

    /* Pass 2: Draw Foregrounds (Glyphs) */
    glColorPointer(4, GL_FLOAT, 0, app.vertex_arrays.fg_colors);
    glDrawArrays(GL_QUADS, 0, app.vertex_arrays.num_quads * 4);

    /* Pass 3: Bold and Italic Characters */
    if (app.vertex_arrays.screen_attr_usage & (ATTR_BOLD | ATTR_ITALIC)) {
        glBindTexture(GL_TEXTURE_2D, app.font_info.atlas_texture);
        glBegin(GL_QUADS);
        for (i = 0; i < app.vertex_arrays.rows; i++) {
            /* Skip rows that don't have bold or italic text */
            if (!(app.vertex_arrays.row_attr_usage[i] & (ATTR_BOLD | ATTR_ITALIC)))
                continue;
            
            /* Skip empty lines if we want to be extra safe, though usage check covers it */
            if (!(app.vertex_arrays.row_attr_usage[i] & ATTR_TEXT))
                continue;

            for (int j = 0; j < app.vertex_arrays.cols; j++) {
                int cell_idx = i * app.vertex_arrays.cols + j;
                unsigned char attr = app.vertex_arrays.attrs[cell_idx];

                if (attr & (ATTR_BOLD | ATTR_ITALIC)) {
                    uint32_t c = app.vertex_arrays.chars[cell_idx];
                    GlyphCoords *glyph = &app.font_info.glyphs[c];
                    float x = j * app.font_info.char_width;
                    float y = i * app.font_info.char_height;
                    float w = app.font_info.char_width;
                    float h = app.font_info.char_height;
                    float shear_top = (attr & ATTR_ITALIC) ? 1.0f : 0.0f;
                    float shear_bot = (attr & ATTR_ITALIC) ? -1.0f : 0.0f;

                    glColor4fv(&app.vertex_arrays.fg_colors[cell_idx * 4].r);

                    /* If Italic, we replaced main pass with space, so draw base here */
                    if (attr & ATTR_ITALIC) {
                        glTexCoord2f(glyph->u0, glyph->v0); glVertex2f(x + shear_top + VERTEX_ADJ, y + VERTEX_ADJ);
                        glTexCoord2f(glyph->u1, glyph->v0); glVertex2f(x + w + shear_top + VERTEX_ADJ, y + VERTEX_ADJ);
                        glTexCoord2f(glyph->u1, glyph->v1); glVertex2f(x + w + shear_bot + VERTEX_ADJ, y + h + VERTEX_ADJ);
                        glTexCoord2f(glyph->u0, glyph->v1); glVertex2f(x + shear_bot + VERTEX_ADJ, y + h + VERTEX_ADJ);
                    }

                    /* If Bold, draw 4 copies shifted by 1px with opacity */
                    if (attr & ATTR_BOLD) {
                        Color c = app.vertex_arrays.fg_colors[cell_idx * 4];
                        c.a = BOLD_ALPHA;
                        glColor4fv(&c.r);
                        
                        float off_x[] = {0.0f, 0.0f, -1.0f, 1.0f};
                        float off_y[] = {-1.0f, 1.0f, 0.0f, 0.0f};
                        for (int k = 0; k < 4; k++) {
                            glTexCoord2f(glyph->u0, glyph->v0); glVertex2f(x + off_x[k] + shear_top + VERTEX_ADJ, y + off_y[k] + VERTEX_ADJ);
                            glTexCoord2f(glyph->u1, glyph->v0); glVertex2f(x + w + off_x[k] + shear_top + VERTEX_ADJ, y + off_y[k] + VERTEX_ADJ);
                            glTexCoord2f(glyph->u1, glyph->v1); glVertex2f(x + w + off_x[k] + shear_bot + VERTEX_ADJ, y + h + off_y[k] + VERTEX_ADJ);
                            glTexCoord2f(glyph->u0, glyph->v1); glVertex2f(x + off_x[k] + shear_bot + VERTEX_ADJ, y + h + off_y[k] + VERTEX_ADJ);
                        }
                    }
                }
            }
        }
        glEnd();
    }

    /* Pass 4: Underlines */
    if (app.vertex_arrays.screen_attr_usage & ATTR_UNDERLINE) {
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        for (i = 0; i < app.vertex_arrays.rows; i++) {
            if (app.vertex_arrays.row_attr_usage[i] & ATTR_UNDERLINE) {
                for (int j = 0; j < app.vertex_arrays.cols; j++) {
                    int cell_idx = i * app.vertex_arrays.cols + j;
                    if (app.vertex_arrays.attrs[cell_idx] & ATTR_UNDERLINE) {
                        float x = j * app.font_info.char_width;
                        float y = (i + 1) * app.font_info.char_height - 1;
                        float w = app.font_info.char_width;

                        glColor4fv(&app.vertex_arrays.fg_colors[cell_idx * 4].r);
                        glVertex2f(x + VERTEX_ADJ, y + VERTEX_ADJ);
                        glVertex2f(x + w + VERTEX_ADJ, y + VERTEX_ADJ);
                        glVertex2f(x + w + VERTEX_ADJ, y + 1.0f + VERTEX_ADJ);
                        glVertex2f(x + VERTEX_ADJ, y + 1.0f + VERTEX_ADJ);
                    }
                }
            }
        }
        glEnd();
        glEnable(GL_TEXTURE_2D);
    }

    /* Pass 5: Box Drawing Characters */
    if (app.vertex_arrays.screen_attr_usage & ATTR_BOXDRAW) {
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_LINES);
        for (i = 0; i < app.vertex_arrays.rows; i++) {
            if (app.vertex_arrays.row_attr_usage[i] & ATTR_BOXDRAW) {
                for (int j = 0; j < app.vertex_arrays.cols; j++) {
                    int cell_idx = i * app.vertex_arrays.cols + j;
                    if (app.vertex_arrays.attrs[cell_idx] & ATTR_BOXDRAW) {
                        float x = j * app.font_info.char_width;
                        float y = i * app.font_info.char_height;
                        glColor4fv(&app.vertex_arrays.fg_colors[cell_idx * 4].r);
                        draw_box_char(x, y, (float)app.font_info.char_width, (float)app.font_info.char_height, app.vertex_arrays.chars[cell_idx]);
                    }
                }
            }
        }
        glEnd();
        glEnable(GL_TEXTURE_2D);
    }

    /* Draw Cursor */
    if (tab->screen && !(tsm_screen_get_flags(tab->screen) & TSM_SCREEN_HIDE_CURSOR)) {
        int cx = app.vertex_arrays.cursor_x;
        int cy = app.vertex_arrays.cursor_y;
        int cw = app.font_info.char_width;
        int ch = app.font_info.char_height;

        /* Calculate breathing opacity (25% to 100%) based on time */
        struct timeval tv;
        gettimeofday(&tv, NULL);
        double t = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
        float alpha = 0.25f + 0.75f * (0.5f * (1.0f + sin(t * 3.0)));

        /* Ensure cursor is within bounds */
        if (cx >= 0 && cx < app.vertex_arrays.cols &&
            cy >= 0 && cy < app.vertex_arrays.rows) {
            float x = cx * cw;
            float y = cy * ch;

            glDisable(GL_TEXTURE_2D);
            glColor4f(app.cursor_color.r, app.cursor_color.g, app.cursor_color.b, alpha);
            glBegin(GL_QUADS);
            glVertex2f(x, y);
            glVertex2f(x + cw, y);
            glVertex2f(x + cw, y + ch);
            glVertex2f(x, y + ch);
            glEnd();
            glEnable(GL_TEXTURE_2D);
        }
    }

    draw_osd();

    glFlush();
    GLwDrawingAreaSwapBuffers(app.gl_area);

    update_scrollbar();
}

/*
 * Schedule a redraw
 */
static void
redraw_terminal(void)
{
    app.dirty = 1;
    if (app.redraw_timer_id == 0) {
        app.redraw_timer_id = XtAppAddTimeOut(app.app_context, 
                                            app.settings.redraw_interval, 
                                            redraw_timer_cb, NULL);
    }
}

/*
 * Resize all terminal screens and PTYs to match current grid
 */
static void
resize_screens(void)
{
    int i;
    for (i = 0; i < app.num_tabs; i++) {
        if (app.tabs[i]->screen) {
            unsigned int old_rows = tsm_screen_get_height(app.tabs[i]->screen);

            tsm_screen_resize(app.tabs[i]->screen,
                              app.vertex_arrays.cols,
                              app.vertex_arrays.rows);

            if (app.vertex_arrays.rows < old_rows) {
                tsm_screen_clear_sb(app.tabs[i]->screen);
            } else {
                tsm_screen_sb_reset(app.tabs[i]->screen);
            }
        }
        if (app.tabs[i]->pty) {
            int r = shl_pty_resize(app.tabs[i]->pty,
                                   app.vertex_arrays.cols,
                                   app.vertex_arrays.rows);
            if (r < 0) {
                fprintf(stderr, "dash: pty resize failed: %s\n", strerror(-r));
            } else {
                fprintf(stderr, "dash: pty resized to %dx%d\n", 
                        app.vertex_arrays.cols, app.vertex_arrays.rows);
            }
        }
    }
}

/*
 * Initialize OpenGL context (ginitCallback)
 * Called when the GL widget is realized and ready for GL operations
 */
static void
gl_init_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    Display *dpy;
    XVisualInfo *vi;
    GLwDrawingAreaCallbackStruct *cb = (GLwDrawingAreaCallbackStruct*)call_data;
    Arg args[1];

    dpy = XtDisplay(w);

    /* Get visual info from widget */
    XtSetArg(args[0], GLwNvisualInfo, &vi);
    XtGetValues(w, args, 1);

    if (!vi) {
        fprintf(stderr, "dash: failed to get visual info\n");
        exit(1);
    }

    /* Create GL context */
    app.gl_context = glXCreateContext(dpy, vi, None, GL_TRUE);
    if (!app.gl_context) {
        fprintf(stderr, "dash: failed to create GL context\n");
        exit(1);
    }

    /* Make context current */
    GLwDrawingAreaMakeCurrent(w, app.gl_context);

    const char *renderer = (const char *)glGetString(GL_RENDERER);
    if (renderer && strstr(renderer, "IMPACT")) {
        app.is_impact = 1;
        printf("dash: detected IMPACT graphics, using GL_LUMINANCE_ALPHA atlas\n");
    }

    /* Initialize OpenGL state */
    glClearColor(0.1, 0.1, 0.2, 1.0);  /* Dark blue background */
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    /* Load font and build glyph atlas */
    if (load_font(app.settings.font_family ? app.settings.font_family : "courier",
                  app.settings.font_size > 0 ? app.settings.font_size : 14) < 0) {
        fprintf(stderr, "dash: failed to load font\n");
        exit(1);
    }

    /* Setup initial viewport using callback dimensions */
    app.width = cb->width;
    app.height = cb->height;
    setup_gl_viewport();

    /* Resize all terminal screens and PTYs now that we have valid dimensions */
    resize_screens();
}

/*
 * Handle GL area resize
 */
static void
gl_resize_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    GLwDrawingAreaCallbackStruct *cb = (GLwDrawingAreaCallbackStruct*)call_data;

    if (!app.gl_context) {
        fprintf(stderr, "dash: resize callback but no GL context yet\n");
        return;
    }

    GLwDrawingAreaMakeCurrent(w, app.gl_context);

    /* Update app dimensions */
    app.width = cb->width;
    app.height = cb->height;

    /* Setup viewport and projection for new size */
    setup_gl_viewport();

    /* Resize all terminal screens and PTYs */
    resize_screens();

    render_frame();    /* Draw immediately */
    redraw_terminal(); /* Ensure loop is running */
}

/*
 * Handle GL area expose (redraw)
 */
static void
gl_expose_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    if (!app.gl_context) {
        return;
    }

    GLwDrawingAreaMakeCurrent(w, app.gl_context);
    render_frame();    /* Draw immediately */
    redraw_terminal(); /* Ensure loop is running */
}

/*
 * Clipboard: Convert selection to string
 */
static Boolean
convert_selection_cb(Widget w, Atom *selection, Atom *target,
                     Atom *type, XtPointer *value,
                     unsigned long *length, int *format)
{
    if (*target == XA_STRING) {
        *type = XA_STRING;
        *value = XtNewString(app.selection_text ? app.selection_text : "");
        *length = strlen((char*)*value);
        *format = 8;
        return True;
    }
    return False;
}

/*
 * Clipboard: Lost selection ownership
 */
static void
lose_selection_cb(Widget w, Atom *selection)
{
    if (*selection == XA_PRIMARY) app.own_primary = 0;
    if (*selection == app.atom_clipboard) app.own_clipboard = 0;

    /* Only clear text if we lost both selections */
    if (!app.own_primary && !app.own_clipboard) {
        if (app.selection_text) {
            free(app.selection_text);
            app.selection_text = NULL;
        }

        /* Clear visual selection in active tab */
        if (app.active_tab >= 0 && app.active_tab < app.num_tabs) {
            DashTab *tab = app.tabs[app.active_tab];
            if (tab->screen) {
                tsm_screen_selection_reset(tab->screen);
                redraw_terminal();
            }
        }
    }
}

/*
 * Clipboard: Paste received data
 */
static void
paste_selection_cb(Widget w, XtPointer client_data, Atom *selection,
                   Atom *type, XtPointer value,
                   unsigned long *length, int *format)
{
    DashTab *tab = (DashTab*)client_data;
    if (*type == XA_STRING && *format == 8 && value) {
        if (tab->pty) {
            shl_pty_write(tab->pty, (char*)value, *length);
            shl_pty_dispatch(tab->pty);
        }
        XtFree((char*)value);
    }
}

/*
 * Handle keyboard/mouse input
 */
static void
input_event_handler(Widget w, XtPointer client_data, XEvent *event, Boolean *cont)
{
    DashTab *tab;
    KeySym keysym;
    char buffer[32];
    int len;
    unsigned int mods = 0;
    uint32_t ucs4 = TSM_VTE_INVALID;
    uint32_t ascii = NoSymbol;
    int col = 0, row = 0;
    unsigned int mouse_mods = 0;

    /* Get active tab */
    if (app.active_tab < 0 || app.active_tab >= app.num_tabs)
        return;

    tab = app.tabs[app.active_tab];
    if (!tab->vte)
        return;

    /* Calculate grid coordinates */
    if (event->type == ButtonPress || event->type == ButtonRelease) {
        col = event->xbutton.x / app.font_info.char_width;
        row = event->xbutton.y / app.font_info.char_height;
    } else if (event->type == MotionNotify) {
        col = event->xmotion.x / app.font_info.char_width;
        row = event->xmotion.y / app.font_info.char_height;
    }

    /* Clamp coordinates */
    if (col < 0) col = 0;
    if (col >= app.vertex_arrays.cols) col = app.vertex_arrays.cols - 1;
    if (row < 0) row = 0;
    if (row >= app.vertex_arrays.rows) row = app.vertex_arrays.rows - 1;

    if (event->type == ButtonPress) {
        XmProcessTraversal(w, XmTRAVERSE_CURRENT);

        /* Map X11 state to TSM modifiers */
        if (event->xbutton.state & ShiftMask)   mouse_mods |= TSM_MOUSE_MODIFIER_SHIFT;
        if (event->xbutton.state & ControlMask) mouse_mods |= TSM_MOUSE_MODIFIER_CTRL;
        if (event->xbutton.state & Mod1Mask)    mouse_mods |= TSM_MOUSE_MODIFIER_META;

        /* Handle Application Mouse Mode (unless Shift is pressed) */
        if (tsm_vte_get_mouse_mode(tab->vte) && !(event->xbutton.state & ShiftMask)) {
            int button = -1;
            switch (event->xbutton.button) {
            case Button1: button = TSM_MOUSE_BUTTON_LEFT; break;
            case Button2: button = TSM_MOUSE_BUTTON_MIDDLE; break;
            case Button3: button = TSM_MOUSE_BUTTON_RIGHT; break;
            case Button4: button = TSM_MOUSE_BUTTON_WHEEL_UP; break;
            case Button5: button = TSM_MOUSE_BUTTON_WHEEL_DOWN; break;
            }
            if (button != -1) {
                tsm_vte_handle_mouse(tab->vte, col, row, event->xbutton.x, event->xbutton.y,
                                     button, TSM_MOUSE_EVENT_PRESSED, mouse_mods);
            }
            /* Don't return, let wheel events fall through if app doesn't handle them? 
             * Actually, if mouse mode is on, we usually consume the event. */
            return;
        }

        /* Local Mouse Handling */
        switch (event->xbutton.button) {
        case Button1: /* Left Click: Start Selection */
            tsm_screen_selection_start(tab->screen, col, row);
            app.selecting = 1;
            app.sel_last_x = col;
            app.sel_last_y = row;
            redraw_terminal();
            break;
        case Button2: /* Middle Click: Paste */
            XtGetSelectionValue(w, XA_PRIMARY, XA_STRING, paste_selection_cb, tab, event->xbutton.time);
            break;
        case Button4: /* Wheel Up: Scrollback */
            tsm_screen_sb_up(tab->screen, 3);
            redraw_terminal();
            break;
        case Button5: /* Wheel Down: Scrollback */
            tsm_screen_sb_down(tab->screen, 3);
            redraw_terminal();
            break;
        }
    }

    if (event->type == MotionNotify) {
        if (app.selecting) {
            /* Only update if cell changed to avoid excessive redraws */
            if (col != app.sel_last_x || row != app.sel_last_y) {
                tsm_screen_selection_target(tab->screen, col, row);
                app.sel_last_x = col;
                app.sel_last_y = row;
                redraw_terminal();
            }
        } else if (tsm_vte_get_mouse_mode(tab->vte) && !(event->xmotion.state & ShiftMask)) {
             tsm_vte_handle_mouse(tab->vte, col, row, event->xmotion.x, event->xmotion.y,
                                  0, TSM_MOUSE_EVENT_MOVED, mouse_mods);
        }
    }

    if (event->type == ButtonRelease) {
        if (app.selecting && event->xbutton.button == Button1) {
            app.selecting = 0;
            /* Copy selection */
            if (app.selection_text) {
                free(app.selection_text);
                app.selection_text = NULL;
            }
            if (tsm_screen_selection_copy(tab->screen, &app.selection_text) > 0) {
                app.own_primary = 1;
                XtOwnSelection(w, XA_PRIMARY, event->xbutton.time,
                               convert_selection_cb, lose_selection_cb, NULL);
            } else {
                tsm_screen_selection_reset(tab->screen);
                redraw_terminal();
            }
        }
        /* TODO: Handle button release for app mouse mode if needed */
    }

    if (event->type == KeyPress) {
        len = XLookupString(&event->xkey, buffer, sizeof(buffer), &keysym, NULL);

        /* Handle Scrollback Shortcuts (Shift+PageUp/Down) */
        if (event->xkey.state & ShiftMask) {
            if (keysym == XK_Page_Up) {
                tsm_screen_sb_page_up(tab->screen, 1);
                redraw_terminal();
                return;
            }
            if (keysym == XK_Page_Down) {
                tsm_screen_sb_page_down(tab->screen, 1);
                redraw_terminal();
                return;
            }
        }

        /* Handle Custom Scrollback Shortcuts (Ctrl+Alt+Arrows/Page) */
        if ((event->xkey.state & ControlMask) && (event->xkey.state & Mod1Mask)) {
            if (keysym == XK_Up) {
                tsm_screen_sb_up(tab->screen, 1);
                redraw_terminal();
                return;
            }
            if (keysym == XK_Down) {
                tsm_screen_sb_down(tab->screen, 1);
                redraw_terminal();
                return;
            }
            if (keysym == XK_Page_Up) {
                tsm_screen_sb_page_up(tab->screen, 1);
                redraw_terminal();
                return;
            }
            if (keysym == XK_Page_Down) {
                tsm_screen_sb_page_down(tab->screen, 1);
                redraw_terminal();
                return;
            }
            if (keysym == XK_t || keysym == XK_T) {
                time_t t = time(NULL);
                struct tm *tm = localtime(&t);
                char buf[64];
                strftime(buf, sizeof(buf), "%H:%M:%S", tm);
                show_osd(buf);
                return;
            }
            if (keysym == XK_n || keysym == XK_N) {
                int tab_index = create_terminal_tab(0);
                if (tab_index >= 0) {
                    switch_to_tab(tab_index);
                }
                return;
            }
            if (keysym == XK_w || keysym == XK_W) {
                close_tab(app.active_tab);
                return;
            }
        }

        /* Handle Zoom Shortcuts (Ctrl+Alt+Plus/Minus) */
        if ((event->xkey.state & ControlMask) && (event->xkey.state & Mod1Mask)) {
            if (keysym == XK_equal || keysym == XK_plus || keysym == XK_KP_Add) {
                update_zoom(1);
                return;
            }
            if (keysym == XK_minus || keysym == XK_KP_Subtract) {
                update_zoom(-1);
                return;
            }
        }

        /* Handle Tab Switching (Ctrl+Alt+1..0, Tab) */
        if ((event->xkey.state & ControlMask) && (event->xkey.state & Mod1Mask)) {
            int target = -1;
            if (keysym >= XK_1 && keysym <= XK_9) {
                target = keysym - XK_1;
            } else if (keysym == XK_0) {
                target = 9;
            } else if (keysym == XK_Tab) {
                target = (app.active_tab + 1) % app.num_tabs;
            }

            if (target != -1 && target < app.num_tabs) {
                switch_to_tab(target);
                return;
            }
        }

        /* Map modifiers */
        if (event->xkey.state & ShiftMask)   mods |= TSM_SHIFT_MASK;
        if (event->xkey.state & LockMask)    mods |= TSM_LOCK_MASK;
        if (event->xkey.state & ControlMask) mods |= TSM_CONTROL_MASK;
        if (event->xkey.state & Mod1Mask)    mods |= TSM_ALT_MASK;

        if (len > 0) {
            /* Simple ASCII/Latin-1 mapping */
            ucs4 = (uint32_t)(unsigned char)buffer[0];
        }

        /* For special keys, invalidate ucs4 so TSM handles them via keysym */
        switch (keysym) {
        case XK_ISO_Left_Tab:
        case XK_Delete:
        case XK_Home:
        case XK_Left:
        case XK_Up:
        case XK_Right:
        case XK_Down:
        case XK_Prior:
        case XK_Next:
        case XK_End:
        case XK_Begin:
        case XK_Insert:
        case XK_F1:  case XK_F2:  case XK_F3:  case XK_F4:
        case XK_F5:  case XK_F6:  case XK_F7:  case XK_F8:
        case XK_F9:  case XK_F10: case XK_F11: case XK_F12:
        case XK_KP_Space:
        case XK_KP_Tab:
        case XK_KP_F1: case XK_KP_F2: case XK_KP_F3: case XK_KP_F4:
        case XK_KP_Home: case XK_KP_Left: case XK_KP_Up: case XK_KP_Right:
        case XK_KP_Down: case XK_KP_Prior:
        case XK_KP_Next: case XK_KP_End:
        case XK_KP_Begin: case XK_KP_Insert: case XK_KP_Delete:
        case XK_KP_Equal: case XK_KP_Multiply: case XK_KP_Add:
        case XK_KP_Separator: case XK_KP_Subtract: case XK_KP_Decimal:
        case XK_KP_Divide:
        case XK_KP_0: case XK_KP_1: case XK_KP_2: case XK_KP_3:
        case XK_KP_4: case XK_KP_5: case XK_KP_6: case XK_KP_7:
        case XK_KP_8: case XK_KP_9:
            ucs4 = TSM_VTE_INVALID;
            break;
        }

        /* Use ucs4 as ascii if it's a valid ASCII char */
        if (ucs4 != TSM_VTE_INVALID && ucs4 < 128) {
            ascii = ucs4;
        }

        if (tsm_vte_handle_keyboard(tab->vte, keysym, ascii, mods, ucs4)) {
            tsm_screen_sb_reset(tab->screen);
            redraw_terminal();
        }
    }
}

/*
 * Callback: Tab button clicked
 */
static void
tab_button_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    int tab_index = (int)(uintptr_t)client_data;
    switch_to_tab(tab_index);
}

/*
 * Callback: Create new terminal tab
 */
static void
new_tab_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    int tab_index;

    tab_index = create_terminal_tab(0);
    if (tab_index >= 0) {
        switch_to_tab(tab_index);
    }
}

/*
 * Callback: Create new console tab
 */
static void
new_console_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    int tab_index;

    tab_index = create_terminal_tab(1);
    if (tab_index >= 0) {
        switch_to_tab(tab_index);
    }
}

/*
 * Callback: Close active tab
 */
static void
close_tab_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    close_tab(app.active_tab);
}

/*
 * Callback: Quit application
 */
static void
quit_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    exit(0);
}

/*
 * Callback: Switch theme
 */
static void
theme_menu_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    char *theme = (char*)client_data;
    int i;

    app.settings.theme = theme;

    for (i = 0; i < app.num_tabs; i++) {
        if (app.tabs[i]->vte) {
            dash_set_theme(app.tabs[i]->vte, theme);
        }
    }
    redraw_terminal();
}

/*
 * Callback: Switch background
 */
static void
bg_menu_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    char *mode = (char*)client_data;

    if (app.bg_state && app.bg_cleanup) {
        app.bg_cleanup(app.bg_state);
        app.bg_state = NULL;
        app.bg_cleanup = NULL;
    }

    if (strcmp(mode, "none") == 0) {
        if (app.bg_state) {
            free(app.bg_state);
            app.bg_state = NULL;
        }
        app.bg_init = NULL;
        app.bg_update = NULL;
        app.bg_draw = NULL;
    } else if (strcmp(mode, "night") == 0) {
        app.bg_init = night_bg_init;
    } else if (strcmp(mode, "sunset") == 0) {
        app.bg_init = sunset_bg_init;
    } else if (strcmp(mode, "ocean") == 0) {
        app.bg_init = ocean_bg_init;
    } else if (strcmp(mode, "aurora") == 0) {
        app.bg_init = aurora_bg_init;
    } else if (strcmp(mode, "embers") == 0) {
        app.bg_init = embers_bg_init;
    } else if (strcmp(mode, "static") == 0) {
        app.bg_init = static_bg_init;
    }

    if (app.bg_init) {
        app.bg_init(app.width, app.height);
    }

    redraw_terminal();
}

/*
 * Setup OpenGL viewport and projection matrix
 */
static void
setup_gl_viewport(void)
{
    /* Set viewport to cover entire widget */
    glViewport(0, 0, app.width, app.height);

    /* Setup orthographic projection for 2D rendering */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, app.width, app.height, 0, -1, 1);  /* Top-left origin like screen coords */

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (app.bg_init) {
        app.bg_init(app.width, app.height);
    }

    /* Setup vertex arrays for batch rendering */
    setup_vertex_arrays(app.width, app.height);
}

/*
 * Free vertex arrays
 */
static void
free_vertex_arrays(void)
{
    if (app.vertex_arrays.vertices) {
        free(app.vertex_arrays.vertices);
        app.vertex_arrays.vertices = NULL;
    }
    if (app.vertex_arrays.texcoords) {
        free(app.vertex_arrays.texcoords);
        app.vertex_arrays.texcoords = NULL;
    }
    if (app.vertex_arrays.fg_colors) {
        free(app.vertex_arrays.fg_colors);
        app.vertex_arrays.fg_colors = NULL;
    }
    if (app.vertex_arrays.bg_colors) {
        free(app.vertex_arrays.bg_colors);
        app.vertex_arrays.bg_colors = NULL;
    }
    if (app.vertex_arrays.row_bg_dirty) {
        free(app.vertex_arrays.row_bg_dirty);
        app.vertex_arrays.row_bg_dirty = NULL;
    }
    if (app.vertex_arrays.chars) {
        free(app.vertex_arrays.chars);
        app.vertex_arrays.chars = NULL;
    }
    if (app.vertex_arrays.attrs) {
        free(app.vertex_arrays.attrs);
        app.vertex_arrays.attrs = NULL;
    }
    if (app.vertex_arrays.row_attr_usage) {
        free(app.vertex_arrays.row_attr_usage);
        app.vertex_arrays.row_attr_usage = NULL;
    }
    app.vertex_arrays.num_quads = 0;
}

/*
 * Setup vertex arrays for batch rendering
 * Creates arrays for all character cells based on viewport size
 */
static void
setup_vertex_arrays(int width, int height)
{
    int col, row, i;
    int char_w, char_h;
    float x, y;

    /* Free existing arrays */
    free_vertex_arrays();

    if (!app.font_info.font) {
        fprintf(stderr, "dash: cannot setup vertex arrays, no font loaded\n");
        return;
    }

    char_w = app.font_info.char_width;
    char_h = app.font_info.char_height;

    /* Calculate terminal dimensions */
    app.vertex_arrays.cols = width / char_w;
    app.vertex_arrays.rows = height / char_h;

    /* Clamp to minimum 1x1 */
    if (app.vertex_arrays.cols < 1) app.vertex_arrays.cols = 1;
    if (app.vertex_arrays.rows < 1) app.vertex_arrays.rows = 1;

    app.vertex_arrays.num_quads = app.vertex_arrays.cols * app.vertex_arrays.rows;

    /* Allocate arrays (4 vertices per quad) */
    app.vertex_arrays.vertices = (Vertex2D*)malloc(
        app.vertex_arrays.num_quads * 4 * sizeof(Vertex2D));
    app.vertex_arrays.texcoords = (TexCoord*)malloc(
        app.vertex_arrays.num_quads * 4 * sizeof(TexCoord));
    app.vertex_arrays.fg_colors = (Color*)malloc(
        app.vertex_arrays.num_quads * 4 * sizeof(Color));
    app.vertex_arrays.bg_colors = (Color*)malloc(
        app.vertex_arrays.num_quads * 4 * sizeof(Color));
    app.vertex_arrays.row_bg_dirty = (unsigned char*)malloc(
        app.vertex_arrays.rows * sizeof(unsigned char));
    app.vertex_arrays.chars = (uint32_t*)malloc(
        app.vertex_arrays.num_quads * sizeof(uint32_t));
    app.vertex_arrays.attrs = (unsigned char*)malloc(
        app.vertex_arrays.num_quads * sizeof(unsigned char));
    app.vertex_arrays.row_attr_usage = (unsigned char*)malloc(
        app.vertex_arrays.rows * sizeof(unsigned char));

    if (!app.vertex_arrays.vertices || !app.vertex_arrays.texcoords ||
        !app.vertex_arrays.fg_colors || !app.vertex_arrays.bg_colors ||
        !app.vertex_arrays.row_bg_dirty || !app.vertex_arrays.chars ||
        !app.vertex_arrays.attrs || !app.vertex_arrays.row_attr_usage) {
        fprintf(stderr, "dash: failed to allocate vertex arrays\n");
        free_vertex_arrays();
        return;
    }
    /* Initialize vertex positions (constant after resize) */
    i = 0;
    for (row = 0; row < app.vertex_arrays.rows; row++) {
        for (col = 0; col < app.vertex_arrays.cols; col++) {
            x = (float)(col * char_w);
            y = (float)(row * char_h);

            /* Quad vertices: TL, TR, BR, BL */
            app.vertex_arrays.vertices[i*4 + 0].x = x + VERTEX_ADJ;
            app.vertex_arrays.vertices[i*4 + 0].y = y + VERTEX_ADJ;

            app.vertex_arrays.vertices[i*4 + 1].x = x + char_w + VERTEX_ADJ;
            app.vertex_arrays.vertices[i*4 + 1].y = y + VERTEX_ADJ;

            app.vertex_arrays.vertices[i*4 + 2].x = x + char_w + VERTEX_ADJ;
            app.vertex_arrays.vertices[i*4 + 2].y = y + char_h + VERTEX_ADJ;

            app.vertex_arrays.vertices[i*4 + 3].x = x + VERTEX_ADJ;
            app.vertex_arrays.vertices[i*4 + 3].y = y + char_h + VERTEX_ADJ;

            i++;
        }
    }

    /* Initialize foreground colors to white */
    for (i = 0; i < app.vertex_arrays.num_quads * 4; i++) {
        app.vertex_arrays.fg_colors[i].r = 1.0f;
        app.vertex_arrays.fg_colors[i].g = 1.0f;
        app.vertex_arrays.fg_colors[i].b = 1.0f;
        app.vertex_arrays.fg_colors[i].a = 1.0f;
    }

    /* Initialize background colors to black */
    for (i = 0; i < app.vertex_arrays.num_quads * 4; i++) {
        app.vertex_arrays.bg_colors[i].r = 0.0f;
        app.vertex_arrays.bg_colors[i].g = 0.0f;
        app.vertex_arrays.bg_colors[i].b = 0.0f;
        app.vertex_arrays.bg_colors[i].a = 1.0f;
    }

    /* Enable vertex arrays */
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    /* Bind arrays (use fg_colors for now) */
    glVertexPointer(2, GL_FLOAT, 0, app.vertex_arrays.vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, app.vertex_arrays.texcoords);
    glColorPointer(4, GL_FLOAT, 0, app.vertex_arrays.fg_colors);
}

/*
 * Calculate next power of 2
 */
static int
next_power_of_2(int n)
{
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

/*
 * Try to load a font with specific parameters
 */
static XFontStruct *
try_load_font(Display *dpy, const char *family, int size, const char *encoding)
{
    char pattern[256];
    XFontStruct *font;

    /* 1. Strict: medium-r-normal (no add style) */
    snprintf(pattern, sizeof(pattern),
             "-*-%s-medium-r-normal--%d-*-*-*-*-*-%s",
             family, size, encoding);
    font = XLoadQueryFont(dpy, pattern);
    if (font) goto found;

    /* 2. Relaxed AddStyle: medium-r-normal-* (e.g. lucida-medium-r-normal-sans) */
    snprintf(pattern, sizeof(pattern),
             "-*-%s-medium-r-normal-*-%d-*-*-*-*-*-%s",
             family, size, encoding);
    font = XLoadQueryFont(dpy, pattern);
    if (font) goto found;

    /* 3. Relaxed Width & Style: medium-r-*-* */
    snprintf(pattern, sizeof(pattern),
             "-*-%s-medium-r-*-*-%d-*-*-*-*-*-%s",
             family, size, encoding);
    font = XLoadQueryFont(dpy, pattern);
    if (font) goto found;
    
    /* 4. Any weight, roman slant: *-r-*-* */
    snprintf(pattern, sizeof(pattern),
             "-*-%s-*-r-*-*-%d-*-*-*-*-*-%s",
             family, size, encoding);
    font = XLoadQueryFont(dpy, pattern);
    if (font) goto found;

    /* 5. Wildcard everything: *-*-*-* (Desperation) */
    snprintf(pattern, sizeof(pattern),
             "-*-%s-*-*-*-*-%d-*-*-*-*-*-%s",
             family, size, encoding);
    font = XLoadQueryFont(dpy, pattern);
    if (font) goto found;

    return NULL;

found:
    if (font) {
        printf("dash: loaded font '%s'\n", pattern);
    }
    return font;
}

/*
 * Find the best matching font, trying encodings and fallbacks
 */
static XFontStruct *
find_best_font(Display *dpy, const char *family, int size)
{
    XFontStruct *font;
    const char *encodings[] = { "iso8859-1", "nil-ascii", "*", NULL };
    int i;

    /* Try requested family with different encodings */
    for (i = 0; encodings[i]; i++) {
        font = try_load_font(dpy, family, size, encodings[i]);
        if (font) return font;
    }

    /* Fallback to 'fixed' */
    if (strcmp(family, "fixed") != 0) {
        fprintf(stderr, "dash: failed to load family '%s', trying 'fixed'\n", family);
        for (i = 0; encodings[i]; i++) {
            font = try_load_font(dpy, "fixed", size, encodings[i]);
            if (font) return font;
        }
    }

    return NULL;
}

/*
 * Cleanup existing font resources
 */
static void
cleanup_font(void)
{
    if (app.font_info.atlas_texture) {
        glDeleteTextures(1, &app.font_info.atlas_texture);
        app.font_info.atlas_texture = 0;
    }
    /* Note: app.font_info.font is managed by XLoadQueryFont/XFreeFont logic in load_font */
}

/*
 * Load font and build glyph texture atlas
 * font_family: font family name like "courier", "fixed", "helvetica"
 * pixel_size: pixel size for display (e.g., 14)
 */
static int
load_font(const char *font_family, int pixel_size)
{
    XFontStruct *font, *scaled_font;
    int scale_factor;
    int actual_scale;

    /* Cleanup previous font resources */
    cleanup_font();
    if (app.font_info.font)
        XFreeFont(app.display, app.font_info.font);

    /* Load display font for metrics */
    font = find_best_font(app.display, font_family, pixel_size);
    if (!font) {
        fprintf(stderr, "dash: failed to load any suitable font\n");
        return -1;
    }

    /* Try to load font at progressively smaller scales (4x, 3x, 2x, 1x) */
    /* IRIX fonts typically max out around pixel size 60 */
    scaled_font = NULL;
    actual_scale = 1;

    for (scale_factor = GLYPH_SCALE; scale_factor >= 1; scale_factor--) {
        int scaled_size = pixel_size * scale_factor;

        scaled_font = find_best_font(app.display, font_family, scaled_size);
        if (scaled_font) {
            actual_scale = scale_factor;
            break;
        }
    }

    if (!scaled_font) {
        fprintf(stderr, "dash: failed to load any scaled font, using display font (no anti-aliasing)\n");
        scaled_font = font;
        actual_scale = 1;
    }

    if (font->min_bounds.width != font->max_bounds.width) {
        printf("dash: warning: font '%s' is proportional (min %d, max %d, '0' width %d)\n",
               font_family, font->min_bounds.width, font->max_bounds.width,
               XTextWidth(font, "0", 1));
    } else {
        printf("dash: font '%s' is monospaced (width %d)\n", font_family, font->max_bounds.width);
    }

    app.font_info.font = font;
    app.font_info.char_width = XTextWidth(font, "0", 1);
    if (app.font_info.char_width <= 0) {
        app.font_info.char_width = font->max_bounds.width;
    }
    app.font_info.char_height = font->ascent + font->descent;
    app.font_info.ascent = font->ascent;
    app.font_info.descent = font->descent;

    /* Build the atlas using the chosen scaled font */
    if (build_font_atlas(scaled_font, actual_scale) < 0) {
        return -1;
    }

    /* Free scaled font if it's different from display font */
    if (scaled_font != font) {
        XFreeFont(app.display, scaled_font);
    }

    return 0;
}

/*
 * Build texture atlas from a scaled font
 */
static int
build_font_atlas(XFontStruct *scaled_font, int actual_scale)
{
    int i, glyph_w, glyph_h, scaled_w, scaled_h;
    int atlas_cols, atlas_rows;
    unsigned int bpp = 1;
    GLenum format = GL_ALPHA;
    float *intermediate;

    /* Calculate atlas dimensions based on DISPLAY size */
    glyph_w = app.font_info.char_width;
    glyph_h = app.font_info.char_height;

    /* Scaled rendering dimensions based on scaled font */
    scaled_w = scaled_font->max_bounds.width;
    scaled_h = scaled_font->ascent + scaled_font->descent;

    atlas_cols = 16;  /* 16x16 grid for 256 glyphs */
    atlas_rows = 16;

    app.font_info.atlas_width = next_power_of_2(atlas_cols * glyph_w);
    app.font_info.atlas_height = next_power_of_2(atlas_rows * glyph_h);

    printf("dash: display glyph %dx%d, scaled glyph %dx%d (scale=%dx), atlas %dx%d\n",
           glyph_w, glyph_h, scaled_w, scaled_h, actual_scale,
           app.font_info.atlas_width, app.font_info.atlas_height);

    if (app.is_impact) {
        bpp = 2;
        format = GL_LUMINANCE_ALPHA;
    }

    /* Allocate intermediate buffer for high-precision data */
    intermediate = calloc(app.font_info.atlas_width * app.font_info.atlas_height, sizeof(float));
    if (!intermediate) {
        fprintf(stderr, "dash: failed to allocate intermediate buffer\n");
        return -1;
    }

    /* Allocate atlas buffer */
    unsigned char *atlas_data;
    atlas_data = (unsigned char*)calloc(
        app.font_info.atlas_width * app.font_info.atlas_height, bpp);

    if (!atlas_data) {
        fprintf(stderr, "dash: failed to allocate atlas buffer\n");
        free(intermediate);
        return -1;
    }

    unsigned char *atlas_data_alpha = atlas_data + (app.is_impact ? 1 : 0);

    if (app.is_impact) {
        int total = app.font_info.atlas_width * app.font_info.atlas_height;
        for (i = 0; i < total; i++) {
            atlas_data[i * 2] = 0xFF;
        }
    }

    GC gc;
    XGCValues gcv;
    /* Create scaled pixmap for rendering */
    Pixmap pixmap = XCreatePixmap(app.display,
                          RootWindow(app.display, DefaultScreen(app.display)),
                          scaled_w, scaled_h, 1);

    gcv.font = scaled_font->fid;  /* Use scaled font for rendering */
    gcv.foreground = 1;
    gcv.background = 0;
    gc = XCreateGC(app.display, pixmap,
                   GCFont | GCForeground | GCBackground, &gcv);

    /* Calculate coverage steps */
    unsigned int step_y = ((unsigned int)scaled_h << 8) / glyph_h;
    unsigned int step_x = ((unsigned int)scaled_w << 8) / glyph_w;

    char str[2] = {0, 0};

    /* Render each glyph and accumulate into atlas */
    for (i = 0; i < NUM_GLYPHS; i++) {
        int col = i % atlas_cols;
        int row = i / atlas_cols;

        /* Clear pixmap */
        XSetForeground(app.display, gc, 0);
        XFillRectangle(app.display, pixmap, gc, 0, 0, scaled_w, scaled_h);
        XSetForeground(app.display, gc, 1);

        /* Draw character using scaled font */
        str[0] = (char)i;
        XDrawString(app.display, pixmap, gc,
                   0, scaled_font->ascent, str, 1);

        /* Get pixmap image */
        XImage *image = XGetImage(app.display, pixmap, 0, 0, scaled_w, scaled_h,
                         1, XYPixmap);

        /* Accumulate pixels into atlas with area coverage (24.8 fixed point) */
        unsigned int sy0 = 0;

        for (int dy = 0; dy < glyph_h; dy++) {
            unsigned int sy1 = sy0 + step_y;
            unsigned int sx0 = 0;

            for (int dx = 0; dx < glyph_w; dx++) {
                unsigned int sx1 = sx0 + step_x;
                unsigned int accum = 0;
                int dest_x = col * glyph_w + dx;
                int dest_y = row * glyph_h + dy;

                if (dest_x < app.font_info.atlas_width &&
                    dest_y < app.font_info.atlas_height) {

                    for (unsigned int py = sy0; py < sy1; py += 256) {
                        unsigned int y_cov;
                        unsigned int bottom = ((py + 256) < sy1) ? (py + 256) : sy1;
                        unsigned int sy = py >> 8;
                        unsigned int row_accum = 0;

                        y_cov = bottom - py;

                        for (unsigned int px = sx0; px < sx1; px += 256) {
                            unsigned int x_cov;
                            unsigned int right = ((px + 256) < sx1) ? (px + 256) : sx1;
                            unsigned int sx = px >> 8;

                            x_cov = right - px;

                            if (XGetPixel(image, sx, sy)) {
                                row_accum += x_cov;
                            }
                        }
                        accum += row_accum * y_cov;
                    }

                    unsigned int idx = dest_y * app.font_info.atlas_width + dest_x;
                    intermediate[idx] = (float)accum;
                }
                sx0 = sx1;
            }
            sy0 = sy1;
        }

        XDestroyImage(image);

        /* Store glyph texture coordinates with pixel/texel center offset */
        /* OpenGL samples at texel centers, so offset by 0.5 pixels */
        app.font_info.glyphs[i].u0 = ((float)(col * glyph_w) + 0.5f) / app.font_info.atlas_width;
        app.font_info.glyphs[i].v0 = ((float)(row * glyph_h) + 0.5f) / app.font_info.atlas_height;
        app.font_info.glyphs[i].u1 = ((float)((col + 1) * glyph_w) - 0.5f) / app.font_info.atlas_width;
        app.font_info.glyphs[i].v1 = ((float)((row + 1) * glyph_h) - 0.5f) / app.font_info.atlas_height;
        app.font_info.glyphs[i].width = glyph_w;
        app.font_info.glyphs[i].height = glyph_h;
    }

    XFreeGC(app.display, gc);
    XFreePixmap(app.display, pixmap);

    /* Pack intermediate data into texture */
    float max_val = 0.0f;
    int total_pixels = app.font_info.atlas_width * app.font_info.atlas_height;

    /* Find max value for normalization */
    for (i = 0; i < total_pixels; i++) {
        if (intermediate[i] > max_val) max_val = intermediate[i];
    }

    for (i = 0; i < total_pixels; i++) {
        float val = intermediate[i];
        unsigned int out_val = 0;
        if (max_val > 0.0f) {
            /* Normalize and apply gamma correction (0.75) to boost dark areas */
            float norm = val / max_val;
            out_val = (unsigned int)(powf(norm, 0.75f) * 255.0f);
        }
        
        atlas_data_alpha[i * bpp] = (unsigned char)out_val;
    }

    free(intermediate);

    /* Create OpenGL texture */
    glGenTextures(1, &app.font_info.atlas_texture);
    glBindTexture(GL_TEXTURE_2D, app.font_info.atlas_texture);
    glEnable(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    while(glGetError() != GL_NO_ERROR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format,
                 app.font_info.atlas_width, app.font_info.atlas_height,
                 0, format, GL_UNSIGNED_BYTE, atlas_data);
    
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        fprintf(stderr, "dash: glTexImage2D failed: 0x%x\n", err);
        exit(1);
    }

    free(atlas_data);

    printf("dash: font atlas created with %dx anti-aliasing\n", actual_scale);

    return 0;
}

/*
 * Update zoom level and rebuild terminal
 */
static void
update_zoom(int step)
{
    int new_size;
    char buf[32];

    if (step == 0) {
        app.zoom_level = 2;
    } else {
        app.zoom_level += step;
    }

    /* Clamp zoom (0.5x to 3.0x) -> level 1 to 6 */
    if (app.zoom_level < 1) app.zoom_level = 1;
    if (app.zoom_level > 6) app.zoom_level = 6;

    new_size = (app.base_font_size * app.zoom_level) / 2;
    if (new_size < 6) new_size = 6; /* Minimum legible size */

    printf("dash: zoom %.1fx (size %d)\n", app.zoom_level / 2.0f, new_size);
    snprintf(buf, sizeof(buf), "ZOOM: %.1fx", app.zoom_level / 2.0f);
    show_osd(buf);

    /* Reload font with new size */
    if (load_font(app.settings.font_family ? app.settings.font_family : "courier", new_size) < 0) {
        fprintf(stderr, "dash: failed to change font size\n");
        return;
    }

    /* Re-setup viewport and vertex arrays with new char dimensions */
    setup_gl_viewport();

    /* Resize all screens */
    resize_screens();

    redraw_terminal();
}

/* Command line options table */ 
static XrmOptionDescRec options[] = {
    {"-dynbg",    "*dynamicBackground", XrmoptionSepArg, NULL},
    {"-fn",       "*fontFamily",        XrmoptionSepArg, NULL},
    {"-size",     "*fontSize",          XrmoptionSepArg, NULL},
    {"-C",        "*consoleMode",       XrmoptionNoArg,  "true"},
    {"-brightness", "*brightness",      XrmoptionSepArg, NULL},
    {"-theme",    "*theme",             XrmoptionSepArg, NULL},
    {"-fg",       "*foreground",        XrmoptionSepArg, NULL},
    {"-bg",       "*background",        XrmoptionSepArg, NULL},
    {"-burn",     "*redrawInterval",    XrmoptionNoArg,  "8"},
};

static XtResource resources[] = {
    {"fontFamily", "FontFamily", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, font_family), XtRImmediate, (XtPointer)"courier"},
    {"fontSize", "FontSize", XtRInt, sizeof(int),
     XtOffsetOf(AppSettings, font_size), XtRImmediate, (XtPointer)14},
    {"dynamicBackground", "DynamicBackground", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, dynamic_background), XtRImmediate, (XtPointer)NULL},
    {"cursorColor", "CursorColor", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, cursor_color), XtRImmediate, (XtPointer)"red"},
    {"scrollbackLines", "ScrollbackLines", XtRInt, sizeof(int),
     XtOffsetOf(AppSettings, scrollback_lines), XtRImmediate, (XtPointer)1000},
    {"consoleMode", "ConsoleMode", XtRBoolean, sizeof(Boolean),
     XtOffsetOf(AppSettings, console_mode), XtRImmediate, (XtPointer)False},
    {"brightness", "Brightness", XtRFloat, sizeof(float),
     XtOffsetOf(AppSettings, brightness), XtRString, (XtPointer)"1.0"},
    {"theme", "Theme", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, theme), XtRImmediate, (XtPointer)"legacy"},
    {"foreground", "Foreground", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, foreground), XtRImmediate, (XtPointer)NULL},
    {"background", "Background", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, background), XtRImmediate, (XtPointer)NULL},
    {"color0", "Color0", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[0]), XtRImmediate, (XtPointer)NULL},
    {"color1", "Color1", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[1]), XtRImmediate, (XtPointer)NULL},
    {"color2", "Color2", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[2]), XtRImmediate, (XtPointer)NULL},
    {"color3", "Color3", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[3]), XtRImmediate, (XtPointer)NULL},
    {"color4", "Color4", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[4]), XtRImmediate, (XtPointer)NULL},
    {"color5", "Color5", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[5]), XtRImmediate, (XtPointer)NULL},
    {"color6", "Color6", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[6]), XtRImmediate, (XtPointer)NULL},
    {"color7", "Color7", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[7]), XtRImmediate, (XtPointer)NULL},
    {"color8", "Color8", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[8]), XtRImmediate, (XtPointer)NULL},
    {"color9", "Color9", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[9]), XtRImmediate, (XtPointer)NULL},
    {"color10", "Color10", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[10]), XtRImmediate, (XtPointer)NULL},
    {"color11", "Color11", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[11]), XtRImmediate, (XtPointer)NULL},
    {"color12", "Color12", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[12]), XtRImmediate, (XtPointer)NULL},
    {"color13", "Color13", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[13]), XtRImmediate, (XtPointer)NULL},
    {"color14", "Color14", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[14]), XtRImmediate, (XtPointer)NULL},
    {"color15", "Color15", XtRString, sizeof(String),
     XtOffsetOf(AppSettings, colors[15]), XtRImmediate, (XtPointer)NULL},
    {"redrawInterval", "RedrawInterval", XtRInt, sizeof(int),
     XtOffsetOf(AppSettings, redraw_interval), XtRImmediate, (XtPointer)DEFAULT_REDRAW_INTERVAL},
};

/*
 * Main entry point
 */
int
main(int argc, char **argv)
{
    XtAppContext app_context;
    Widget work_area, term_form;
    Display *dpy;
    XVisualInfo *visinfo;
    Colormap cmap;
    Arg args[20];
    int n;
    int dblBuf[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 1,
                    GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1, None};
    int snglBuf[] = {GLX_RGBA, GLX_RED_SIZE, 1,
                     GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1, None};

    /* Initialize application state */
    app.num_tabs = 0;
    app.active_tab = -1;
    app.sb_cached_max = -1;
    app.sb_cached_slider = -1;
    app.sb_cached_val = -1;
    app.selecting = 0;
    app.sel_last_x = -1;
    app.sel_last_y = -1;
    app.selection_text = NULL;
    app.redraw_timer_id = 0;
    app.osd_text = NULL;
    app.zoom_level = 2;
    /* base_font_size initialized after loading settings */

    /* Initialize Xt */
    app.toplevel = XtVaOpenApplication(&app_context, "Dash",
        options, XtNumber(options),
        &argc, argv,
        NULL,
        sessionShellWidgetClass,
        XmNtitle, "dash - Terminal Emulator",
        XmNwidth, 800,
        XmNheight, 600,
        NULL);

    /* Save app context for later use */
    app.app_context = app_context;

    /* Get display and choose visual */
    dpy = XtDisplay(app.toplevel);
    app.display = dpy;
    
    /* Intern clipboard atom */
    app.atom_clipboard = XInternAtom(dpy, "CLIPBOARD", False);

    /* Load application resources */
    XtGetApplicationResources(app.toplevel, &app.settings,
                              resources, XtNumber(resources),
                              NULL, 0);

    app.base_font_size = app.settings.font_size > 0 ? app.settings.font_size : 14;
    printf("dash: will use font family '%s', pixel size %d\n", app.settings.font_family, app.settings.font_size);

    /* Try double buffered first */
    visinfo = glXChooseVisual(dpy, DefaultScreen(dpy), dblBuf);
    if (!visinfo) {
        /* Fall back to single buffered */
        visinfo = glXChooseVisual(dpy, DefaultScreen(dpy), snglBuf);
        if (!visinfo) {
            fprintf(stderr, "dash: no suitable OpenGL visual found\n");
            exit(1);
        }
        printf("dash: using single-buffered visual\n");
    } else {
        printf("dash: using double-buffered visual\n");
    }

    /* Create colormap for the visual */
    cmap = XCreateColormap(dpy, RootWindow(dpy, visinfo->screen),
                          visinfo->visual, AllocNone);

    /* Query cursor color */
    {
        XColor color;
        /* Default to red */
        app.cursor_color.r = 1.0f;
        app.cursor_color.g = 0.0f;
        app.cursor_color.b = 0.0f;
        app.cursor_color.a = 0.5f;

        if (app.settings.cursor_color) {
            if (XParseColor(dpy, cmap, app.settings.cursor_color, &color)) {
                app.cursor_color.r = color.red / 65535.0f;
                app.cursor_color.g = color.green / 65535.0f;
                app.cursor_color.b = color.blue / 65535.0f;
            }
        }
    }

    /* Create main window */
    app.main_window = XtVaCreateManagedWidget("mainWindow",
        xmMainWindowWidgetClass, app.toplevel,
        NULL);

    /* Create menu bar */
    app.menubar = create_menubar(app.main_window);

    /* Create work area form */
    work_area = XtVaCreateManagedWidget("workArea",
        xmFormWidgetClass, app.main_window,
        NULL);

    /* Create tab bar (row of buttons at top) */
    app.tab_bar = XtVaCreateManagedWidget("tabBar",
        xmRowColumnWidgetClass, work_area,
        XmNorientation, XmHORIZONTAL,
        XmNpacking, XmPACK_TIGHT,
        XmNtopAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNmarginHeight, 0,
        XmNmarginWidth, 0,
        XmNspacing, 0,
        NULL);

    /* Create terminal display area (form with GL widget and scrollbar) */
    term_form = XtVaCreateManagedWidget("termForm",
        xmFormWidgetClass, work_area,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, app.tab_bar,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_FORM,
        NULL);
    app.term_form = term_form;

    /* Create scrollbar on right side */
    app.scrollbar = XtVaCreateManagedWidget("scrollbar",
        xmScrollBarWidgetClass, term_form,
        XmNorientation, XmVERTICAL,
        XmNtopAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNwidth, 20,
        NULL);
    XtAddCallback(app.scrollbar, XmNvalueChangedCallback, scrollbar_cb, NULL);
    XtAddCallback(app.scrollbar, XmNdragCallback, scrollbar_cb, NULL);

    /* Create GLwMDrawingArea widget with explicit visual */
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNrightWidget, app.scrollbar); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], GLwNvisualInfo, visinfo); n++;
    XtSetArg(args[n], XtNcolormap, cmap); n++;
    XtSetArg(args[n], XmNtraversalOn, True); n++;

    app.gl_area = XtCreateManagedWidget("glArea",
        glwMDrawingAreaWidgetClass, term_form,
        args, n);

    /* Override translations to prevent Motif from stealing navigation keys */
    {
        XtTranslations trans;
        static char *trans_str =
            "<Key>Tab: \n"
            "<Key>ISO_Left_Tab: \n"
            "<Key>Return: \n"
            "<Key>KP_Enter: \n"
            "<Key>Up: \n"
            "<Key>Down: \n"
            "<Key>Left: \n"
            "<Key>Right: \n"
            "<Key>Home: \n"
            "<Key>End: \n"
            "<Key>Page_Up: \n"
            "<Key>Page_Down: \n"
            "<Key>Insert: \n"
            "<Key>Delete: ";
        trans = XtParseTranslationTable(trans_str);
        XtOverrideTranslations(app.gl_area, trans);
    }

    /* Add GL callbacks */
    XtAddCallback(app.gl_area, GLwNginitCallback, gl_init_cb, NULL);
    XtAddCallback(app.gl_area, GLwNresizeCallback, gl_resize_cb, NULL);
    XtAddCallback(app.gl_area, GLwNexposeCallback, gl_expose_cb, NULL);
    
    XtAddEventHandler(app.gl_area, KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask, False, input_event_handler, NULL);

    /* Set menu bar and work area */
    XmMainWindowSetAreas(app.main_window, app.menubar,
        NULL, NULL, NULL, work_area);

    /* Set background init function (will be called on first resize/expose) */
    if (app.settings.dynamic_background && strcmp(app.settings.dynamic_background, "night") == 0) {
        app.bg_init = night_bg_init;
    } else if (app.settings.dynamic_background && strcmp(app.settings.dynamic_background, "sunset") == 0) {
        app.bg_init = sunset_bg_init;
    } else if (app.settings.dynamic_background && strcmp(app.settings.dynamic_background, "ocean") == 0) {
        app.bg_init = ocean_bg_init;
    } else if (app.settings.dynamic_background && strcmp(app.settings.dynamic_background, "aurora") == 0) {
        app.bg_init = aurora_bg_init;
    } else if (app.settings.dynamic_background && strcmp(app.settings.dynamic_background, "embers") == 0) {
        app.bg_init = embers_bg_init;
    } else if (app.settings.dynamic_background && strcmp(app.settings.dynamic_background, "static") == 0) {
        app.bg_init = static_bg_init;
    } else {
        app.bg_init = NULL;
    }
    if (app.settings.dynamic_background)
        printf("background %s %p\n", app.settings.dynamic_background, app.bg_init);

    /* Create initial terminal tab */
    if (create_terminal_tab(app.settings.console_mode ? 1 : 0) >= 0) {
        switch_to_tab(0);
    }

    /* Realize widgets and enter event loop */
    XtRealizeWidget(app.toplevel);

    /* Set initial focus to GL widget */
    XmProcessTraversal(app.gl_area, XmTRAVERSE_CURRENT);

    printf("dash: Terminal emulator started\n");
    printf("dash: Named after a very good cat\n");

    XtAppMainLoop(app_context);

    return 0;
}
