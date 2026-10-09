#!/usr/bin/env python3
"""Compares MLX Capture's libpipewire (projects/desktop/libpipewire) with
PipeWire's on the functions that work on values alone: the version and
names, the state and direction strings, the properties (pw_properties_*:
building, lookup, parsing, copying, updating, the dict C reads), the
object infos (pw_node_info_update and friends: merging by change_mask),
and a thread loop (events, timers, timed waits). Both libraries are driven
the same way through ctypes; every result must be the same.

    tools/libpipewire_values.py OURS.so THEIRS.so

Prints what differs and exits 1; the last line counts the results compared.
"""
import ctypes, os, sys, time

os.environ.setdefault("PIPEWIRE_DEBUG", "0")   # PipeWire's warnings about the values meant to fail
from ctypes import (c_uint32, c_uint64, c_int32, c_int, c_char_p, c_void_p, POINTER, Structure, byref, cast, CFUNCTYPE)


class Item(Structure):
    _fields_ = [("key", c_char_p), ("value", c_char_p)]


class Dict(Structure):
    _fields_ = [("flags", c_uint32), ("n_items", c_uint32), ("items", POINTER(Item))]


class Properties(Structure):
    _fields_ = [("dict", Dict), ("flags", c_uint32)]


class Param(Structure):
    _fields_ = [("id", c_uint32), ("flags", c_uint32), ("user", c_uint32), ("seq", c_int32), ("pad", c_uint32 * 4)]


class Node(Structure):
    _fields_ = [("id", c_uint32), ("max_in", c_uint32), ("max_out", c_uint32), ("change_mask", c_uint64),
                ("n_in", c_uint32), ("n_out", c_uint32), ("state", c_uint32), ("error", c_char_p),
                ("props", POINTER(Dict)), ("params", POINTER(Param)), ("n_params", c_uint32)]


class Port(Structure):
    _fields_ = [("id", c_uint32), ("direction", c_uint32), ("change_mask", c_uint64), ("props", POINTER(Dict)),
                ("params", POINTER(Param)), ("n_params", c_uint32)]


class Core(Structure):
    _fields_ = [("id", c_uint32), ("cookie", c_uint32), ("user_name", c_char_p), ("host_name", c_char_p),
                ("version", c_char_p), ("name", c_char_p), ("change_mask", c_uint64), ("props", POINTER(Dict))]


def text(address):
    return cast(address, c_char_p).value


def dict_items(dict_pointer):
    if not dict_pointer:
        return None
    d = dict_pointer.contents
    return [(d.items[i].key, d.items[i].value) for i in range(d.n_items)]


def results(path):
    lib = ctypes.CDLL(path)
    out = []
    for name in ("pw_properties_new", "pw_properties_new_dict", "pw_properties_new_string", "pw_properties_copy",
                 "pw_properties_get", "pw_properties_iterate", "pw_get_library_version", "pw_get_application_name",
                 "pw_get_prgname", "pw_direction_as_string", "pw_stream_state_as_string", "pw_filter_state_as_string",
                 "pw_node_state_as_string", "pw_link_state_as_string", "pw_get_client_name", "pw_get_user_name",
                 "pw_get_host_name", "pw_strip", "pw_split_strv", "pw_node_info_update", "pw_node_info_merge",
                 "pw_port_info_update", "pw_core_info_update", "pw_thread_loop_new", "pw_thread_loop_get_loop"):
        getattr(lib, name).restype = c_void_p
    lib.pw_init(None, None)

    # Versions and names.
    out.append(text(lib.pw_get_library_version()))
    out.append(text(lib.pw_get_application_name()) is None)
    out.append(lib.pw_check_library_version(0, 3, 0))
    out.append(lib.pw_check_library_version(1, 0, 5))
    out.append(lib.pw_check_library_version(2, 0, 0))
    for d in (0, 1, 2):
        out.append(text(lib.pw_direction_as_string(d)))
    for s in (-1, 0, 1, 2, 3, 4):
        out.append(text(lib.pw_stream_state_as_string(s)))
        out.append(text(lib.pw_filter_state_as_string(s)))
        out.append(text(lib.pw_node_state_as_string(s)))
        out.append(text(lib.pw_link_state_as_string(s)))
    out.append(text(lib.pw_get_client_name()) is not None)
    out.append(text(lib.pw_get_user_name()) is not None)
    out.append(text(lib.pw_get_host_name()) is not None)

    # Strings.
    lib.pw_strip.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    buffer = ctypes.create_string_buffer(b"  hello world \t\n")
    out.append(text(lib.pw_strip(buffer, b" \t\n")))
    count = c_int(0)
    lib.pw_split_strv.argtypes = [c_char_p, c_char_p, c_int, POINTER(c_int)]
    strv = lib.pw_split_strv(b"a, b,,c", b", ", 10, byref(count))
    parts = cast(strv, POINTER(c_char_p))
    out.append((count.value, [parts[i] for i in range(count.value)]))
    lib.pw_free_strv.argtypes = [c_void_p]
    lib.pw_free_strv(strv)

    # Properties.
    lib.pw_properties_new.argtypes = [c_char_p, c_char_p, c_char_p, c_char_p, c_char_p]
    props = lib.pw_properties_new(b"media.class", b"Video/Source", b"node.name", b"cast", None)
    lib.pw_properties_get.argtypes = [c_void_p, c_char_p]
    lib.pw_properties_set.argtypes = [c_void_p, c_char_p, c_char_p]
    lib.pw_properties_fetch_uint32.argtypes = [c_void_p, c_char_p, POINTER(c_uint32)]
    lib.pw_properties_fetch_bool.argtypes = [c_void_p, c_char_p, POINTER(ctypes.c_bool)]
    lib.pw_properties_fetch_int32.argtypes = [c_void_p, c_char_p, POINTER(c_int32)]
    lib.pw_properties_fetch_int64.argtypes = [c_void_p, c_char_p, POINTER(ctypes.c_int64)]
    lib.pw_properties_fetch_uint64.argtypes = [c_void_p, c_char_p, POINTER(c_uint64)]
    lib.pw_properties_add.argtypes = [c_void_p, c_void_p]
    lib.pw_properties_add_keys.argtypes = [c_void_p, c_void_p, POINTER(c_char_p)]
    lib.pw_properties_iterate.argtypes = [c_void_p, POINTER(c_void_p)]
    lib.pw_properties_serialize_dict.argtypes = [c_void_p, c_void_p, c_uint32]
    p = cast(props, POINTER(Properties)).contents
    out.append((p.dict.n_items, p.dict.flags, p.flags))
    out.append(text(lib.pw_properties_get(props, b"node.name")))
    out.append(text(lib.pw_properties_get(props, b"nothing")))
    out.append(lib.pw_properties_set(props, b"node.name", b"cast2"))      # changed: 1
    out.append(lib.pw_properties_set(props, b"node.name", b"cast2"))      # same: 0
    out.append(lib.pw_properties_set(props, b"node.rate", b"30"))         # new: 1
    out.append(lib.pw_properties_set(props, b"nothing", None))            # removing what is not there: 0
    out.append(lib.pw_properties_set(props, b"media.class", None))        # removed: 1
    out.append(dict_items(cast(props, POINTER(Dict))))
    value = c_uint32(0)
    out.append((lib.pw_properties_fetch_uint32(props, b"node.rate", byref(value)), value.value))
    out.append(lib.pw_properties_fetch_uint32(props, b"node.name", byref(value)))
    flag = ctypes.c_bool(False)
    lib.pw_properties_set(props, b"node.flag", b"true")
    out.append((lib.pw_properties_fetch_bool(props, b"node.flag", byref(flag)), flag.value))
    for word in (b"true", b"1", b"yes", b"false", b"0", b"other", b"TRUE"):
        lib.pw_properties_set(props, b"node.flag", word)
        out.append((lib.pw_properties_fetch_bool(props, b"node.flag", byref(flag)), flag.value))
    number32 = c_int32(0)
    number64 = ctypes.c_int64(0)
    unsigned64 = c_uint64(0)
    for word in (b"42", b"-7", b"0x10", b"junk", b"", b"9999999999"):
        lib.pw_properties_set(props, b"node.number", word)
        out.append((lib.pw_properties_fetch_int32(props, b"node.number", byref(number32)), number32.value))
        out.append((lib.pw_properties_fetch_int64(props, b"node.number", byref(number64)), number64.value))
        out.append((lib.pw_properties_fetch_uint64(props, b"node.number", byref(unsigned64)), unsigned64.value))
    out.append(lib.pw_properties_set(props, b"node.number", None))
    state = c_void_p(None)
    seen = []
    while True:
        key = lib.pw_properties_iterate(props, byref(state))
        if key is None:
            break
        seen.append(text(key))
    out.append(seen)
    extra = (Item * 2)(Item(b"node.flag", b"never"), Item(b"node.extra", b"yes"))
    extra_dict = Dict(0, 2, extra)
    out.append(lib.pw_properties_add(props, byref(extra_dict)))             # only node.extra is new: 1
    out.append(dict_items(cast(props, POINTER(Dict))))
    lib.pw_properties_set(props, b"node.extra", None)
    only = (c_char_p * 2)(b"node.flag", None)
    out.append(lib.pw_properties_add_keys(props, byref(extra_dict), only))  # node.flag is there: 0
    out.append(dict_items(cast(props, POINTER(Dict))))
    copy = lib.pw_properties_copy(props)
    lib.pw_properties_set(copy, b"node.name", b"copy")
    out.append((text(lib.pw_properties_get(props, b"node.name")), text(lib.pw_properties_get(copy, b"node.name"))))
    lib.pw_properties_update.argtypes = [c_void_p, c_void_p]
    out.append(lib.pw_properties_update(props, byref(cast(copy, POINTER(Dict)).contents)))
    out.append(dict_items(cast(props, POINTER(Dict))))
    lib.pw_properties_update_keys.argtypes = [c_void_p, c_void_p, POINTER(c_char_p)]
    keys = (c_char_p * 3)(b"node.name", b"never", None)
    lib.pw_properties_set(copy, b"node.name", b"keyed")
    lib.pw_properties_set(copy, b"node.rate", b"60")
    out.append(lib.pw_properties_update_keys(props, byref(cast(copy, POINTER(Dict)).contents), keys))
    out.append(dict_items(cast(props, POINTER(Dict))))
    lib.pw_properties_update_ignore.argtypes = [c_void_p, c_void_p, POINTER(c_char_p)]
    out.append(lib.pw_properties_update_ignore(props, byref(cast(copy, POINTER(Dict)).contents), keys))
    out.append(dict_items(cast(props, POINTER(Dict))))
    lib.pw_properties_update_string.argtypes = [c_void_p, c_char_p, ctypes.c_size_t]
    line = b'{ a.b = "quoted value", c = 5, node.rate = 24 }'
    out.append(lib.pw_properties_update_string(props, line, len(line)))
    out.append(dict_items(cast(props, POINTER(Dict))))
    lib.pw_properties_new_string.argtypes = [c_char_p]
    from_string = lib.pw_properties_new_string(b"x=1 y = two")
    out.append(dict_items(cast(from_string, POINTER(Dict))))
    libc = ctypes.CDLL(None)
    libc.tmpfile.restype = c_void_p
    libc.fflush.argtypes = [c_void_p]
    libc.rewind.argtypes = [c_void_p]
    libc.fread.argtypes = [c_void_p, ctypes.c_size_t, ctypes.c_size_t, c_void_p]
    libc.fclose.argtypes = [c_void_p]
    for flags in (0, 1):
        stream = libc.tmpfile()
        out.append(lib.pw_properties_serialize_dict(stream, byref(cast(props, POINTER(Dict)).contents), flags))
        libc.fflush(stream)
        libc.rewind(stream)
        serialized = ctypes.create_string_buffer(4096)
        got = libc.fread(serialized, 1, 4096, stream)
        libc.fclose(stream)
        out.append(serialized.raw[:got])
    lib.pw_properties_clear.argtypes = [c_void_p]
    lib.pw_properties_clear(copy)
    out.append(cast(copy, POINTER(Dict)).contents.n_items)
    lib.pw_properties_free.argtypes = [c_void_p]
    for each in (props, copy, from_string):
        lib.pw_properties_free(each)

    # Object infos.
    items = (Item * 2)(Item(b"node.name", b"cast"), Item(b"media.class", None))
    d = Dict(0, 2, items)
    params = (Param * 2)(Param(3, 1, 0, 0), Param(4, 3, 0, 0))
    update = Node(7, 1, 2, 31, 1, 1, 3, b"oops", ctypes.pointer(d), params, 2)
    lib.pw_node_info_update.argtypes = [c_void_p, POINTER(Node)]
    lib.pw_node_info_merge.argtypes = [c_void_p, POINTER(Node), ctypes.c_bool]
    lib.pw_node_info_free.argtypes = [c_void_p]
    info = lib.pw_node_info_update(None, byref(update))
    i = cast(info, POINTER(Node)).contents
    out.append([i.id, i.max_in, i.max_out, i.change_mask, i.n_in, i.n_out, i.state, i.error,
                dict_items(i.props), i.n_params, [(p.id, p.flags, p.user) for p in i.params[:2]]])
    params2 = (Param * 3)(Param(3, 1, 0, 0), Param(4, 5, 0, 0), Param(9, 1, 0, 0))
    update2 = Node(7, 1, 2, 16, 0, 0, 0, None, None, params2, 3)
    info = lib.pw_node_info_update(info, byref(update2))
    i = cast(info, POINTER(Node)).contents
    out.append([i.change_mask, i.n_in, i.state, i.error, dict_items(i.props), i.n_params,
                [(p.id, p.flags, p.user) for p in i.params[:3]]])
    info = lib.pw_node_info_merge(info, byref(update), False)
    i = cast(info, POINTER(Node)).contents
    out.append([i.change_mask, i.n_params, [(p.id, p.flags, p.user) for p in i.params[:2]], i.error])
    lib.pw_node_info_free(info)
    lib.pw_port_info_update.argtypes = [c_void_p, POINTER(Port)]
    lib.pw_port_info_free.argtypes = [c_void_p]
    port = Port(5, 1, 3, ctypes.pointer(d), params, 2)
    info = lib.pw_port_info_update(None, byref(port))
    i = cast(info, POINTER(Port)).contents
    out.append([i.id, i.direction, i.change_mask, dict_items(i.props), i.n_params, [(p.id, p.flags, p.user) for p in i.params[:2]]])
    lib.pw_port_info_free(info)
    lib.pw_core_info_update.argtypes = [c_void_p, POINTER(Core)]
    lib.pw_core_info_free.argtypes = [c_void_p]
    core = Core(0, 99, b"me", b"here", b"1.0.5", b"pipewire-0", 1, ctypes.pointer(d))
    info = lib.pw_core_info_update(None, byref(core))
    i = cast(info, POINTER(Core)).contents
    out.append([i.id, i.cookie, i.user_name, i.host_name, i.version, i.name, i.change_mask, dict_items(i.props)])
    lib.pw_core_info_free(info)

    # A thread loop: an event and a timer fire on its thread, a timed wait
    # times out, a signal ends a wait.
    lib.pw_thread_loop_new.argtypes = [c_char_p, c_void_p]
    lib.pw_thread_loop_get_loop.argtypes = [c_void_p]
    for name in ("pw_thread_loop_start", "pw_thread_loop_stop", "pw_thread_loop_lock", "pw_thread_loop_unlock",
                 "pw_thread_loop_wait", "pw_thread_loop_destroy", "pw_thread_loop_in_thread"):
        getattr(lib, name).argtypes = [c_void_p]
    lib.pw_thread_loop_signal.argtypes = [c_void_p, ctypes.c_bool]
    lib.pw_thread_loop_in_thread.restype = ctypes.c_bool
    lib.pw_thread_loop_timed_wait.argtypes = [c_void_p, c_int]
    loop = lib.pw_thread_loop_new(b"check", None)
    inner = lib.pw_thread_loop_get_loop(loop)
    fired = []
    EVENT = CFUNCTYPE(None, c_void_p, c_uint64)
    TIMER = CFUNCTYPE(None, c_void_p, c_uint64)
    on_event = EVENT(lambda data, count: fired.append(("event", count, bool(lib.pw_thread_loop_in_thread(loop)))) or lib.pw_thread_loop_signal(loop, False))
    on_timer = TIMER(lambda data, expirations: fired.append(("timer", expirations)) or lib.pw_thread_loop_signal(loop, False))
    # pw_loop's functions are method tables; the headers' inline wrappers
    # are what programs call, so the table is read as they read it.
    iface_utils = cast(inner + 24, POINTER(c_void_p)).contents.value   # loop->utils
    funcs = cast(iface_utils + 16, POINTER(c_void_p)).contents.value   # utils->iface.cb.funcs
    add_event = CFUNCTYPE(c_void_p, c_void_p, EVENT, c_void_p)(cast(funcs + 8 + 4 * 8, POINTER(c_void_p)).contents.value)
    signal_event = CFUNCTYPE(c_int, c_void_p, c_void_p)(cast(funcs + 8 + 5 * 8, POINTER(c_void_p)).contents.value)
    add_timer = CFUNCTYPE(c_void_p, c_void_p, TIMER, c_void_p)(cast(funcs + 8 + 6 * 8, POINTER(c_void_p)).contents.value)
    update_timer = CFUNCTYPE(c_int, c_void_p, c_void_p, c_void_p, c_void_p, ctypes.c_bool)(cast(funcs + 8 + 7 * 8, POINTER(c_void_p)).contents.value)
    data = cast(iface_utils + 24, POINTER(c_void_p)).contents.value     # utils->iface.cb.data
    lib.pw_thread_loop_lock(loop)
    event = add_event(data, on_event, None)
    timer = add_timer(data, on_timer, None)
    out.append(lib.pw_thread_loop_start(loop))
    out.append(lib.pw_thread_loop_timed_wait(loop, 1))                  # nobody signals: -ETIMEDOUT
    signal_event(data, event)
    signal_event(data, event)
    lib.pw_thread_loop_wait(loop)
    out.append(fired[-1])
    when = (ctypes.c_long * 2)(0, 20_000_000)
    out.append(update_timer(data, timer, byref(when), None, False))
    lib.pw_thread_loop_wait(loop)
    out.append(fired[-1])
    out.append(bool(lib.pw_thread_loop_in_thread(loop)))
    lib.pw_thread_loop_unlock(loop)
    lib.pw_thread_loop_stop(loop)
    lib.pw_thread_loop_destroy(loop)
    lib.pw_deinit()
    return out


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    ours = results(sys.argv[1])
    theirs = results(sys.argv[2])
    differences = 0
    for index, (a, b) in enumerate(zip(ours, theirs)):
        if a != b:
            differences += 1
            print(f"result {index}: ours {a!r}, PipeWire's {b!r}")
    if len(ours) != len(theirs):
        differences += 1
        print(f"{len(ours)} results against {len(theirs)}")
    print(f"{len(theirs)} results compared, {differences} differ")
    sys.exit(1 if differences else 0)


if __name__ == "__main__":
    main()
