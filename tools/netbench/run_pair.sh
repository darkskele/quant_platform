#!/bin/bash
# One rig run on the box. The peer in qp-peer, the backend under test in qp-rx, joined only by
# the veth pair.
#
#   tools/netbench/run_pair.sh <kernel_tcp|kernel_udp> [-d receive|send] [-s none|kernel]
#       [-n peers] [-c cpus] [-o dir] [-- traffic args...]
#
# receive, the default, has peers send to the backend and writes peer-<k>.json per peer and
# receiver.json into dir (netbench_out). send has the backend send to one peer and writes
# sender.json and receiver.json. Traffic args go to whoever sends, for example
# -- --sizes 512 --rate 10000. Several peers need udp and receive, each gets its own id, seed and
# ack port. -c runs the peers on those CPUs, outside the bench partition.
set -euo pipefail

usage="usage: run_pair.sh <kernel_tcp|kernel_udp> [-d receive|send] [-s none|kernel] [-n peers] [-c cpus] [-o dir] [-- traffic args...]"
backend=${1:?$usage}
shift
direction=receive
stamps=none
peers=1
peer_cpus=
out=netbench_out
while getopts d:s:n:c:o: opt; do
    case $opt in
        d) direction=$OPTARG ;;
        s) stamps=$OPTARG ;;
        n) peers=$OPTARG ;;
        c) peer_cpus=$OPTARG ;;
        o) out=$OPTARG ;;
        *) echo "$usage" >&2; exit 2 ;;
    esac
done
shift $((OPTIND - 1))
[[ ${1:-} == -- ]] && shift

case $backend in
    kernel_tcp) proto=tcp ;;
    kernel_udp) proto=udp ;;
    *) echo "$usage" >&2; exit 2 ;;
esac
[[ $peers =~ ^[1-9][0-9]*$ ]] || { echo "$usage" >&2; exit 2; }
[[ $direction == receive || $direction == send ]] || { echo "$usage" >&2; exit 2; }
[[ $peers == 1 || ($proto == udp && $direction == receive) ]] ||
    { echo "run_pair: several peers need udp and receive" >&2; exit 2; }

bin=${QP_BUILD_DIR:-build/release}
peer_ip=10.99.0.1
rx_ip=10.99.0.2
port=${QP_RIG_PORT:-9100}
ack_port=${QP_RIG_ACK_PORT:-9101}
mkdir -p "$out"

if [[ $direction == send ]]; then
    # The peer listens or binds, the backend connects or targets it, acks come back to the backend.
    QP_RIG_PEER_CPUS=$peer_cpus sudo -n qp-netns-run qp-peer "$bin/netbench_peer" --role receive \
        --proto "$proto" --target "$peer_ip:$port" --ack "$rx_ip:$ack_port" \
        --out "$out/receiver.json" &
    peer=$!
    tx_rc=0
    sudo -n qp-netns-run qp-rx "$bin/netbench_$backend" send --target "$peer_ip:$port" \
        --ack "$rx_ip:$ack_port" --out "$out/sender.json" "$@" || tx_rc=$?
    peer_rc=0
    wait "$peer" || peer_rc=$?
    echo "run_pair: $backend send sender=$tx_rc peer=$peer_rc, results in $out" >&2
    (( tx_rc == 0 && peer_rc == 0 ))
    exit
fi

# A TCP receiver connects to the peer, a UDP receiver is sent to.
if [[ $proto == tcp ]]; then target=$peer_ip:$port; else target=$rx_ip:$port; fi

pids=()
for (( k = 0; k < peers; k++ )); do
    # After the shared args, so each peer's own id, seed and ack port win.
    QP_RIG_PEER_CPUS=$peer_cpus sudo -n qp-netns-run qp-peer "$bin/netbench_peer" \
        --proto "$proto" --target "$target" \
        --out "$out/peer-$k.json" "$@" --sender "$k" --seed "$((k + 1))" \
        --ack "$peer_ip:$((ack_port + k))" &
    pids+=($!)
done
rx_rc=0
sudo -n qp-netns-run qp-rx "$bin/netbench_$backend" --target "$target" --ack "$peer_ip:$ack_port" \
    --peers "$peers" --stamps "$stamps" --out "$out/receiver.json" || rx_rc=$?
peer_rc=0
for pid in "${pids[@]}"; do wait "$pid" || peer_rc=$?; done

echo "run_pair: $backend stamps=$stamps peers=$peers peer=$peer_rc receiver=$rx_rc, results in $out" >&2
(( rx_rc == 0 && peer_rc == 0 ))
