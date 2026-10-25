#!/bin/bash
# One rig run on the box. The peer in qp-peer, a receiver in qp-rx, joined only by the veth pair.
#
#   tools/netbench/run_pair.sh <kernel_tcp|kernel_udp> [-s none|kernel] [-o dir] [-- peer args...]
#
# Writes peer.json and receiver.json into dir (netbench_out). Peer args pass straight through,
# for example -- --size 512 --rate 10000. Defaults are the peer's own.
set -euo pipefail

usage="usage: run_pair.sh <kernel_tcp|kernel_udp> [-s none|kernel] [-o dir] [-- peer args...]"
backend=${1:?$usage}
shift
stamps=none
out=netbench_out
while getopts s:o: opt; do
    case $opt in
        s) stamps=$OPTARG ;;
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

bin=${QP_BUILD_DIR:-build/release}
peer_ip=10.99.0.1
rx_ip=10.99.0.2
port=${QP_RIG_PORT:-9100}
ack=$peer_ip:${QP_RIG_ACK_PORT:-9101}
# A TCP receiver connects to the peer, a UDP receiver is sent to.
if [[ $proto == tcp ]]; then target=$peer_ip:$port; else target=$rx_ip:$port; fi

mkdir -p "$out"
sudo -n qp-netns-run qp-peer "$bin/netbench_peer" --proto "$proto" --target "$target" \
    --ack "$ack" --out "$out/peer.json" "$@" &
peer=$!
rx_rc=0
sudo -n qp-netns-run qp-rx "$bin/netbench_$backend" --target "$target" --ack "$ack" \
    --stamps "$stamps" --out "$out/receiver.json" || rx_rc=$?
peer_rc=0
wait "$peer" || peer_rc=$?

echo "run_pair: $backend stamps=$stamps peer=$peer_rc receiver=$rx_rc, results in $out" >&2
(( rx_rc == 0 && peer_rc == 0 ))
