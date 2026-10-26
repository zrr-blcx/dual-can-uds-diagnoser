from diagnoser.emu.memory_bus import MemoryBus
from diagnoser.tools.bus_load import BusLoadSimulator, frame_bits
from can_test_fakes import QueueBus


def test_frame_bits_for_eight_bytes() -> None:
    assert frame_bits(8) == 111


def test_bus_load_simulator_approaches_target() -> None:
    bus = MemoryBus("load-test", node_id=0x100)
    simulator = BusLoadSimulator(
        bus, arbitration_id=0x100, dlc=8, baudrate=500000, target_load=0.5
    )
    simulator.start()
    import time

    time.sleep(0.4)
    simulator.stop()
    load = simulator.measured_load()
    assert 0.3 <= load <= 0.7
    bus.shutdown()


def test_bus_load_sequence_prefix_is_monotonic() -> None:
    bus = QueueBus()
    simulator = BusLoadSimulator(
        bus,
        arbitration_id=0x100,
        dlc=8,
        baudrate=500000,
        target_load=0.5,
        sequence_prefix=True,
    )
    simulator.start()
    import time

    time.sleep(0.1)
    simulator.stop()

    sequences = [
        int.from_bytes(message.data[:4], "big")
        for message in bus.sent
    ]
    assert len(sequences) >= 3
    assert sequences == list(range(len(sequences)))
    bus.shutdown()


def test_bus_load_skips_missed_deadlines_without_bursting() -> None:
    class SlowBus(QueueBus):
        def send(self, msg, timeout: float | None = None) -> None:
            import time

            del timeout
            time.sleep(0.01)
            self.sent.append(msg)

    bus = SlowBus()
    simulator = BusLoadSimulator(
        bus,
        arbitration_id=0x7F0,
        dlc=8,
        baudrate=500000,
        target_load=0.9,
    )
    try:
        simulator.start()
        import time

        time.sleep(0.12)
        simulator.stop()

        assert len(bus.sent) <= 20
        assert simulator.missed_deadlines > 0
        assert simulator.measured_load() < 0.9
    finally:
        simulator.stop()
        bus.shutdown()
