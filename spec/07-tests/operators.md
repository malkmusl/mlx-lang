# Normative operator conformance

An implementation is not Mlx 1.0 conforming until it has positive and negative tests for:

- `u1`, `u2`, `u3`, non-byte widths, `u4096`/`i4096`, and rejection of width 0 or >4096
- checked, wrapping and saturating `+ - *` and every compound-assignment form
- `& | ^ ~` and compound assignments
- `<< >> <<% >>% <<| <<%|` and assignment forms
- prefix shift-combine: `&<< |<< ^<<`, `&>> |>> ^>>`
- suffix shift-combine: `<<& <<^`, `>>& >>| >>^`
- arithmetic shift-combine, including checked/wrapping/saturating combiner forms
- every valid shift-combine compound assignment
- destination single-evaluation for compound assignment
- constant invalid shift count -> `MLX-E4012`
- constant checked left-shift overflow -> `MLX-E4013`
- invalid operand types -> `MLX-E4014`
- invalid assignment target -> `MLX-E4015`
- identical explicit wrapping/saturating semantics in all optimization modes
