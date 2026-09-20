/examples/coreutils/build.sh
building mlx-true
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 31
[mlx4 info] imports discovered: 57
[mlx4 info] source bytes: 95354
[mlx4 info] tokens scanned: 22830
[mlx4 info] AST nodes parsed: 11411
[mlx4 info] declarations discovered: 810
[mlx4 info] functions discovered: 328
[mlx4 info] source I/O: 1.877 ms
[mlx4 info] lexing: 9.643 ms
[mlx4 info] parsing: 6.638 ms
[mlx4 1/9] done in 20.167 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.594 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 323
[mlx4 info] symbols registered: 632
[mlx4 3/9] done in 6.368 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2122
[mlx4 info] type intern probes: 1839
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 3733
[mlx4 info] aggregate method lookups: 444
[mlx4 info] aggregate method probes: 74
[mlx4 info] maximum aggregate method probes: 2
[mlx4 4/9] done in 17.603 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 0
[mlx4 info] calls automatically inlined: 0
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 4181
[mlx4 info] expression type cache hits: 1710
[mlx4 info] resolved type queries: 467
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 13129
[mlx4 info] LIR extra words: 2627
[mlx4 info] LIR symbols: 283
[mlx4 info] string literals: 7
[mlx4 info] type intern calls: 2469
[mlx4 info] type intern probes: 2194
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 3942
[mlx4 info] aggregate method lookups: 1044
[mlx4 info] aggregate method probes: 125
[mlx4 info] maximum aggregate method probes: 2
[mlx4 5/9] done in 23.502 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 13129
[mlx4 info] LIR instructions after optimization: 445
[mlx4 info] constants folded: 1
[mlx4 info] constant branches simplified: 0
[mlx4 info] comparison branches fused: 7
[mlx4 info] local loads forwarded: 25
[mlx4 info] copy operands propagated: 25
[mlx4 info] local stack slots promoted: 25
[mlx4 info] local stores eliminated: 15
[mlx4 info] unreachable functions eliminated: 269
[mlx4 info] unreachable function instructions eliminated: 12571
[mlx4 info] post-terminator instructions eliminated: 40
[mlx4 info] dead instructions eliminated: 26
[mlx4 6/9] done in 11.595 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 3854
[mlx4 info] backend symbols: 75
[mlx4 info] backend fixups: 48
[mlx4 info] backend stack loads forwarded: 4
[mlx4 info] backend cached rax loads: 31
[mlx4 info] backend memory operands: 14
[mlx4 info] backend deferred spills: 74
[mlx4 info] backend retained rax uses: 74
[mlx4 info] backend fallthrough branches removed: 37
[mlx4 info] backend direct calls: 18
[mlx4 info] aggregate allocation sites: 3
[mlx4 info] aggregate allocation bytes (static): 40
[mlx4 7/9] done in 3.166 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 52
[mlx4 info] maximum lookup probes: 2
[mlx4 8/9] done in 0.141 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5029448
[mlx4 9/9] done in 1.440 ms
[mlx4] total: 85.588 ms
building mlx-false
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 31
[mlx4 info] imports discovered: 57
[mlx4 info] source bytes: 95359
[mlx4 info] tokens scanned: 22830
[mlx4 info] AST nodes parsed: 11411
[mlx4 info] declarations discovered: 810
[mlx4 info] functions discovered: 328
[mlx4 info] source I/O: 1.447 ms
[mlx4 info] lexing: 7.667 ms
[mlx4 info] parsing: 5.015 ms
[mlx4 1/9] done in 15.590 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.384 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 323
[mlx4 info] symbols registered: 632
[mlx4 3/9] done in 5.175 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2122
[mlx4 info] type intern probes: 1841
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 3733
[mlx4 info] aggregate method lookups: 444
[mlx4 info] aggregate method probes: 74
[mlx4 info] maximum aggregate method probes: 2
[mlx4 4/9] done in 14.145 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 0
[mlx4 info] calls automatically inlined: 0
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 4181
[mlx4 info] expression type cache hits: 1710
[mlx4 info] resolved type queries: 467
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 13129
[mlx4 info] LIR extra words: 2627
[mlx4 info] LIR symbols: 283
[mlx4 info] string literals: 7
[mlx4 info] type intern calls: 2469
[mlx4 info] type intern probes: 2196
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 3942
[mlx4 info] aggregate method lookups: 1044
[mlx4 info] aggregate method probes: 125
[mlx4 info] maximum aggregate method probes: 2
[mlx4 5/9] done in 23.478 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 13129
[mlx4 info] LIR instructions after optimization: 445
[mlx4 info] constants folded: 1
[mlx4 info] constant branches simplified: 0
[mlx4 info] comparison branches fused: 7
[mlx4 info] local loads forwarded: 25
[mlx4 info] copy operands propagated: 25
[mlx4 info] local stack slots promoted: 25
[mlx4 info] local stores eliminated: 15
[mlx4 info] unreachable functions eliminated: 269
[mlx4 info] unreachable function instructions eliminated: 12571
[mlx4 info] post-terminator instructions eliminated: 40
[mlx4 info] dead instructions eliminated: 26
[mlx4 6/9] done in 10.068 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 3862
[mlx4 info] backend symbols: 75
[mlx4 info] backend fixups: 48
[mlx4 info] backend stack loads forwarded: 4
[mlx4 info] backend cached rax loads: 31
[mlx4 info] backend memory operands: 14
[mlx4 info] backend deferred spills: 74
[mlx4 info] backend retained rax uses: 74
[mlx4 info] backend fallthrough branches removed: 37
[mlx4 info] backend direct calls: 18
[mlx4 info] aggregate allocation sites: 3
[mlx4 info] aggregate allocation bytes (static): 40
[mlx4 7/9] done in 3.060 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 52
[mlx4 info] maximum lookup probes: 2
[mlx4 8/9] done in 0.158 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5029504
[mlx4 9/9] done in 1.122 ms
[mlx4] total: 74.189 ms
building mlx-echo
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 31
[mlx4 info] imports discovered: 57
[mlx4 info] source bytes: 99154
[mlx4 info] tokens scanned: 23818
[mlx4 info] AST nodes parsed: 12014
[mlx4 info] declarations discovered: 838
[mlx4 info] functions discovered: 335
[mlx4 info] source I/O: 1.626 ms
[mlx4 info] lexing: 8.526 ms
[mlx4 info] parsing: 5.434 ms
[mlx4 1/9] done in 17.093 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.405 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 330
[mlx4 info] symbols registered: 643
[mlx4 3/9] done in 4.892 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2156
[mlx4 info] type intern probes: 1863
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4014
[mlx4 info] aggregate method lookups: 468
[mlx4 info] aggregate method probes: 74
[mlx4 info] maximum aggregate method probes: 2
[mlx4 4/9] done in 13.453 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 1
[mlx4 info] calls automatically inlined: 1
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 4513
[mlx4 info] expression type cache hits: 1835
[mlx4 info] resolved type queries: 500
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 14209
[mlx4 info] LIR extra words: 2746
[mlx4 info] LIR symbols: 290
[mlx4 info] string literals: 5
[mlx4 info] type intern calls: 2523
[mlx4 info] type intern probes: 2240
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4226
[mlx4 info] aggregate method lookups: 1101
[mlx4 info] aggregate method probes: 125
[mlx4 info] maximum aggregate method probes: 2
[mlx4 5/9] done in 22.814 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 14209
[mlx4 info] LIR instructions after optimization: 1315
[mlx4 info] constants folded: 4
[mlx4 info] constant branches simplified: 3
[mlx4 info] comparison branches fused: 24
[mlx4 info] local loads forwarded: 43
[mlx4 info] copy operands propagated: 43
[mlx4 info] local stack slots promoted: 66
[mlx4 info] local stores eliminated: 27
[mlx4 info] unreachable functions eliminated: 265
[mlx4 info] unreachable function instructions eliminated: 12615
[mlx4 info] post-terminator instructions eliminated: 110
[mlx4 info] dead instructions eliminated: 52
[mlx4 6/9] done in 11.519 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 9766
[mlx4 info] backend symbols: 243
[mlx4 info] backend fixups: 148
[mlx4 info] backend stack loads forwarded: 49
[mlx4 info] backend cached rax loads: 127
[mlx4 info] backend memory operands: 42
[mlx4 info] backend deferred spills: 255
[mlx4 info] backend retained rax uses: 255
[mlx4 info] backend fallthrough branches removed: 139
[mlx4 info] backend direct calls: 41
[mlx4 info] aggregate allocation sites: 11
[mlx4 info] aggregate allocation bytes (static): 104
[mlx4 7/9] done in 3.736 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 167
[mlx4 info] maximum lookup probes: 3
[mlx4 8/9] done in 0.195 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5264752
[mlx4 9/9] done in 1.325 ms
[mlx4] total: 76.442 ms
building mlx-cat
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 32
[mlx4 info] imports discovered: 59
[mlx4 info] source bytes: 109315
[mlx4 info] tokens scanned: 26239
[mlx4 info] AST nodes parsed: 13353
[mlx4 info] declarations discovered: 914
[mlx4 info] functions discovered: 355
[mlx4 info] source I/O: 1.701 ms
[mlx4 info] lexing: 9.427 ms
[mlx4 info] parsing: 5.997 ms
[mlx4 1/9] done in 18.740 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.384 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 358
[mlx4 info] symbols registered: 671
[mlx4 3/9] done in 5.657 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2273
[mlx4 info] type intern probes: 1951
[mlx4 info] maximum type intern probes: 2
[mlx4 info] primitive type cache hits: 4453
[mlx4 info] aggregate method lookups: 557
[mlx4 info] aggregate method probes: 73
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 17.640 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 17
[mlx4 info] calls automatically inlined: 17
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 5236
[mlx4 info] expression type cache hits: 2163
[mlx4 info] resolved type queries: 558
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 15904
[mlx4 info] LIR extra words: 3138
[mlx4 info] LIR symbols: 310
[mlx4 info] string literals: 27
[mlx4 info] type intern calls: 2641
[mlx4 info] type intern probes: 2319
[mlx4 info] maximum type intern probes: 2
[mlx4 info] primitive type cache hits: 4682
[mlx4 info] aggregate method lookups: 1303
[mlx4 info] aggregate method probes: 121
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 28.459 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 15904
[mlx4 info] LIR instructions after optimization: 3093
[mlx4 info] constants folded: 3
[mlx4 info] constant branches simplified: 6
[mlx4 info] comparison branches fused: 30
[mlx4 info] local loads forwarded: 104
[mlx4 info] copy operands propagated: 105
[mlx4 info] local stack slots promoted: 150
[mlx4 info] local stores eliminated: 86
[mlx4 info] unreachable functions eliminated: 253
[mlx4 info] unreachable function instructions eliminated: 12203
[mlx4 info] post-terminator instructions eliminated: 228
[mlx4 info] dead instructions eliminated: 114
[mlx4 6/9] done in 13.238 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 24813
[mlx4 info] backend symbols: 513
[mlx4 info] backend fixups: 361
[mlx4 info] backend stack loads forwarded: 81
[mlx4 info] backend cached rax loads: 311
[mlx4 info] backend memory operands: 54
[mlx4 info] backend deferred spills: 639
[mlx4 info] backend retained rax uses: 639
[mlx4 info] backend fallthrough branches removed: 286
[mlx4 info] backend direct calls: 122
[mlx4 info] aggregate allocation sites: 17
[mlx4 info] aggregate allocation bytes (static): 248
[mlx4 7/9] done in 5.463 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 438
[mlx4 info] maximum lookup probes: 7
[mlx4 8/9] done in 0.255 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5782392
[mlx4 9/9] done in 1.837 ms
[mlx4] total: 92.682 ms
building mlx-wc
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 32
[mlx4 info] imports discovered: 59
[mlx4 info] source bytes: 120626
[mlx4 info] tokens scanned: 29045
[mlx4 info] AST nodes parsed: 14988
[mlx4 info] declarations discovered: 1003
[mlx4 info] functions discovered: 373
[mlx4 info] source I/O: 1.813 ms
[mlx4 info] lexing: 10.506 ms
[mlx4 info] parsing: 7.061 ms
[mlx4 1/9] done in 21.181 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.483 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 379
[mlx4 info] symbols registered: 691
[mlx4 3/9] done in 5.572 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2426
[mlx4 info] type intern probes: 2084
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 5019
[mlx4 info] aggregate method lookups: 677
[mlx4 info] aggregate method probes: 81
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 24.068 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 26
[mlx4 info] calls automatically inlined: 18
[mlx4 info] inline requests kept as calls: 26
[mlx4 info] expression type queries: 6371
[mlx4 info] expression type cache hits: 2668
[mlx4 info] resolved type queries: 649
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 18767
[mlx4 info] LIR extra words: 3642
[mlx4 info] LIR symbols: 328
[mlx4 info] string literals: 48
[mlx4 info] type intern calls: 2822
[mlx4 info] type intern probes: 2484
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 5300
[mlx4 info] aggregate method lookups: 1588
[mlx4 info] aggregate method probes: 137
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 37.174 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 18767
[mlx4 info] LIR instructions after optimization: 6319
[mlx4 info] constants folded: 11
[mlx4 info] constant branches simplified: 12
[mlx4 info] comparison branches fused: 67
[mlx4 info] local loads forwarded: 192
[mlx4 info] copy operands propagated: 195
[mlx4 info] local stack slots promoted: 217
[mlx4 info] local stores eliminated: 148
[mlx4 info] unreachable functions eliminated: 244
[mlx4 info] unreachable function instructions eliminated: 11535
[mlx4 info] post-terminator instructions eliminated: 264
[mlx4 info] dead instructions eliminated: 217
[mlx4 6/9] done in 16.322 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 52725
[mlx4 info] backend symbols: 925
[mlx4 info] backend fixups: 701
[mlx4 info] backend stack loads forwarded: 174
[mlx4 info] backend cached rax loads: 603
[mlx4 info] backend memory operands: 137
[mlx4 info] backend deferred spills: 1541
[mlx4 info] backend retained rax uses: 1541
[mlx4 info] backend fallthrough branches removed: 518
[mlx4 info] backend direct calls: 208
[mlx4 info] aggregate allocation sites: 16
[mlx4 info] aggregate allocation bytes (static): 504
[mlx4 7/9] done in 7.797 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 1010
[mlx4 info] maximum lookup probes: 11
[mlx4 8/9] done in 0.456 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 6403664
[mlx4 9/9] done in 2.215 ms
[mlx4] total: 116.279 ms
building mlx-pwd
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 32
[mlx4 info] imports discovered: 59
[mlx4 info] source bytes: 104595
[mlx4 info] tokens scanned: 25077
[mlx4 info] AST nodes parsed: 12653
[mlx4 info] declarations discovered: 900
[mlx4 info] functions discovered: 346
[mlx4 info] source I/O: 1.702 ms
[mlx4 info] lexing: 9.203 ms
[mlx4 info] parsing: 5.846 ms
[mlx4 1/9] done in 18.464 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.532 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 343
[mlx4 info] symbols registered: 658
[mlx4 3/9] done in 5.293 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2233
[mlx4 info] type intern probes: 2048
[mlx4 info] maximum type intern probes: 4
[mlx4 info] primitive type cache hits: 4143
[mlx4 info] aggregate method lookups: 513
[mlx4 info] aggregate method probes: 73
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 16.518 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 16
[mlx4 info] calls automatically inlined: 16
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 4855
[mlx4 info] expression type cache hits: 2036
[mlx4 info] resolved type queries: 555
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 14915
[mlx4 info] LIR extra words: 2943
[mlx4 info] LIR symbols: 301
[mlx4 info] string literals: 27
[mlx4 info] type intern calls: 2597
[mlx4 info] type intern probes: 2419
[mlx4 info] maximum type intern probes: 4
[mlx4 info] primitive type cache hits: 4369
[mlx4 info] aggregate method lookups: 1217
[mlx4 info] aggregate method probes: 121
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 28.649 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 14915
[mlx4 info] LIR instructions after optimization: 1945
[mlx4 info] constants folded: 1
[mlx4 info] constant branches simplified: 0
[mlx4 info] comparison branches fused: 28
[mlx4 info] local loads forwarded: 78
[mlx4 info] copy operands propagated: 78
[mlx4 info] local stack slots promoted: 87
[mlx4 info] local stores eliminated: 57
[mlx4 info] unreachable functions eliminated: 263
[mlx4 info] unreachable function instructions eliminated: 12597
[mlx4 info] post-terminator instructions eliminated: 122
[mlx4 info] dead instructions eliminated: 79
[mlx4 6/9] done in 12.330 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 16525
[mlx4 info] backend symbols: 318
[mlx4 info] backend fixups: 215
[mlx4 info] backend stack loads forwarded: 35
[mlx4 info] backend cached rax loads: 174
[mlx4 info] backend memory operands: 47
[mlx4 info] backend deferred spills: 402
[mlx4 info] backend retained rax uses: 402
[mlx4 info] backend fallthrough branches removed: 172
[mlx4 info] backend direct calls: 63
[mlx4 info] aggregate allocation sites: 8
[mlx4 info] aggregate allocation bytes (static): 376
[mlx4 7/9] done in 5.036 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 234
[mlx4 info] maximum lookup probes: 3
[mlx4 8/9] done in 0.248 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5512624
[mlx4 9/9] done in 1.472 ms
[mlx4] total: 89.565 ms
building mlx-mkdir
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 34
[mlx4 info] imports discovered: 63
[mlx4 info] source bytes: 112412
[mlx4 info] tokens scanned: 27098
[mlx4 info] AST nodes parsed: 13878
[mlx4 info] declarations discovered: 951
[mlx4 info] functions discovered: 367
[mlx4 info] source I/O: 1.899 ms
[mlx4 info] lexing: 9.936 ms
[mlx4 info] parsing: 6.552 ms
[mlx4 1/9] done in 20.100 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.565 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 363
[mlx4 info] symbols registered: 685
[mlx4 3/9] done in 5.720 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2328
[mlx4 info] type intern probes: 2036
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4630
[mlx4 info] aggregate method lookups: 550
[mlx4 info] aggregate method probes: 76
[mlx4 info] maximum aggregate method probes: 2
[mlx4 4/9] done in 17.816 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 15
[mlx4 info] calls automatically inlined: 15
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 5506
[mlx4 info] expression type cache hits: 2256
[mlx4 info] resolved type queries: 589
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 16712
[mlx4 info] LIR extra words: 3244
[mlx4 info] LIR symbols: 322
[mlx4 info] string literals: 31
[mlx4 info] type intern calls: 2699
[mlx4 info] type intern probes: 2420
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4886
[mlx4 info] aggregate method lookups: 1295
[mlx4 info] aggregate method probes: 127
[mlx4 info] maximum aggregate method probes: 2
[mlx4 5/9] done in 30.697 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 16712
[mlx4 info] LIR instructions after optimization: 3615
[mlx4 info] constants folded: 5
[mlx4 info] constant branches simplified: 4
[mlx4 info] comparison branches fused: 38
[mlx4 info] local loads forwarded: 102
[mlx4 info] copy operands propagated: 102
[mlx4 info] local stack slots promoted: 153
[mlx4 info] local stores eliminated: 93
[mlx4 info] unreachable functions eliminated: 257
[mlx4 info] unreachable function instructions eliminated: 12471
[mlx4 info] post-terminator instructions eliminated: 228
[mlx4 info] dead instructions eliminated: 114
[mlx4 6/9] done in 14.488 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 29444
[mlx4 info] backend symbols: 578
[mlx4 info] backend fixups: 416
[mlx4 info] backend stack loads forwarded: 63
[mlx4 info] backend cached rax loads: 355
[mlx4 info] backend memory operands: 85
[mlx4 info] backend deferred spills: 792
[mlx4 info] backend retained rax uses: 792
[mlx4 info] backend fallthrough branches removed: 318
[mlx4 info] backend direct calls: 133
[mlx4 info] aggregate allocation sites: 15
[mlx4 info] aggregate allocation bytes (static): 520
[mlx4 7/9] done in 6.711 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 464
[mlx4 info] maximum lookup probes: 4
[mlx4 8/9] done in 0.334 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 6013448
[mlx4 9/9] done in 1.860 ms
[mlx4] total: 99.300 ms
building mlx-rmdir
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 32
[mlx4 info] imports discovered: 59
[mlx4 info] source bytes: 103809
[mlx4 info] tokens scanned: 24925
[mlx4 info] AST nodes parsed: 12553
[mlx4 info] declarations discovered: 896
[mlx4 info] functions discovered: 346
[mlx4 info] source I/O: 1.552 ms
[mlx4 info] lexing: 8.589 ms
[mlx4 info] parsing: 5.482 ms
[mlx4 1/9] done in 17.027 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.510 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 344
[mlx4 info] symbols registered: 658
[mlx4 3/9] done in 5.603 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2216
[mlx4 info] type intern probes: 2024
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4112
[mlx4 info] aggregate method lookups: 507
[mlx4 info] aggregate method probes: 73
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 15.454 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 14
[mlx4 info] calls automatically inlined: 14
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 4793
[mlx4 info] expression type cache hits: 2007
[mlx4 info] resolved type queries: 544
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 14698
[mlx4 info] LIR extra words: 2918
[mlx4 info] LIR symbols: 301
[mlx4 info] string literals: 24
[mlx4 info] type intern calls: 2573
[mlx4 info] type intern probes: 2389
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4338
[mlx4 info] aggregate method lookups: 1199
[mlx4 info] aggregate method probes: 121
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 26.385 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 14698
[mlx4 info] LIR instructions after optimization: 1689
[mlx4 info] constants folded: 1
[mlx4 info] constant branches simplified: 0
[mlx4 info] comparison branches fused: 22
[mlx4 info] local loads forwarded: 60
[mlx4 info] copy operands propagated: 60
[mlx4 info] local stack slots promoted: 74
[mlx4 info] local stores eliminated: 54
[mlx4 info] unreachable functions eliminated: 264
[mlx4 info] unreachable function instructions eliminated: 12702
[mlx4 info] post-terminator instructions eliminated: 96
[mlx4 info] dead instructions eliminated: 61
[mlx4 6/9] done in 11.948 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 14824
[mlx4 info] backend symbols: 248
[mlx4 info] backend fixups: 193
[mlx4 info] backend stack loads forwarded: 27
[mlx4 info] backend cached rax loads: 144
[mlx4 info] backend memory operands: 41
[mlx4 info] backend deferred spills: 360
[mlx4 info] backend retained rax uses: 360
[mlx4 info] backend fallthrough branches removed: 128
[mlx4 info] backend direct calls: 72
[mlx4 info] aggregate allocation sites: 7
[mlx4 info] aggregate allocation bytes (static): 120
[mlx4 7/9] done in 4.450 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 266
[mlx4 info] maximum lookup probes: 7
[mlx4 8/9] done in 0.222 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5446672
[mlx4 9/9] done in 1.398 ms
[mlx4] total: 84.009 ms
building mlx-basename
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 32
[mlx4 info] imports discovered: 59
[mlx4 info] source bytes: 105259
[mlx4 info] tokens scanned: 25331
[mlx4 info] AST nodes parsed: 12798
[mlx4 info] declarations discovered: 897
[mlx4 info] functions discovered: 348
[mlx4 info] source I/O: 1.699 ms
[mlx4 info] lexing: 8.627 ms
[mlx4 info] parsing: 5.616 ms
[mlx4 1/9] done in 17.502 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.274 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 348
[mlx4 info] symbols registered: 660
[mlx4 3/9] done in 5.420 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2239
[mlx4 info] type intern probes: 1918
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4190
[mlx4 info] aggregate method lookups: 515
[mlx4 info] aggregate method probes: 73
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 16.167 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 16
[mlx4 info] calls automatically inlined: 16
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 4912
[mlx4 info] expression type cache hits: 2051
[mlx4 info] resolved type queries: 547
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 15034
[mlx4 info] LIR extra words: 2993
[mlx4 info] LIR symbols: 303
[mlx4 info] string literals: 23
[mlx4 info] type intern calls: 2605
[mlx4 info] type intern probes: 2284
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4417
[mlx4 info] aggregate method lookups: 1220
[mlx4 info] aggregate method probes: 121
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 26.082 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 15034
[mlx4 info] LIR instructions after optimization: 1725
[mlx4 info] constants folded: 3
[mlx4 info] constant branches simplified: 2
[mlx4 info] comparison branches fused: 22
[mlx4 info] local loads forwarded: 58
[mlx4 info] copy operands propagated: 58
[mlx4 info] local stack slots promoted: 71
[mlx4 info] local stores eliminated: 40
[mlx4 info] unreachable functions eliminated: 270
[mlx4 info] unreachable function instructions eliminated: 12990
[mlx4 info] post-terminator instructions eliminated: 124
[mlx4 info] dead instructions eliminated: 62
[mlx4 6/9] done in 13.213 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 14830
[mlx4 info] backend symbols: 267
[mlx4 info] backend fixups: 184
[mlx4 info] backend stack loads forwarded: 36
[mlx4 info] backend cached rax loads: 159
[mlx4 info] backend memory operands: 43
[mlx4 info] backend deferred spills: 384
[mlx4 info] backend retained rax uses: 384
[mlx4 info] backend fallthrough branches removed: 140
[mlx4 info] backend direct calls: 65
[mlx4 info] aggregate allocation sites: 8
[mlx4 info] aggregate allocation bytes (static): 128
[mlx4 7/9] done in 4.407 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 202
[mlx4 info] maximum lookup probes: 2
[mlx4 8/9] done in 0.173 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5538992
[mlx4 9/9] done in 1.801 ms
[mlx4] total: 86.048 ms
building mlx-dirname
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 32
[mlx4 info] imports discovered: 59
[mlx4 info] source bytes: 102946
[mlx4 info] tokens scanned: 24791
[mlx4 info] AST nodes parsed: 12479
[mlx4 info] declarations discovered: 886
[mlx4 info] functions discovered: 345
[mlx4 info] source I/O: 1.607 ms
[mlx4 info] lexing: 8.623 ms
[mlx4 info] parsing: 5.529 ms
[mlx4 1/9] done in 17.430 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.478 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 344
[mlx4 info] symbols registered: 657
[mlx4 3/9] done in 4.977 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2218
[mlx4 info] type intern probes: 1948
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4073
[mlx4 info] aggregate method lookups: 498
[mlx4 info] aggregate method probes: 73
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 15.452 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 16
[mlx4 info] calls automatically inlined: 16
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 4738
[mlx4 info] expression type cache hits: 1976
[mlx4 info] resolved type queries: 538
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 14557
[mlx4 info] LIR extra words: 2887
[mlx4 info] LIR symbols: 300
[mlx4 info] string literals: 22
[mlx4 info] type intern calls: 2583
[mlx4 info] type intern probes: 2323
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4298
[mlx4 info] aggregate method lookups: 1181
[mlx4 info] aggregate method probes: 121
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 25.204 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 14557
[mlx4 info] LIR instructions after optimization: 1150
[mlx4 info] constants folded: 3
[mlx4 info] constant branches simplified: 2
[mlx4 info] comparison branches fused: 16
[mlx4 info] local loads forwarded: 47
[mlx4 info] copy operands propagated: 47
[mlx4 info] local stack slots promoted: 55
[mlx4 info] local stores eliminated: 36
[mlx4 info] unreachable functions eliminated: 272
[mlx4 info] unreachable function instructions eliminated: 13163
[mlx4 info] post-terminator instructions eliminated: 86
[mlx4 info] dead instructions eliminated: 51
[mlx4 6/9] done in 12.090 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 9898
[mlx4 info] backend symbols: 194
[mlx4 info] backend fixups: 137
[mlx4 info] backend stack loads forwarded: 17
[mlx4 info] backend cached rax loads: 109
[mlx4 info] backend memory operands: 32
[mlx4 info] backend deferred spills: 226
[mlx4 info] backend retained rax uses: 226
[mlx4 info] backend fallthrough branches removed: 96
[mlx4 info] backend direct calls: 50
[mlx4 info] aggregate allocation sites: 8
[mlx4 info] aggregate allocation bytes (static): 112
[mlx4 7/9] done in 4.094 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 156
[mlx4 info] maximum lookup probes: 4
[mlx4 8/9] done in 0.200 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5410456
[mlx4 9/9] done in 1.489 ms
[mlx4] total: 82.424 ms
building mlx-head
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 33
[mlx4 info] imports discovered: 61
[mlx4 info] source bytes: 112541
[mlx4 info] tokens scanned: 27204
[mlx4 info] AST nodes parsed: 13884
[mlx4 info] declarations discovered: 945
[mlx4 info] functions discovered: 357
[mlx4 info] source I/O: 1.800 ms
[mlx4 info] lexing: 10.121 ms
[mlx4 info] parsing: 6.863 ms
[mlx4 1/9] done in 20.699 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.548 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 360
[mlx4 info] symbols registered: 675
[mlx4 3/9] done in 5.649 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2309
[mlx4 info] type intern probes: 1983
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4653
[mlx4 info] aggregate method lookups: 561
[mlx4 info] aggregate method probes: 74
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 18.999 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 19
[mlx4 info] calls automatically inlined: 19
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 5515
[mlx4 info] expression type cache hits: 2303
[mlx4 info] resolved type queries: 595
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 16769
[mlx4 info] LIR extra words: 3239
[mlx4 info] LIR symbols: 312
[mlx4 info] string literals: 31
[mlx4 info] type intern calls: 2686
[mlx4 info] type intern probes: 2368
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4900
[mlx4 info] aggregate method lookups: 1338
[mlx4 info] aggregate method probes: 122
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 29.991 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 16769
[mlx4 info] LIR instructions after optimization: 3706
[mlx4 info] constants folded: 1
[mlx4 info] constant branches simplified: 1
[mlx4 info] comparison branches fused: 50
[mlx4 info] local loads forwarded: 119
[mlx4 info] copy operands propagated: 120
[mlx4 info] local stack slots promoted: 154
[mlx4 info] local stores eliminated: 82
[mlx4 info] unreachable functions eliminated: 252
[mlx4 info] unreachable function instructions eliminated: 12405
[mlx4 info] post-terminator instructions eliminated: 250
[mlx4 info] dead instructions eliminated: 122
[mlx4 6/9] done in 15.562 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 30385
[mlx4 info] backend symbols: 579
[mlx4 info] backend fixups: 395
[mlx4 info] backend stack loads forwarded: 80
[mlx4 info] backend cached rax loads: 358
[mlx4 info] backend memory operands: 87
[mlx4 info] backend deferred spills: 844
[mlx4 info] backend retained rax uses: 844
[mlx4 info] backend fallthrough branches removed: 327
[mlx4 info] backend direct calls: 123
[mlx4 info] aggregate allocation sites: 22
[mlx4 info] aggregate allocation bytes (static): 400
[mlx4 7/9] done in 6.505 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 462
[mlx4 info] maximum lookup probes: 4
[mlx4 8/9] done in 0.275 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5971424
[mlx4 9/9] done in 1.982 ms
[mlx4] total: 101.221 ms
building mlx-tail
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 34
[mlx4 info] imports discovered: 63
[mlx4 info] source bytes: 123451
[mlx4 info] tokens scanned: 29910
[mlx4 info] AST nodes parsed: 15460
[mlx4 info] declarations discovered: 1002
[mlx4 info] functions discovered: 373
[mlx4 info] source I/O: 1.617 ms
[mlx4 info] lexing: 9.578 ms
[mlx4 info] parsing: 6.294 ms
[mlx4 1/9] done in 19.163 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.389 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 374
[mlx4 info] symbols registered: 697
[mlx4 3/9] done in 5.672 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2415
[mlx4 info] type intern probes: 2358
[mlx4 info] maximum type intern probes: 5
[mlx4 info] primitive type cache hits: 5240
[mlx4 info] aggregate method lookups: 632
[mlx4 info] aggregate method probes: 79
[mlx4 info] maximum aggregate method probes: 2
[mlx4 4/9] done in 20.772 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 20
[mlx4 info] calls automatically inlined: 20
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 6324
[mlx4 info] expression type cache hits: 2610
[mlx4 info] resolved type queries: 649
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 19113
[mlx4 info] LIR extra words: 3671
[mlx4 info] LIR symbols: 328
[mlx4 info] string literals: 49
[mlx4 info] type intern calls: 2807
[mlx4 info] type intern probes: 2777
[mlx4 info] maximum type intern probes: 5
[mlx4 info] primitive type cache hits: 5514
[mlx4 info] aggregate method lookups: 1500
[mlx4 info] aggregate method probes: 133
[mlx4 info] maximum aggregate method probes: 2
[mlx4 5/9] done in 35.331 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 19113
[mlx4 info] LIR instructions after optimization: 6521
[mlx4 info] constants folded: 3
[mlx4 info] constant branches simplified: 7
[mlx4 info] comparison branches fused: 73
[mlx4 info] local loads forwarded: 198
[mlx4 info] copy operands propagated: 199
[mlx4 info] local stack slots promoted: 258
[mlx4 info] local stores eliminated: 116
[mlx4 info] unreachable functions eliminated: 237
[mlx4 info] unreachable function instructions eliminated: 11488
[mlx4 info] post-terminator instructions eliminated: 446
[mlx4 info] dead instructions eliminated: 211
[mlx4 6/9] done in 16.746 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 53806
[mlx4 info] backend symbols: 1029
[mlx4 info] backend fixups: 701
[mlx4 info] backend stack loads forwarded: 192
[mlx4 info] backend cached rax loads: 650
[mlx4 info] backend memory operands: 137
[mlx4 info] backend deferred spills: 1463
[mlx4 info] backend retained rax uses: 1463
[mlx4 info] backend fallthrough branches removed: 592
[mlx4 info] backend direct calls: 220
[mlx4 info] aggregate allocation sites: 55
[mlx4 info] aggregate allocation bytes (static): 1304
[mlx4 7/9] done in 8.889 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 798
[mlx4 info] maximum lookup probes: 7
[mlx4 8/9] done in 0.450 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 6659112
[mlx4 9/9] done in 2.815 ms
[mlx4] total: 111.236 ms
building mlx-tee
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 32
[mlx4 info] imports discovered: 59
[mlx4 info] source bytes: 108436
[mlx4 info] tokens scanned: 25978
[mlx4 info] AST nodes parsed: 13104
[mlx4 info] declarations discovered: 926
[mlx4 info] functions discovered: 351
[mlx4 info] source I/O: 2.095 ms
[mlx4 info] lexing: 10.760 ms
[mlx4 info] parsing: 7.282 ms
[mlx4 1/9] done in 22.267 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.590 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 356
[mlx4 info] symbols registered: 669
[mlx4 3/9] done in 6.480 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2296
[mlx4 info] type intern probes: 1982
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4289
[mlx4 info] aggregate method lookups: 538
[mlx4 info] aggregate method probes: 73
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 22.476 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 16
[mlx4 info] calls automatically inlined: 16
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 5053
[mlx4 info] expression type cache hits: 2104
[mlx4 info] resolved type queries: 582
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 15477
[mlx4 info] LIR extra words: 3053
[mlx4 info] LIR symbols: 306
[mlx4 info] string literals: 38
[mlx4 info] type intern calls: 2672
[mlx4 info] type intern probes: 2361
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4525
[mlx4 info] aggregate method lookups: 1267
[mlx4 info] aggregate method probes: 121
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 28.600 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 15477
[mlx4 info] LIR instructions after optimization: 2470
[mlx4 info] constants folded: 2
[mlx4 info] constant branches simplified: 1
[mlx4 info] comparison branches fused: 27
[mlx4 info] local loads forwarded: 86
[mlx4 info] copy operands propagated: 86
[mlx4 info] local stack slots promoted: 93
[mlx4 info] local stores eliminated: 62
[mlx4 info] unreachable functions eliminated: 261
[mlx4 info] unreachable function instructions eliminated: 12599
[mlx4 info] post-terminator instructions eliminated: 136
[mlx4 info] dead instructions eliminated: 90
[mlx4 6/9] done in 12.504 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 22274
[mlx4 info] backend symbols: 360
[mlx4 info] backend fixups: 276
[mlx4 info] backend stack loads forwarded: 54
[mlx4 info] backend cached rax loads: 204
[mlx4 info] backend memory operands: 47
[mlx4 info] backend deferred spills: 552
[mlx4 info] backend retained rax uses: 552
[mlx4 info] backend fallthrough branches removed: 188
[mlx4 info] backend direct calls: 91
[mlx4 info] aggregate allocation sites: 16
[mlx4 info] aggregate allocation bytes (static): 296
[mlx4 7/9] done in 5.304 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 338
[mlx4 info] maximum lookup probes: 5
[mlx4 8/9] done in 0.192 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5667496
[mlx4 9/9] done in 1.621 ms
[mlx4] total: 101.045 ms
building mlx-yes
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 32
[mlx4 info] imports discovered: 59
[mlx4 info] source bytes: 103269
[mlx4 info] tokens scanned: 24838
[mlx4 info] AST nodes parsed: 12477
[mlx4 info] declarations discovered: 895
[mlx4 info] functions discovered: 346
[mlx4 info] source I/O: 1.779 ms
[mlx4 info] lexing: 9.256 ms
[mlx4 info] parsing: 5.980 ms
[mlx4 1/9] done in 18.607 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.491 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 343
[mlx4 info] symbols registered: 656
[mlx4 3/9] done in 4.890 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2228
[mlx4 info] type intern probes: 1932
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4024
[mlx4 info] aggregate method lookups: 484
[mlx4 info] aggregate method probes: 73
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 15.423 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 15
[mlx4 info] calls automatically inlined: 15
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 4669
[mlx4 info] expression type cache hits: 1929
[mlx4 info] resolved type queries: 548
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 14486
[mlx4 info] LIR extra words: 2936
[mlx4 info] LIR symbols: 301
[mlx4 info] string literals: 18
[mlx4 info] type intern calls: 2595
[mlx4 info] type intern probes: 2301
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4245
[mlx4 info] aggregate method lookups: 1145
[mlx4 info] aggregate method probes: 121
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 27.248 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 14486
[mlx4 info] LIR instructions after optimization: 1285
[mlx4 info] constants folded: 1
[mlx4 info] constant branches simplified: 0
[mlx4 info] comparison branches fused: 18
[mlx4 info] local loads forwarded: 64
[mlx4 info] copy operands propagated: 64
[mlx4 info] local stack slots promoted: 60
[mlx4 info] local stores eliminated: 41
[mlx4 info] unreachable functions eliminated: 271
[mlx4 info] unreachable function instructions eliminated: 12935
[mlx4 info] post-terminator instructions eliminated: 82
[mlx4 info] dead instructions eliminated: 65
[mlx4 6/9] done in 12.039 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 11699
[mlx4 info] backend symbols: 190
[mlx4 info] backend fixups: 138
[mlx4 info] backend stack loads forwarded: 28
[mlx4 info] backend cached rax loads: 88
[mlx4 info] backend memory operands: 37
[mlx4 info] backend deferred spills: 240
[mlx4 info] backend retained rax uses: 240
[mlx4 info] backend fallthrough branches removed: 97
[mlx4 info] backend direct calls: 54
[mlx4 info] aggregate allocation sites: 5
[mlx4 info] aggregate allocation bytes (static): 80
[mlx4 7/9] done in 3.976 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 163
[mlx4 info] maximum lookup probes: 3
[mlx4 8/9] done in 0.114 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5408296
[mlx4 9/9] done in 1.396 ms
[mlx4] total: 85.195 ms
building mlx-sleep
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 33
[mlx4 info] imports discovered: 61
[mlx4 info] source bytes: 103410
[mlx4 info] tokens scanned: 24844
[mlx4 info] AST nodes parsed: 12508
[mlx4 info] declarations discovered: 896
[mlx4 info] functions discovered: 345
[mlx4 info] source I/O: 1.697 ms
[mlx4 info] lexing: 8.810 ms
[mlx4 info] parsing: 5.880 ms
[mlx4 1/9] done in 18.085 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.537 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 344
[mlx4 info] symbols registered: 659
[mlx4 3/9] done in 6.614 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2223
[mlx4 info] type intern probes: 1995
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4078
[mlx4 info] aggregate method lookups: 479
[mlx4 info] aggregate method probes: 72
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 17.087 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 14
[mlx4 info] calls automatically inlined: 14
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 4690
[mlx4 info] expression type cache hits: 1922
[mlx4 info] resolved type queries: 549
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 14598
[mlx4 info] LIR extra words: 2886
[mlx4 info] LIR symbols: 300
[mlx4 info] string literals: 21
[mlx4 info] type intern calls: 2586
[mlx4 info] type intern probes: 2383
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4309
[mlx4 info] aggregate method lookups: 1134
[mlx4 info] aggregate method probes: 118
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 25.164 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 14598
[mlx4 info] LIR instructions after optimization: 1159
[mlx4 info] constants folded: 1
[mlx4 info] constant branches simplified: 1
[mlx4 info] comparison branches fused: 18
[mlx4 info] local loads forwarded: 45
[mlx4 info] copy operands propagated: 45
[mlx4 info] local stack slots promoted: 54
[mlx4 info] local stores eliminated: 29
[mlx4 info] unreachable functions eliminated: 277
[mlx4 info] unreachable function instructions eliminated: 13202
[mlx4 info] post-terminator instructions eliminated: 88
[mlx4 info] dead instructions eliminated: 48
[mlx4 6/9] done in 11.994 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 10232
[mlx4 info] backend symbols: 199
[mlx4 info] backend fixups: 128
[mlx4 info] backend stack loads forwarded: 40
[mlx4 info] backend cached rax loads: 93
[mlx4 info] backend memory operands: 36
[mlx4 info] backend deferred spills: 224
[mlx4 info] backend retained rax uses: 224
[mlx4 info] backend fallthrough branches removed: 104
[mlx4 info] backend direct calls: 40
[mlx4 info] aggregate allocation sites: 13
[mlx4 info] aggregate allocation bytes (static): 192
[mlx4 7/9] done in 3.821 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 168
[mlx4 info] maximum lookup probes: 5
[mlx4 8/9] done in 0.186 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5445376
[mlx4 9/9] done in 1.330 ms
[mlx4] total: 85.826 ms
building mlx-uname
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 32
[mlx4 info] imports discovered: 59
[mlx4 info] source bytes: 105830
[mlx4 info] tokens scanned: 25422
[mlx4 info] AST nodes parsed: 12872
[mlx4 info] declarations discovered: 892
[mlx4 info] functions discovered: 350
[mlx4 info] source I/O: 1.746 ms
[mlx4 info] lexing: 9.574 ms
[mlx4 info] parsing: 6.136 ms
[mlx4 1/9] done in 19.199 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.503 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 350
[mlx4 info] symbols registered: 662
[mlx4 3/9] done in 5.106 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2261
[mlx4 info] type intern probes: 1945
[mlx4 info] maximum type intern probes: 4
[mlx4 info] primitive type cache hits: 4247
[mlx4 info] aggregate method lookups: 524
[mlx4 info] aggregate method probes: 73
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 16.372 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 14
[mlx4 info] calls automatically inlined: 14
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 4916
[mlx4 info] expression type cache hits: 2030
[mlx4 info] resolved type queries: 538
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 15127
[mlx4 info] LIR extra words: 2973
[mlx4 info] LIR symbols: 305
[mlx4 info] string literals: 33
[mlx4 info] type intern calls: 2621
[mlx4 info] type intern probes: 2307
[mlx4 info] maximum type intern probes: 4
[mlx4 info] primitive type cache hits: 4471
[mlx4 info] aggregate method lookups: 1225
[mlx4 info] aggregate method probes: 121
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 28.050 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 15127
[mlx4 info] LIR instructions after optimization: 1582
[mlx4 info] constants folded: 1
[mlx4 info] constant branches simplified: 0
[mlx4 info] comparison branches fused: 13
[mlx4 info] local loads forwarded: 48
[mlx4 info] copy operands propagated: 48
[mlx4 info] local stack slots promoted: 72
[mlx4 info] local stores eliminated: 39
[mlx4 info] unreachable functions eliminated: 273
[mlx4 info] unreachable function instructions eliminated: 13250
[mlx4 info] post-terminator instructions eliminated: 122
[mlx4 info] dead instructions eliminated: 49
[mlx4 6/9] done in 12.089 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 12940
[mlx4 info] backend symbols: 291
[mlx4 info] backend fixups: 215
[mlx4 info] backend stack loads forwarded: 30
[mlx4 info] backend cached rax loads: 161
[mlx4 info] backend memory operands: 24
[mlx4 info] backend deferred spills: 333
[mlx4 info] backend retained rax uses: 333
[mlx4 info] backend fallthrough branches removed: 151
[mlx4 info] backend direct calls: 73
[mlx4 info] aggregate allocation sites: 6
[mlx4 info] aggregate allocation bytes (static): 456
[mlx4 7/9] done in 4.481 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 271
[mlx4 info] maximum lookup probes: 4
[mlx4 8/9] done in 0.120 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5564680
[mlx4 9/9] done in 1.613 ms
[mlx4] total: 88.542 ms
building mlx-printenv
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 32
[mlx4 info] imports discovered: 59
[mlx4 info] source bytes: 102997
[mlx4 info] tokens scanned: 24775
[mlx4 info] AST nodes parsed: 12441
[mlx4 info] declarations discovered: 890
[mlx4 info] functions discovered: 346
[mlx4 info] source I/O: 1.929 ms
[mlx4 info] lexing: 10.137 ms
[mlx4 info] parsing: 6.767 ms
[mlx4 1/9] done in 20.775 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.448 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 345
[mlx4 info] symbols registered: 658
[mlx4 3/9] done in 5.753 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2223
[mlx4 info] type intern probes: 2027
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4033
[mlx4 info] aggregate method lookups: 489
[mlx4 info] aggregate method probes: 73
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 17.173 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 16
[mlx4 info] calls automatically inlined: 16
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 4655
[mlx4 info] expression type cache hits: 1929
[mlx4 info] resolved type queries: 542
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 14442
[mlx4 info] LIR extra words: 2895
[mlx4 info] LIR symbols: 301
[mlx4 info] string literals: 20
[mlx4 info] type intern calls: 2589
[mlx4 info] type intern probes: 2403
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4255
[mlx4 info] aggregate method lookups: 1156
[mlx4 info] aggregate method probes: 121
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 27.655 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 14442
[mlx4 info] LIR instructions after optimization: 1214
[mlx4 info] constants folded: 3
[mlx4 info] constant branches simplified: 2
[mlx4 info] comparison branches fused: 18
[mlx4 info] local loads forwarded: 64
[mlx4 info] copy operands propagated: 64
[mlx4 info] local stack slots promoted: 67
[mlx4 info] local stores eliminated: 46
[mlx4 info] unreachable functions eliminated: 269
[mlx4 info] unreachable function instructions eliminated: 12933
[mlx4 info] post-terminator instructions eliminated: 96
[mlx4 info] dead instructions eliminated: 68
[mlx4 6/9] done in 12.329 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 10908
[mlx4 info] backend symbols: 196
[mlx4 info] backend fixups: 144
[mlx4 info] backend stack loads forwarded: 24
[mlx4 info] backend cached rax loads: 99
[mlx4 info] backend memory operands: 36
[mlx4 info] backend deferred spills: 209
[mlx4 info] backend retained rax uses: 209
[mlx4 info] backend fallthrough branches removed: 96
[mlx4 info] backend direct calls: 61
[mlx4 info] aggregate allocation sites: 6
[mlx4 info] aggregate allocation bytes (static): 80
[mlx4 7/9] done in 3.746 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 161
[mlx4 info] maximum lookup probes: 3
[mlx4 8/9] done in 0.131 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5401688
[mlx4 9/9] done in 1.320 ms
[mlx4] total: 90.345 ms
building mlx-env
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 34
[mlx4 info] imports discovered: 63
[mlx4 info] source bytes: 131943
[mlx4 info] tokens scanned: 31909
[mlx4 info] AST nodes parsed: 16593
[mlx4 info] declarations discovered: 1045
[mlx4 info] functions discovered: 386
[mlx4 info] source I/O: 1.674 ms
[mlx4 info] lexing: 10.439 ms
[mlx4 info] parsing: 6.811 ms
[mlx4 1/9] done in 20.675 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.541 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 388
[mlx4 info] symbols registered: 712
[mlx4 3/9] done in 5.552 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2673
[mlx4 info] type intern probes: 2327
[mlx4 info] maximum type intern probes: 2
[mlx4 info] primitive type cache hits: 5586
[mlx4 info] aggregate method lookups: 736
[mlx4 info] aggregate method probes: 76
[mlx4 info] maximum aggregate method probes: 2
[mlx4 4/9] done in 24.424 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 16
[mlx4 info] calls automatically inlined: 16
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 7071
[mlx4 info] expression type cache hits: 2996
[mlx4 info] resolved type queries: 705
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 20997
[mlx4 info] LIR extra words: 4078
[mlx4 info] LIR symbols: 341
[mlx4 info] string literals: 116
[mlx4 info] type intern calls: 3128
[mlx4 info] type intern probes: 2791
[mlx4 info] maximum type intern probes: 2
[mlx4 info] primitive type cache hits: 5849
[mlx4 info] aggregate method lookups: 1757
[mlx4 info] aggregate method probes: 127
[mlx4 info] maximum aggregate method probes: 2
[mlx4 5/9] done in 39.224 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 20997
[mlx4 info] LIR instructions after optimization: 6655
[mlx4 info] constants folded: 7
[mlx4 info] constant branches simplified: 2
[mlx4 info] comparison branches fused: 73
[mlx4 info] local loads forwarded: 173
[mlx4 info] copy operands propagated: 173
[mlx4 info] local stack slots promoted: 228
[mlx4 info] local stores eliminated: 113
[mlx4 info] unreachable functions eliminated: 260
[mlx4 info] unreachable function instructions eliminated: 13384
[mlx4 info] post-terminator instructions eliminated: 362
[mlx4 info] dead instructions eliminated: 182
[mlx4 6/9] done in 16.425 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 58507
[mlx4 info] backend symbols: 1099
[mlx4 info] backend fixups: 865
[mlx4 info] backend stack loads forwarded: 167
[mlx4 info] backend cached rax loads: 636
[mlx4 info] backend memory operands: 146
[mlx4 info] backend deferred spills: 1460
[mlx4 info] backend retained rax uses: 1460
[mlx4 info] backend fallthrough branches removed: 605
[mlx4 info] backend direct calls: 280
[mlx4 info] aggregate allocation sites: 21
[mlx4 info] aggregate allocation bytes (static): 552
[mlx4 7/9] done in 8.414 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 1039
[mlx4 info] maximum lookup probes: 5
[mlx4 8/9] done in 0.431 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 7076664
[mlx4 9/9] done in 2.964 ms
[mlx4] total: 119.659 ms
building mlx-nproc
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 32
[mlx4 info] imports discovered: 59
[mlx4 info] source bytes: 104860
[mlx4 info] tokens scanned: 25254
[mlx4 info] AST nodes parsed: 12718
[mlx4 info] declarations discovered: 905
[mlx4 info] functions discovered: 347
[mlx4 info] source I/O: 1.545 ms
[mlx4 info] lexing: 8.448 ms
[mlx4 info] parsing: 5.480 ms
[mlx4 1/9] done in 16.998 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.471 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 344
[mlx4 info] symbols registered: 661
[mlx4 3/9] done in 5.220 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2232
[mlx4 info] type intern probes: 1910
[mlx4 info] maximum type intern probes: 2
[mlx4 info] primitive type cache hits: 4144
[mlx4 info] aggregate method lookups: 509
[mlx4 info] aggregate method probes: 73
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 19.686 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 15
[mlx4 info] calls automatically inlined: 15
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 4858
[mlx4 info] expression type cache hits: 2017
[mlx4 info] resolved type queries: 563
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 14857
[mlx4 info] LIR extra words: 2949
[mlx4 info] LIR symbols: 302
[mlx4 info] string literals: 21
[mlx4 info] type intern calls: 2608
[mlx4 info] type intern probes: 2287
[mlx4 info] maximum type intern probes: 2
[mlx4 info] primitive type cache hits: 4375
[mlx4 info] aggregate method lookups: 1203
[mlx4 info] aggregate method probes: 121
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 38.810 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 14857
[mlx4 info] LIR instructions after optimization: 1989
[mlx4 info] constants folded: 8
[mlx4 info] constant branches simplified: 7
[mlx4 info] comparison branches fused: 28
[mlx4 info] local loads forwarded: 71
[mlx4 info] copy operands propagated: 73
[mlx4 info] local stack slots promoted: 79
[mlx4 info] local stores eliminated: 50
[mlx4 info] unreachable functions eliminated: 265
[mlx4 info] unreachable function instructions eliminated: 12502
[mlx4 info] post-terminator instructions eliminated: 124
[mlx4 info] dead instructions eliminated: 85
[mlx4 6/9] done in 13.997 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 16675
[mlx4 info] backend symbols: 306
[mlx4 info] backend fixups: 205
[mlx4 info] backend stack loads forwarded: 64
[mlx4 info] backend cached rax loads: 160
[mlx4 info] backend memory operands: 55
[mlx4 info] backend deferred spills: 434
[mlx4 info] backend retained rax uses: 434
[mlx4 info] backend fallthrough branches removed: 158
[mlx4 info] backend direct calls: 65
[mlx4 info] aggregate allocation sites: 15
[mlx4 info] aggregate allocation bytes (static): 592
[mlx4 7/9] done in 5.031 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 244
[mlx4 info] maximum lookup probes: 3
[mlx4 8/9] done in 0.227 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5519488
[mlx4 9/9] done in 1.565 ms
[mlx4] total: 103.014 ms
building mlx-link
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 32
[mlx4 info] imports discovered: 59
[mlx4 info] source bytes: 101133
[mlx4 info] tokens scanned: 24323
[mlx4 info] AST nodes parsed: 12197
[mlx4 info] declarations discovered: 876
[mlx4 info] functions discovered: 342
[mlx4 info] source I/O: 1.552 ms
[mlx4 info] lexing: 7.772 ms
[mlx4 info] parsing: 5.380 ms
[mlx4 1/9] done in 16.201 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.490 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 340
[mlx4 info] symbols registered: 652
[mlx4 3/9] done in 5.337 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2196
[mlx4 info] type intern probes: 1899
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 3945
[mlx4 info] aggregate method lookups: 474
[mlx4 info] aggregate method probes: 73
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 14.815 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 14
[mlx4 info] calls automatically inlined: 14
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 4527
[mlx4 info] expression type cache hits: 1870
[mlx4 info] resolved type queries: 528
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 14081
[mlx4 info] LIR extra words: 2837
[mlx4 info] LIR symbols: 297
[mlx4 info] string literals: 19
[mlx4 info] type intern calls: 2552
[mlx4 info] type intern probes: 2258
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4168
[mlx4 info] aggregate method lookups: 1119
[mlx4 info] aggregate method probes: 121
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 24.290 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 14081
[mlx4 info] LIR instructions after optimization: 681
[mlx4 info] constants folded: 1
[mlx4 info] constant branches simplified: 0
[mlx4 info] comparison branches fused: 11
[mlx4 info] local loads forwarded: 41
[mlx4 info] copy operands propagated: 41
[mlx4 info] local stack slots promoted: 49
[mlx4 info] local stores eliminated: 38
[mlx4 info] unreachable functions eliminated: 273
[mlx4 info] unreachable function instructions eliminated: 13200
[mlx4 info] post-terminator instructions eliminated: 60
[mlx4 info] dead instructions eliminated: 42
[mlx4 6/9] done in 10.870 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 6674
[mlx4 info] backend symbols: 114
[mlx4 info] backend fixups: 100
[mlx4 info] backend stack loads forwarded: 6
[mlx4 info] backend cached rax loads: 45
[mlx4 info] backend memory operands: 19
[mlx4 info] backend deferred spills: 98
[mlx4 info] backend retained rax uses: 98
[mlx4 info] backend fallthrough branches removed: 48
[mlx4 info] backend direct calls: 51
[mlx4 info] aggregate allocation sites: 4
[mlx4 info] aggregate allocation bytes (static): 48
[mlx4 7/9] done in 3.563 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 125
[mlx4 info] maximum lookup probes: 4
[mlx4 8/9] done in 0.173 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5297560
[mlx4 9/9] done in 1.194 ms
[mlx4] total: 77.945 ms
building mlx-unlink
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 32
[mlx4 info] imports discovered: 59
[mlx4 info] source bytes: 101008
[mlx4 info] tokens scanned: 24303
[mlx4 info] AST nodes parsed: 12186
[mlx4 info] declarations discovered: 875
[mlx4 info] functions discovered: 342
[mlx4 info] source I/O: 1.652 ms
[mlx4 info] lexing: 8.640 ms
[mlx4 info] parsing: 5.618 ms
[mlx4 1/9] done in 17.449 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.417 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 340
[mlx4 info] symbols registered: 652
[mlx4 3/9] done in 5.452 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2196
[mlx4 info] type intern probes: 1901
[mlx4 info] maximum type intern probes: 2
[mlx4 info] primitive type cache hits: 3943
[mlx4 info] aggregate method lookups: 473
[mlx4 info] aggregate method probes: 73
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 14.203 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 14
[mlx4 info] calls automatically inlined: 14
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 4519
[mlx4 info] expression type cache hits: 1867
[mlx4 info] resolved type queries: 527
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 14068
[mlx4 info] LIR extra words: 2830
[mlx4 info] LIR symbols: 297
[mlx4 info] string literals: 19
[mlx4 info] type intern calls: 2552
[mlx4 info] type intern probes: 2260
[mlx4 info] maximum type intern probes: 2
[mlx4 info] primitive type cache hits: 4165
[mlx4 info] aggregate method lookups: 1117
[mlx4 info] aggregate method probes: 121
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 23.833 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 14068
[mlx4 info] LIR instructions after optimization: 667
[mlx4 info] constants folded: 1
[mlx4 info] constant branches simplified: 0
[mlx4 info] comparison branches fused: 11
[mlx4 info] local loads forwarded: 40
[mlx4 info] copy operands propagated: 40
[mlx4 info] local stack slots promoted: 48
[mlx4 info] local stores eliminated: 37
[mlx4 info] unreachable functions eliminated: 273
[mlx4 info] unreachable function instructions eliminated: 13204
[mlx4 info] post-terminator instructions eliminated: 60
[mlx4 info] dead instructions eliminated: 41
[mlx4 6/9] done in 10.626 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 6455
[mlx4 info] backend symbols: 114
[mlx4 info] backend fixups: 99
[mlx4 info] backend stack loads forwarded: 6
[mlx4 info] backend cached rax loads: 43
[mlx4 info] backend memory operands: 19
[mlx4 info] backend deferred spills: 95
[mlx4 info] backend retained rax uses: 95
[mlx4 info] backend fallthrough branches removed: 48
[mlx4 info] backend direct calls: 50
[mlx4 info] aggregate allocation sites: 4
[mlx4 info] aggregate allocation bytes (static): 48
[mlx4 7/9] done in 3.724 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 123
[mlx4 info] maximum lookup probes: 4
[mlx4 8/9] done in 0.159 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5294144
[mlx4 9/9] done in 1.201 ms
[mlx4] total: 78.074 ms
building mlx-touch
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 33
[mlx4 info] imports discovered: 61
[mlx4 info] source bytes: 116652
[mlx4 info] tokens scanned: 28189
[mlx4 info] AST nodes parsed: 14436
[mlx4 info] declarations discovered: 963
[mlx4 info] functions discovered: 367
[mlx4 info] source I/O: 1.611 ms
[mlx4 info] lexing: 9.166 ms
[mlx4 info] parsing: 5.924 ms
[mlx4 1/9] done in 18.263 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.388 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 364
[mlx4 info] symbols registered: 689
[mlx4 3/9] done in 5.134 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2343
[mlx4 info] type intern probes: 2010
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4886
[mlx4 info] aggregate method lookups: 645
[mlx4 info] aggregate method probes: 72
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 21.710 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 15
[mlx4 info] calls automatically inlined: 15
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 6007
[mlx4 info] expression type cache hits: 2568
[mlx4 info] resolved type queries: 608
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 17860
[mlx4 info] LIR extra words: 3398
[mlx4 info] LIR symbols: 322
[mlx4 info] string literals: 35
[mlx4 info] type intern calls: 2722
[mlx4 info] type intern probes: 2392
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 5149
[mlx4 info] aggregate method lookups: 1533
[mlx4 info] aggregate method probes: 118
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 34.751 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 17860
[mlx4 info] LIR instructions after optimization: 4529
[mlx4 info] constants folded: 10
[mlx4 info] constant branches simplified: 8
[mlx4 info] comparison branches fused: 46
[mlx4 info] local loads forwarded: 103
[mlx4 info] copy operands propagated: 104
[mlx4 info] local stack slots promoted: 123
[mlx4 info] local stores eliminated: 68
[mlx4 info] unreachable functions eliminated: 265
[mlx4 info] unreachable function instructions eliminated: 12741
[mlx4 info] post-terminator instructions eliminated: 226
[mlx4 info] dead instructions eliminated: 127
[mlx4 6/9] done in 15.151 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 37539
[mlx4 info] backend symbols: 661
[mlx4 info] backend fixups: 492
[mlx4 info] backend stack loads forwarded: 103
[mlx4 info] backend cached rax loads: 477
[mlx4 info] backend memory operands: 87
[mlx4 info] backend deferred spills: 1117
[mlx4 info] backend retained rax uses: 1117
[mlx4 info] backend fallthrough branches removed: 367
[mlx4 info] backend direct calls: 151
[mlx4 info] aggregate allocation sites: 35
[mlx4 info] aggregate allocation bytes (static): 1048
[mlx4 7/9] done in 6.662 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 570
[mlx4 info] maximum lookup probes: 4
[mlx4 8/9] done in 0.279 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 6176848
[mlx4 9/9] done in 1.829 ms
[mlx4] total: 105.178 ms
building mlx-truncate
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 33
[mlx4 info] imports discovered: 61
[mlx4 info] source bytes: 112644
[mlx4 info] tokens scanned: 27311
[mlx4 info] AST nodes parsed: 13918
[mlx4 info] declarations discovered: 942
[mlx4 info] functions discovered: 358
[mlx4 info] source I/O: 1.656 ms
[mlx4 info] lexing: 9.296 ms
[mlx4 info] parsing: 6.008 ms
[mlx4 1/9] done in 18.631 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.452 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 361
[mlx4 info] symbols registered: 680
[mlx4 3/9] done in 5.828 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2298
[mlx4 info] type intern probes: 2006
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4702
[mlx4 info] aggregate method lookups: 573
[mlx4 info] aggregate method probes: 72
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 18.053 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 14
[mlx4 info] calls automatically inlined: 14
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 5570
[mlx4 info] expression type cache hits: 2348
[mlx4 info] resolved type queries: 586
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 17005
[mlx4 info] LIR extra words: 3215
[mlx4 info] LIR symbols: 313
[mlx4 info] string literals: 27
[mlx4 info] type intern calls: 2667
[mlx4 info] type intern probes: 2387
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4950
[mlx4 info] aggregate method lookups: 1373
[mlx4 info] aggregate method probes: 118
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 29.966 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 17005
[mlx4 info] LIR instructions after optimization: 3389
[mlx4 info] constants folded: 1
[mlx4 info] constant branches simplified: 0
[mlx4 info] comparison branches fused: 38
[mlx4 info] local loads forwarded: 87
[mlx4 info] copy operands propagated: 88
[mlx4 info] local stack slots promoted: 129
[mlx4 info] local stores eliminated: 65
[mlx4 info] unreachable functions eliminated: 266
[mlx4 info] unreachable function instructions eliminated: 13083
[mlx4 info] post-terminator instructions eliminated: 212
[mlx4 info] dead instructions eliminated: 89
[mlx4 6/9] done in 14.241 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 28163
[mlx4 info] backend symbols: 516
[mlx4 info] backend fixups: 353
[mlx4 info] backend stack loads forwarded: 93
[mlx4 info] backend cached rax loads: 333
[mlx4 info] backend memory operands: 65
[mlx4 info] backend deferred spills: 822
[mlx4 info] backend retained rax uses: 822
[mlx4 info] backend fallthrough branches removed: 295
[mlx4 info] backend direct calls: 103
[mlx4 info] aggregate allocation sites: 42
[mlx4 info] aggregate allocation bytes (static): 952
[mlx4 7/9] done in 6.136 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 408
[mlx4 info] maximum lookup probes: 3
[mlx4 8/9] done in 0.235 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5991560
[mlx4 9/9] done in 1.790 ms
[mlx4] total: 96.342 ms
building mlx-mkfifo
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 34
[mlx4 info] imports discovered: 63
[mlx4 info] source bytes: 110063
[mlx4 info] tokens scanned: 26570
[mlx4 info] AST nodes parsed: 13565
[mlx4 info] declarations discovered: 936
[mlx4 info] functions discovered: 363
[mlx4 info] source I/O: 2.092 ms
[mlx4 info] lexing: 10.271 ms
[mlx4 info] parsing: 6.684 ms
[mlx4 1/9] done in 20.943 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.554 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 362
[mlx4 info] symbols registered: 681
[mlx4 3/9] done in 6.105 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2312
[mlx4 info] type intern probes: 1989
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4501
[mlx4 info] aggregate method lookups: 528
[mlx4 info] aggregate method probes: 76
[mlx4 info] maximum aggregate method probes: 2
[mlx4 4/9] done in 17.325 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 15
[mlx4 info] calls automatically inlined: 15
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 5316
[mlx4 info] expression type cache hits: 2172
[mlx4 info] resolved type queries: 575
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 16278
[mlx4 info] LIR extra words: 3160
[mlx4 info] LIR symbols: 318
[mlx4 info] string literals: 27
[mlx4 info] type intern calls: 2681
[mlx4 info] type intern probes: 2365
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4753
[mlx4 info] aggregate method lookups: 1248
[mlx4 info] aggregate method probes: 127
[mlx4 info] maximum aggregate method probes: 2
[mlx4 5/9] done in 30.796 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 16278
[mlx4 info] LIR instructions after optimization: 2809
[mlx4 info] constants folded: 6
[mlx4 info] constant branches simplified: 4
[mlx4 info] comparison branches fused: 32
[mlx4 info] local loads forwarded: 85
[mlx4 info] copy operands propagated: 85
[mlx4 info] local stack slots promoted: 119
[mlx4 info] local stores eliminated: 79
[mlx4 info] unreachable functions eliminated: 266
[mlx4 info] unreachable function instructions eliminated: 12967
[mlx4 info] post-terminator instructions eliminated: 176
[mlx4 info] dead instructions eliminated: 96
[mlx4 6/9] done in 14.739 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 23008
[mlx4 info] backend symbols: 472
[mlx4 info] backend fixups: 345
[mlx4 info] backend stack loads forwarded: 40
[mlx4 info] backend cached rax loads: 265
[mlx4 info] backend memory operands: 75
[mlx4 info] backend deferred spills: 578
[mlx4 info] backend retained rax uses: 578
[mlx4 info] backend fallthrough branches removed: 257
[mlx4 info] backend direct calls: 108
[mlx4 info] aggregate allocation sites: 12
[mlx4 info] aggregate allocation bytes (static): 320
[mlx4 7/9] done in 5.553 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 428
[mlx4 info] maximum lookup probes: 6
[mlx4 8/9] done in 0.244 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5876008
[mlx4 9/9] done in 1.604 ms
[mlx4] total: 98.876 ms
building mlx-sync
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 32
[mlx4 info] imports discovered: 59
[mlx4 info] source bytes: 102822
[mlx4 info] tokens scanned: 24687
[mlx4 info] AST nodes parsed: 12397
[mlx4 info] declarations discovered: 886
[mlx4 info] functions discovered: 344
[mlx4 info] source I/O: 1.888 ms
[mlx4 info] lexing: 9.518 ms
[mlx4 info] parsing: 6.187 ms
[mlx4 1/9] done in 19.365 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.459 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 343
[mlx4 info] symbols registered: 656
[mlx4 3/9] done in 5.967 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2211
[mlx4 info] type intern probes: 1895
[mlx4 info] maximum type intern probes: 2
[mlx4 info] primitive type cache hits: 4035
[mlx4 info] aggregate method lookups: 496
[mlx4 info] aggregate method probes: 73
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 17.352 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 14
[mlx4 info] calls automatically inlined: 14
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 4665
[mlx4 info] expression type cache hits: 1942
[mlx4 info] resolved type queries: 534
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 14446
[mlx4 info] LIR extra words: 2885
[mlx4 info] LIR symbols: 299
[mlx4 info] string literals: 23
[mlx4 info] type intern calls: 2566
[mlx4 info] type intern probes: 2259
[mlx4 info] maximum type intern probes: 2
[mlx4 info] primitive type cache hits: 4259
[mlx4 info] aggregate method lookups: 1171
[mlx4 info] aggregate method probes: 121
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 29.776 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 14446
[mlx4 info] LIR instructions after optimization: 1090
[mlx4 info] constants folded: 1
[mlx4 info] constant branches simplified: 0
[mlx4 info] comparison branches fused: 11
[mlx4 info] local loads forwarded: 49
[mlx4 info] copy operands propagated: 49
[mlx4 info] local stack slots promoted: 61
[mlx4 info] local stores eliminated: 47
[mlx4 info] unreachable functions eliminated: 268
[mlx4 info] unreachable function instructions eliminated: 13107
[mlx4 info] post-terminator instructions eliminated: 80
[mlx4 info] dead instructions eliminated: 50
[mlx4 6/9] done in 12.858 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 9879
[mlx4 info] backend symbols: 181
[mlx4 info] backend fixups: 148
[mlx4 info] backend stack loads forwarded: 16
[mlx4 info] backend cached rax loads: 92
[mlx4 info] backend memory operands: 22
[mlx4 info] backend deferred spills: 202
[mlx4 info] backend retained rax uses: 202
[mlx4 info] backend fallthrough branches removed: 86
[mlx4 info] backend direct calls: 60
[mlx4 info] aggregate allocation sites: 6
[mlx4 info] aggregate allocation bytes (static): 80
[mlx4 7/9] done in 4.221 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 170
[mlx4 info] maximum lookup probes: 6
[mlx4 8/9] done in 0.179 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5391296
[mlx4 9/9] done in 1.355 ms
[mlx4] total: 92.541 ms
building mlx-cp
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 33
[mlx4 info] imports discovered: 62
[mlx4 info] source bytes: 111607
[mlx4 info] tokens scanned: 26791
[mlx4 info] AST nodes parsed: 13638
[mlx4 info] declarations discovered: 946
[mlx4 info] functions discovered: 356
[mlx4 info] source I/O: 1.771 ms
[mlx4 info] lexing: 10.063 ms
[mlx4 info] parsing: 6.498 ms
[mlx4 1/9] done in 20.141 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.460 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 354
[mlx4 info] symbols registered: 671
[mlx4 3/9] done in 6.111 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2293
[mlx4 info] type intern probes: 1980
[mlx4 info] maximum type intern probes: 5
[mlx4 info] primitive type cache hits: 4490
[mlx4 info] aggregate method lookups: 576
[mlx4 info] aggregate method probes: 72
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 18.517 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 14
[mlx4 info] calls automatically inlined: 14
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 5450
[mlx4 info] expression type cache hits: 2296
[mlx4 info] resolved type queries: 592
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 16300
[mlx4 info] LIR extra words: 3244
[mlx4 info] LIR symbols: 311
[mlx4 info] string literals: 37
[mlx4 info] type intern calls: 2657
[mlx4 info] type intern probes: 2360
[mlx4 info] maximum type intern probes: 5
[mlx4 info] primitive type cache hits: 4739
[mlx4 info] aggregate method lookups: 1367
[mlx4 info] aggregate method probes: 118
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 30.008 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 16300
[mlx4 info] LIR instructions after optimization: 3451
[mlx4 info] constants folded: 9
[mlx4 info] constant branches simplified: 8
[mlx4 info] comparison branches fused: 30
[mlx4 info] local loads forwarded: 121
[mlx4 info] copy operands propagated: 122
[mlx4 info] local stack slots promoted: 140
[mlx4 info] local stores eliminated: 92
[mlx4 info] unreachable functions eliminated: 249
[mlx4 info] unreachable function instructions eliminated: 12232
[mlx4 info] post-terminator instructions eliminated: 212
[mlx4 info] dead instructions eliminated: 143
[mlx4 6/9] done in 12.996 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 30187
[mlx4 info] backend symbols: 513
[mlx4 info] backend fixups: 387
[mlx4 info] backend stack loads forwarded: 83
[mlx4 info] backend cached rax loads: 314
[mlx4 info] backend memory operands: 57
[mlx4 info] backend deferred spills: 733
[mlx4 info] backend retained rax uses: 733
[mlx4 info] backend fallthrough branches removed: 270
[mlx4 info] backend direct calls: 138
[mlx4 info] aggregate allocation sites: 21
[mlx4 info] aggregate allocation bytes (static): 848
[mlx4 7/9] done in 6.312 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 439
[mlx4 info] maximum lookup probes: 4
[mlx4 8/9] done in 0.311 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5901040
[mlx4 9/9] done in 1.860 ms
[mlx4] total: 97.726 ms
building mlx-mv
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 33
[mlx4 info] imports discovered: 62
[mlx4 info] source bytes: 110137
[mlx4 info] tokens scanned: 26439
[mlx4 info] AST nodes parsed: 13435
[mlx4 info] declarations discovered: 936
[mlx4 info] functions discovered: 354
[mlx4 info] source I/O: 1.638 ms
[mlx4 info] lexing: 8.890 ms
[mlx4 info] parsing: 5.949 ms
[mlx4 1/9] done in 18.049 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.506 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 352
[mlx4 info] symbols registered: 669
[mlx4 3/9] done in 5.908 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2290
[mlx4 info] type intern probes: 1989
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4433
[mlx4 info] aggregate method lookups: 562
[mlx4 info] aggregate method probes: 72
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 19.397 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 14
[mlx4 info] calls automatically inlined: 14
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 5348
[mlx4 info] expression type cache hits: 2251
[mlx4 info] resolved type queries: 582
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 16026
[mlx4 info] LIR extra words: 3181
[mlx4 info] LIR symbols: 309
[mlx4 info] string literals: 38
[mlx4 info] type intern calls: 2653
[mlx4 info] type intern probes: 2363
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4681
[mlx4 info] aggregate method lookups: 1337
[mlx4 info] aggregate method probes: 118
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 28.382 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 16026
[mlx4 info] LIR instructions after optimization: 2638
[mlx4 info] constants folded: 1
[mlx4 info] constant branches simplified: 0
[mlx4 info] comparison branches fused: 25
[mlx4 info] local loads forwarded: 85
[mlx4 info] copy operands propagated: 86
[mlx4 info] local stack slots promoted: 109
[mlx4 info] local stores eliminated: 78
[mlx4 info] unreachable functions eliminated: 263
[mlx4 info] unreachable function instructions eliminated: 12943
[mlx4 info] post-terminator instructions eliminated: 146
[mlx4 info] dead instructions eliminated: 87
[mlx4 6/9] done in 13.519 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 23330
[mlx4 info] backend symbols: 403
[mlx4 info] backend fixups: 312
[mlx4 info] backend stack loads forwarded: 66
[mlx4 info] backend cached rax loads: 238
[mlx4 info] backend memory operands: 49
[mlx4 info] backend deferred spills: 574
[mlx4 info] backend retained rax uses: 574
[mlx4 info] backend fallthrough branches removed: 214
[mlx4 info] backend direct calls: 105
[mlx4 info] aggregate allocation sites: 12
[mlx4 info] aggregate allocation bytes (static): 608
[mlx4 7/9] done in 5.105 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 379
[mlx4 info] maximum lookup probes: 4
[mlx4 8/9] done in 0.187 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 5803168
[mlx4 9/9] done in 1.801 ms
[mlx4] total: 93.863 ms
building mlx-rm
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 34
[mlx4 info] imports discovered: 65
[mlx4 info] source bytes: 114763
[mlx4 info] tokens scanned: 27599
[mlx4 info] AST nodes parsed: 14100
[mlx4 info] declarations discovered: 971
[mlx4 info] functions discovered: 364
[mlx4 info] source I/O: 1.868 ms
[mlx4 info] lexing: 10.421 ms
[mlx4 info] parsing: 6.967 ms
[mlx4 1/9] done in 21.189 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.532 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 360
[mlx4 info] symbols registered: 684
[mlx4 3/9] done in 5.355 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2329
[mlx4 info] type intern probes: 2018
[mlx4 info] maximum type intern probes: 6
[mlx4 info] primitive type cache hits: 4690
[mlx4 info] aggregate method lookups: 600
[mlx4 info] aggregate method probes: 76
[mlx4 info] maximum aggregate method probes: 2
[mlx4 4/9] done in 18.827 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 17
[mlx4 info] calls automatically inlined: 17
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 5691
[mlx4 info] expression type cache hits: 2404
[mlx4 info] resolved type queries: 613
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 17047
[mlx4 info] LIR extra words: 3338
[mlx4 info] LIR symbols: 319
[mlx4 info] string literals: 45
[mlx4 info] type intern calls: 2716
[mlx4 info] type intern probes: 2410
[mlx4 info] maximum type intern probes: 6
[mlx4 info] primitive type cache hits: 4936
[mlx4 info] aggregate method lookups: 1422
[mlx4 info] aggregate method probes: 127
[mlx4 info] maximum aggregate method probes: 2
[mlx4 5/9] done in 32.644 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 17047
[mlx4 info] LIR instructions after optimization: 4165
[mlx4 info] constants folded: 12
[mlx4 info] constant branches simplified: 8
[mlx4 info] comparison branches fused: 46
[mlx4 info] local loads forwarded: 136
[mlx4 info] copy operands propagated: 138
[mlx4 info] local stack slots promoted: 162
[mlx4 info] local stores eliminated: 112
[mlx4 info] unreachable functions eliminated: 243
[mlx4 info] unreachable function instructions eliminated: 12159
[mlx4 info] post-terminator instructions eliminated: 242
[mlx4 info] dead instructions eliminated: 161
[mlx4 6/9] done in 14.647 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 35030
[mlx4 info] backend symbols: 638
[mlx4 info] backend fixups: 473
[mlx4 info] backend stack loads forwarded: 104
[mlx4 info] backend cached rax loads: 366
[mlx4 info] backend memory operands: 78
[mlx4 info] backend deferred spills: 910
[mlx4 info] backend retained rax uses: 910
[mlx4 info] backend fallthrough branches removed: 336
[mlx4 info] backend direct calls: 149
[mlx4 info] aggregate allocation sites: 18
[mlx4 info] aggregate allocation bytes (static): 656
[mlx4 7/9] done in 6.050 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 559
[mlx4 info] maximum lookup probes: 5
[mlx4 8/9] done in 0.320 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 6097000
[mlx4 9/9] done in 10.049 ms
[mlx4] total: 110.623 ms
building mlx-ln
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 34
[mlx4 info] imports discovered: 65
[mlx4 info] source bytes: 128997
[mlx4 info] tokens scanned: 30930
[mlx4 info] AST nodes parsed: 16071
[mlx4 info] declarations discovered: 1057
[mlx4 info] functions discovered: 384
[mlx4 info] source I/O: 2.122 ms
[mlx4 info] lexing: 11.768 ms
[mlx4 info] parsing: 7.702 ms
[mlx4 1/9] done in 23.670 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.767 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 374
[mlx4 info] symbols registered: 709
[mlx4 3/9] done in 6.558 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2510
[mlx4 info] type intern probes: 2348
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 5417
[mlx4 info] aggregate method lookups: 730
[mlx4 info] aggregate method probes: 76
[mlx4 info] maximum aggregate method probes: 2
[mlx4 4/9] done in 26.382 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 16
[mlx4 info] calls automatically inlined: 16
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 6989
[mlx4 info] expression type cache hits: 3002
[mlx4 info] resolved type queries: 712
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 20095
[mlx4 info] LIR extra words: 3955
[mlx4 info] LIR symbols: 339
[mlx4 info] string literals: 56
[mlx4 info] type intern calls: 2919
[mlx4 info] type intern probes: 2781
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 5714
[mlx4 info] aggregate method lookups: 1741
[mlx4 info] aggregate method probes: 127
[mlx4 info] maximum aggregate method probes: 2
[mlx4 5/9] done in 41.982 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 20095
[mlx4 info] LIR instructions after optimization: 6603
[mlx4 info] constants folded: 5
[mlx4 info] constant branches simplified: 8
[mlx4 info] comparison branches fused: 74
[mlx4 info] local loads forwarded: 198
[mlx4 info] copy operands propagated: 198
[mlx4 info] local stack slots promoted: 219
[mlx4 info] local stores eliminated: 131
[mlx4 info] unreachable functions eliminated: 251
[mlx4 info] unreachable function instructions eliminated: 12514
[mlx4 info] post-terminator instructions eliminated: 336
[mlx4 info] dead instructions eliminated: 218
[mlx4 6/9] done in 16.413 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 56176
[mlx4 info] backend symbols: 979
[mlx4 info] backend fixups: 713
[mlx4 info] backend stack loads forwarded: 183
[mlx4 info] backend cached rax loads: 634
[mlx4 info] backend memory operands: 126
[mlx4 info] backend deferred spills: 1514
[mlx4 info] backend retained rax uses: 1514
[mlx4 info] backend fallthrough branches removed: 556
[mlx4 info] backend direct calls: 215
[mlx4 info] aggregate allocation sites: 27
[mlx4 info] aggregate allocation bytes (static): 1472
[mlx4 7/9] done in 8.680 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 936
[mlx4 info] maximum lookup probes: 13
[mlx4 8/9] done in 0.395 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 6767800
[mlx4 9/9] done in 2.483 ms
[mlx4] total: 128.341 ms
building mlx-chmod
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 35
[mlx4 info] imports discovered: 67
[mlx4 info] source bytes: 120494
[mlx4 info] tokens scanned: 29118
[mlx4 info] AST nodes parsed: 15042
[mlx4 info] declarations discovered: 1009
[mlx4 info] functions discovered: 379
[mlx4 info] source I/O: 1.963 ms
[mlx4 info] lexing: 10.561 ms
[mlx4 info] parsing: 7.042 ms
[mlx4 1/9] done in 21.549 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.572 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 375
[mlx4 info] symbols registered: 703
[mlx4 3/9] done in 5.599 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2400
[mlx4 info] type intern probes: 2227
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 5089
[mlx4 info] aggregate method lookups: 626
[mlx4 info] aggregate method probes: 72
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 20.543 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 14
[mlx4 info] calls automatically inlined: 14
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 6247
[mlx4 info] expression type cache hits: 2606
[mlx4 info] resolved type queries: 646
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 18595
[mlx4 info] LIR extra words: 3585
[mlx4 info] LIR symbols: 334
[mlx4 info] string literals: 42
[mlx4 info] type intern calls: 2790
[mlx4 info] type intern probes: 2632
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 5371
[mlx4 info] aggregate method lookups: 1494
[mlx4 info] aggregate method probes: 118
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 34.830 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 18595
[mlx4 info] LIR instructions after optimization: 4990
[mlx4 info] constants folded: 9
[mlx4 info] constant branches simplified: 6
[mlx4 info] comparison branches fused: 49
[mlx4 info] local loads forwarded: 151
[mlx4 info] copy operands propagated: 153
[mlx4 info] local stack slots promoted: 177
[mlx4 info] local stores eliminated: 114
[mlx4 info] unreachable functions eliminated: 255
[mlx4 info] unreachable function instructions eliminated: 12815
[mlx4 info] post-terminator instructions eliminated: 272
[mlx4 info] dead instructions eliminated: 178
[mlx4 6/9] done in 16.624 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 41247
[mlx4 info] backend symbols: 778
[mlx4 info] backend fixups: 562
[mlx4 info] backend stack loads forwarded: 99
[mlx4 info] backend cached rax loads: 484
[mlx4 info] backend memory operands: 98
[mlx4 info] backend deferred spills: 1090
[mlx4 info] backend retained rax uses: 1090
[mlx4 info] backend fallthrough branches removed: 433
[mlx4 info] backend direct calls: 160
[mlx4 info] aggregate allocation sites: 21
[mlx4 info] aggregate allocation bytes (static): 880
[mlx4 7/9] done in 8.076 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 646
[mlx4 info] maximum lookup probes: 5
[mlx4 8/9] done in 0.368 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 6461416
[mlx4 9/9] done in 2.245 ms
[mlx4] total: 111.416 ms
building mlx-readlink
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 33
[mlx4 info] imports discovered: 62
[mlx4 info] source bytes: 117572
[mlx4 info] tokens scanned: 28239
[mlx4 info] AST nodes parsed: 14505
[mlx4 info] declarations discovered: 984
[mlx4 info] functions discovered: 368
[mlx4 info] source I/O: 1.736 ms
[mlx4 info] lexing: 9.592 ms
[mlx4 info] parsing: 6.244 ms
[mlx4 1/9] done in 19.231 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.411 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 365
[mlx4 info] symbols registered: 689
[mlx4 3/9] done in 6.093 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2398
[mlx4 info] type intern probes: 2073
[mlx4 info] maximum type intern probes: 5
[mlx4 info] primitive type cache hits: 4868
[mlx4 info] aggregate method lookups: 633
[mlx4 info] aggregate method probes: 72
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 20.405 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 17
[mlx4 info] calls automatically inlined: 17
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 6089
[mlx4 info] expression type cache hits: 2600
[mlx4 info] resolved type queries: 649
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 17689
[mlx4 info] LIR extra words: 3499
[mlx4 info] LIR symbols: 323
[mlx4 info] string literals: 30
[mlx4 info] type intern calls: 2796
[mlx4 info] type intern probes: 2471
[mlx4 info] maximum type intern probes: 5
[mlx4 info] primitive type cache hits: 5147
[mlx4 info] aggregate method lookups: 1517
[mlx4 info] aggregate method probes: 118
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 34.552 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 17689
[mlx4 info] LIR instructions after optimization: 3658
[mlx4 info] constants folded: 3
[mlx4 info] constant branches simplified: 2
[mlx4 info] comparison branches fused: 46
[mlx4 info] local loads forwarded: 115
[mlx4 info] copy operands propagated: 115
[mlx4 info] local stack slots promoted: 132
[mlx4 info] local stores eliminated: 86
[mlx4 info] unreachable functions eliminated: 267
[mlx4 info] unreachable function instructions eliminated: 13462
[mlx4 info] post-terminator instructions eliminated: 182
[mlx4 info] dead instructions eliminated: 123
[mlx4 6/9] done in 14.949 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 30937
[mlx4 info] backend symbols: 548
[mlx4 info] backend fixups: 390
[mlx4 info] backend stack loads forwarded: 88
[mlx4 info] backend cached rax loads: 360
[mlx4 info] backend memory operands: 77
[mlx4 info] backend deferred spills: 783
[mlx4 info] backend retained rax uses: 783
[mlx4 info] backend fallthrough branches removed: 305
[mlx4 info] backend direct calls: 110
[mlx4 info] aggregate allocation sites: 12
[mlx4 info] aggregate allocation bytes (static): 496
[mlx4 7/9] done in 6.462 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 429
[mlx4 info] maximum lookup probes: 3
[mlx4 8/9] done in 0.277 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 6178784
[mlx4 9/9] done in 2.046 ms
[mlx4] total: 105.436 ms
building mlx-realpath
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 33
[mlx4 info] imports discovered: 62
[mlx4 info] source bytes: 120630
[mlx4 info] tokens scanned: 28960
[mlx4 info] AST nodes parsed: 14932
[mlx4 info] declarations discovered: 997
[mlx4 info] functions discovered: 371
[mlx4 info] source I/O: 1.669 ms
[mlx4 info] lexing: 9.763 ms
[mlx4 info] parsing: 6.302 ms
[mlx4 1/9] done in 19.363 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.541 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 370
[mlx4 info] symbols registered: 691
[mlx4 3/9] done in 5.506 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2445
[mlx4 info] type intern probes: 2109
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 4986
[mlx4 info] aggregate method lookups: 647
[mlx4 info] aggregate method probes: 74
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 20.961 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 16
[mlx4 info] calls automatically inlined: 16
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 6262
[mlx4 info] expression type cache hits: 2652
[mlx4 info] resolved type queries: 658
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 18279
[mlx4 info] LIR extra words: 3658
[mlx4 info] LIR symbols: 326
[mlx4 info] string literals: 39
[mlx4 info] type intern calls: 2844
[mlx4 info] type intern probes: 2508
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 5261
[mlx4 info] aggregate method lookups: 1543
[mlx4 info] aggregate method probes: 122
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 35.466 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 18279
[mlx4 info] LIR instructions after optimization: 5046
[mlx4 info] constants folded: 3
[mlx4 info] constant branches simplified: 5
[mlx4 info] comparison branches fused: 56
[mlx4 info] local loads forwarded: 153
[mlx4 info] copy operands propagated: 153
[mlx4 info] local stack slots promoted: 177
[mlx4 info] local stores eliminated: 103
[mlx4 info] unreachable functions eliminated: 259
[mlx4 info] unreachable function instructions eliminated: 12466
[mlx4 info] post-terminator instructions eliminated: 266
[mlx4 info] dead instructions eliminated: 165
[mlx4 6/9] done in 15.212 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 42577
[mlx4 info] backend symbols: 756
[mlx4 info] backend fixups: 540
[mlx4 info] backend stack loads forwarded: 133
[mlx4 info] backend cached rax loads: 496
[mlx4 info] backend memory operands: 95
[mlx4 info] backend deferred spills: 1122
[mlx4 info] backend retained rax uses: 1122
[mlx4 info] backend fallthrough branches removed: 433
[mlx4 info] backend direct calls: 160
[mlx4 info] aggregate allocation sites: 16
[mlx4 info] aggregate allocation bytes (static): 624
[mlx4 7/9] done in 7.448 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 626
[mlx4 info] maximum lookup probes: 4
[mlx4 8/9] done in 0.286 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 6353392
[mlx4 9/9] done in 2.052 ms
[mlx4] total: 107.864 ms
building mlx-chown
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 36
[mlx4 info] imports discovered: 69
[mlx4 info] source bytes: 127523
[mlx4 info] tokens scanned: 30913
[mlx4 info] AST nodes parsed: 15855
[mlx4 info] declarations discovered: 1074
[mlx4 info] functions discovered: 388
[mlx4 info] source I/O: 2.052 ms
[mlx4 info] lexing: 11.326 ms
[mlx4 info] parsing: 7.407 ms
[mlx4 1/9] done in 22.873 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.731 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 388
[mlx4 info] symbols registered: 721
[mlx4 3/9] done in 6.475 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2486
[mlx4 info] type intern probes: 2174
[mlx4 info] maximum type intern probes: 6
[mlx4 info] primitive type cache hits: 5311
[mlx4 info] aggregate method lookups: 710
[mlx4 info] aggregate method probes: 72
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 24.142 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 16
[mlx4 info] calls automatically inlined: 16
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 6616
[mlx4 info] expression type cache hits: 2841
[mlx4 info] resolved type queries: 713
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 19612
[mlx4 info] LIR extra words: 3882
[mlx4 info] LIR symbols: 343
[mlx4 info] string literals: 58
[mlx4 info] type intern calls: 2917
[mlx4 info] type intern probes: 2623
[mlx4 info] maximum type intern probes: 6
[mlx4 info] primitive type cache hits: 5581
[mlx4 info] aggregate method lookups: 1705
[mlx4 info] aggregate method probes: 118
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 41.237 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 19612
[mlx4 info] LIR instructions after optimization: 5731
[mlx4 info] constants folded: 5
[mlx4 info] constant branches simplified: 2
[mlx4 info] comparison branches fused: 51
[mlx4 info] local loads forwarded: 181
[mlx4 info] copy operands propagated: 182
[mlx4 info] local stack slots promoted: 186
[mlx4 info] local stores eliminated: 117
[mlx4 info] unreachable functions eliminated: 258
[mlx4 info] unreachable function instructions eliminated: 13025
[mlx4 info] post-terminator instructions eliminated: 304
[mlx4 info] dead instructions eliminated: 198
[mlx4 6/9] done in 15.919 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 49202
[mlx4 info] backend symbols: 814
[mlx4 info] backend fixups: 605
[mlx4 info] backend stack loads forwarded: 180
[mlx4 info] backend cached rax loads: 551
[mlx4 info] backend memory operands: 85
[mlx4 info] backend deferred spills: 1312
[mlx4 info] backend retained rax uses: 1312
[mlx4 info] backend fallthrough branches removed: 450
[mlx4 info] backend direct calls: 194
[mlx4 info] aggregate allocation sites: 34
[mlx4 info] aggregate allocation bytes (static): 1128
[mlx4 7/9] done in 7.572 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 761
[mlx4 info] maximum lookup probes: 4
[mlx4 8/9] done in 0.385 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 6736288
[mlx4 9/9] done in 2.471 ms
[mlx4] total: 122.814 ms
building mlx-chgrp
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 36
[mlx4 info] imports discovered: 69
[mlx4 info] source bytes: 127523
[mlx4 info] tokens scanned: 30914
[mlx4 info] AST nodes parsed: 15855
[mlx4 info] declarations discovered: 1074
[mlx4 info] functions discovered: 388
[mlx4 info] source I/O: 1.795 ms
[mlx4 info] lexing: 10.332 ms
[mlx4 info] parsing: 6.961 ms
[mlx4 1/9] done in 21.012 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.524 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 388
[mlx4 info] symbols registered: 721
[mlx4 3/9] done in 6.050 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2486
[mlx4 info] type intern probes: 2172
[mlx4 info] maximum type intern probes: 4
[mlx4 info] primitive type cache hits: 5311
[mlx4 info] aggregate method lookups: 710
[mlx4 info] aggregate method probes: 72
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 26.643 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 16
[mlx4 info] calls automatically inlined: 16
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 6616
[mlx4 info] expression type cache hits: 2841
[mlx4 info] resolved type queries: 713
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 19612
[mlx4 info] LIR extra words: 3882
[mlx4 info] LIR symbols: 343
[mlx4 info] string literals: 58
[mlx4 info] type intern calls: 2917
[mlx4 info] type intern probes: 2623
[mlx4 info] maximum type intern probes: 4
[mlx4 info] primitive type cache hits: 5581
[mlx4 info] aggregate method lookups: 1705
[mlx4 info] aggregate method probes: 118
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 41.465 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 19612
[mlx4 info] LIR instructions after optimization: 5731
[mlx4 info] constants folded: 5
[mlx4 info] constant branches simplified: 2
[mlx4 info] comparison branches fused: 51
[mlx4 info] local loads forwarded: 181
[mlx4 info] copy operands propagated: 182
[mlx4 info] local stack slots promoted: 186
[mlx4 info] local stores eliminated: 117
[mlx4 info] unreachable functions eliminated: 258
[mlx4 info] unreachable function instructions eliminated: 13025
[mlx4 info] post-terminator instructions eliminated: 304
[mlx4 info] dead instructions eliminated: 198
[mlx4 6/9] done in 16.632 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 49205
[mlx4 info] backend symbols: 814
[mlx4 info] backend fixups: 605
[mlx4 info] backend stack loads forwarded: 180
[mlx4 info] backend cached rax loads: 551
[mlx4 info] backend memory operands: 85
[mlx4 info] backend deferred spills: 1312
[mlx4 info] backend retained rax uses: 1312
[mlx4 info] backend fallthrough branches removed: 450
[mlx4 info] backend direct calls: 194
[mlx4 info] aggregate allocation sites: 34
[mlx4 info] aggregate allocation bytes (static): 1128
[mlx4 7/9] done in 8.665 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 761
[mlx4 info] maximum lookup probes: 4
[mlx4 8/9] done in 0.313 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 6736344
[mlx4 9/9] done in 2.319 ms
[mlx4] total: 124.634 ms
building mlx-stat
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 35
[mlx4 info] imports discovered: 65
[mlx4 info] source bytes: 130349
[mlx4 info] tokens scanned: 31666
[mlx4 info] AST nodes parsed: 16497
[mlx4 info] declarations discovered: 1054
[mlx4 info] functions discovered: 395
[mlx4 info] source I/O: 2.152 ms
[mlx4 info] lexing: 12.380 ms
[mlx4 info] parsing: 8.394 ms
[mlx4 1/9] done in 25.064 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.802 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 391
[mlx4 info] symbols registered: 726
[mlx4 3/9] done in 6.936 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 2588
[mlx4 info] type intern probes: 2260
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 5629
[mlx4 info] aggregate method lookups: 722
[mlx4 info] aggregate method probes: 72
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 26.898 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 28
[mlx4 info] calls automatically inlined: 28
[mlx4 info] inline requests kept as calls: 0
[mlx4 info] expression type queries: 7180
[mlx4 info] expression type cache hits: 3028
[mlx4 info] resolved type queries: 716
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 21051
[mlx4 info] LIR extra words: 3956
[mlx4 info] LIR symbols: 350
[mlx4 info] string literals: 74
[mlx4 info] type intern calls: 3070
[mlx4 info] type intern probes: 2766
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 5917
[mlx4 info] aggregate method lookups: 1744
[mlx4 info] aggregate method probes: 118
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 42.787 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 21051
[mlx4 info] LIR instructions after optimization: 6772
[mlx4 info] constants folded: 29
[mlx4 info] constant branches simplified: 28
[mlx4 info] comparison branches fused: 61
[mlx4 info] local loads forwarded: 166
[mlx4 info] copy operands propagated: 176
[mlx4 info] local stack slots promoted: 248
[mlx4 info] local stores eliminated: 108
[mlx4 info] unreachable functions eliminated: 260
[mlx4 info] unreachable function instructions eliminated: 13179
[mlx4 info] post-terminator instructions eliminated: 462
[mlx4 info] dead instructions eliminated: 221
[mlx4 6/9] done in 18.718 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 55104
[mlx4 info] backend symbols: 1117
[mlx4 info] backend fixups: 762
[mlx4 info] backend stack loads forwarded: 142
[mlx4 info] backend cached rax loads: 748
[mlx4 info] backend memory operands: 115
[mlx4 info] backend deferred spills: 1579
[mlx4 info] backend retained rax uses: 1579
[mlx4 info] backend fallthrough branches removed: 599
[mlx4 info] backend direct calls: 231
[mlx4 info] aggregate allocation sites: 26
[mlx4 info] aggregate allocation bytes (static): 5408
[mlx4 7/9] done in 8.917 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 883
[mlx4 info] maximum lookup probes: 5
[mlx4 8/9] done in 0.466 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 7002136
[mlx4 9/9] done in 2.473 ms
[mlx4] total: 134.072 ms
building mlx-ls
[mlx4 1/9] load and parse module graph
[mlx4 info] modules loaded: 45
[mlx4 info] imports discovered: 109
[mlx4 info] source bytes: 194428
[mlx4 info] tokens scanned: 48352
[mlx4 info] AST nodes parsed: 26294
[mlx4 info] declarations discovered: 1475
[mlx4 info] functions discovered: 516
[mlx4 info] source I/O: 2.508 ms
[mlx4 info] lexing: 15.706 ms
[mlx4 info] parsing: 11.076 ms
[mlx4 1/9] done in 31.837 ms
[mlx4 2/9] resolve modules and declarations
[mlx4 2/9] done in 1.962 ms
[mlx4 3/9] analyze declaration types
[mlx4 info] types resolved: 493
[mlx4 info] symbols registered: 918
[mlx4 3/9] done in 9.020 ms
[mlx4 4/9] analyze function bodies
[mlx4 info] type intern calls: 3492
[mlx4 info] type intern probes: 3104
[mlx4 info] maximum type intern probes: 3
[mlx4 info] primitive type cache hits: 9429
[mlx4 info] aggregate method lookups: 1350
[mlx4 info] aggregate method probes: 122
[mlx4 info] maximum aggregate method probes: 1
[mlx4 4/9] done in 57.969 ms
[mlx4 5/9] lower typed AST to LIR
[mlx4 info] runtime safety checks: 1
[mlx4 info] generic instances: 0
[mlx4 info] optimization level: 2
[mlx4 info] calls inlined: 40
[mlx4 info] calls automatically inlined: 40
[mlx4 info] inline requests kept as calls: 6
[mlx4 info] expression type queries: 13537
[mlx4 info] expression type cache hits: 5834
[mlx4 info] resolved type queries: 1064
[mlx4 info] resolved type cache hits: 0
[mlx4 info] LIR instructions: 36708
[mlx4 info] LIR extra words: 6688
[mlx4 info] LIR symbols: 471
[mlx4 info] string literals: 253
[mlx4 info] type intern calls: 4119
[mlx4 info] type intern probes: 3740
[mlx4 info] maximum type intern probes: 4
[mlx4 info] primitive type cache hits: 9947
[mlx4 info] aggregate method lookups: 3380
[mlx4 info] aggregate method probes: 234
[mlx4 info] maximum aggregate method probes: 1
[mlx4 5/9] done in 105.808 ms
[mlx4 6/9] optimize LIR
[mlx4 info] LIR instructions before optimization: 36708
[mlx4 info] LIR instructions after optimization: 22573
[mlx4 info] constants folded: 37
[mlx4 info] constant branches simplified: 35
[mlx4 info] comparison branches fused: 223
[mlx4 info] local loads forwarded: 488
[mlx4 info] copy operands propagated: 494
[mlx4 info] local stack slots promoted: 592
[mlx4 info] local stores eliminated: 238
[mlx4 info] unreachable functions eliminated: 241
[mlx4 info] unreachable function instructions eliminated: 11382
[mlx4 info] post-terminator instructions eliminated: 1132
[mlx4 info] dead instructions eliminated: 568
[mlx4 6/9] done in 33.538 ms
[mlx4 7/9] generate x86_64 machine code
[mlx4 info] machine-code bytes: 186072
[mlx4 info] backend symbols: 3617
[mlx4 info] backend fixups: 2671
[mlx4 info] backend stack loads forwarded: 514
[mlx4 info] backend cached rax loads: 2421
[mlx4 info] backend memory operands: 428
[mlx4 info] backend deferred spills: 5196
[mlx4 info] backend retained rax uses: 5196
[mlx4 info] backend fallthrough branches removed: 2054
[mlx4 info] backend direct calls: 751
[mlx4 info] aggregate allocation sites: 63
[mlx4 info] aggregate allocation bytes (static): 7208
[mlx4 7/9] done in 22.189 ms
[mlx4 8/9] resolve backend symbols
[mlx4 info] symbol lookup probes: 3674
[mlx4 info] maximum lookup probes: 19
[mlx4 8/9] done in 1.244 ms
[mlx4 9/9] write ELF64 executable
[mlx4 info] compiler arena bytes: 11461704
[mlx4 9/9] done in 5.804 ms
[mlx4] total: 269.400 ms