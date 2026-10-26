#!/bin/bash
# One rig run on the box. The peer in qp-peer, a receiver in qp-rx, joined only by the veth pair.
#
#   tools/netbench/run_pair.sh <kernel_tcp|kernel_udp> [-s none|kernel] [-n peers] [-o dir]
#       [-- peer args...]
#
# Writes peer-<k>.json per peer and receiver.json into dir (netbench_out). Peer args pass to
# every peer, for example -- --sizes 512 --rate 10000. Several peers need udp, each gets its own
# id, seed and ack port.
set -euo pipefail

usage="usage: run_pair.sh <kernel_tcp|kernel_udp> [-s none|kernel] [-n peers] [-o dir] [-- peer args...]"
backend=${1:?$usage}
shift
stamps=none
peers=1
out=netbench_out
while getopts s:n:o: opt; do
    case $opt in
        s) stamps=$OPTARG ;;
        n) peers=$OPTARG ;;
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
[[ $peers == 1 || $proto == udp ]] || { echo "run_pair: several peers need udp" >&2; exit 2; }

bin=${QP_BUILD_DIR:-build/release}
peer_ip=10.99.0.1
rx_ip=10.99.0.2
port=${QP_RIG_PORT:-9100}
ack_port=${QP_RIG_ACK_PORT:-9101}
# A TCP receiver connects to the peer, a UDP receiver is sent to.
if [[ $proto == tcp ]]; then target=$peer_ip:$port; else target=$rx_ip:$port; fi

mkdir -p "$out"
pids=()
for (( k = 0; k < peers; k++ )); do
    # After the shared args, so each peer's own id, seed and ack port win.
    sudo -n qp-netns-run qp-peer "$bin/netbench_peer" --proto "$proto" --target "$target" \
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
