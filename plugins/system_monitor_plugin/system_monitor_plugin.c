#include "system_monitor_plugin.h"

#include <cairo.h>
#include <fcntl.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CPU_ACTION_CODE 0
#define RAM_ACTION_CODE 1

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

typedef struct {
    unsigned long long total;
    unsigned long long free;
    unsigned long long available;
    unsigned long long buffers;
    unsigned long long cached;
    unsigned long long sreclaimable;
} MemInfo;

typedef struct _SystemMonitorPluginPrivate {
    // CPU
    CpuStat prev_cpu_stat;
    guint cpu_update_timer;
    int cpu_percent;

    // RAM
    MemInfo prev_mem_info;
    guint ram_update_timer;
    int ram_percent;

    // Common
    int action_type; // 0 = CPU, 1 = RAM
    gchar *command;
} SystemMonitorPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(SystemMonitorPlugin, system_monitor_plugin, DECK_TYPE_PLUGIN)

typedef enum { SYSTEM_MONITOR_PROP_COMMAND, N_PROPERTIES } SystemMonitorPluginProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {NULL};

DeckPlugin *system_monitor_plugin_clone(DeckPlugin *self, int action);
DeckPlugin *system_monitor_plugin_clone_with_code(DeckPlugin *self, int code);
DeckPluginInfo *system_monitor_plugin_info(DeckPlugin *self);
static void system_monitor_plugin_render(DeckPlugin *self, cairo_t *cr, int width, int height);

// CPU Functions
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

// RAM Functions
static gboolean read_mem_info(MemInfo *info) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        return FALSE;
    }

    char line[256];
    unsigned long long value;
    char key[32];

    // Initialize all fields
    info->total = 0;
    info->free = 0;
    info->available = 0;
    info->buffers = 0;
    info->cached = 0;
    info->sreclaimable = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%31s %llu", key, &value) == 2) {
            // Remove the colon from key
            size_t len = strlen(key);
            if (len > 0 && key[len - 1] == ':') {
                key[len - 1] = '\0';
            }

            if (strcmp(key, "MemTotal") == 0) {
                info->total = value;
            } else if (strcmp(key, "MemFree") == 0) {
                info->free = value;
            } else if (strcmp(key, "MemAvailable") == 0) {
                info->available = value;
            } else if (strcmp(key, "Buffers") == 0) {
                info->buffers = value;
            } else if (strcmp(key, "Cached") == 0) {
                info->cached = value;
            } else if (strcmp(key, "SReclaimable") == 0) {
                info->sreclaimable = value;
            }
        }
    }

    fclose(fp);
    return info->total > 0;
}

static int calculate_ram_percent(MemInfo *info) {
    // Use MemAvailable if available (Linux 3.14+), otherwise calculate manually
    unsigned long long used;
    if (info->available > 0) {
        used = info->total - info->available;
    } else {
        // Fallback: total - free - buffers - cached - sreclaimable
        used = info->total - info->free - info->buffers - info->cached - info->sreclaimable;
    }

    if (info->total == 0) {
        return 0;
    }

    return (int)((used * 100) / info->total);
}

// Update timers
static gboolean cpu_update_timer(gpointer user_data) {
    SystemMonitorPlugin *self = SYSTEM_MONITOR_PLUGIN(user_data);
    SystemMonitorPluginPrivate *priv = system_monitor_plugin_get_instance_private(self);

    CpuStat curr_stat;
    if (read_cpu_stat(&curr_stat)) {
        priv->cpu_percent = calculate_cpu_percent(&priv->prev_cpu_stat, &curr_stat);
        priv->prev_cpu_stat = curr_stat;

        if (priv->action_type == 0) {
            deck_plugin_reset(DECK_PLUGIN(self));
        }
    }

    return G_SOURCE_CONTINUE;
}

static gboolean ram_update_timer(gpointer user_data) {
    SystemMonitorPlugin *self = SYSTEM_MONITOR_PLUGIN(user_data);
    SystemMonitorPluginPrivate *priv = system_monitor_plugin_get_instance_private(self);

    MemInfo curr_info;
    if (read_mem_info(&curr_info)) {
        priv->ram_percent = calculate_ram_percent(&curr_info);
        priv->prev_mem_info = curr_info;

        if (priv->action_type == 1) {
            deck_plugin_reset(DECK_PLUGIN(self));
        }
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

static void system_monitor_config(DeckPlugin *self, GtkBox *parent);
static void system_monitor_exec(DeckPlugin *self);
static void system_monitor_save(DeckPlugin *self, char *group, GKeyFile *key_file);
static void system_monitor_load(DeckPlugin *self, char *group, GKeyFile *key_file);

static void on_command_changed(GtkEditable *editable, gpointer user_data) {
    SystemMonitorPlugin *self = SYSTEM_MONITOR_PLUGIN(user_data);
    SystemMonitorPluginPrivate *priv = system_monitor_plugin_get_instance_private(self);

    char *text = gtk_editable_get_chars(editable, 0, -1);
    g_free(priv->command);
    if (strlen(text) > 0) {
        priv->command = g_strdup(text);
    } else {
        priv->command = NULL;
    }
    g_free(text);
}

static void system_monitor_config(DeckPlugin *self, GtkBox *parent) {
    SystemMonitorPluginPrivate *priv =
        system_monitor_plugin_get_instance_private(SYSTEM_MONITOR_PLUGIN(self));

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

static void system_monitor_exec(DeckPlugin *self) {
    SystemMonitorPluginPrivate *priv =
        system_monitor_plugin_get_instance_private(SYSTEM_MONITOR_PLUGIN(self));

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

static void system_monitor_save(DeckPlugin *self, char *group, GKeyFile *key_file) {
    SystemMonitorPluginPrivate *priv =
        system_monitor_plugin_get_instance_private(SYSTEM_MONITOR_PLUGIN(self));

    if (priv->command != NULL) {
        g_key_file_set_string(key_file, group, "command", priv->command);
    }
    g_key_file_set_integer(key_file, group, "action_type", priv->action_type);
}

static void system_monitor_load(DeckPlugin *self, char *group, GKeyFile *key_file) {
    SystemMonitorPluginPrivate *priv =
        system_monitor_plugin_get_instance_private(SYSTEM_MONITOR_PLUGIN(self));

    g_autofree gchar *command = g_key_file_get_string(key_file, group, "command", NULL);
    if (command != NULL) {
        g_free(priv->command);
        priv->command = g_strdup(command);
    }

    if (g_key_file_has_key(key_file, group, "action_type", NULL)) {
        priv->action_type = g_key_file_get_integer(key_file, group, "action_type", NULL);
    }
}

DeckPluginInfo SYSTEM_MONITOR_PLUGIN_INFO = {
    "SystemMonitor",
    2,
    {
        {BUTTON_MODE_NORMAL, "CPU Usage", "/ar/com/p39/gtkdeck/plugins/cpu.png", NULL,
         CPU_ACTION_CODE, system_monitor_config, system_monitor_exec, system_monitor_save,
         system_monitor_load},
        {BUTTON_MODE_NORMAL, "RAM Usage", "/ar/com/p39/gtkdeck/plugins/ram.png", NULL,
         RAM_ACTION_CODE, system_monitor_config, system_monitor_exec, system_monitor_save,
         system_monitor_load},
    },
};

static void system_monitor_plugin_init(SystemMonitorPlugin *self) {
    SystemMonitorPluginPrivate *priv = system_monitor_plugin_get_instance_private(self);

    // Initialize CPU
    memset(&priv->prev_cpu_stat, 0, sizeof(CpuStat));
    read_cpu_stat(&priv->prev_cpu_stat);
    priv->cpu_percent = 0;

    // Initialize RAM
    memset(&priv->prev_mem_info, 0, sizeof(MemInfo));
    read_mem_info(&priv->prev_mem_info);
    priv->ram_percent = calculate_ram_percent(&priv->prev_mem_info);

    priv->action_type = 0; // Default to CPU
    priv->command = NULL;

    // Start update timers
    priv->cpu_update_timer = g_timeout_add_seconds(1, cpu_update_timer, self);
    priv->ram_update_timer = g_timeout_add_seconds(1, ram_update_timer, self);
}

static void system_monitor_plugin_finalize(GObject *object) {
    SystemMonitorPlugin *self = SYSTEM_MONITOR_PLUGIN(object);
    SystemMonitorPluginPrivate *priv = system_monitor_plugin_get_instance_private(self);

    if (priv->cpu_update_timer > 0) {
        g_source_remove(priv->cpu_update_timer);
        priv->cpu_update_timer = 0;
    }

    if (priv->ram_update_timer > 0) {
        g_source_remove(priv->ram_update_timer);
        priv->ram_update_timer = 0;
    }

    g_free(priv->command);

    G_OBJECT_CLASS(system_monitor_plugin_parent_class)->finalize(object);
}

static void system_monitor_plugin_set_property(GObject *object, guint property_id,
                                               const GValue *value, GParamSpec *pspec) {
    SystemMonitorPlugin *self = SYSTEM_MONITOR_PLUGIN(object);
    SystemMonitorPluginPrivate *priv = system_monitor_plugin_get_instance_private(self);

    switch ((SystemMonitorPluginProperty)property_id) {
    case SYSTEM_MONITOR_PROP_COMMAND:
        g_free(priv->command);
        priv->command = g_strdup(g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void system_monitor_plugin_get_property(GObject *object, guint property_id, GValue *value,
                                               GParamSpec *pspec) {
    SystemMonitorPlugin *self = SYSTEM_MONITOR_PLUGIN(object);
    SystemMonitorPluginPrivate *priv = system_monitor_plugin_get_instance_private(self);

    switch ((SystemMonitorPluginProperty)property_id) {
    case SYSTEM_MONITOR_PROP_COMMAND:
        g_value_set_string(value, priv->command);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void system_monitor_plugin_class_init(SystemMonitorPluginClass *klass) {
    DeckPluginClass *deck_plugin_klass = DECK_PLUGIN_CLASS(klass);

    deck_plugin_klass->info = system_monitor_plugin_info;
    deck_plugin_klass->clone = system_monitor_plugin_clone;
    deck_plugin_klass->clone_with_code = system_monitor_plugin_clone_with_code;
    deck_plugin_klass->render = system_monitor_plugin_render;

    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->set_property = system_monitor_plugin_set_property;
    object_class->get_property = system_monitor_plugin_get_property;
    object_class->finalize = system_monitor_plugin_finalize;

    obj_properties[SYSTEM_MONITOR_PROP_COMMAND] = g_param_spec_string(
        "command", "Command", "Command to execute on press.", NULL, G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

DeckPlugin *system_monitor_plugin_new() {
    return g_object_new(SYSTEM_MONITOR_TYPE_PLUGIN, "name", "SystemMonitor", NULL);
}

DeckPlugin *system_monitor_plugin_clone(DeckPlugin *self, int action) {
    SystemMonitorPlugin *clone =
        g_object_new(SYSTEM_MONITOR_TYPE_PLUGIN, "name", "SystemMonitor", "action",
                     &SYSTEM_MONITOR_PLUGIN_INFO.actions[action], NULL);
    SystemMonitorPluginPrivate *priv = system_monitor_plugin_get_instance_private(clone);
    priv->action_type = action;
    return DECK_PLUGIN(clone);
}

DeckPluginInfo *system_monitor_plugin_info(DeckPlugin *self) { return &SYSTEM_MONITOR_PLUGIN_INFO; }

DeckPlugin *system_monitor_plugin_clone_with_code(DeckPlugin *self, int code) {
    for (int i = 0; i < SYSTEM_MONITOR_PLUGIN_INFO.actions_count; i++) {
        if (code == SYSTEM_MONITOR_PLUGIN_INFO.actions[i].code) {
            return system_monitor_plugin_clone(self, i);
        }
    }
    return NULL;
}

static void system_monitor_plugin_render(DeckPlugin *self, cairo_t *cr, int width, int height) {
    SystemMonitorPluginPrivate *priv =
        system_monitor_plugin_get_instance_private(SYSTEM_MONITOR_PLUGIN(self));

    gchar *text;
    if (priv->action_type == 0) {
        // CPU
        text = g_strdup_printf("%d%%", priv->cpu_percent);
    } else {
        // RAM
        text = g_strdup_printf("%d%%", priv->ram_percent);
    }

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
