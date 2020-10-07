#include "libobsws.h"
#include <libwebsockets.h>

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
}

// Public Members

ObsWs *obs_ws_new() { return g_object_new(OBS_TYPE_WS, NULL); }

// Private

#define LWS_PLUGIN_STATIC
#include "protocol_lws_minimal_client_echo.c"

static struct lws_protocols protocols[] = {
    LWS_PLUGIN_PROTOCOL_MINIMAL_CLIENT_ECHO, {NULL, NULL, 0, 0} /* terminator */
};

static struct lws_context *context;
static int interrupted, port = 4444, options = 0;
static const char *url = "/", *ads = "localhost", *iface = NULL;
static ObsWsClass *klass = NULL;

/* pass pointers to shared vars to the protocol */

static const struct lws_protocol_vhost_options pvo_iface = {
    NULL, NULL, "iface", /* pvo name */
    (void *)&iface       /* pvo value */
};

static const struct lws_protocol_vhost_options pvo_ads = {
    &pvo_iface, NULL, "ads", /* pvo name */
    (void *)&ads             /* pvo value */
};

static const struct lws_protocol_vhost_options pvo_url = {
    &pvo_ads, NULL, "url", /* pvo name */
    (void *)&url           /* pvo value */
};

static const struct lws_protocol_vhost_options pvo_options = {
    &pvo_url, NULL, "options", /* pvo name */
    (void *)&options           /* pvo value */
};

static const struct lws_protocol_vhost_options pvo_port = {
    &pvo_options, NULL, "port", /* pvo name */
    (void *)&port               /* pvo value */
};

static const struct lws_protocol_vhost_options pvo_interrupted = {
    &pvo_port, NULL, "interrupted", /* pvo name */
    (void *)&interrupted            /* pvo value */
};

static const struct lws_protocol_vhost_options pvo_klass = {
    &pvo_interrupted, NULL, "klass", /* pvo name */
    (void *)&klass                   /* pvo value */
};

static const struct lws_protocol_vhost_options pvo = {
    NULL,                      /* "next" pvo linked-list */
    &pvo_klass,                /* "child" pvo linked-list */
    "lws-minimal-client-echo", /* protocol name we belong to on this vhost */
    ""                         /* ignored */
};
// static const struct lws_extension extensions[] = {{"permessage-deflate",
//                                                    lws_extension_callback_pm_deflate,
//                                                    "permessage-deflate"
//                                                    "; client_no_context_takeover"
//                                                    "; client_max_window_bits"},
//                                                   {NULL, NULL, NULL /* terminator */}};

void sigint_handler(int sig) { interrupted = 1; }

static gpointer obs_ws_main(gpointer user_data) {
    // Save Global Thread Klass to use in the Protocol
    klass = (ObsWsClass *)user_data;

    struct lws_context_creation_info info;
    int n, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
        /* for LLL_ verbosity above NOTICE to be built into lws,
         * lws must have been configured and built with
         * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
        /* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
        /* | LLL_EXT */ /* | LLL_CLIENT */  /* | LLL_LATENCY */
        /* | LLL_DEBUG */;

    lws_set_log_level(logs, NULL);
    lwsl_user("LWS minimal ws client echo + permessage-deflate + multifragment bulk message\n");
    lwsl_user("   lws-minimal-ws-client-echo [-n (no exts)] [-u url] [-p port] [-o (once)]\n");

    options |= 1;

    // if (lws_cmdline_option(argc, argv, "--ssl"))
    // options |= 2;

    // if ((p = lws_cmdline_option(argc, argv, "-s")))
    // ads = p;

    // if ((p = lws_cmdline_option(argc, argv, "-i")))
    // iface = p;

    lwsl_user("options %d, ads %s\n", options, ads);

    memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.pvo = &pvo;
    // if (!lws_cmdline_option(argc, argv, "-n"))
    //    info.extensions = extensions;
    info.pt_serv_buf_size = 32 * 1024;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT | LWS_SERVER_OPTION_VALIDATE_UTF8;
    /*
     * since we know this lws context is only ever going to be used with
     * one client wsis / fds / sockets at a time, let lws know it doesn't
     * have to use the default allocations for fd tables up to ulimit -n.
     * It will just allocate for 1 internal and 1 (+ 1 http2 nwsi) that we
     * will use.
     */
    info.fd_limit_per_thread = 1 + 1 + 1;

    // if (lws_cmdline_option(argc, argv, "--libuv"))
    info.options |= LWS_SERVER_OPTION_LIBUV;
    // else
    //    signal(SIGINT, sigint_handler);

    context = lws_create_context(&info);
    if (!context) {
        lwsl_err("lws init failed\n");
        return NULL;
    }

    while (!lws_service(context, 0) && !interrupted)
        ;

    lws_context_destroy(context);

    n = (options & 1) ? interrupted != 2 : interrupted == 3;
    lwsl_user("Completed %d %s\n", interrupted, !n ? "OK" : "failed");

    return NULL;
}