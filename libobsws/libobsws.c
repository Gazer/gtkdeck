#include "libobsws.h"
#include "log.h"
#include "transport.h"
#include "wic.h"
#include <glib.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

// OBS WebSocket v5 OpCodes
#define OPCODE_HELLO 0
#define OPCODE_IDENTIFY 1
#define OPCODE_IDENTIFIED 2
#define OPCODE_REIDENTIFY 3
#define OPCODE_EVENT 5
#define OPCODE_REQUEST 6
#define OPCODE_REQUEST_RESPONSE 7
#define OPCODE_REQUEST_BATCH 8
#define OPCODE_REQUEST_BATCH_RESPONSE 9

#define RPC_VERSION 1

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

typedef enum { PROFILE = 1, SCENE, N_PROPERTIES } ObsWsProperty;

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
    klass->callbacks =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_list_free);
    klass->callbacks_data =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_list_free);

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
static ObsWs *global_ws = NULL;
static gchar *auth_password = NULL;
static GMutex ws_mutex;
static gboolean mutex_initialized = FALSE;

#define LOCK_WS() g_mutex_lock(&ws_mutex)
#define UNLOCK_WS() g_mutex_unlock(&ws_mutex)

ObsWs *obs_ws_new() {
    if (!mutex_initialized) {
        g_mutex_init(&ws_mutex);
        mutex_initialized = TRUE;
    }
    if (global_ws == NULL) {
        printf("create .....................\n");
        global_ws = g_object_new(OBS_TYPE_WS, NULL);
    }
    return global_ws;
}

void obs_ws_set_password(const gchar *password) {
    if (auth_password != NULL) {
        g_free(auth_password);
    }
    auth_password = g_strdup(password);
}

GList *obs_ws_get_scenes(ObsWs *self, result_callback callback, gpointer user_data) {
    // ObsWsPrivate *priv = obs_ws_get_instance_private(self);
    ObsWsClass *klass = OBS_WS_GET_CLASS(self);

    // Get inst pointer with lock, then release before sending
    LOCK_WS();
    struct wic_inst *inst = klass->inst;
    UNLOCK_WS();

    if (inst != NULL) {
        ws_send_command(inst, "GetSceneList", callback, user_data);
    }

    return NULL;
}

void on_scene_set(JsonObject *json, gpointer user_data) { printf("scene changed\n"); }

void obs_ws_set_current_scene(ObsWs *self, const char *scene) {
    // ObsWsPrivate *priv = obs_ws_get_instance_private(self);
    ObsWsClass *klass = OBS_WS_GET_CLASS(self);

    GHashTable *map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(map, g_strdup("request-type"), g_strdup("SetCurrentProgramScene"));
    g_hash_table_insert(map, g_strdup("sceneName"), g_strdup(scene));

    int message_id = uwsc_message_id();

    // Get inst pointer with lock, then release before sending
    LOCK_WS();
    struct wic_inst *inst = klass->inst;
    UNLOCK_WS();

    if (inst != NULL) {
        ws_send(inst, message_id, map, on_scene_set, self);
    }

    g_hash_table_unref(map);
}

void obs_ws_get_current_scene(ObsWs *self, result_callback callback, gpointer user_data) {
    // ObsWsPrivate *priv = obs_ws_get_instance_private(self);
    ObsWsClass *klass = OBS_WS_GET_CLASS(self);

    GHashTable *map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(map, g_strdup("request-type"), g_strdup("GetCurrentProgramScene"));

    int message_id = uwsc_message_id();

    // Get inst pointer with lock, then release before sending
    LOCK_WS();
    struct wic_inst *inst = klass->inst;
    UNLOCK_WS();

    if (inst != NULL) {
        ws_send(inst, message_id, map, callback, user_data);
    }

    g_hash_table_unref(map);
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

const gboolean json_object_get_boolean_value(JsonObject *json, const gchar *key) {
    if (json_object_has_member(json, key)) {
        JsonNode *updateType = json_object_get_member(json, key);
        if (JSON_NODE_HOLDS_VALUE(updateType)) {
            return json_node_get_boolean(updateType);
        }
    }
    return FALSE;
}

JsonObject *json_object_from_boolean_value(gboolean value) {
    JsonObject *object = json_object_new();
    JsonNode *node = json_node_new(JSON_NODE_VALUE);
    json_node_set_boolean(node, value);
    json_object_set_member(object, "value", node);

    return object;
}

static ObsWsClass *klass = NULL;

static int uwsc_message_id() {
    static int next_message_id = 0;
    return next_message_id++;
}

// Base64 encoding helper
static gchar *base64_encode(const guchar *data, gsize len) {
    BIO *bio, *b64;
    BUF_MEM *buffer_ptr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, data, len);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &buffer_ptr);

    gchar *result = g_strndup(buffer_ptr->data, buffer_ptr->length);

    BIO_free_all(bio);
    return result;
}

// SHA256 helper
static gchar *sha256_hash(const gchar *input) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_sha256();
    unsigned char hash[SHA256_DIGEST_LENGTH];
    unsigned int hash_len;

    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, input, strlen(input));
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);

    return base64_encode(hash, hash_len);
}

static gchar *generate_auth_hash(const gchar *password, const gchar *salt, const gchar *challenge) {
    // Step 1: base64(sha256(password + salt))
    gchar *password_salt = g_strconcat(password, salt, NULL);
    gchar *step1 = sha256_hash(password_salt);
    g_free(password_salt);

    // Step 2: base64(sha256(base64(sha256(password + salt)) + challenge))
    gchar *step1_challenge = g_strconcat(step1, challenge, NULL);
    gchar *result = sha256_hash(step1_challenge);
    g_free(step1);
    g_free(step1_challenge);

    return result;
}

typedef struct {
    result_callback callback;
    JsonObject *object;
    gpointer user_data;
} CallbackData;

static gboolean invoke_callback_idle(gpointer user_data) {
    CallbackData *data = (CallbackData *)user_data;
    if (data->callback != NULL && data->object != NULL) {
        data->callback(data->object, data->user_data);
        json_object_unref(data->object);
    }
    g_free(data);
    return G_SOURCE_REMOVE;
}

static void ws_emit(const char *event, JsonObject *object) {
    if (klass == NULL) {
        LOG("WARNING: Cannot emit event, klass not initialized");
        return;
    }

    char event_to_emit[50];

    const char *request_id = json_object_get_string_value(object, "requestId");
    const char *event_type = json_object_get_string_value(object, "eventType");

    if (request_id != NULL) {
        g_snprintf(event_to_emit, sizeof(event_to_emit), "emit:response:%s", request_id);
    } else if (event_type != NULL) {
        g_snprintf(event_to_emit, sizeof(event_to_emit), "%s", event_type);
    } else if (event != NULL) {
        g_snprintf(event_to_emit, sizeof(event_to_emit), "%s", event);
    } else {
        g_snprintf(event_to_emit, sizeof(event_to_emit), "unknown");
    }

    printf("Event: %s\n", event_to_emit);

    LOCK_WS();
    GList *callback_list = NULL;
    GList *callback_data_list = NULL;
    g_hash_table_lookup_extended(klass->callbacks, event_to_emit, NULL, (gpointer *)&callback_list);
    g_hash_table_lookup_extended(klass->callbacks_data, event_to_emit, NULL, (gpointer *)&callback_data_list);
    printf("got callback for %s ... calling\n", event_to_emit);
    while (callback_list!= NULL) {
        result_callback callback = (result_callback)callback_list->data;
        gpointer user_data = callback_data_list->data;

        CallbackData *cb_data = g_new0(CallbackData, 1);
        cb_data->callback = callback;
        cb_data->object = object ? json_object_ref(object) : NULL;
        cb_data->user_data = user_data;
        g_idle_add(invoke_callback_idle, cb_data);

        callback_list= callback_list->next;
        callback_data_list= callback_data_list->next;
    }
    UNLOCK_WS();
}

void obs_ws_register_callback(ObsWs *self, const char *callbackId, result_callback callback, gpointer user_data) {
    ObsWsClass *klass = OBS_WS_GET_CLASS(self);

    LOCK_WS();
    GList *callback_list = NULL;
    GList *callback_data_list = NULL;
    gboolean exists = g_hash_table_lookup_extended(klass->callbacks, callbackId, NULL, (gpointer *)&callback_list);
    g_hash_table_lookup_extended(klass->callbacks_data, callbackId, NULL, (gpointer *)&callback_data_list);

    callback_list = g_list_append(callback_list, callback);
    callback_data_list = g_list_append(callback_data_list, user_data);

    if (!exists) {
        g_hash_table_insert(klass->callbacks, g_strdup(callbackId), callback_list);
        g_hash_table_insert(klass->callbacks_data, g_strdup(callbackId), callback_data_list);
    }

    if (g_strcmp0(callbackId, "Identified") == 0 && klass->inst != NULL) {
        printf("Already connected, invoking Identified callback immediately\n");
        CallbackData *cb_data = g_new0(CallbackData, 1);
        cb_data->callback = callback;
        cb_data->object = json_object_new();
        json_object_set_boolean_member(cb_data->object, "connected", TRUE);
        cb_data->user_data = user_data;
        g_idle_add(invoke_callback_idle, cb_data);
    }
    UNLOCK_WS();
}

static void ws_send(struct wic_inst *inst, int message_id, GHashTable *map,
                    result_callback callback, gpointer user_data) {
    char request_id[20];
    g_snprintf(request_id, sizeof(request_id), "%d", message_id);

    // Register callback FIRST (before sending, while we know we have a valid connection)
    if (callback != NULL && global_ws != NULL) {
        char callback_id[50];
        g_snprintf(callback_id, sizeof(callback_id), "emit:response:%s", request_id);
        obs_ws_register_callback(global_ws, callback_id, callback, user_data);
    }

    // Build the "d" envelope with requestId and requestType
    JsonObject *d_object = json_object_new();
    json_object_set_string_member(d_object, "requestId", request_id);

    // Get request type from map
    gpointer request_type = g_hash_table_lookup(map, "request-type");
    if (request_type) {
        json_object_set_string_member(d_object, "requestType", request_type);
    }

    JsonObject *request_data = json_object_new();
    gboolean has_request_data = FALSE;

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, map);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        if (g_strcmp0(key, "request-type") == 0) {
            continue;
        }
        has_request_data = TRUE;
        if (g_strcmp0("true", value) == 0) {
            json_object_set_boolean_member(request_data, key, TRUE);
        } else if (g_strcmp0("false", value) == 0) {
            json_object_set_boolean_member(request_data, key, FALSE);
        } else {
            json_object_set_string_member(request_data, key, value);
        }
    }

    if (has_request_data) {
        JsonNode *rd_node = json_node_new(JSON_NODE_OBJECT);
        json_node_set_object(rd_node, request_data);
        json_object_set_member(d_object, "requestData", rd_node);
    }
    json_object_unref(request_data);

    // Build full message with op code
    JsonObject *root = json_object_new();
    json_object_set_int_member(root, "op", OPCODE_REQUEST);

    JsonNode *data_node = json_node_new(JSON_NODE_OBJECT);
    json_node_set_object(data_node, d_object);
    json_object_set_member(root, "d", data_node);

    JsonGenerator *json = json_generator_new();
    JsonNode *root_node = json_node_new(JSON_NODE_OBJECT);
    json_node_set_object(root_node, root);
    json_generator_set_root(json, root_node);

    gsize len;
    gchar *data = json_generator_to_data(json, &len);
    printf(">> %s\n", data);

    // Send without holding any locks - wic_send_text is thread-safe
    printf(">> Send Status: %d\n", wic_send_text(inst, true, data, len));

    g_free(data);
    json_object_unref(d_object);
    json_object_unref(root);
    json_node_unref(root_node);
    g_object_unref(json);
}

static void ws_send_command(struct wic_inst *inst, const gchar *command, result_callback callback,
                            gpointer user_data) {
    GHashTable *map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(map, g_strdup("request-type"), g_strdup(command));

    int message_id = uwsc_message_id();
    ws_send(inst, message_id, map, callback, user_data);

    g_hash_table_unref(map);
}

static gpointer obs_ws_main(gpointer user_data) {
    // Save Global Thread Klass to use in the Protocol
    klass = (ObsWsClass *)user_data;

    int s;
    static uint8_t rx[16384];
    static char url[256] = "ws://127.0.0.1:4455/";
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
                LOCK_WS();
                klass->inst = &inst;
                UNLOCK_WS();
                while (transport_recv(s, &inst))
                    ;
            } else {
                transport_close(&s);
            }
        }

        // Wait and try to connect again
        g_usleep(3000000);
    }

    return NULL;
}

static GString *message_buffer = NULL;

static void on_hello_handler(struct wic_inst *inst, JsonObject *data);
static void on_identified_handler(struct wic_inst *inst, JsonObject *data);

static bool on_message_handler(struct wic_inst *inst, enum wic_encoding encoding, bool fin,
                               const char *data, uint16_t size) {

    printf("got message to process\n");
    if (message_buffer == NULL) {
        message_buffer = g_string_new_len(data, size);
    } else {
        g_string_append_len(message_buffer, data, size);
    }

    if (fin) {
        GError *error = NULL;
        JsonParser *parser = json_parser_new();
        if (json_parser_load_from_data(parser, (const gchar *)message_buffer->str,
                                       message_buffer->len, &error)) {
            JsonNode *root = json_parser_get_root(parser);
            printf("Got json...");
            if (JSON_NODE_HOLDS_OBJECT(root)) {
                JsonObject *message = json_node_get_object(root);

                if (json_object_has_member(message, "op")) {
                    JsonNode *op_node = json_object_get_member(message, "op");
                    int op_code = json_node_get_int(op_node);

                    JsonObject *data_obj = NULL;
                    if (json_object_has_member(message, "d")) {
                        JsonNode *data_node = json_object_get_member(message, "d");
                        if (JSON_NODE_HOLDS_OBJECT(data_node)) {
                            data_obj = json_node_get_object(data_node);
                        }
                    }

                    printf("Received OpCode: %d\n", op_code);

                    switch (op_code) {
                    case OPCODE_HELLO:
                        on_hello_handler(inst, data_obj);
                        break;
                    case OPCODE_IDENTIFIED:
                        on_identified_handler(inst, data_obj);
                        break;
                    case OPCODE_EVENT:
                        ws_emit(NULL, data_obj);
                        break;
                    case OPCODE_REQUEST_RESPONSE:
                        ws_emit(NULL, data_obj);
                        break;
                    default:
                        printf("Unhandled OpCode: %d\n", op_code);
                        break;
                    }
                }
            }
        } else {
            printf("Failed to parse JSON: %s\n", error->message);
        }

        g_object_unref(parser);
        g_string_free(message_buffer, TRUE);
        message_buffer = NULL;
    }

    return true;
}

static void on_handshake_failure_handler(struct wic_inst *inst, enum wic_handshake_failure reason) {
    LOG("websocket handshake failed for reason %d", reason);
}

static void on_hello_handler(struct wic_inst *inst, JsonObject *data) {
    LOG("Received Hello message from OBS");

    // Extract authentication info if present
    const gchar *challenge = NULL;
    const gchar *salt = NULL;
    gboolean auth_required = FALSE;

    if (json_object_has_member(data, "authentication")) {
        JsonNode *auth_node = json_object_get_member(data, "authentication");
        if (JSON_NODE_HOLDS_OBJECT(auth_node)) {
            JsonObject *auth_obj = json_node_get_object(auth_node);
            challenge = json_object_get_string_value(auth_obj, "challenge");
            salt = json_object_get_string_value(auth_obj, "salt");
            auth_required = TRUE;
        }
    }

    // Build Identify message (OpCode 1)
    JsonObject *identify_data = json_object_new();
    json_object_set_int_member(identify_data, "rpcVersion", RPC_VERSION);
    json_object_set_int_member(identify_data, "eventSubscriptions", 2047);

    // Add authentication if required
    if (auth_required && auth_password != NULL && challenge != NULL && salt != NULL) {
        gchar *auth_hash = generate_auth_hash(auth_password, salt, challenge);
        json_object_set_string_member(identify_data, "authentication", auth_hash);
        g_free(auth_hash);
        LOG("Sending authentication with password");
    } else if (auth_required && auth_password == NULL) {
        LOG("WARNING: Authentication required but no password set!");
    }

    // Build full message
    JsonObject *root = json_object_new();
    json_object_set_int_member(root, "op", OPCODE_IDENTIFY);

    JsonNode *data_node = json_node_new(JSON_NODE_OBJECT);
    json_node_set_object(data_node, identify_data);
    json_object_set_member(root, "d", data_node);

    // Send Identify
    JsonGenerator *json = json_generator_new();
    JsonNode *root_node = json_node_new(JSON_NODE_OBJECT);
    json_node_set_object(root_node, root);
    json_generator_set_root(json, root_node);

    gsize len;
    gchar *json_data = json_generator_to_data(json, &len);
    printf(">> Identify: %s\n", json_data);

    wic_send_text(inst, true, json_data, len);

    g_free(json_data);
    json_object_unref(identify_data);
    json_object_unref(root);
    json_node_unref(root_node);
    g_object_unref(json);
}

static void on_identified_handler(struct wic_inst *inst, JsonObject *data) {
    LOG("Successfully identified with OBS");

    JsonObject *connected_obj = json_object_new();
    json_object_set_boolean_member(connected_obj, "connected", TRUE);
    ws_emit("Identified", connected_obj);
    json_object_unref(connected_obj);
}

static void on_open_handler(struct wic_inst *inst) {
    const char *name, *value;

    LOG("websocket is open");

    LOG("received handshake:");

    for (value = wic_get_next_header(inst, &name); value;
         value = wic_get_next_header(inst, &name)) {

        LOG("%s: %s", name, value);
    }
}

static void on_close_handler(struct wic_inst *inst, uint16_t code, const char *reason,
                             uint16_t size) {
    JsonObject *object = json_object_from_boolean_value(FALSE);
    ws_emit("Connected", object);
    json_object_unref(object);

    LOCK_WS();
    klass->inst = NULL;
    UNLOCK_WS();

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
    static uint8_t tx[16384U];

    *max_size = sizeof(tx);

    return (min_size <= sizeof(tx)) ? tx : NULL;
}
