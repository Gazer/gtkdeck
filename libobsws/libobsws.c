#include "libobsws.h"
#include "libuwsc/uwsc.h"
#include <json-glib/json-glib.h>

static gpointer obs_ws_main(gpointer user_data);

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

GList *obs_ws_get_scenes(ObsWs *self) {
    ObsWsClass *klass = OBS_WS_GET_CLASS(self);

    // {"request-type":"GetSceneList","message-id":"4"}
    // struct vhd_minimal_client_echo *vhd = klass->vhd;

    // if (!vhd->established) {
    // printf("Not connected\n");
    // return NULL;
    // }

    // lws_ring_insert()
    return NULL;
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

static void uwsc_onmessage(struct uwsc_client *cl, void *data, size_t len, bool binary) {
    // printf("Recv:");

    if (!binary) {
        JsonParser *parser = json_parser_new();

        if (json_parser_load_from_data(parser, (const gchar *)data, len, NULL)) {
            JsonNode *root = json_parser_get_root(parser);
            if (JSON_NODE_HOLDS_OBJECT(root)) {
                JsonObject *message = json_node_get_object(root);
                const gchar *updateType = json_object_get_string_value(message, "update-type");
                const gchar *currentProfile =
                    json_object_get_string_value(message, "current-profile");
                const gchar *currentScene = json_object_get_string_value(message, "current-scene");
                // const gchar *updateType = json_object_get_boolean_value(message,
                // "recording"); const gchar *updateType = json_object_get_string_value(message,
                // "streaming");

                if (g_strcmp0(updateType, "Heartbeat") == 0) {
                    printf("%s\n", currentScene);
                    klass->heartbeat(currentProfile, currentScene);
                }
            }
        } else {
            // TODO: We may receive multiple packets for the same message
            // We need a buffer and check for lws_is_final_fragment and lws_is_first_fragment
            // printf("Cant parse\n");
            // printf("%s\n", (const gchar *)in);
        }
        g_object_unref(parser);
        // printf("[%.*s]\n", (int)len, (char *)data);
    }
}

static void uwsc_onerror(struct uwsc_client *cl, int err, const char *msg) {
    uwsc_log_err("onerror:%d: %s\n", err, msg);
    ev_break(cl->loop, EVBREAK_ALL);
}

static void uwsc_onclose(struct uwsc_client *cl, int code, const char *reason) {
    uwsc_log_err("onclose:%d: %s\n", code, reason);
    ev_break(cl->loop, EVBREAK_ALL);
}

static void uwsc_send(struct uwsc_client *cl, int message_id, GHashTable *map) {
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
    cl->send(cl, (const void *)data, (size_t)len, 0);

    g_free(data);
    g_object_unref(json);
}

static void uwsc_onopen(struct uwsc_client *cl) {
    // static struct ev_io stdin_watcher;

    uwsc_log_info("onopen\n");

    GHashTable *map = g_hash_table_new(g_str_hash, g_str_equal);
    // {"enable":true,"request-type":"SetHeartbeat","message-id":"1"}
    // {"enable":true,"request-type":"SetHeartbeat","message-id":"2"}
    // g_hash_table_insert(map, "enable", "true");
    // g_hash_table_insert(map, "request-type", "SetHeartbeat");
    // g_hash_table_insert(map, "message-id", message);

    g_hash_table_insert(map, "request-type", "GetAuthRequired");

    int message_id = uwsc_message_id();
    uwsc_send(cl, message_id, map);
}

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents) {
    if (w->signum == SIGINT) {
        ev_break(loop, EVBREAK_ALL);
        uwsc_log_info("Normal quit\n");
    }
}

static gpointer obs_ws_main(gpointer user_data) {
    // Save Global Thread Klass to use in the Protocol
    klass = (ObsWsClass *)user_data;

    const char *url = "ws://127.0.0.1:4444/";
    struct ev_loop *loop = EV_DEFAULT;
    struct ev_signal signal_watcher;
    int ping_interval = 10; /* second */
    struct uwsc_client *cl;

    cl = uwsc_new(loop, url, ping_interval, NULL);
    if (!cl) {
        printf("Can not connect");
        return NULL;
    }

    uwsc_log_info("Start connect...\n");

    cl->onopen = uwsc_onopen;
    cl->onmessage = uwsc_onmessage;
    cl->onerror = uwsc_onerror;
    cl->onclose = uwsc_onclose;

    ev_signal_init(&signal_watcher, signal_cb, SIGINT);
    ev_signal_start(loop, &signal_watcher);

    ev_run(loop, 0);

    free(cl);

    return NULL;
}
