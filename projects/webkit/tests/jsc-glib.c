/* JavaScriptCore of the WPE port through its GLib API (<jsc/jsc.h>, as
   WebKit's users of it see it): a context evaluating scripts, values made
   and read from C, a C function called from JavaScript, an exception, a
   C class with a method; built by ./mlx build webkit-wpe-jsc-glib-test,
   run by tools/check_wpe_jsc.sh against tests/jsc-glib.expected. */
#include <jsc/jsc.h>
#include <stdio.h>
#include <string.h>

static int add_numbers(int a, int b)
{
    return a + b;
}

static char *greet(const char *name)
{
    return g_strdup_printf("hello, %s", name);
}

typedef struct {
    int count;
} Counter;

static Counter *counter_new(void)
{
    Counter *counter = g_new0(Counter, 1);
    return counter;
}

static int counter_increment(Counter *counter, int by)
{
    counter->count += by;
    return counter->count;
}

static void print_value(const char *what, JSCValue *value)
{
    char *text = jsc_value_to_string(value);
    printf("%s: %s", what, text);
    if (jsc_value_is_number(value))
        printf(" (number %g)", jsc_value_to_double(value));
    if (jsc_value_is_boolean(value))
        printf(" (boolean %d)", jsc_value_to_boolean(value));
    if (jsc_value_is_array(value))
        printf(" (array)");
    printf("\n");
    g_free(text);
}

int main(void)
{
    printf("JSC %u.%u.%u\n", jsc_get_major_version(), jsc_get_minor_version(), jsc_get_micro_version());
    JSCContext *context = jsc_context_new();

    JSCValue *value = jsc_context_evaluate(context, "6 * 7", -1);
    print_value("6 * 7", value);
    g_object_unref(value);

    value = jsc_context_evaluate(context, "[1, 2, 3].map(x => x * x).join(' ')", -1);
    print_value("squares", value);
    g_object_unref(value);

    value = jsc_context_evaluate(context, "JSON.stringify({ a: [1, 'two', null], b: { c: true } })", -1);
    print_value("json", value);
    g_object_unref(value);

    value = jsc_context_evaluate(context, "'Grüße'.toUpperCase() + ' ' + 'ß'.length + ' ' + (0.1 + 0.2)", -1);
    print_value("text", value);
    g_object_unref(value);

    JSCValue *number = jsc_value_new_number(context, 2.5);
    jsc_context_set_value(context, "fromC", number);
    g_object_unref(number);
    value = jsc_context_evaluate(context, "fromC * 4", -1);
    print_value("fromC * 4", value);
    g_object_unref(value);

    JSCValue *function = jsc_value_new_function(context, "addNumbers", G_CALLBACK(add_numbers), NULL, NULL, G_TYPE_INT, 2, G_TYPE_INT, G_TYPE_INT);
    jsc_context_set_value(context, "addNumbers", function);
    g_object_unref(function);
    value = jsc_context_evaluate(context, "addNumbers(40, 2) + addNumbers(-1, 1)", -1);
    print_value("addNumbers", value);
    g_object_unref(value);

    function = jsc_value_new_function(context, "greet", G_CALLBACK(greet), NULL, NULL, G_TYPE_STRING, 1, G_TYPE_STRING);
    jsc_context_set_value(context, "greet", function);
    g_object_unref(function);
    value = jsc_context_evaluate(context, "greet('WPE')", -1);
    print_value("greet", value);
    g_object_unref(value);

    JSCClass *counter_class = jsc_context_register_class(context, "Counter", NULL, NULL, g_free);
    JSCValue *constructor = jsc_class_add_constructor(counter_class, NULL, G_CALLBACK(counter_new), NULL, NULL, G_TYPE_POINTER, 0, G_TYPE_NONE);
    jsc_context_set_value(context, "Counter", constructor);
    g_object_unref(constructor);
    jsc_class_add_method(counter_class, "increment", G_CALLBACK(counter_increment), NULL, NULL, G_TYPE_INT, 1, G_TYPE_INT);
    value = jsc_context_evaluate(context, "let c = new Counter(); c.increment(2); c.increment(5)", -1);
    print_value("counter", value);
    g_object_unref(value);

    value = jsc_context_evaluate(context, "null.property", -1);
    JSCException *exception = jsc_context_get_exception(context);
    printf("exception: %s (line %u)\n", exception ? jsc_exception_get_message(exception) : "none", exception ? jsc_exception_get_line_number(exception) : 0);
    jsc_context_clear_exception(context);
    g_object_unref(value);

    JSCValue *array = jsc_context_evaluate(context, "[10, 20, 30]", -1);
    JSCValue *second = jsc_value_object_get_property_at_index(array, 1);
    print_value("array[1]", second);
    g_object_unref(second);
    g_object_unref(array);

    value = jsc_context_evaluate(context, "let s = 0; for (let i = 0; i < 100000; i++) s += i % 7; s", -1);
    print_value("loop", value);
    g_object_unref(value);

    g_object_unref(context);
    printf("done\n");
    return 0;
}
