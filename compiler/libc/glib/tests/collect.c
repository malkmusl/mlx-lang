/* What JavaScriptCore's GLib API (Source/JavaScriptCore/API/glib) asks of
   GLib's headers beside the functions: a final type declared by
   G_DECLARE_FINAL_TYPE and defined by hand, values collected from a
   variadic function's arguments by G_VALUE_COLLECT_INIT and stored back
   by G_VALUE_LCOPY, room from g_alloca, the declaration and attribute
   macros; collect.expected is what GLib 2.80 prints for the same
   program. */
#include <glib-object.h>
#include <gobject/gvaluecollector.h>
#include <stdio.h>
#include <string.h>

G_BEGIN_DECLS

#define TEST_TYPE_THING (test_thing_get_type())
G_DECLARE_FINAL_TYPE(TestThing, test_thing, TEST, THING, GObject)

G_END_DECLS

struct _TestThing {
    GObject parent;
    int number;
};

static void test_thing_class_init(TestThingClass *klass) { }
static void test_thing_init(TestThing *self) { self->number = 42; }

GType test_thing_get_type(void)
{
    static gsize type_id = 0;
    if (g_once_init_enter(&type_id)) {
        GType type = g_type_register_static_simple(G_TYPE_OBJECT, g_intern_static_string("TestThing"), sizeof(TestThingClass),
            (GClassInitFunc)(void (*)(void))test_thing_class_init, sizeof(TestThing), (GInstanceInitFunc)(void (*)(void))test_thing_init, G_TYPE_FLAG_FINAL);
        g_once_init_leave(&type_id, type);
    }
    return type_id;
}

static G_GNUC_PRINTF(1, 2) void say(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

static void print_value(const GValue *value)
{
    if (G_VALUE_HOLDS_OBJECT(value)) {
        GObject *object = g_value_get_object(value);
        say("  %s: %s\n", G_VALUE_TYPE_NAME(value), object ? G_OBJECT_TYPE_NAME(object) : "NULL");
        return;
    }
    char *text = g_strdup_value_contents(value);
    say("  %s: %s\n", G_VALUE_TYPE_NAME(value), text);
    g_free(text);
}

/* Values of the types given, then their arguments, until G_TYPE_INVALID. */
static void collect_values(GType first, ...)
{
    va_list args;
    va_start(args, first);
    GType type = first;
    int count = 0;
    while (type != G_TYPE_INVALID) {
        GValue value = G_VALUE_INIT;
        char *error = NULL;
        G_VALUE_COLLECT_INIT(&value, type, args, 0, &error);
        if (error) {
            say("  error: %s\n", error);
            g_free(error);
        } else
            print_value(&value);
        g_value_unset(&value);
        count++;
        type = va_arg(args, GType);
    }
    va_end(args);
    say("collected %d values\n", count);
}

/* VALUE stored through the pointer that follows. */
static void lcopy_value(const GValue *value, ...)
{
    va_list args;
    va_start(args, value);
    char *error = NULL;
    G_VALUE_LCOPY(value, args, 0, &error);
    if (error) {
        say("  lcopy error: %s\n", error);
        g_free(error);
    }
    va_end(args);
}

static int sum_on_stack(int count)
{
    int *numbers = g_newa(int, count);
    for (int i = 0; i < count; i++)
        numbers[i] = i * i;
    GValue *values = (GValue *)g_alloca(sizeof(GValue) * count);
    memset(values, 0, sizeof(GValue) * count);
    int sum = 0;
    for (int i = 0; i < count; i++) {
        g_value_init(&values[i], G_TYPE_INT);
        g_value_set_int(&values[i], numbers[i]);
        sum += g_value_get_int(&values[i]);
    }
    return sum;
}

int main(void)
{
    TestThing *thing = g_object_new(TEST_TYPE_THING, NULL);
    say("thing: %s, is a thing: %d, is an object: %d, number %d\n", G_OBJECT_TYPE_NAME(thing), TEST_IS_THING(thing), G_IS_OBJECT(thing), TEST_THING(thing)->number);
    say("final: %d, class size ok: %d\n", G_TYPE_IS_FINAL(TEST_TYPE_THING), (int)sizeof(TestThingClass) == (int)sizeof(GObjectClass));
    GObject *object = g_object_new(G_TYPE_OBJECT, NULL);
    say("an object is a thing: %d\n", TEST_IS_THING(object));
    glib_autoptr_clear_TestThing(NULL);

    say("collect:\n");
    collect_values(G_TYPE_INT, -7, G_TYPE_BOOLEAN, TRUE, G_TYPE_LONG, 1234567890123L, G_TYPE_INT64, (gint64)-5000000000LL,
        G_TYPE_DOUBLE, 2.5, G_TYPE_FLOAT, 0.25, G_TYPE_STRING, "text", G_TYPE_OBJECT, thing, G_TYPE_OBJECT, NULL, G_TYPE_POINTER, (gpointer)0x1234,
        G_TYPE_UINT64, (guint64)18000000000000000000ULL, G_TYPE_INVALID);

    GValue number = G_VALUE_INIT;
    g_value_init(&number, G_TYPE_INT);
    g_value_set_int(&number, 99);
    int copied = 0;
    lcopy_value(&number, &copied);
    GValue text = G_VALUE_INIT;
    g_value_init(&text, G_TYPE_STRING);
    g_value_set_string(&text, "copied text");
    char *copiedText = NULL;
    lcopy_value(&text, &copiedText);
    say("lcopy: %d '%s'\n", copied, copiedText);
    g_free(copiedText);
    g_value_unset(&text);

    say("on the stack: %d %d\n", sum_on_stack(10), sum_on_stack(3));
    say("stringify: %s, paste: %d\n", G_STRINGIFY(TEST_TYPE_THING), G_PASTE(4, 2));
    g_object_unref(object);
    g_object_unref(thing);
    return 0;
}
