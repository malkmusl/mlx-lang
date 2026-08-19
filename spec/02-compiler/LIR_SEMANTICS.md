# Zin LIR instruction semantics

This file is normative together with `lir.xml`.

## Integer arithmetic

All integer instructions carry an exact bit width and signedness. No backend-native width silently changes source semantics.

- `add/sub/mul`: checked arithmetic. A statically known overflow is `ZIN-E4002`; safe runtime overflow traps.
- `add_wrap/sub_wrap/mul_wrap`: modulo 2^N.
- `add_sat/sub_sat/mul_sat`: clamp to the exact iN/uN numeric bounds.
- `div/rem`: signedness is explicit. Division by zero traps in safe modes; constant division by zero is a compile error.
- `and/or/xor/not`: exact-width bitwise operations.

## Shifts

Each shift carries:
- direction: left/right
- count policy: checked/wrapping
- result policy: checked/saturating where applicable
- exact integer width N

`shl_checked`: count must be 0..N-1; value loss is checked.
`shr_checked`: count must be 0..N-1; unsigned is logical, signed is arithmetic.
`shl_count_wrap` / `shr_count_wrap`: effective count = count mod N.
`shl_sat`: checked count, saturating value result.
`shl_count_wrap_sat`: wrapped count plus saturating value result.

## Shift-combine

`shift_combine` contains:
- `position = prefix|suffix`
- `direction = left|right`
- `count_policy = checked|wrapping`
- `shift_result_policy = checked|saturating`
- `combiner = and|or|xor|add|sub|mul`
- `combiner_overflow = checked|wrapping|saturating` for arithmetic combiners

Operands are evaluated exactly once. The shift is performed from the original left value, then the original and shifted values are combined in the specified position.

## Compound assignment

Compound assignment is lowered through an addressable destination temporary:
1. evaluate destination address once
2. load old value once
3. evaluate RHS once
4. execute operator
5. store once

No source-visible getter/index/call used to locate the destination may execute twice.

## Arbitrary-width lowering

The canonical LIR preserves iN/uN widths up to 4096 bits. Backend lowering may use:
- one native register for <=64 bits,
- register pairs or target-supported 128-bit sequences for <=128 bits,
- multi-limb stack/register sequences for larger widths.

Every operation masks or sign-normalizes high unused bits as required by its exact source width. Packed loads/stores use the exact bit range and may not overwrite adjacent fields.
