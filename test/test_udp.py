"""
UDP / WiFi transport tests — requires 2 nodes, both with WiFi configured on the
same network (add a `wifi:` block per node in nodes.yaml). Delivery over WiFi/UDP
is much faster than LoRa and needs no frequency-band handling.

Note: nodes discover each other as UDP peers via the announce exchange that the
session fixture triggers. If these skip/fail, verify both nodes are on the same
WiFi and reachable (check `peers` shows the other node on port 1).
"""

import time
import pytest

UDP_TIMEOUT = 10.0   # WiFi/UDP is fast; generous margin for reconnect/announce


def _both_have_wifi(config) -> bool:
    nodes = config.get("nodes", [])
    return len(nodes) >= 2 and all(n.get("wifi") for n in nodes[:2])


@pytest.mark.min_nodes(2)
class TestUdpMessaging:

    def test_udp_direct_message(self, node_a, node_b, config):
        """A -> B message is delivered when both are on WiFi."""
        if not _both_have_wifi(config):
            pytest.skip("Both nodes need a `wifi:` config for UDP transport tests")

        node_b.drain_events()
        time.sleep(1.0)
        dst = config["nodes"][1]["call"]
        node_a.send_message(dst, "udp hello")

        rx = node_b.wait_for_event("rx", timeout=UDP_TIMEOUT,
                                   frameType=3, text="udp hello")
        assert rx is not None, "B did not receive the message over WiFi/UDP"

    def test_udp_transport_used(self, node_a, node_b, config):
        """When a WiFi peer is known, delivery uses port 1 (WiFi/UDP), not LoRa."""
        if not _both_have_wifi(config):
            pytest.skip("Both nodes need a `wifi:` config for UDP transport tests")

        node_b.drain_events()
        time.sleep(1.0)
        dst = config["nodes"][1]["call"]
        node_a.send_message(dst, "udp port check")

        rx = node_b.wait_for_event("rx", timeout=UDP_TIMEOUT,
                                   frameType=3, text="udp port check")
        assert rx is not None, "message not received"
        # port 1 == WiFi/UDP. (LoRa is port 0.) Informational if the field is present.
        if "port" in rx:
            assert rx["port"] == 1, f"expected WiFi/UDP (port 1), got port {rx['port']}"

    def test_udp_ack(self, node_a, node_b, config):
        """Sender receives an ACK back over WiFi/UDP."""
        if not _both_have_wifi(config):
            pytest.skip("Both nodes need a `wifi:` config for UDP transport tests")

        node_a.drain_events()
        time.sleep(1.0)
        dst = config["nodes"][1]["call"]
        node_a.send_message(dst, "udp ack test")

        ack = node_a.wait_for_event("ack", timeout=UDP_TIMEOUT)
        assert ack is not None, "A did not receive an ACK over WiFi/UDP"
