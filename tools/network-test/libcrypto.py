"""AES-128 and the RFC 3394 key wrap from OpenSSL's libcrypto (ctypes),
an implementation of its own to hold std.aes against in the Wi-Fi checks.
"""
import ctypes
import ctypes.util

_lib = ctypes.CDLL(ctypes.util.find_library('crypto') or 'libcrypto.so.3')


def _key(key, decrypt=False):
    state = ctypes.create_string_buffer(256)
    setter = _lib.AES_set_decrypt_key if decrypt else _lib.AES_set_encrypt_key
    if setter(bytes(key), 128, state) != 0:
        raise ValueError('bad AES key')
    return state


def encrypt(key, block):
    out = ctypes.create_string_buffer(16)
    _lib.AES_encrypt(bytes(block), out, _key(key))
    return out.raw


def decrypt(key, block):
    out = ctypes.create_string_buffer(16)
    _lib.AES_decrypt(bytes(block), out, _key(key, True))
    return out.raw


def wrap(kek, plain):
    out = ctypes.create_string_buffer(len(plain) + 8)
    count = _lib.AES_wrap_key(_key(kek), None, out, bytes(plain), len(plain))
    if count <= 0:
        raise ValueError('AES_wrap_key refused')
    return out.raw[:count]


def unwrap(kek, wrapped):
    out = ctypes.create_string_buffer(len(wrapped))
    count = _lib.AES_unwrap_key(_key(kek, True), None, out, bytes(wrapped), len(wrapped))
    return out.raw[:count] if count > 0 else None
