from can_test_fakes import QueueBus
from diagnoser.transport.isotp import IsoTpTransport
from diagnoser.transport.shared import SharedCanBus


def test_shared_router_delivers_each_id_only_to_its_endpoint() -> None:
    raw_bus = QueueBus()
    shared = SharedCanBus(raw_bus)
    ecu1 = shared.endpoint(0x7E8)
    ecu2 = shared.endpoint(0x7E9)

    try:
        raw_bus.inject(0x7E9, bytes([0x02, 0x7E, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC]))
        raw_bus.inject(0x7E8, bytes([0x02, 0x7E, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC]))

        message1 = ecu1.recv(timeout=1.0)
        message2 = ecu2.recv(timeout=1.0)

        assert message1 is not None
        assert message2 is not None
        assert message1.arbitration_id == 0x7E8
        assert message2.arbitration_id == 0x7E9
    finally:
        shared.shutdown()


def test_two_isotp_transports_share_one_underlying_bus() -> None:
    raw_bus = QueueBus()
    shared = SharedCanBus(raw_bus)
    transport1 = IsoTpTransport(
        shared.endpoint(0x7E8),
        txid=0x7E0,
        rxid=0x7E8,
        timeout=1.0,
    )
    transport2 = IsoTpTransport(
        shared.endpoint(0x7E9),
        txid=0x7E1,
        rxid=0x7E9,
        timeout=1.0,
    )

    try:
        raw_bus.inject(0x7E8, bytes([0x02, 0x7E, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC]))
        raw_bus.inject(0x7E9, bytes([0x02, 0x7E, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC]))

        assert transport1.receive() == bytes([0x7E, 0x00])
        assert transport2.receive() == bytes([0x7E, 0x00])
        assert raw_bus.shutdown_count == 0
    finally:
        shared.shutdown()

    assert raw_bus.shutdown_count == 1
