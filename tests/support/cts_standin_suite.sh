#!/usr/bin/env bash
# A stand-in for deqp-vk, for checking tools/run_cts.sh without the suite
# (tools/check_cts.sh --runner): it takes the options run_cts.sh gives the
# suite and writes a result file as the suite does. It has six cases, each
# ending as its name says: s.pass, s.fail, s.unsupported (the device's),
# s.port (not supported for want of the port), s.crash (the suite dies in
# it) and s.after (passes; the case after the crash). A name it does not
# have is not run, as the suite does with a case list. Its error output
# has the line the port's driver library writes for a command it could
# not give the suite.
set -u
cases="s.pass s.fail s.unsupported s.port s.crash s.after"
list=""
log=""
export_file=""
for argument in "$@"; do
    case $argument in
    --deqp-caselist-file=*) list=${argument#*=} ;;
    --deqp-log-filename=*) log=${argument#*=} ;;
    --deqp-caselist-export-file=*) export_file=${argument#*=} ;;
    esac
done
if [[ -n "$export_file" ]]; then
    for c in $cases; do echo "TEST: $c"; done > "$export_file"
    exit 0
fi
[[ -n "$list" && -n "$log" ]] || { echo "stand-in: no case list or log file" >&2; exit 2; }
echo "mlxcts: the driver's vkCmdSetLineWidth is not given to the suite: it takes a float by value, which the library's entries do not carry" >&2
{
    echo "#sessionInfo releaseName vulkan-cts-1.3.10.0"
    echo "#sessionInfo commandLineParameters \"$*\""
    echo
    echo "#beginSession"
} > "$log"
result() { # CASE STATUS TEXT
    printf '#beginTestCaseResult %s\n<TestCaseResult CasePath="%s">\n <Result StatusCode="%s">%s</Result>\n</TestCaseResult>\n#endTestCaseResult\n\n' "$1" "$1" "$2" "$3" >> "$log"
}
# The cases in the suite's order, those of the list alone.
for c in $cases; do
    grep -qxF "$c" "$list" || continue
    case $c in
    s.pass|s.after) result "$c" Pass Ok ;;
    s.fail) result "$c" Fail "the value is wrong" ;;
    s.unsupported) result "$c" NotSupported "the feature is not supported" ;;
    s.port) result "$c" NotSupported "Amber is not built in this port: the case's script is not run" ;;
    s.crash)
        printf '#beginTestCaseResult %s\n<TestCaseResult CasePath="%s">\n' "$c" "$c" >> "$log"
        exit 139
        ;;
    esac
done
echo "#endSession" >> "$log"
exit 0
