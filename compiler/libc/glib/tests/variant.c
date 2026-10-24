/* mlx-glib's GVariant: values made and read back, their serialized data,
   containers, builders, iterators, dictionaries, format strings, data
   from outside (good and bad), byte swapping and the text form;
   variant.expected is what GLib 2.80 prints for the same program. */
#include <glib.h>
#include <stdio.h>
#include <string.h>

static void show(const char *label, GVariant *v)
{
    g_variant_ref_sink(v);
    gchar *text = g_variant_print(v, TRUE);
    gchar *plain = g_variant_print(v, FALSE);
    printf("%s: %s | %s | %s | %lu bytes:", label, g_variant_get_type_string(v), text, plain,
        (unsigned long)g_variant_get_size(v));
    const guchar *data = g_variant_get_data(v);
    if (!data && g_variant_get_size(v))
        printf(" (no data)");
    else for (gsize i = 0; i < g_variant_get_size(v); i++)
        printf(" %02x", data[i]);
    printf("\n");
    g_free(text);
    g_free(plain);
    g_variant_unref(v);
}

static void types(void)
{
    const char *strings[] = { "i", "as", "a{sv}", "(ss)", "m(ii)", "{sv}", "a{vs}", "(", "aa", "r", "a{?*}", "(i)x", "" };
    for (unsigned i = 0; i < G_N_ELEMENTS(strings); i++)
        printf("valid '%s' %d\n", strings[i], g_variant_type_string_is_valid(strings[i]));
    const GVariantType *t = G_VARIANT_TYPE("(sa{sv}i)");
    printf("n items %lu first %.*s\n", (unsigned long)g_variant_type_n_items(t),
        (int)g_variant_type_get_string_length(g_variant_type_first(t)), g_variant_type_peek_string(g_variant_type_first(t)));
    const GVariantType *second = g_variant_type_next(g_variant_type_first(t));
    printf("second %.*s array %d element %.*s\n", (int)g_variant_type_get_string_length(second),
        g_variant_type_peek_string(second), g_variant_type_is_array(second),
        (int)g_variant_type_get_string_length(g_variant_type_element(second)), g_variant_type_peek_string(g_variant_type_element(second)));
    const GVariantType *entry = g_variant_type_element(second);
    printf("key %.*s value %.*s\n", (int)g_variant_type_get_string_length(g_variant_type_key(entry)), g_variant_type_peek_string(g_variant_type_key(entry)),
        (int)g_variant_type_get_string_length(g_variant_type_value(entry)), g_variant_type_peek_string(g_variant_type_value(entry)));
    printf("subtype %d %d %d %d\n", g_variant_type_is_subtype_of(t, G_VARIANT_TYPE_TUPLE),
        g_variant_type_is_subtype_of(G_VARIANT_TYPE("as"), G_VARIANT_TYPE("a*")),
        g_variant_type_is_subtype_of(G_VARIANT_TYPE("a{sv}"), G_VARIANT_TYPE("a{?*}")),
        g_variant_type_is_subtype_of(G_VARIANT_TYPE("av"), G_VARIANT_TYPE("a?")));
    GVariantType *made = g_variant_type_new_dict_entry(G_VARIANT_TYPE_STRING, G_VARIANT_TYPE_VARIANT);
    GVariantType *array = g_variant_type_new_array(made);
    gchar *text = g_variant_type_dup_string(array);
    printf("made %s definite %d hash equal %d\n", text, g_variant_type_is_definite(array),
        g_variant_type_hash(array) == g_variant_type_hash(G_VARIANT_TYPE_VARDICT) && g_variant_type_equal(array, G_VARIANT_TYPE_VARDICT));
    g_free(text);
    g_variant_type_free(made);
    g_variant_type_free(array);
}

static void basics(void)
{
    show("boolean", g_variant_new_boolean(TRUE));
    show("byte", g_variant_new_byte(200));
    show("int16", g_variant_new_int16(-300));
    show("uint16", g_variant_new_uint16(65000));
    show("int32", g_variant_new_int32(-123456));
    show("uint32", g_variant_new_uint32(4000000000u));
    show("int64", g_variant_new_int64(-1234567890123LL));
    show("uint64", g_variant_new_uint64(18000000000000000000ULL));
    show("handle", g_variant_new_handle(3));
    show("double", g_variant_new_double(3.25));
    show("double whole", g_variant_new_double(2.0));
    show("double third", g_variant_new_double(1.0 / 3.0));
    show("string", g_variant_new_string("hello 'world'\n\tdone \xc3\xa9"));
    show("object path", g_variant_new_object_path("/org/example/Thing"));
    show("signature", g_variant_new_signature("a{sv}"));
    show("printf", g_variant_new_printf("%d-%s", 7, "x"));
    printf("paths %d %d %d %d\n", g_variant_is_object_path("/"), g_variant_is_object_path("/a/b_c"),
        g_variant_is_object_path("/a/"), g_variant_is_object_path("a"));
    printf("signatures %d %d %d\n", g_variant_is_signature("a{sv}(ii)"), g_variant_is_signature("m"), g_variant_is_signature(""));
}

static void containers(void)
{
    show("tuple ss", g_variant_new("(ss)", "org.example", "Name"));
    show("tuple tti", g_variant_new("(tti)", (guint64)1234, (guint64)5678, -5));
    show("unit", g_variant_new("()"));
    show("one", g_variant_new("(i)", 1));
    show("mixed", g_variant_new("(ybnqiuxtdsogv)", 1, TRUE, -2, 3, -4, 5u, (gint64)-6, (guint64)7, 8.5, "s", "/o", "g",
        g_variant_new_int32(9)));
    const gchar *names[] = { "a", "bb", "", "dddd", NULL };
    show("strv", g_variant_new_strv(names, -1));
    show("objv", g_variant_new_objv((const gchar *[]) { "/a", "/b/c" }, 2));
    show("bytestring", g_variant_new_bytestring("bytes"));
    show("bytestring quote", g_variant_new_bytestring("it's"));
    show("caret as", g_variant_new("^as", names));
    show("caret aay", g_variant_new("^aay", names));
    gint32 numbers[] = { 1, -2, 300000 };
    show("fixed array", g_variant_new_fixed_array(G_VARIANT_TYPE_INT32, numbers, 3, sizeof(gint32)));
    show("empty array", g_variant_new_array(G_VARIANT_TYPE_STRING, NULL, 0));
    show("empty dict", g_variant_new("a{sv}", NULL));
    show("maybe just", g_variant_new("ms", "here"));
    show("maybe nothing", g_variant_new("ms", NULL));
    show("maybe int", g_variant_new("mi", TRUE, 42));
    show("maybe int nothing", g_variant_new("mi", FALSE, 42));
    show("maybe maybe", g_variant_new_maybe(NULL, g_variant_new_maybe(G_VARIANT_TYPE_INT32, NULL)));
    show("variant", g_variant_new_variant(g_variant_new_variant(g_variant_new_string("deep"))));
    show("at", g_variant_new("(@ai@s)", g_variant_new_array(G_VARIANT_TYPE_INT32, NULL, 0), g_variant_new_string("x")));

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&builder, "{sv}", "name", g_variant_new_string("value"));
    g_variant_builder_add(&builder, "{sv}", "count", g_variant_new_uint32(3));
    g_variant_builder_add(&builder, "{sv}", "flag", g_variant_new_boolean(FALSE));
    g_variant_builder_add(&builder, "{sv}", "list", g_variant_new_strv(names, 2));
    show("vardict", g_variant_builder_end(&builder));

    GVariantBuilder *heap = g_variant_builder_new(G_VARIANT_TYPE("a(sai)"));
    for (int i = 0; i < 3; i++) {
        g_variant_builder_open(heap, G_VARIANT_TYPE("(sai)"));
        g_variant_builder_add(heap, "s", i == 1 ? "longer string here" : "s");
        g_variant_builder_open(heap, G_VARIANT_TYPE("ai"));
        for (int k = 0; k <= i; k++)
            g_variant_builder_add(heap, "i", k * 10);
        g_variant_builder_close(heap);
        g_variant_builder_close(heap);
    }
    show("nested", g_variant_builder_end(heap));
    g_variant_builder_unref(heap);

    GVariantBuilder inferred;
    g_variant_builder_init(&inferred, G_VARIANT_TYPE("a*"));
    g_variant_builder_add(&inferred, "x", (gint64)1);
    g_variant_builder_add(&inferred, "x", (gint64)2);
    show("inferred", g_variant_builder_end(&inferred));
    g_variant_builder_init(&inferred, G_VARIANT_TYPE("r"));
    g_variant_builder_add(&inferred, "s", "a");
    g_variant_builder_add(&inferred, "u", 7u);
    show("inferred tuple", g_variant_builder_end(&inferred));

    GVariantBuilder big;
    g_variant_builder_init(&big, G_VARIANT_TYPE("as"));
    for (int i = 0; i < 100; i++)
        g_variant_builder_add(&big, "s", "a string of twenty-ch");
    GVariant *large = g_variant_ref_sink(g_variant_builder_end(&big));
    printf("large size %lu children %lu\n", (unsigned long)g_variant_get_size(large), (unsigned long)g_variant_n_children(large));
    GVariant *last = g_variant_get_child_value(large, 99);
    printf("last %s\n", g_variant_get_string(last, NULL));
    g_variant_unref(last);
    g_variant_unref(large);
}

static void reading(void)
{
    GVariant *v = g_variant_ref_sink(g_variant_new("(sa{sv}ixmsd)", "title", NULL, -9, (gint64)1 << 40, "maybe", 0.5));
    const gchar *title;
    GVariantIter *iter;
    gint32 i;
    gint64 x;
    gchar *maybe;
    gdouble d;
    g_variant_get(v, "(&sa{sv}ixmsd)", &title, &iter, &i, &x, &maybe, &d);
    printf("get: %s %lu %d %lld %s %g\n", title, (unsigned long)g_variant_iter_n_children(iter), i, (long long)x, maybe, d);
    g_variant_iter_free(iter);
    g_free(maybe);
    g_variant_get(v, "(sa{sv}ixmsd)", NULL, NULL, &i, NULL, NULL, NULL);
    printf("only i %d\n", i);
    g_variant_unref(v);

    v = g_variant_ref_sink(g_variant_new("(v)", g_variant_new_int64(77)));
    GVariant *property = NULL;
    g_variant_get(v, "(v)", &property);
    printf("(v): is int32 %d is int64 %d value %lld floating %d\n", g_variant_is_of_type(property, G_VARIANT_TYPE_INT32),
        g_variant_is_of_type(property, G_VARIANT_TYPE_INT64), (long long)g_variant_get_int64(property), g_variant_is_floating(property));
    g_variant_unref(property);
    g_variant_unref(v);

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&builder, "{sv}", "Timezone", g_variant_new_string("Europe/Paris"));
    g_variant_builder_add(&builder, "{sv}", "Count", g_variant_new_int32(12));
    GVariant *dict = g_variant_ref_sink(g_variant_builder_end(&builder));
    const char *zone = NULL;
    gint32 count = 0;
    printf("lookup Timezone %d", g_variant_lookup(dict, "Timezone", "&s", &zone));
    printf(" %s\n", zone);
    printf("lookup Count as s %d, as i %d", g_variant_lookup(dict, "Count", "s", NULL), g_variant_lookup(dict, "Count", "i", &count));
    printf(" %d\n", count);
    printf("lookup missing %d\n", g_variant_lookup(dict, "Missing", "s", NULL));
    GVariant *value = g_variant_lookup_value(dict, "Count", NULL);
    show("lookup value", value);

    GVariantIter it;
    gchar *key;
    GVariant *inner;
    g_variant_iter_init(&it, dict);
    while (g_variant_iter_loop(&it, "{sv}", &key, &inner)) {
        gchar *text = g_variant_print(inner, TRUE);
        printf("loop %s = %s\n", key, text);
        g_free(text);
    }
    g_variant_iter_init(&it, dict);
    while (g_variant_iter_next(&it, "{&sv}", &zone, &inner)) {
        printf("next %s %s\n", zone, g_variant_get_type_string(inner));
        g_variant_unref(inner);
    }
    g_variant_unref(dict);

    const gchar *names[] = { "one", "two", "three", NULL };
    v = g_variant_ref_sink(g_variant_new_strv(names, -1));
    gsize length;
    const gchar **got = g_variant_get_strv(v, &length);
    printf("strv %lu:", (unsigned long)length);
    for (gsize k = 0; got[k]; k++)
        printf(" %s", got[k]);
    printf("\n");
    g_free(got);
    gchar **dup = g_variant_dup_strv(v, NULL);
    printf("dup strv %u %s\n", g_strv_length(dup), dup[2]);
    g_strfreev(dup);
    gchar **caret;
    g_variant_get(v, "^as", &caret);
    printf("caret %s\n", caret[1]);
    g_strfreev(caret);
    g_variant_unref(v);

    gint32 numbers[] = { 5, 6, 7, 8 };
    v = g_variant_ref_sink(g_variant_new_fixed_array(G_VARIANT_TYPE_INT32, numbers, 4, 4));
    gsize n;
    const gint32 *fixed = g_variant_get_fixed_array(v, &n, 4);
    printf("fixed %lu %d %d\n", (unsigned long)n, fixed[0], fixed[3]);
    g_variant_unref(v);

    v = g_variant_ref_sink(g_variant_new("(mimsm(ii))", TRUE, 5, NULL, FALSE, 1, 2));
    gboolean has_i, has_pair;
    gint32 mi, p1, p2;
    gchar *ms;
    g_variant_get(v, "(mimsm(ii))", &has_i, &mi, &ms, &has_pair, &p1, &p2);
    printf("maybes %d %d %s %d %d %d\n", has_i, mi, ms ? ms : "(null)", has_pair, p1, p2);
    g_variant_unref(v);

    v = g_variant_ref_sink(g_variant_new("(is)", 3, "child"));
    gint32 ci;
    g_variant_get_child(v, 0, "i", &ci);
    GVariant *child = g_variant_get_child_value(v, 1);
    printf("child %d %s floating %d\n", ci, g_variant_get_string(child, NULL), g_variant_is_floating(child));
    g_variant_unref(child);
    g_variant_unref(v);
}

static void data(void)
{
    GVariant *v = g_variant_ref_sink(g_variant_new("(sai)", "abc", NULL));
    gsize size = g_variant_get_size(v);
    gchar *copy = g_malloc(size);
    g_variant_store(v, copy);
    GVariant *back = g_variant_ref_sink(g_variant_new_from_data(G_VARIANT_TYPE("(sai)"), copy, size, FALSE, g_free, copy));
    printf("roundtrip equal %d normal %d\n", g_variant_equal(v, back), g_variant_is_normal_form(back));
    g_variant_unref(back);
    g_variant_unref(v);

    /* A message as WebKit's IPC carries one: a type string and data. */
    GVariant *message = g_variant_ref_sink(g_variant_new("(uta{sv}ay)", 7u, (guint64)99, NULL, NULL));
    GVariantType *type = g_variant_type_new("(uta{sv}ay)");
    GBytes *bytes = g_variant_get_data_as_bytes(message);
    GVariant *received = g_variant_ref_sink(g_variant_new_from_bytes(type, bytes, FALSE));
    show("received", g_variant_ref(received));
    GVariant *swapped = g_variant_byteswap(received);
    printf("swapped floating %d\n", g_variant_is_floating(swapped));
    show("swapped", swapped);
    g_variant_unref(received);
    g_bytes_unref(bytes);
    g_variant_type_free(type);
    g_variant_unref(message);

    static const guchar bad_string[] = { 'a', 'b' };
    show("bad string", g_variant_new_from_data(G_VARIANT_TYPE_STRING, bad_string, 2, FALSE, NULL, NULL));
    static const guchar short_int[] = { 1, 2 };
    show("short int", g_variant_new_from_data(G_VARIANT_TYPE_INT32, short_int, 2, FALSE, NULL, NULL));
    static const guchar bad_array[] = { 'a', 0, 'b', 0, 9 };
    GVariant *broken = g_variant_ref_sink(g_variant_new_from_data(G_VARIANT_TYPE("as"), bad_array, 5, FALSE, NULL, NULL));
    printf("bad array children %lu normal %d\n", (unsigned long)g_variant_n_children(broken), g_variant_is_normal_form(broken));
    g_variant_unref(broken);
    static const guchar bad_variant[] = { 1, 2, 3, 4, 0, 'q', 'q' };
    show("bad variant", g_variant_new_from_data(G_VARIANT_TYPE_VARIANT, bad_variant, 7, FALSE, NULL, NULL));
    /* GLib reads data from outside at the alignment of their type. */
    guchar *good_variant = g_malloc(6);
    memcpy(good_variant, (const guchar[]) { 5, 0, 0, 0, 0, 'i' }, 6);
    show("good variant", g_variant_new_from_data(G_VARIANT_TYPE_VARIANT, good_variant, 6, FALSE, g_free, good_variant));
    static const guchar wide[] = { 1, 0, 0, 0, 2, 0, 0, 0 };
    show("swap source", g_variant_new_from_data(G_VARIANT_TYPE("(ii)"), wide, 8, TRUE, NULL, NULL));
    GVariant *source = g_variant_ref_sink(g_variant_new_from_data(G_VARIANT_TYPE("(ii)"), wide, 8, TRUE, NULL, NULL));
    show("swapped pair", g_variant_byteswap(source));
    g_variant_unref(source);
}

static void comparing(void)
{
    GVariant *a = g_variant_ref_sink(g_variant_new_string("apple"));
    GVariant *b = g_variant_ref_sink(g_variant_new_string("banana"));
    GVariant *a2 = g_variant_ref_sink(g_variant_new_string("apple"));
    printf("strings equal %d %d compare %d %d hash %d\n", g_variant_equal(a, a2), g_variant_equal(a, b),
        g_variant_compare(a, b) < 0, g_variant_compare(b, a) > 0, g_variant_hash(a) == g_variant_hash(a2));
    printf("string hash %u\n", g_variant_hash(a));
    GVariant *i = g_variant_ref_sink(g_variant_new_int32(-5));
    GVariant *j = g_variant_ref_sink(g_variant_new_int32(5));
    printf("ints compare %d hash %u\n", g_variant_compare(i, j), g_variant_hash(j));
    GVariant *x = g_variant_ref_sink(g_variant_new_int64(5));
    printf("int32 vs int64 equal %d\n", g_variant_equal(j, x));
    GVariant *t1 = g_variant_ref_sink(g_variant_new("(si)", "k", 1));
    GVariant *t2 = g_variant_ref_sink(g_variant_new("(si)", "k", 1));
    printf("tuples equal %d\n", g_variant_equal(t1, t2));
    g_variant_unref(a);
    g_variant_unref(b);
    g_variant_unref(a2);
    g_variant_unref(i);
    g_variant_unref(j);
    g_variant_unref(x);
    g_variant_unref(t1);
    g_variant_unref(t2);

    GVariant *f = g_variant_new_int32(1);
    printf("floating %d", g_variant_is_floating(f));
    g_variant_ref_sink(f);
    printf(" after sink %d", g_variant_is_floating(f));
    GVariant *taken = g_variant_take_ref(g_variant_new_int32(2));
    printf(" taken %d\n", g_variant_is_floating(taken));
    g_variant_unref(f);
    g_variant_unref(taken);
}

static void dictionaries(void)
{
    GVariantDict dict;
    g_variant_dict_init(&dict, NULL);
    g_variant_dict_insert(&dict, "width", "i", 640);
    g_variant_dict_insert(&dict, "title", "s", "window");
    g_variant_dict_insert_value(&dict, "width", g_variant_new_int32(800));
    gint32 width = 0;
    printf("dict contains %d %d lookup %d", g_variant_dict_contains(&dict, "title"), g_variant_dict_contains(&dict, "nope"),
        g_variant_dict_lookup(&dict, "width", "i", &width));
    printf(" width %d\n", width);
    gboolean removed = g_variant_dict_remove(&dict, "title");
    printf("remove %d %d\n", removed, g_variant_dict_remove(&dict, "title"));
    show("dict end", g_variant_dict_end(&dict));
    g_variant_dict_clear(&dict);
}

int main(void)
{
    types();
    basics();
    containers();
    reading();
    data();
    comparing();
    dictionaries();
    return 0;
}
