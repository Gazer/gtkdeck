#include "cpu_plugin.h"

#include <cairo.h>
#include <fcntl.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CPU_ACTION_CODE 0

typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
} CpuStat;

typedef struct _CpuPluginPrivate {
    CpuStat prev_stat;
    guint update_timer;
    int cpu_percent;

    gchar *command;
} CpuPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(CpuPlugin, cpu_plugin, DECK_TYPE_PLUGIN)

typedef enum {
    CPU_PROP_COMMAND,
    N_PROPERTIES
} CpuPluginProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {NULL};

DeckPlugin *cpu_plugin_clone(DeckPlugin *self, int action);
DeckPlugin *cpu_plugin_clone_with_code(DeckPlugin *self, int code);
DeckPluginInfo *cpu_plugin_info(DeckPlugin *self);
static void cpu_plugin_render(DeckPlugin *self, cairo_t *cr, int width, int height);

static gboolean read_cpu_stat(CpuStat *stat) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        return FALSE;
    }

    char line[256];
    if (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu ", 4) == 0) {
            sscanf(line + 4, "%llu %llu %llu %llu %llu %llu %llu %llu", &stat->user, &stat->nice,
                   &stat->system, &stat->idle, &stat->iowait, &stat->irq, &stat->softirq,
                   &stat->steal);
            fclose(fp);
            return TRUE;
        }
    }

    fclose(fp);
    return FALSE;
}

static int calculate_cpu_percent(CpuStat *prev, CpuStat *curr) {
    unsigned long long prev_idle = prev->idle + prev->iowait;
    unsigned long long curr_idle = curr->idle + curr->iowait;

    unsigned long long prev_total = prev->user + prev->nice + prev->system + prev->irq +
                                    prev->softirq + prev->steal + prev_idle;
    unsigned long long curr_total = curr->user + curr->nice + curr->system + curr->irq +
                                    curr->softirq + curr->steal + curr_idle;

    unsigned long long total_diff = curr_total - prev_total;
    unsigned long long idle_diff = curr_idle - prev_idle;

    if (total_diff == 0) {
        return 0;
    }

    return (int)(((total_diff - idle_diff) * 100) / total_diff);
}

static gboolean cpu_update_timer(gpointer user_data) {
    CpuPlugin *self = CPU_PLUGIN(user_data);
    CpuPluginPrivate *priv = cpu_plugin_get_instance_private(self);

    CpuStat curr_stat;
    if (read_cpu_stat(&curr_stat)) {
        priv->cpu_percent = calculate_cpu_percent(&priv->prev_stat, &curr_stat);
        priv->prev_stat = curr_stat;

        deck_plugin_reset(DECK_PLUGIN(self));
    }

    return G_SOURCE_CONTINUE;
}

static gchar *find_system_monitor() {
    const gchar *commands[] = {"gnome-system-monitor",
                               "ksysguard",
                               "xfce4-taskmanager",
                               "mate-system-monitor",
                               "htop",
                               "top",
                               NULL};

    for (int i = 0; commands[i] != NULL; i++) {
        gchar *path = g_find_program_in_path(commands[i]);
        if (path != NULL) {
            return path;
        }
    }

    return g_strdup("xterm -e top");
}

static void cpu_config(DeckPlugin *self, GtkBox *parent);
static void cpu_exec(DeckPlugin *self);
static void cpu_save(DeckPlugin *self, char *group, GKeyFile *key_file);
static void cpu_load(DeckPlugin *self, char *group, GKeyFile *key_file);

static void on_command_changed(GtkEditable *editable, gpointer user_data) {
    CpuPlugin *self = CPU_PLUGIN(user_data);
    CpuPluginPrivate *priv = cpu_plugin_get_instance_private(self);

    char *text = gtk_editable_get_chars(editable, 0, -1);
    g_free(priv->command);
    if (strlen(text) > 0) {
        priv->command = g_strdup(text);
    } else {
        priv->command = NULL;
    }
    g_free(text);
}

static void cpu_config(DeckPlugin *self, GtkBox *parent) {
    CpuPluginPrivate *priv = cpu_plugin_get_instance_private(CPU_PLUGIN(self));

    GtkWidget *hbox_cmd = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl_cmd = gtk_label_new("Command:");
    GtkWidget *entry_cmd = gtk_entry_new();
    if (priv->command != NULL) {
        gtk_entry_set_text(GTK_ENTRY(entry_cmd), priv->command);
    }
    gtk_widget_set_tooltip_text(entry_cmd,
                                "Command to run when button is pressed (default: system monitor)");
    g_signal_connect(entry_cmd, "changed", G_CALLBACK(on_command_changed), self);
    gtk_box_pack_start(GTK_BOX(hbox_cmd), lbl_cmd, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(hbox_cmd), entry_cmd, TRUE, TRUE, 5);
    gtk_box_pack_start(parent, GTK_WIDGET(hbox_cmd), TRUE, FALSE, 5);
}

static void cpu_exec(DeckPlugin *self) {
    CpuPluginPrivate *priv = cpu_plugin_get_instance_private(CPU_PLUGIN(self));

    gchar *cmd = priv->command != NULL ? priv->command : find_system_monitor();

    GError *error = NULL;
    if (!g_spawn_command_line_async(cmd, &error)) {
        g_warning("Failed to launch system monitor: %s", error->message);
        g_error_free(error);
    }

    if (priv->command == NULL) {
        g_free(cmd);
    }
}

static void cpu_save(DeckPlugin *self, char *group, GKeyFile *key_file) {
    CpuPluginPrivate *priv = cpu_plugin_get_instance_private(CPU_PLUGIN(self));

    if (priv->command != NULL) {
        g_key_file_set_string(key_file, group, "command", priv->command);
    }
}

static void cpu_load(DeckPlugin *self, char *group, GKeyFile *key_file) {
    CpuPluginPrivate *priv = cpu_plugin_get_instance_private(CPU_PLUGIN(self));

    g_autofree gchar *command = g_key_file_get_string(key_file, group, "command", NULL);
    if (command != NULL) {
        g_free(priv->command);
        priv->command = g_strdup(command);
    }
}

DeckPluginInfo CPU_PLUGIN_INFO = {
    "CPU",
    1,
    {
        {BUTTON_MODE_NORMAL, "CPU Usage", "/ar/com/p39/gtkdeck/plugins/cpu.png", NULL,
         CPU_ACTION_CODE, cpu_config, cpu_exec, cpu_save, cpu_load},
    },
};

static void cpu_plugin_init(CpuPlugin *self) {
    CpuPluginPrivate *priv = cpu_plugin_get_instance_private(self);

    // Initialize CPU stat
    memset(&priv->prev_stat, 0, sizeof(CpuStat));
    read_cpu_stat(&priv->prev_stat);

    priv->cpu_percent = 0;
    priv->command = NULL;

    priv->update_timer = g_timeout_add_seconds(1, cpu_update_timer, self);
}

static void cpu_plugin_finalize(GObject *object) {
    CpuPlugin *self = CPU_PLUGIN(object);
    CpuPluginPrivate *priv = cpu_plugin_get_instance_private(self);

    if (priv->update_timer > 0) {
        g_source_remove(priv->update_timer);
        priv->update_timer = 0;
    }

    g_free(priv->command);

    G_OBJECT_CLASS(cpu_plugin_parent_class)->finalize(object);
}

static void cpu_plugin_set_property(GObject *object, guint property_id, const GValue *value,
                                    GParamSpec *pspec) {
    CpuPlugin *self = CPU_PLUGIN(object);
    CpuPluginPrivate *priv = cpu_plugin_get_instance_private(self);

    switch ((CpuPluginProperty)property_id) {
    case CPU_PROP_COMMAND:
        g_free(priv->command);
        priv->command = g_strdup(g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void cpu_plugin_get_property(GObject *object, guint property_id, GValue *value,
                                    GParamSpec *pspec) {
    CpuPlugin *self = CPU_PLUGIN(object);
    CpuPluginPrivate *priv = cpu_plugin_get_instance_private(self);

    switch ((CpuPluginProperty)property_id) {
    case CPU_PROP_COMMAND:
        g_value_set_string(value, priv->command);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void cpu_plugin_class_init(CpuPluginClass *klass) {
    DeckPluginClass *deck_plugin_klass = DECK_PLUGIN_CLASS(klass);

    deck_plugin_klass->info = cpu_plugin_info;
    deck_plugin_klass->clone = cpu_plugin_clone;
    deck_plugin_klass->clone_with_code = cpu_plugin_clone_with_code;
    deck_plugin_klass->render = cpu_plugin_render;

    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->set_property = cpu_plugin_set_property;
    object_class->get_property = cpu_plugin_get_property;
    object_class->finalize = cpu_plugin_finalize;

    obj_properties[CPU_PROP_COMMAND] = g_param_spec_string(
        "command", "Command", "Command to execute on press.", NULL, G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

DeckPlugin *cpu_plugin_new() { return g_object_new(CPU_TYPE_PLUGIN, "name", "CPU", NULL); }

DeckPlugin *cpu_plugin_clone(DeckPlugin *self, int action) {
    return g_object_new(CPU_TYPE_PLUGIN, "name", "CPU", "action", &CPU_PLUGIN_INFO.actions[action],
                        NULL);
}

DeckPluginInfo *cpu_plugin_info(DeckPlugin *self) { return &CPU_PLUGIN_INFO; }

DeckPlugin *cpu_plugin_clone_with_code(DeckPlugin *self, int code) {
    for (int i = 0; i < CPU_PLUGIN_INFO.actions_count; i++) {
        if (code == CPU_PLUGIN_INFO.actions[i].code) {
            return cpu_plugin_clone(self, i);
        }
    }
    return NULL;
}

static void cpu_plugin_render(DeckPlugin *self, cairo_t *cr, int width, int height) {
    CpuPluginPrivate *priv = cpu_plugin_get_instance_private(CPU_PLUGIN(self));

    gchar *text = g_strdup_printf("%d%%", priv->cpu_percent);

    PangoLayout *layout = pango_cairo_create_layout(cr);
    pango_layout_set_text(layout, text, -1);

    PangoFontDescription *desc = pango_font_description_from_string("Sans Bold 60");
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    int text_width, text_height;
    pango_layout_get_size(layout, &text_width, &text_height);
    text_width /= PANGO_SCALE;
    text_height /= PANGO_SCALE;

    int x = (width - text_width) / 2;
    int y = (height - text_height) / 2;

    cairo_save(cr);

    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_move_to(cr, x + 2, y + 2);
    pango_cairo_show_layout(cr, layout);

    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, layout);

    cairo_restore(cr);

    g_free(text);
    g_object_unref(layout);
}
