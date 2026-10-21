/* mlx-glib's GObject: types registered as G_DEFINE_TYPE_WITH_PRIVATE
   writes them out (get_type with g_once_init, class_intern_init with the
   private offset), an interface, properties (construct, ranges,
   overrides, notification and its freezing), signals (class handlers,
   handlers before and after, details, accumulators, blocking, chaining
   up), references (floating, weak pointers and references, GWeakRef,
   toggle references), data by key, GValue, enums and flags, boxed types
   and parameter specifications; gobject.expected is what GLib 2.80
   prints for the same program. */
#include <glib-object.h>
#include <stdio.h>
#include <string.h>

/* An interface with one method. */
typedef struct _TestNamedInterface {
    GTypeInterface g_iface;
    const char *(*describe)(gpointer self);
} TestNamedInterface;

static void test_named_default_init(TestNamedInterface *iface)
{
    printf("interface default init\n");
    g_object_interface_install_property(iface, g_param_spec_string("label", "Label", "A label", "none", G_PARAM_READWRITE));
}

static GType test_named_get_type(void)
{
    static gsize type_id = 0;
    if (g_once_init_enter(&type_id)) {
        GType type = g_type_register_static_simple(G_TYPE_INTERFACE, g_intern_static_string("TestNamed"),
            sizeof(TestNamedInterface), (GClassInitFunc)(void (*)(void))test_named_default_init, 0, NULL, 0);
        g_type_interface_add_prerequisite(type, G_TYPE_OBJECT);
        g_once_init_leave(&type_id, type);
    }
    return type_id;
}

/* TestThing: a GObject with private data, properties and signals. */
typedef struct {
    GObject parent;
    int count;
} TestThing;

typedef struct {
    GObjectClass parent_class;
    void (*changed)(TestThing *self, int value);
    gboolean (*query)(TestThing *self, const char *what);
} TestThingClass;

typedef struct {
    char *name;
    gboolean flag;
    double ratio;
    char *label;
    GObject *peer;
} TestThingPrivate;

enum { PROP_0, PROP_COUNT, PROP_NAME, PROP_FLAG, PROP_RATIO, PROP_PEER, PROP_LABEL, N_PROPS };
enum { CHANGED, QUERY, N_SIGNALS };

static GParamSpec *props[N_PROPS];
static guint thing_signals[N_SIGNALS];
static gpointer test_thing_parent_class = NULL;
static gint TestThing_private_offset;

static TestThingPrivate *test_thing_get_instance_private(TestThing *self)
{
    return (TestThingPrivate *)G_STRUCT_MEMBER_P(self, TestThing_private_offset);
}

static void test_thing_init(TestThing *self)
{
    TestThingPrivate *priv = test_thing_get_instance_private(self);
    self->count = 1;
    priv->name = g_strdup("unnamed");
    priv->label = g_strdup("thing");
    printf("thing init, private zeroed %d\n", priv->peer == NULL);
}

static void test_thing_set_property(GObject *object, guint id, const GValue *value, GParamSpec *pspec)
{
    TestThing *self = (TestThing *)object;
    TestThingPrivate *priv = test_thing_get_instance_private(self);
    switch (id) {
    case PROP_COUNT:
        self->count = g_value_get_int(value);
        break;
    case PROP_NAME:
        g_free(priv->name);
        priv->name = g_value_dup_string(value);
        break;
    case PROP_FLAG:
        priv->flag = g_value_get_boolean(value);
        printf("set flag %d\n", priv->flag);
        break;
    case PROP_RATIO:
        priv->ratio = g_value_get_double(value);
        break;
    case PROP_PEER:
        g_clear_object(&priv->peer);
        priv->peer = g_value_dup_object(value);
        break;
    case PROP_LABEL:
        g_free(priv->label);
        priv->label = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, pspec);
    }
}

static void test_thing_get_property(GObject *object, guint id, GValue *value, GParamSpec *pspec)
{
    TestThing *self = (TestThing *)object;
    TestThingPrivate *priv = test_thing_get_instance_private(self);
    switch (id) {
    case PROP_COUNT:
        g_value_set_int(value, self->count);
        break;
    case PROP_NAME:
        g_value_set_string(value, priv->name);
        break;
    case PROP_FLAG:
        g_value_set_boolean(value, priv->flag);
        break;
    case PROP_RATIO:
        g_value_set_double(value, priv->ratio);
        break;
    case PROP_PEER:
        g_value_set_object(value, priv->peer);
        break;
    case PROP_LABEL:
        g_value_set_string(value, priv->label);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, pspec);
    }
}

static void test_thing_dispose(GObject *object)
{
    TestThingPrivate *priv = test_thing_get_instance_private((TestThing *)object);
    printf("dispose %s\n", G_OBJECT_TYPE_NAME(object));
    g_clear_object(&priv->peer);
    G_OBJECT_CLASS(test_thing_parent_class)->dispose(object);
}

static void test_thing_finalize(GObject *object)
{
    TestThingPrivate *priv = test_thing_get_instance_private((TestThing *)object);
    printf("finalize %s name %s\n", G_OBJECT_TYPE_NAME(object), priv->name);
    g_free(priv->name);
    g_free(priv->label);
    G_OBJECT_CLASS(test_thing_parent_class)->finalize(object);
}

static void test_thing_constructed(GObject *object)
{
    printf("constructed count %d\n", ((TestThing *)object)->count);
    G_OBJECT_CLASS(test_thing_parent_class)->constructed(object);
}

static void test_thing_real_changed(TestThing *self, int value)
{
    printf("class handler changed %d\n", value);
}

static gboolean test_thing_real_query(TestThing *self, const char *what)
{
    printf("class handler query %s\n", what);
    return strcmp(what, "class") == 0;
}

static void test_thing_class_init(TestThingClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->set_property = test_thing_set_property;
    object_class->get_property = test_thing_get_property;
    object_class->dispose = test_thing_dispose;
    object_class->finalize = test_thing_finalize;
    object_class->constructed = test_thing_constructed;
    klass->changed = test_thing_real_changed;
    klass->query = test_thing_real_query;
    props[PROP_COUNT] = g_param_spec_int("count", "Count", "How many", 0, 100, 1, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
    props[PROP_NAME] = g_param_spec_string("name", "Name", "The name", "unnamed", G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_FLAG] = g_param_spec_boolean("flag", NULL, NULL, TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
    props[PROP_RATIO] = g_param_spec_double("ratio", NULL, NULL, -1.0, 1.0, 0.5, G_PARAM_READWRITE);
    props[PROP_PEER] = g_param_spec_object("peer", NULL, NULL, G_TYPE_OBJECT, G_PARAM_READWRITE);
    g_object_class_install_properties(object_class, PROP_LABEL, props);
    g_object_class_override_property(object_class, PROP_LABEL, "label");
    thing_signals[CHANGED] = g_signal_new("changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        G_STRUCT_OFFSET(TestThingClass, changed), NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_INT);
    thing_signals[QUERY] = g_signal_new("query", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(TestThingClass, query), g_signal_accumulator_true_handled, NULL, NULL, G_TYPE_BOOLEAN, 1, G_TYPE_STRING);
}

static void test_thing_class_intern_init(gpointer klass)
{
    test_thing_parent_class = g_type_class_peek_parent(klass);
    if (TestThing_private_offset != 0)
        g_type_class_adjust_private_offset(klass, &TestThing_private_offset);
    test_thing_class_init((TestThingClass *)klass);
}

static const char *test_thing_describe(gpointer self)
{
    return "a thing";
}

static void test_thing_named_init(TestNamedInterface *iface)
{
    printf("thing implements named\n");
    iface->describe = test_thing_describe;
}

static GType test_thing_get_type(void)
{
    static gsize type_id = 0;
    if (g_once_init_enter(&type_id)) {
        GType type = g_type_register_static_simple(G_TYPE_OBJECT, g_intern_static_string("TestThing"), sizeof(TestThingClass),
            (GClassInitFunc)(void (*)(void))test_thing_class_intern_init, sizeof(TestThing),
            (GInstanceInitFunc)(void (*)(void))test_thing_init, 0);
        TestThing_private_offset = g_type_add_instance_private(type, sizeof(TestThingPrivate));
        const GInterfaceInfo info = { (GInterfaceInitFunc)(void (*)(void))test_thing_named_init, NULL, NULL };
        g_type_add_interface_static(type, test_named_get_type(), &info);
        g_once_init_leave(&type_id, type);
    }
    return type_id;
}

/* TestSub: overrides the class handler of "changed" and chains up. */
typedef struct {
    TestThing parent;
} TestSub;

typedef struct {
    TestThingClass parent_class;
} TestSubClass;

static gpointer test_sub_parent_class = NULL;

static void test_sub_changed(TestThing *self, int value)
{
    printf("sub class handler changed %d, chaining up\n", value);
    ((TestThingClass *)test_sub_parent_class)->changed(self, value);
}

static void test_sub_init(TestSub *self)
{
    printf("sub init, count %d\n", ((TestThing *)self)->count);
}

static void test_sub_class_init(gpointer klass)
{
    test_sub_parent_class = g_type_class_peek_parent(klass);
    ((TestThingClass *)klass)->changed = test_sub_changed;
}

static GType test_sub_get_type(void)
{
    static gsize type_id = 0;
    if (g_once_init_enter(&type_id)) {
        GType type = g_type_register_static_simple(test_thing_get_type(), g_intern_static_string("TestSub"), sizeof(TestSubClass),
            (GClassInitFunc)(void (*)(void))test_sub_class_init, sizeof(TestSub), (GInstanceInitFunc)(void (*)(void))test_sub_init, 0);
        g_once_init_leave(&type_id, type);
    }
    return type_id;
}

/* A floating type. */
static GType test_floating_get_type(void)
{
    static gsize type_id = 0;
    if (g_once_init_enter(&type_id)) {
        GType type = g_type_register_static_simple(G_TYPE_INITIALLY_UNOWNED, g_intern_static_string("TestFloating"),
            sizeof(GInitiallyUnownedClass), NULL, sizeof(GInitiallyUnowned), NULL, 0);
        g_once_init_leave(&type_id, type);
    }
    return type_id;
}

static void on_notify(GObject *object, GParamSpec *pspec, gpointer data)
{
    printf("notify %s (%s)\n", pspec->name, (const char *)data);
}

static void on_changed(TestThing *self, int value, gpointer data)
{
    printf("handler %s changed %d\n", (const char *)data, value);
}

static void on_changed_swapped(gpointer data, int value, TestThing *self)
{
    printf("swapped handler data %s value %d instance %s\n", (const char *)data, value, G_OBJECT_TYPE_NAME(self));
}

static gboolean on_query(TestThing *self, const char *what, gpointer data)
{
    printf("query handler %s for %s\n", (const char *)data, what);
    return strcmp(what, (const char *)data) == 0;
}

static void weak_notify(gpointer data, GObject *where)
{
    printf("weak notify %s\n", (const char *)data);
}

static void toggle_notify(gpointer data, GObject *object, gboolean is_last)
{
    printf("toggle notify last %d\n", is_last);
}

static void data_destroyed(gpointer data)
{
    printf("data destroyed %s\n", (const char *)data);
}

static const char weak_one[] = "one";
static const char weak_two[] = "two";
static const char swap_data[] = "swap";

static void types(void)
{
    GType thing = test_thing_get_type();
    GType sub = test_sub_get_type();
    printf("names %s %s parent %s depth %u %u\n", g_type_name(thing), g_type_name(sub), g_type_name(g_type_parent(sub)),
        g_type_depth(thing), g_type_depth(sub));
    printf("is_a %d %d %d %d\n", g_type_is_a(sub, thing), g_type_is_a(sub, G_TYPE_OBJECT), g_type_is_a(thing, sub),
        g_type_is_a(sub, test_named_get_type()));
    printf("from name %d fundamental %s next base %s\n", g_type_from_name("TestSub") == sub, g_type_name(G_TYPE_FUNDAMENTAL(sub)),
        g_type_name(g_type_next_base(sub, G_TYPE_OBJECT)));
    printf("fundamental names:");
    GType fundamentals[] = { G_TYPE_NONE, G_TYPE_INTERFACE, G_TYPE_CHAR, G_TYPE_UCHAR, G_TYPE_BOOLEAN, G_TYPE_INT, G_TYPE_UINT,
        G_TYPE_LONG, G_TYPE_ULONG, G_TYPE_INT64, G_TYPE_UINT64, G_TYPE_ENUM, G_TYPE_FLAGS, G_TYPE_FLOAT, G_TYPE_DOUBLE,
        G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_BOXED, G_TYPE_PARAM, G_TYPE_OBJECT, G_TYPE_VARIANT };
    for (unsigned i = 0; i < G_N_ELEMENTS(fundamentals); i++)
        printf(" %s", g_type_name(fundamentals[i]));
    printf("\n");
    printf("abstract %d classed %d instantiatable %d derivable %d\n", G_TYPE_IS_ABSTRACT(G_TYPE_INITIALLY_UNOWNED),
        G_TYPE_IS_CLASSED(thing), G_TYPE_IS_INSTANTIATABLE(thing), G_TYPE_IS_DERIVABLE(G_TYPE_INT));
    guint n;
    GType *children = g_type_children(thing, &n);
    printf("children of thing %u %s\n", n, g_type_name(children[0]));
    g_free(children);
    GType *ifaces = g_type_interfaces(sub, &n);
    printf("interfaces of sub %u %s\n", n, n ? g_type_name(ifaces[0]) : "-");
    g_free(ifaces);
    printf("param type names %s %s %s\n", g_type_name(G_TYPE_PARAM_INT), g_type_name(G_TYPE_PARAM_STRING), g_type_name(G_TYPE_PARAM_OBJECT));
    printf("boxed %s %s %s gtype %s\n", g_type_name(G_TYPE_STRV), g_type_name(G_TYPE_BYTES), g_type_name(G_TYPE_HASH_TABLE), g_type_name(G_TYPE_GTYPE));
}

static void properties(void)
{
    TestThing *thing = g_object_new(test_thing_get_type(), "count", 5, "name", "first", "flag", FALSE, NULL);
    TestThingPrivate *priv = test_thing_get_instance_private(thing);
    printf("new: count %d name %s flag %d ratio %g label %s\n", thing->count, priv->name, priv->flag, priv->ratio, priv->label);
    g_signal_connect(thing, "notify", G_CALLBACK(on_notify), "any");
    g_signal_connect(thing, "notify::name", G_CALLBACK(on_notify), "name only");
    g_object_set(thing, "name", "second", "ratio", 0.25, NULL);
    g_object_set(thing, "count", 7, NULL);
    g_object_notify(G_OBJECT(thing), "count");
    g_object_notify_by_pspec(G_OBJECT(thing), props[PROP_RATIO]);
    char *name;
    int count;
    double ratio;
    gboolean flag;
    g_object_get(thing, "name", &name, "count", &count, "ratio", &ratio, "flag", &flag, NULL);
    printf("get: name %s count %d ratio %g flag %d\n", name, count, ratio, flag);
    g_free(name);
    printf("freeze\n");
    g_object_freeze_notify(G_OBJECT(thing));
    g_object_set(thing, "ratio", -0.5, NULL);
    g_object_set(thing, "name", "third", NULL);
    g_object_set(thing, "ratio", 0.75, NULL);
    printf("thaw\n");
    g_object_thaw_notify(G_OBJECT(thing));
    GValue value = G_VALUE_INIT;
    g_value_init(&value, G_TYPE_STRING);
    g_object_get_property(G_OBJECT(thing), "count", &value);
    printf("count as string %s\n", g_value_get_string(&value));
    g_value_unset(&value);
    g_value_init(&value, G_TYPE_INT);
    g_value_set_int(&value, 42);
    g_object_set_property(G_OBJECT(thing), "count", &value);
    printf("count via GValue %d\n", thing->count);
    g_value_unset(&value);
    g_object_set(thing, "label", "renamed", NULL);
    printf("label %s\n", priv->label);
    GParamSpec *found = g_object_class_find_property(G_OBJECT_GET_CLASS(thing), "label");
    printf("label pspec owner %s blurb %s\n", g_type_name(found->owner_type), g_param_spec_get_blurb(found));
    guint n;
    GParamSpec **list = g_object_class_list_properties(G_OBJECT_GET_CLASS(thing), &n);
    printf("properties %u:", n);
    for (guint i = 0; i < n; i++)
        printf(" %s(%s)", list[i]->name, g_type_name(G_PARAM_SPEC_VALUE_TYPE(list[i])));
    printf("\n");
    g_free(list);
    GParamSpec *count_spec = g_object_class_find_property(G_OBJECT_GET_CLASS(thing), "count");
    printf("count nick %s default %d\n", g_param_spec_get_nick(count_spec), g_value_get_int(g_param_spec_get_default_value(count_spec)));
    TestNamedInterface *iface = G_TYPE_INSTANCE_GET_INTERFACE(thing, test_named_get_type(), TestNamedInterface);
    printf("interface describe: %s\n", iface->describe(thing));
    g_object_unref(thing);
}

static void signals(void)
{
    TestThing *thing = g_object_new(test_thing_get_type(), NULL);
    gulong first = g_signal_connect(thing, "changed", G_CALLBACK(on_changed), "first");
    g_signal_connect_after(thing, "changed", G_CALLBACK(on_changed), "after");
    g_signal_connect(thing, "changed::detail", G_CALLBACK(on_changed), "detailed");
    g_signal_connect_swapped(thing, "changed", G_CALLBACK(on_changed_swapped), (gpointer)swap_data);
    printf("emit plain\n");
    g_signal_emit(thing, thing_signals[CHANGED], 0, 1);
    printf("emit detail\n");
    g_signal_emit(thing, thing_signals[CHANGED], g_quark_from_string("detail"), 2);
    printf("emit by name\n");
    g_signal_emit_by_name(thing, "changed::detail", 3);
    g_signal_handler_block(thing, first);
    printf("blocked\n");
    g_signal_emit(thing, thing_signals[CHANGED], 0, 4);
    g_signal_handler_unblock(thing, first);
    g_signal_handler_disconnect(thing, first);
    printf("connected %d\n", g_signal_handler_is_connected(thing, first));
    printf("disconnected by data %u\n", g_signal_handlers_disconnect_by_data(thing, (gpointer)swap_data));
    g_signal_emit(thing, thing_signals[CHANGED], 0, 5);
    gboolean handled = FALSE;
    g_signal_connect(thing, "query", G_CALLBACK(on_query), "first");
    g_signal_connect(thing, "query", G_CALLBACK(on_query), "second");
    g_signal_emit(thing, thing_signals[QUERY], 0, "second", &handled);
    printf("query second handled %d\n", handled);
    g_signal_emit(thing, thing_signals[QUERY], 0, "class", &handled);
    printf("query class handled %d\n", handled);
    g_signal_emit(thing, thing_signals[QUERY], 0, "nobody", &handled);
    printf("query nobody handled %d\n", handled);
    GSignalQuery query;
    g_signal_query(thing_signals[QUERY], &query);
    printf("query: %s owner %s params %u return %s\n", query.signal_name, g_type_name(query.itype), query.n_params, g_type_name(query.return_type));
    printf("lookup %d name %s\n", g_signal_lookup("changed", test_sub_get_type()) == thing_signals[CHANGED], g_signal_name(thing_signals[CHANGED]));
    printf("pending %d\n", g_signal_has_handler_pending(thing, thing_signals[CHANGED], 0, FALSE));
    g_object_unref(thing);

    TestThing *sub = g_object_new(test_sub_get_type(), "count", 9, NULL);
    g_signal_connect(sub, "changed", G_CALLBACK(on_changed), "sub handler");
    g_signal_emit(sub, thing_signals[CHANGED], 0, 6);
    g_object_unref(sub);
}

static void references(void)
{
    GObject *object = g_object_new(test_thing_get_type(), "name", "weakly held", NULL);
    gpointer pointer = object;
    g_object_add_weak_pointer(object, &pointer);
    g_object_weak_ref(object, weak_notify, (gpointer)weak_one);
    g_object_weak_ref(object, weak_notify, (gpointer)weak_two);
    g_object_weak_unref(object, weak_notify, (gpointer)weak_two);
    GWeakRef weak;
    g_weak_ref_init(&weak, object);
    GObject *strong = g_weak_ref_get(&weak);
    printf("weak ref get same %d refs %u\n", strong == object, object->ref_count);
    g_object_unref(strong);
    g_object_set_data_full(object, "key", "value", data_destroyed);
    printf("data %s\n", (const char *)g_object_get_data(object, "key"));
    g_object_set_data_full(object, "key", "replaced", data_destroyed);
    g_object_set_data(object, "plain", GINT_TO_POINTER(3));
    printf("plain data %d missing %p\n", GPOINTER_TO_INT(g_object_get_data(object, "plain")), g_object_get_data(object, "missing"));
    printf("steal %s\n", (const char *)g_object_steal_data(object, "plain") ? "yes" : "no");
    g_object_add_toggle_ref(object, toggle_notify, NULL);
    printf("unref to toggle\n");
    g_object_unref(object);
    g_object_ref(object);
    g_object_unref(object);
    printf("remove toggle (last reference)\n");
    g_object_remove_toggle_ref(object, toggle_notify, NULL);
    printf("weak pointer cleared %d weak ref %p\n", pointer == NULL, (void *)g_weak_ref_get(&weak));
    g_weak_ref_clear(&weak);

    GObject *floating = g_object_new(test_floating_get_type(), NULL);
    printf("floating %d", g_object_is_floating(floating));
    g_object_ref_sink(floating);
    printf(" after sink %d refs %u\n", g_object_is_floating(floating), floating->ref_count);
    g_object_unref(floating);
}

static void values(void)
{
    GValue a = G_VALUE_INIT, b = G_VALUE_INIT;
    g_value_init(&a, G_TYPE_INT);
    g_value_set_int(&a, -42);
    g_value_init(&b, G_TYPE_DOUBLE);
    printf("transformable int->double %d string->int %d\n", g_value_type_transformable(G_TYPE_INT, G_TYPE_DOUBLE),
        g_value_type_transformable(G_TYPE_STRING, G_TYPE_INT));
    g_value_transform(&a, &b);
    printf("int to double %g\n", g_value_get_double(&b));
    g_value_unset(&b);
    g_value_init(&b, G_TYPE_STRING);
    g_value_transform(&a, &b);
    printf("int to string %s\n", g_value_get_string(&b));
    char *contents = g_strdup_value_contents(&b);
    printf("contents %s\n", contents);
    g_free(contents);
    g_value_unset(&b);
    g_value_init(&b, G_TYPE_BOOLEAN);
    g_value_transform(&a, &b);
    printf("int to boolean %d\n", g_value_get_boolean(&b));
    g_value_unset(&b);
    g_value_init(&b, G_TYPE_UINT);
    g_value_transform(&a, &b);
    printf("int to uint %u\n", g_value_get_uint(&b));
    g_value_unset(&b);
    g_value_unset(&a);

    static const GEnumValue colors[] = { { 0, "COLOR_RED", "red" }, { 1, "COLOR_GREEN", "green" }, { 5, "COLOR_BLUE", "blue" }, { 0, NULL, NULL } };
    static const GFlagsValue modes[] = { { 1, "MODE_READ", "read" }, { 2, "MODE_WRITE", "write" }, { 4, "MODE_EXEC", "exec" }, { 0, NULL, NULL } };
    GType color = g_enum_register_static("TestColor", colors);
    GType mode = g_flags_register_static("TestMode", modes);
    GEnumClass *enum_class = g_type_class_ref(color);
    printf("enum min %d max %d n %u by nick %s\n", enum_class->minimum, enum_class->maximum, enum_class->n_values,
        g_enum_get_value_by_nick(enum_class, "blue")->value_name);
    char *text = g_enum_to_string(color, 5);
    printf("enum to string %s", text);
    g_free(text);
    text = g_enum_to_string(color, 3);
    printf(" %s\n", text);
    g_free(text);
    text = g_flags_to_string(mode, 5);
    printf("flags to string %s", text);
    g_free(text);
    text = g_flags_to_string(mode, 9);
    printf(" | %s", text);
    g_free(text);
    text = g_flags_to_string(mode, 0);
    printf(" | %s\n", text);
    g_free(text);
    g_type_class_unref(enum_class);
    g_value_init(&a, color);
    g_value_set_enum(&a, 1);
    contents = g_strdup_value_contents(&a);
    printf("enum contents %s\n", contents);
    g_free(contents);
    g_value_unset(&a);

    const char *strv[] = { "x", "y", NULL };
    g_value_init(&a, G_TYPE_STRV);
    g_value_set_boxed(&a, strv);
    char **copied = g_value_dup_boxed(&a);
    printf("boxed strv copy %s %s different %d\n", copied[0], copied[1], (void *)copied != (void *)strv);
    g_strfreev(copied);
    contents = g_strdup_value_contents(&a);
    printf("strv contents %s\n", contents);
    g_free(contents);
    g_value_unset(&a);

    GParamSpec *spec = g_param_spec_int("bounded", NULL, NULL, -5, 5, 0, G_PARAM_READWRITE);
    g_param_spec_ref_sink(spec);
    g_value_init(&a, G_TYPE_INT);
    g_value_set_int(&a, 99);
    printf("validate changed %d now %d defaults %d\n", g_param_value_validate(spec, &a), g_value_get_int(&a), g_param_value_defaults(spec, &a));
    g_param_value_set_default(spec, &a);
    printf("default %d defaults %d\n", g_value_get_int(&a), g_param_value_defaults(spec, &a));
    g_value_unset(&a);
    g_param_spec_unref(spec);
    GParamSpec *bad = g_param_spec_string("bad_name", NULL, NULL, NULL, G_PARAM_READABLE);
    printf("canonical name %s\n", g_param_spec_get_name(bad));
    g_param_spec_ref_sink(bad);
    g_param_spec_unref(bad);

    GValueArray *array = g_value_array_new(0);
    g_value_init(&a, G_TYPE_STRING);
    g_value_set_static_string(&a, "element");
    g_value_array_append(array, &a);
    g_value_array_append(array, &a);
    g_value_array_remove(array, 0);
    printf("value array %u %s\n", array->n_values, g_value_get_string(g_value_array_get_nth(array, 0)));
    g_value_array_free(array);
    g_value_unset(&a);
}

static void closure_finalized(gpointer data, GClosure *closure)
{
    printf("closure finalized %s\n", (const char *)data);
}

static void closure_invalidated(gpointer data, GClosure *closure)
{
    printf("closure invalidated %s\n", (const char *)data);
}

static int add_ints(int a, int b, gpointer data)
{
    return a + b + GPOINTER_TO_INT(data);
}

static void closures(void)
{
    GClosure *closure = g_cclosure_new(G_CALLBACK(add_ints), GINT_TO_POINTER(100), NULL);
    g_closure_add_finalize_notifier(closure, "c", closure_finalized);
    g_closure_add_invalidate_notifier(closure, "c", closure_invalidated);
    g_closure_set_marshal(closure, g_cclosure_marshal_generic);
    g_closure_ref(closure);
    g_closure_sink(closure);
    GValue params[2] = { G_VALUE_INIT, G_VALUE_INIT };
    GValue result = G_VALUE_INIT;
    g_value_init(&params[0], G_TYPE_INT);
    g_value_set_int(&params[0], 1);
    g_value_init(&params[1], G_TYPE_INT);
    g_value_set_int(&params[1], 2);
    g_value_init(&result, G_TYPE_INT);
    g_closure_invoke(closure, &result, 2, params, NULL);
    printf("closure result %d\n", g_value_get_int(&result));
    g_closure_invalidate(closure);
    g_closure_unref(closure);

    GObject *object = g_object_new(test_thing_get_type(), NULL);
    GClosure *watched = g_cclosure_new_object(G_CALLBACK(add_ints), object);
    g_closure_add_invalidate_notifier(watched, "watched", closure_invalidated);
    g_closure_ref(watched);
    g_closure_sink(watched);
    printf("unref watched object\n");
    g_object_unref(object);
    g_closure_unref(watched);
}

int main(void)
{
    printf("== types\n");
    types();
    printf("== properties\n");
    properties();
    printf("== signals\n");
    signals();
    printf("== references\n");
    references();
    printf("== values\n");
    values();
    printf("== closures\n");
    closures();
    return 0;
}
