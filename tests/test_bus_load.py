from diagnoser.emu.memory_bus import MemoryBus
from diagnoser.tools.bus_load import BusLoadSimulator, frame_bits


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
