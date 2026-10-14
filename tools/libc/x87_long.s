# The long double functions of <math.h> for mlxlibc, as x87 code: a
# long double argument is 16 bytes on the stack at [rsp+8] (the second
# at [rsp+24]), the result comes back in st0, and an integer argument in
# edi or rdi, a pointer in rdi (System V). The x87 transcendental
# instructions do the work (fsin, fcos, fptan, fpatan, f2xm1, fyl2x,
# fscale, fprem, fsqrt, frndint), as glibc's do on x86_64.
#
# tools/libc/gen_x87.py assembles this (GNU as), takes the bytes and the
# symbols, and writes projects/libc/x87_long.mlx; the loader copies the
# bytes into a page of their own and binds the names to it
# (loader.mlx's special symbols). Nothing here refers to anything
# outside: every call and jump is relative, every constant comes from an
# instruction or the red zone below rsp.
        .intel_syntax noprefix
        .text

# --- the rounding helpers -----------------------------------------------
# round_mode: st0 rounded to a whole number with the mode in ax (the two
# RC bits, 0x0000 nearest, 0x0400 down, 0x0800 up, 0x0c00 toward zero);
# the control word is put back.
round_mode:
        fnstcw  word ptr [rsp-4]
        movzx   ecx, word ptr [rsp-4]
        and     cx, 0xf3ff
        or      cx, ax
        mov     word ptr [rsp-8], cx
        fldcw   word ptr [rsp-8]
        frndint
        fldcw   word ptr [rsp-4]
        ret

        .globl floorl
floorl:
        fld     tbyte ptr [rsp+8]
        mov     ax, 0x0400
        jmp     round_mode

        .globl ceill
ceill:
        fld     tbyte ptr [rsp+8]
        mov     ax, 0x0800
        jmp     round_mode

        .globl truncl
truncl:
        fld     tbyte ptr [rsp+8]
        mov     ax, 0x0c00
        jmp     round_mode

        .globl rintl
rintl:
        fld     tbyte ptr [rsp+8]
        frndint
        ret

        .globl nearbyintl
nearbyintl:
        fld     tbyte ptr [rsp+8]
        frndint
        ret

# roundl: halves away from zero. t = trunc(x); if |x - t| >= 1/2 then
# t +/- 1 (x - t is exact).
        .globl roundl
roundl:
        fld     tbyte ptr [rsp+8]       # x
        fld     st(0)                   # x x
        mov     ax, 0x0c00
        call    round_mode              # t x
        fld     st(1)                   # x t x
        fsub    st(0), st(1)            # d=x-t t x
        fabs                            # |d| t x
        fadd    st(0), st(0)            # 2|d| t x
        fld1                            # 1 2|d| t x
        fcomip  st(0), st(1)            # 1 ? 2|d|  (CF: 1 < 2|d|; ZF: equal)
        fstp    st(0)                   # t x
        ja      round_done              # 1 > 2|d|: t stays
        fldz                            # 0 t x
        fcomip  st(0), st(2)            # 0 ? x
        fld1                            # 1 t x
        ja      round_negative          # 0 > x
        faddp   st(1), st(0)            # t+1 x
        jmp     round_done
round_negative:
        fsubp   st(1), st(0)            # t-1 x
round_done:
        fstp    st(1)                   # t
        ret

        .globl lrintl
lrintl:
        fld     tbyte ptr [rsp+8]
        fistp   qword ptr [rsp-8]
        mov     rax, qword ptr [rsp-8]
        ret

        .globl llrintl
llrintl:
        jmp     lrintl

        .globl lroundl
lroundl:
        call    roundl_inner
        fistp   qword ptr [rsp-8]
        mov     rax, qword ptr [rsp-8]
        ret

        .globl llroundl
llroundl:
        jmp     lroundl

# roundl on the argument of the caller's caller (the frame one return
# address further up): [rsp+16] here.
roundl_inner:
        fld     tbyte ptr [rsp+16]
        fld     st(0)
        mov     ax, 0x0c00
        call    round_mode
        fld     st(1)
        fsub    st(0), st(1)
        fabs
        fadd    st(0), st(0)
        fld1
        fcomip  st(0), st(1)
        fstp    st(0)
        ja      round_inner_done
        fldz
        fcomip  st(0), st(2)
        fld1
        ja      round_inner_negative
        faddp   st(1), st(0)
        jmp     round_inner_done
round_inner_negative:
        fsubp   st(1), st(0)
round_inner_done:
        fstp    st(1)
        ret

# --- absolute values, signs, extremes ----------------------------------
        .globl fabsl
fabsl:
        fld     tbyte ptr [rsp+8]
        fabs
        ret

        .globl copysignl
copysignl:
        mov     al, byte ptr [rsp+33]   # y's sign byte
        and     al, 0x80
        mov     cl, byte ptr [rsp+17]   # x's
        and     cl, 0x7f
        or      al, cl
        mov     byte ptr [rsp+17], al
        fld     tbyte ptr [rsp+8]
        ret

        .globl fminl
fminl:
        fld     tbyte ptr [rsp+24]      # y
        fld     tbyte ptr [rsp+8]       # x y
        fucomi  st(0), st(1)
        fcmovnb st(0), st(1)            # y when not x < y
        fstp    st(1)
        ret

        .globl fmaxl
fmaxl:
        fld     tbyte ptr [rsp+24]      # y
        fld     tbyte ptr [rsp+8]       # x y
        fucomi  st(0), st(1)
        fcmovb  st(0), st(1)            # y when x < y
        fstp    st(1)
        ret

# fdiml: x - y when x > y, else 0.
        .globl fdiml
fdiml:
        fld     tbyte ptr [rsp+24]      # y
        fld     tbyte ptr [rsp+8]       # x y
        fucomi  st(0), st(1)
        jbe     fdim_zero
        fsubrp  st(1), st(0)            # x-y
        ret
fdim_zero:
        fstp    st(0)
        fstp    st(0)
        fldz
        ret

# --- square and cube roots, hypot ---------------------------------------
        .globl sqrtl
sqrtl:
        fld     tbyte ptr [rsp+8]
        fsqrt
        ret

        .globl hypotl
hypotl:
        fld     tbyte ptr [rsp+8]
        fmul    st(0), st(0)
        fld     tbyte ptr [rsp+24]
        fmul    st(0), st(0)
        faddp   st(1), st(0)
        fsqrt
        ret

# cbrtl: 2^(log2|x| / 3) with x's sign; 0 and the infinities as they are.
        .globl cbrtl
cbrtl:
        fld     tbyte ptr [rsp+8]       # x
        fxam
        fnstsw  ax
        and     ah, 0x45
        cmp     ah, 0x40                # zero
        je      cbrt_asis
        cmp     ah, 0x05                # infinity
        je      cbrt_asis
        cmp     ah, 0x01                # nan
        je      cbrt_asis
        fld     st(0)                   # x x
        fabs                            # |x| x
        fld1                            # 1 |x| x
        fxch    st(1)                   # |x| 1 x
        fyl2x                           # log2|x| x
        fld1
        fld1
        fld1
        faddp   st(1), st(0)
        faddp   st(1), st(0)            # 3 log2|x| x
        fdivp   st(1), st(0)            # log2|x|/3 x
        call    exp2_core               # cbrt|x| x
        fxch    st(1)                   # x cbrt|x|
        fldz                            # 0 x cbrt|x|
        fcomip  st(0), st(1)            # 0 ? x
        fstp    st(0)                   # cbrt|x|
        jbe     cbrt_asis
        fchs
cbrt_asis:
        ret

# --- powers of two and the exponentials ----------------------------------
# exp2_core: st0 = 2^st0 for a finite st0 (the whole part through fscale,
# the fraction through f2xm1); the infinities and nan as 2^x should be.
exp2_core:
        fxam
        fnstsw  ax
        and     ah, 0x45
        cmp     ah, 0x05                # infinity
        je      exp2_infinite
        cmp     ah, 0x01                # nan
        je      exp2_asis
        fld     st(0)                   # t t
        frndint                         # n t
        fsub    st(1), st(0)            # n f
        fxch    st(1)                   # f n
        f2xm1                           # 2^f-1 n
        fld1
        faddp   st(1), st(0)            # 2^f n
        fscale                          # 2^f*2^n n
        fstp    st(1)
        ret
exp2_infinite:
        fldz
        fcomip  st(0), st(1)            # 0 ? inf
        fstp    st(0)
        jb      exp2_plus_infinite
        fldz                            # 2^-inf = 0
        ret
exp2_plus_infinite:
        fld1
        fld1
        fchs
        fdivp   st(1), st(0)            # -1 / ... no: a plain +inf below
        fabs
        fld1
        fldz
        fdivp   st(1), st(0)            # 1/0 = +inf
        fstp    st(1)
exp2_asis:
        ret

        .globl exp2l
exp2l:
        fld     tbyte ptr [rsp+8]
        jmp     exp2_core

        .globl expl
expl:
        fld     tbyte ptr [rsp+8]
        fldl2e
        fmulp   st(1), st(0)            # x log2 e
        jmp     exp2_core

# --- the logarithms ------------------------------------------------------
        .globl logl
logl:
        fldln2
        fld     tbyte ptr [rsp+8]
        fyl2x
        ret

        .globl log2l
log2l:
        fld1
        fld     tbyte ptr [rsp+8]
        fyl2x
        ret

        .globl log10l
log10l:
        fldlg2
        fld     tbyte ptr [rsp+8]
        fyl2x
        ret

# powl(x, y) = 2^(y log2 x) for x > 0; x = 0 and x < 0 (a whole y takes
# |x| with the sign of (-1)^y, another y gives nan) by cases; y = 0 is 1.
        .globl powl
powl:
        fld     tbyte ptr [rsp+24]      # y
        fldz
        fcomip  st(0), st(1)            # 0 ? y
        jne     pow_y_nonzero
        jp      pow_y_nonzero
        fstp    st(0)
        fld1                            # x^0 = 1
        ret
pow_y_nonzero:
        fld     tbyte ptr [rsp+8]       # x y
        fldz
        fcomip  st(0), st(1)            # 0 ? x
        ja      pow_x_negative
        je      pow_x_zero
        jp      pow_nan
pow_positive:                           # x y, x > 0
        fyl2x                           # y log2 x
        jmp     exp2_core
pow_x_zero:                             # x y: 0^y
        fstp    st(0)                   # y
        fldz
        fcomip  st(0), st(1)            # 0 ? y
        fstp    st(0)
        ja      pow_zero_negative_y     # y < 0: infinity
        fldz
        ret
pow_zero_negative_y:
        fld1
        fldz
        fdivp   st(1), st(0)            # +inf
        ret
pow_x_negative:                         # x y, x < 0: y must be whole
        fld     st(1)                   # y x y
        fld     st(0)                   # y y x y
        frndint                         # n y x y
        fucomip st(0), st(1)            # n ? y
        fstp    st(0)                   # x y
        jne     pow_nan
        jp      pow_nan
        fabs                            # |x| y
        fld     st(1)                   # y |x| y
        fld1
        fadd    st(0), st(0)            # 2 y |x| y
        fxch    st(1)                   # y 2 |x| y
        fprem                           # y mod 2 2 |x| y
        fstp    st(1)                   # y mod 2 |x| y
        fabs                            # |y mod 2| |x| y
        fld1
        fcomip  st(0), st(1)            # 1 ? |y mod 2|  (equal: y odd)
        fstp    st(0)                   # |x| y
        sete    byte ptr [rsp-16]
        fyl2x                           # y log2|x|
        call    exp2_core
        cmp     byte ptr [rsp-16], 1
        jne     pow_done
        fchs
pow_done:
        ret
pow_nan:
        fstp    st(0)
        fstp    st(0)
        fldz
        fldz
        fdivp   st(1), st(0)            # 0/0 = nan
        ret

# --- trigonometry ----------------------------------------------------------
        .globl sinl
sinl:
        fld     tbyte ptr [rsp+8]
        fsin
        ret

        .globl cosl
cosl:
        fld     tbyte ptr [rsp+8]
        fcos
        ret

        .globl tanl
tanl:
        fld     tbyte ptr [rsp+8]
        fptan
        fstp    st(0)
        ret

        .globl atanl
atanl:
        fld     tbyte ptr [rsp+8]
        fld1
        fpatan                          # atan(x/1)
        ret

        .globl atan2l
atan2l:
        fld     tbyte ptr [rsp+8]       # y
        fld     tbyte ptr [rsp+24]      # x y
        fpatan                          # atan(y/x)
        ret

        .globl asinl
asinl:
        fld     tbyte ptr [rsp+8]       # x
        fld     st(0)                   # x x
        fmul    st(0), st(0)            # x^2 x
        fld1                            # 1 x^2 x
        fsubrp  st(1), st(0)            # 1-x^2 x
        fsqrt                           # sqrt(1-x^2) x
        fpatan                          # atan(x / sqrt(1-x^2))
        ret

        .globl acosl
acosl:
        fld     tbyte ptr [rsp+8]       # x
        fld     st(0)                   # x x
        fmul    st(0), st(0)            # x^2 x
        fld1                            # 1 x^2 x
        fsubrp  st(1), st(0)            # 1-x^2 x
        fsqrt                           # sqrt(1-x^2) x
        fxch    st(1)                   # x sqrt(1-x^2)
        fpatan                          # atan(sqrt(1-x^2) / x)
        ret

# --- the hyperbolic functions, on e^x -----------------------------------
# exp_core: st0 = e^st0.
exp_core:
        fldl2e
        fmulp   st(1), st(0)
        jmp     exp2_core

        .globl sinhl
sinhl:
        fld     tbyte ptr [rsp+8]
        call    exp_core                # e^x
        fld1
        fdiv    st(0), st(1)            # e^-x e^x
        fsubp   st(1), st(0)            # e^x - e^-x
        fld1
        fadd    st(0), st(0)
        fdivp   st(1), st(0)            # / 2
        ret

        .globl coshl
coshl:
        fld     tbyte ptr [rsp+8]
        call    exp_core
        fld1
        fdiv    st(0), st(1)
        faddp   st(1), st(0)            # e^x + e^-x
        fld1
        fadd    st(0), st(0)
        fdivp   st(1), st(0)
        ret

        .globl tanhl
tanhl:
        fld     tbyte ptr [rsp+8]
        fadd    st(0), st(0)            # 2x
        call    exp_core                # e^2x
        fld1                            # 1 e
        fld     st(1)                   # e 1 e
        fsub    st(0), st(1)            # e-1 1 e
        fxch    st(2)                   # e 1 e-1
        faddp   st(1), st(0)            # e+1 e-1
        fdivp   st(1), st(0)            # (e-1)/(e+1)
        ret

# --- remainders, scaling -------------------------------------------------
        .globl fmodl
fmodl:
        fld     tbyte ptr [rsp+24]      # y
        fld     tbyte ptr [rsp+8]       # x y
fmod_loop:
        fprem
        fnstsw  ax
        test    ah, 4
        jnz     fmod_loop
        fstp    st(1)
        ret

        .globl remainderl
remainderl:
        fld     tbyte ptr [rsp+24]
        fld     tbyte ptr [rsp+8]
remainder_loop:
        fprem1
        fnstsw  ax
        test    ah, 4
        jnz     remainder_loop
        fstp    st(1)
        ret

        .globl ldexpl
ldexpl:
        movsxd  rax, edi
        mov     qword ptr [rsp-8], rax
        fild    qword ptr [rsp-8]       # n
        fld     tbyte ptr [rsp+8]       # x n
        fscale                          # x 2^n n
        fstp    st(1)
        ret

        .globl scalbnl
scalbnl:
        jmp     ldexpl

        .globl scalblnl
scalblnl:
        mov     qword ptr [rsp-8], rdi
        fild    qword ptr [rsp-8]
        fld     tbyte ptr [rsp+8]
        fscale
        fstp    st(1)
        ret

# frexpl(x, int *e): x = m 2^e with m in [1/2, 1); 0, the infinities
# and nan as they are with e = 0.
        .globl frexpl
frexpl:
        fld     tbyte ptr [rsp+8]       # x
        fxam
        fnstsw  ax
        and     ah, 0x45
        cmp     ah, 0x40                # zero
        je      frexp_asis
        cmp     ah, 0x05                # infinity
        je      frexp_asis
        cmp     ah, 0x01                # nan
        je      frexp_asis
        fxtract                         # sig exp  (sig in [1, 2))
        fxch    st(1)                   # exp sig
        fld1
        faddp   st(1), st(0)            # exp+1 sig
        fistp   dword ptr [rdi]         # sig
        fld1
        fadd    st(0), st(0)            # 2 sig
        fdivp   st(1), st(0)            # sig/2
        ret
frexp_asis:
        mov     dword ptr [rdi], 0
        ret

# modfl(x, long double *whole): the fraction; the whole part stored.
        .globl modfl
modfl:
        fld     tbyte ptr [rsp+8]       # x
        fxam
        fnstsw  ax
        and     ah, 0x45
        cmp     ah, 0x05                # infinity: the whole part, 0 with its sign
        je      modf_infinite
        fld     st(0)                   # x x
        mov     ax, 0x0c00
        call    round_mode              # t x
        fld     st(0)                   # t t x
        fstp    tbyte ptr [rdi]         # t x
        fsubp   st(1), st(0)            # x-t
        ret
modf_infinite:
        fld     st(0)
        fstp    tbyte ptr [rdi]
        fldz                            # 0 x
        fxch    st(1)                   # x 0
        fstp    st(0)                   # 0
        mov     al, byte ptr [rsp+17]
        and     al, 0x80
        jz      modf_done
        fchs
modf_done:
        ret
