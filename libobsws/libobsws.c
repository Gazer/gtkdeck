#include "libobsws.h"
#include "wic.h"
#include "log.h"

static gpointer obs_ws_main(gpointer user_data);
static int uwsc_message_id();
static void ws_send(struct wic_inst *inst, int message_id, GHashTable *map,
                    result_callback callback, gpointer user_data);
static void ws_send_command(struct wic_inst *inst, const gchar *command, result_callback callback,
                            gpointer user_data);
static void ws_emit(const char *event, JsonObject *object);
static void on_open_handler(struct wic_inst *inst);
static bool on_message_handler(struct wic_inst *inst, enum wic_encoding encoding, bool fin,
                               const char *data, uint16_t size);
static void on_close_handler(struct wic_inst *inst, uint16_t code, const char *reason,
                             uint16_t size);
static void on_close_transport_handler(struct wic_inst *inst);
static void on_send_handler(struct wic_inst *inst, const void *data, size_t size,
                            enum wic_buffer type);
static void on_handshake_failure_handler(struct wic_inst *inst, enum wic_handshake_failure reason);
static void *on_buffer_handler(struct wic_inst *inst, size_t min_size, enum wic_buffer type,
                               size_t *max_size);

typedef struct _ObsWsPrivate {
    char *profile;
    char *scene;
} ObsWsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(ObsWs, obs_ws, G_TYPE_OBJECT)

typedef enum { PROFILE = 1, SCENE, ACTION, N_PROPERTIES } ObsWsProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

static void obs_ws_heartbeat(const gchar *profile, const gchar *scene);

static void obs_ws_set_property(GObject *object, guint property_id, const GValue *value,
                                GParamSpec *pspec) {
    ObsWs *self = OBS_WS(object);
    ObsWsPrivate *priv = obs_ws_get_instance_private(self);

    switch ((ObsWsProperty)property_id) {
    case PROFILE: {
        if (priv->profile != NULL) {
            g_free(priv->profile);
        }
        priv->profile = g_value_dup_string(value);
        break;
    }
    case SCENE: {
        if (priv->scene != NULL) {
            g_free(priv->scene);
        }
        priv->scene = g_value_dup_string(value);
        break;
    }
    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void obs_ws_get_property(GObject *object, guint property_id, GValue *value,
                                GParamSpec *pspec) {
    ObsWs *self = OBS_WS(object);
    ObsWsPrivate *priv = obs_ws_get_instance_private(self);

    switch ((ObsWsProperty)property_id) {
    case PROFILE: {
        g_value_set_string(value, priv->profile);
        break;
    }
    case SCENE: {
        g_value_set_string(value, priv->scene);
        break;
    }
    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void obs_ws_init(ObsWs *self) {
    ObsWsPrivate *priv = obs_ws_get_instance_private(self);

    priv->profile = NULL;
    priv->scene = NULL;
}

static void obs_ws_finalize(GObject *object) {
    ObsWs *self = OBS_WS(object);
    ObsWsPrivate *priv = obs_ws_get_instance_private(self);

    if (priv->profile != NULL) {
        g_free(priv->profile);
    }
    if (priv->scene != NULL) {
        g_free(priv->scene);
    }
}

static void obs_ws_constructed(GObject *object) {}

static GObject *obs_ws_constructor(GType type, guint n_construct_params,
                                   GObjectConstructParam *construct_params) {
    static GObject *self = NULL;

    if (self == NULL) {
        self = G_OBJECT_CLASS(obs_ws_parent_class)
                   ->constructor(type, n_construct_params, construct_params);
        g_object_add_weak_pointer(self, (gpointer)&self);
        return self;
    }

    return g_object_ref(self);
}

static void obs_ws_class_init(ObsWsClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = obs_ws_set_property;
    object_class->get_property = obs_ws_get_property;
    object_class->constructed = obs_ws_constructed;
    object_class->constructor = obs_ws_constructor;

    object_class->finalize = obs_ws_finalize;

    klass->heartbeat = obs_ws_heartbeat;

    klass->thread = g_thread_new("obs", obs_ws_main, klass);
    klass->callbacks = g_hash_table_new(g_str_hash, g_str_equal);
    klass->callbacks_data = g_hash_table_new(g_str_hash, g_str_equal);

    obj_properties[PROFILE] =
        g_param_spec_string("profile", "Profile", "Profile.", NULL, G_PARAM_READWRITE);

    obj_properties[SCENE] =
        g_param_spec_string("scene", "Scene", "Scene.", NULL, G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

// Class Members

static void obs_ws_heartbeat(const gchar *profile, const gchar *scene) {
    ObsWs *obs = obs_ws_new();
    ObsWsPrivate *priv = obs_ws_get_instance_private(obs);

    if (g_strcmp0(profile, priv->profile) != 0) {
        g_object_set(G_OBJECT(obs), "profile", profile, NULL);
    }
    if (g_strcmp0(scene, priv->scene) != 0) {
        g_object_set(G_OBJECT(obs), "scene", scene, NULL);
    }

    g_object_unref(G_OBJECT(obs));
}

// Public Members

ObsWs *obs_ws_new() { return g_object_new(OBS_TYPE_WS, NULL); }

GList *obs_ws_get_scenes(ObsWs *self, result_callback callback, gpointer user_data) {
    ObsWsClass *klass = OBS_WS_GET_CLASS(self);

    ws_send_command(klass->inst, "GetSceneList", callback, user_data);

    return NULL;
}

void obs_ws_set_current_scene(ObsWs *self, const char *scene) {
    ObsWsClass *klass = OBS_WS_GET_CLASS(self);

    GHashTable *map = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(map, "request-type", g_strdup("SetCurrentScene"));
    g_hash_table_insert(map, "scene-name", g_strdup(scene));

    int message_id = uwsc_message_id();
    ws_send(klass->inst, message_id, map, NULL, NULL);
}
// Private

const gchar *json_object_get_string_value(JsonObject *json, const gchar *key) {
    if (json_object_has_member(json, key)) {
        JsonNode *updateType = json_object_get_member(json, key);
        if (JSON_NODE_HOLDS_VALUE(updateType)) {
            return json_node_get_string(updateType);
        }
    }
    return NULL;
}

static ObsWsClass *klass = NULL;

static int uwsc_message_id() {
    static int next_message_id = 0;
    return next_message_id++;
}

static void ws_emit(const char *event, JsonObject *object) {
    GHashTableIter iter;

    gpointer key, value;
    const char event_to_emit[50];

    const char *message_id = json_object_get_string_value(object, "message-id");
    if (message_id == NULL) {
        g_sprintf(event_to_emit, "%s", event);
    } else {
        g_sprintf(event_to_emit, "emit:message:%s", message_id);
    }

    g_hash_table_iter_init(&iter, klass->callbacks);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        if (g_strcmp0(event_to_emit, key) == 0) {
            printf("got callback for %s ... calling\n", event_to_emit);
            result_callback callback = (result_callback)value;
            if (callback != NULL) {
                gpointer user_data;
                g_hash_table_lookup_extended(klass->callbacks_data, event_to_emit, NULL,
                                             &user_data);
                printf("got data for %s ... %p\n", event_to_emit, user_data);
                callback(object, user_data);
            }
        }
    }
}

static ws_emit_register_callback(const char *callbackId, result_callback callback,
                                 gpointer user_data) {
    printf("Registering callback for %s with data %p\n", callbackId, user_data);
    printf("%d\n", g_hash_table_insert(klass->callbacks, g_strdup(callbackId), callback));
    printf("%d\n", g_hash_table_insert(klass->callbacks_data, g_strdup(callbackId), user_data));
}

static void ws_send(struct wic_inst *inst, int message_id, GHashTable *map,
                    result_callback callback, gpointer user_data) {
    char message[10];
    g_sprintf(message, "%d", message_id);
    g_hash_table_insert(map, "message-id", g_strdup(message));

    GHashTableIter iter;

    gpointer key, value;
    JsonGenerator *json = json_generator_new();
    JsonObject *obj = json_object_new();
    JsonNode *node = json_node_new(JSON_NODE_OBJECT);

    g_hash_table_iter_init(&iter, map);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        if (g_strcmp0("true", value) == 0) {
            json_object_set_boolean_member(obj, key, TRUE);
        } else {
            json_object_set_string_member(obj, key, value);
        }
    }

    json_node_set_object(node, obj);
    json_generator_set_root(json, node);

    gsize len;
    gchar *data = json_generator_to_data(json, &len);
    printf(">> %s\n", data);

    char callback_id[50];
    g_sprintf(callback_id, "emit:message:%d", message_id);

    ws_emit_register_callback(callback_id, callback, user_data);
    wic_send_text(inst, true, data, len);

    g_free(data);
    g_object_unref(json);
}

static void ws_send_command(struct wic_inst *inst, const gchar *command, result_callback callback,
                            gpointer user_data) {
    GHashTable *map = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(map, "request-type", g_strdup(command));

    int message_id = uwsc_message_id();
    ws_send(inst, message_id, map, callback, user_data);
}

// static void ws_onopen(struct uwsc_client *cl) {
//     // static struct ev_io stdin_watcher;

//     uwsc_log_info("onopen\n");

//     GHashTable *map = g_hash_table_new(g_str_hash, g_str_equal);
//     // {"enable":true,"request-type":"SetHeartbeat","message-id":"1"}
//     // {"enable":true,"request-type":"SetHeartbeat","message-id":"2"}
//     // g_hash_table_insert(map, "enable", "true");
//     // g_hash_table_insert(map, "request-type", "SetHeartbeat");
//     // g_hash_table_insert(map, "message-id", message);

//     g_hash_table_insert(map, "request-type", "GetAuthRequired");

//     int message_id = uwsc_message_id();
//     uwsc_send(cl, message_id, map);
// }

// static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents) {
//     if (w->signum == SIGINT) {
//         ev_break(loop, EVBREAK_ALL);
//         uwsc_log_info("Normal quit\n");
//     }
// }

static gpointer obs_ws_main(gpointer user_data) {
    // Save Global Thread Klass to use in the Protocol
    klass = (ObsWsClass *)user_data;

    int s, redirects = 3;
    static uint8_t rx[1000];
    static char url[1000] = "ws://127.0.0.1:4444/";
    struct wic_inst inst;
    struct wic_init_arg arg = {0};

    arg.rx = rx;
    arg.rx_max = sizeof(rx);
    arg.on_send = on_send_handler;
    arg.on_buffer = on_buffer_handler;
    arg.on_message = on_message_handler;
    arg.on_open = on_open_handler;
    arg.on_close = on_close_handler;
    arg.on_close_transport = on_close_transport_handler;
    arg.on_handshake_failure = on_handshake_failure_handler;
    arg.app = &s;
    arg.url = url;
    arg.role = WIC_ROLE_CLIENT;

    for (;;) {

        if (!wic_init(&inst, &arg)) {

            exit(EXIT_FAILURE);
        };

        struct wic_header user_agent = {.name = "User-Agent", .value = "wic"};

        (void)wic_set_header(&inst, &user_agent);

        if (transport_open_client(wic_get_url_schema(&inst), wic_get_url_hostname(&inst),
                                  wic_get_url_port(&inst), &s)) {

            if (wic_start(&inst) == WIC_STATUS_SUCCESS) {
                klass->inst = &inst;
                while (transport_recv(s, &inst))
                    ;
            } else {

                transport_close(&s);
            }
        }

        if (wic_get_redirect_url(&inst) && redirects) {

            redirects--;
            strcpy(url, wic_get_redirect_url(&inst));
        } else {

            break;
        }
    }

    return NULL;
}

GString *message_buffer = NULL;
static bool on_message_handler(struct wic_inst *inst, enum wic_encoding encoding, bool fin,
                               const char *data, uint16_t size) {

    if (message_buffer == NULL) {
        message_buffer = g_string_new(data);
    } else {
        g_string_append_len(message_buffer, data, size);
    }

    if (fin) {
        JsonParser *parser = json_parser_new();
        if (json_parser_load_from_data(parser, (const gchar *)message_buffer->str,
                                       message_buffer->len, NULL)) {
            JsonNode *root = json_parser_get_root(parser);
            if (JSON_NODE_HOLDS_OBJECT(root)) {
                JsonObject *message = json_node_get_object(root);
                ws_emit("", message);
            }
        }

        g_string_free(message_buffer, TRUE);
        message_buffer = NULL;
    }

    return true;
}

static void on_handshake_failure_handler(struct wic_inst *inst, enum wic_handshake_failure reason) {
    LOG("websocket handshake failed for reason %d", reason);
}

void on_get_auth_result(JsonObject *object, gpointer user_data) {
    // TODO: Check if we need to login
}

static void on_open_handler(struct wic_inst *inst) {
    const char *name, *value;

    LOG("websocket is open");

    LOG("received handshake:");

    for (value = wic_get_next_header(inst, &name); value;
         value = wic_get_next_header(inst, &name)) {

        LOG("%s: %s", name, value);
    }

    ws_send_command(inst, "GetAuthRequired", on_get_auth_result, NULL);
}

static void on_close_handler(struct wic_inst *inst, uint16_t code, const char *reason,
                             uint16_t size) {
    LOG("websocket closed for reason %u", code);
}

static void on_close_transport_handler(struct wic_inst *inst) {
    transport_close((int *)wic_get_app(inst));
}

static void on_send_handler(struct wic_inst *inst, const void *data, size_t size,
                            enum wic_buffer type) {
    LOG("sending buffer type %d", type);

    transport_write(*(int *)wic_get_app(inst), data, size);
}

static void *on_buffer_handler(struct wic_inst *inst, size_t min_size, enum wic_buffer type,
                               size_t *max_size) {
    static uint8_t tx[1000U];

    *max_size = sizeof(tx);

    return (min_size <= sizeof(tx)) ? tx : NULL;
}