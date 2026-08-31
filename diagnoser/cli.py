"""Command-line UDS diagnostic tool V1.0."""

from __future__ import annotations

import argparse
import sys
import time
from collections.abc import Callable
from pathlib import Path

from diagnoser.config import NetworkConfig
from diagnoser.dbc.generator import (
    CanMessage,
    CanNetwork,
    CanSignal,
    DbcGenerator,
    SignalStructGenerator,
)
from diagnoser.emu.memory_bus import MemoryBus
from diagnoser.emu.virtual_ecu import VirtualEcu
from diagnoser.tools.fault_injection import FaultInjector
from diagnoser.tools.report import render_markdown
from diagnoser.tools.scheduler import DualNodeScheduler, SchedulerNode
from diagnoser.tools.stress import run_stress
from diagnoser.transport import create_bus
from diagnoser.transport.isotp import IsoTpTransport
from diagnoser.uds.client import UdsClient
from diagnoser.uds.errors import NegativeResponse, UdsError

PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONFIG = PROJECT_ROOT / "configs" / "network.json"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="diagnoser",
        description="Dual-node CAN + UDS diagnostic tool V1.0",
    )
    parser.add_argument("--config", default=str(DEFAULT_CONFIG), help="network JSON config")
    parser.add_argument("--interface", help="python-can interface (memory, slcan, ...)")
    parser.add_argument("--channel", help="CAN channel or serial device")
    parser.add_argument("--baudrate", type=int, help="CAN bitrate")
    parser.add_argument("--timeout", type=float, help="ISO-TP timeout in seconds")
    parser.add_argument("--padding", type=lambda s: int(s, 0), help="padding byte")

    sub = parser.add_subparsers(dest="command", required=True)

    p_session = sub.add_parser("session", help="0x10 diagnostic session control")
    p_session.add_argument("--node", default="ecu1")
    p_session.add_argument("session", type=lambda s: int(s, 0), help="session id, e.g. 0x03")

    p_read = sub.add_parser("read-did", help="0x22 read data by identifier")
    p_read.add_argument("--node", default="ecu1")
    p_read.add_argument("did", type=lambda s: int(s, 0), help="DID, e.g. 0xF190")

    p_write = sub.add_parser("write-did", help="0x2E write data by identifier")
    p_write.add_argument("--node", default="ecu1")
    p_write.add_argument("did", type=lambda s: int(s, 0), help="DID, e.g. 0xF191")
    p_write.add_argument("data", help="hex bytes, e.g. 010203")

    p_routine = sub.add_parser("routine", help="0x31 routine control")
    p_routine.add_argument("--node", default="ecu1")
    p_routine.add_argument("--subfunction", type=lambda s: int(s, 0), default=0x01)
    p_routine.add_argument("routine_id", type=lambda s: int(s, 0), help="routine id, e.g. 0x0203")
    p_routine.add_argument("--data", default="", help="optional hex data")

    p_raw = sub.add_parser("raw", help="send a raw UDS request")
    p_raw.add_argument("--node", default="ecu1")
    p_raw.add_argument("data", help="hex bytes, e.g. 1003")

    p_dbc = sub.add_parser("dbc-generate", help="generate a DBC file from message definitions")
    p_dbc.add_argument("--output", default=str(PROJECT_ROOT / "outputs" / "project.dbc"))

    p_structs = sub.add_parser("structs", help="generate signal structs from message definitions")
    p_structs.add_argument("--language", choices=["python", "c"], default="python")
    p_structs.add_argument("--output", default=str(PROJECT_ROOT / "outputs" / "generated_structs.py"))

    p_demo = sub.add_parser("demo", help="run the software virtual dual-node demo")

    p_stress = sub.add_parser("stress", help="run dual-node diagnostic stress test")
    p_stress.add_argument("--frames", type=int, default=1000, help="total diagnostic requests")
    p_stress.add_argument("--load", type=float, default=0.9, help="target bus load, e.g. 0.9")
    p_stress.add_argument("--did", type=lambda s: int(s, 0), default=0xF190)
    p_stress.add_argument("--report", help="optional markdown report output path")

    p_fault = sub.add_parser("fault", help="inject faults into a virtual or real ECU")
    p_fault.add_argument("--node", default="ecu1")
    p_fault.add_argument("type", choices=["dtc", "comm-loss", "bus-off", "nrc", "clear"])
    p_fault.add_argument("--dtc", type=lambda s: int(s, 0), default=0x010203)
    p_fault.add_argument("--status", type=lambda s: int(s, 0), default=0x2A)
    p_fault.add_argument("--seconds", type=float, default=2.0)
    p_fault.add_argument("--service", type=lambda s: int(s, 0), default=0x22)
    p_fault.add_argument("--nrc", type=lambda s: int(s, 0), default=0x78)

    return parser


def _network(args: argparse.Namespace) -> NetworkConfig:
    config = NetworkConfig.load(args.config)
    if args.interface:
        config.interface = args.interface
    if args.channel:
        config.channel = args.channel
    if args.baudrate:
        config.baudrate = args.baudrate
    if args.timeout:
        config.timeout = args.timeout
    if args.padding is not None:
        config.padding = args.padding
    return config


def _hex_bytes(value: str) -> bytes:
    value = value.replace(" ", "").replace("0x", "")
    if len(value) % 2:
        raise ValueError("hex data must contain an even number of digits")
    return bytes.fromhex(value)


def _open_client(
    args: argparse.Namespace, node: str
) -> tuple[UdsClient, Callable[[], None]]:
    config = _network(args)
    ecu = config.get_ecu(node)
    if config.interface == "memory":
        ecu_bus = MemoryBus(config.channel, node_id=ecu.request_id, bitrate=config.baudrate)
        tester_bus = MemoryBus(
            config.channel, node_id=ecu.response_id, bitrate=config.baudrate
        )
        virtual_ecu = VirtualEcu(node, ecu_bus, ecu.request_id, ecu.response_id)
        virtual_ecu.start()
        transport = IsoTpTransport(
            tester_bus,
            txid=ecu.request_id,
            rxid=ecu.response_id,
            timeout=config.timeout,
            padding=config.padding,
        )
        client = UdsClient(transport, timeout=config.timeout)

        def cleanup() -> None:
            virtual_ecu.stop()
            ecu_bus.shutdown()
            tester_bus.shutdown()

        return client, cleanup

    bus = create_bus(config)
    transport = IsoTpTransport(
        bus,
        txid=ecu.request_id,
        rxid=ecu.response_id,
        timeout=config.timeout,
        padding=config.padding,
    )
    return UdsClient(transport, timeout=config.timeout), bus.shutdown


def _print_response(title: str, response: bytes) -> None:
    print(f"{title}: {response.hex(' ').upper()}")


def cmd_session(args: argparse.Namespace) -> int:
    client, cleanup = _open_client(args, args.node)
    try:
        response = client.change_session(args.session)
        _print_response("session control", response)
        return 0
    finally:
        cleanup()


def cmd_read_did(args: argparse.Namespace) -> int:
    client, cleanup = _open_client(args, args.node)
    try:
        response = client.read_data_by_identifier(args.did)
        _print_response(f"read DID 0x{args.did:04X}", response)
        return 0
    finally:
        cleanup()


def cmd_write_did(args: argparse.Namespace) -> int:
    client, cleanup = _open_client(args, args.node)
    try:
        response = client.write_data_by_identifier(args.did, _hex_bytes(args.data))
        _print_response(f"write DID 0x{args.did:04X}", response)
        return 0
    finally:
        cleanup()


def cmd_routine(args: argparse.Namespace) -> int:
    client, cleanup = _open_client(args, args.node)
    try:
        response = client.routine_control(
            args.subfunction, args.routine_id, _hex_bytes(args.data)
        )
        _print_response(f"routine 0x{args.routine_id:04X}", response)
        return 0
    finally:
        cleanup()


def cmd_raw(args: argparse.Namespace) -> int:
    client, cleanup = _open_client(args, args.node)
    try:
        response = client.send_raw(_hex_bytes(args.data))
        _print_response("raw response", response)
        return 0
    finally:
        cleanup()


def _demo_network() -> CanNetwork:
    return CanNetwork(
        nodes=["ECU_A", "ECU_B"],
        messages=[
            CanMessage(
                arbitration_id=0x180,
                name="EngineData",
                dlc=8,
                sender="ECU_A",
                signals=[
                    CanSignal("EngineSpeed", 0, 16, "big_endian", 0.25, 0.0, 0.0, 16383.75, "rpm"),
                    CanSignal("CoolantTemp", 16, 8, "big_endian", 1.0, -40.0, -40.0, 215.0, "degC"),
                ],
            ),
            CanMessage(
                arbitration_id=0x280,
                name="VehicleStatus",
                dlc=8,
                sender="ECU_B",
                signals=[
                    CanSignal("VehicleSpeed", 0, 16, "little_endian", 0.01, 0.0, 0.0, 655.35, "km/h"),
                    CanSignal("BrakeActive", 16, 1, "little_endian", 1.0, 0.0, 0.0, 1.0, ""),
                ],
            ),
        ],
    )


def cmd_dbc_generate(args: argparse.Namespace) -> int:
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(DbcGenerator.generate(_demo_network()), encoding="utf-8")
    print(f"DBC written to {output}")
    return 0


def cmd_structs(args: argparse.Namespace) -> int:
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    if args.language == "python":
        code = SignalStructGenerator.generate_python(_demo_network())
    else:
        code = SignalStructGenerator.generate_c(_demo_network())
    output.write_text(code, encoding="utf-8")
    print(f"Structs written to {output}")
    return 0


def cmd_demo(args: argparse.Namespace) -> int:
    config = _network(args)
    scheduler = DualNodeScheduler(
        [SchedulerNode(name=ecu.name) for ecu in config.ecus]
    )
    clients: dict[str, tuple[UdsClient, Callable[[], None]]] = {}
    try:
        for ecu in config.ecus:
            clients[ecu.name] = _open_client(args, ecu.name)
        for node, (client, _) in clients.items():
            scheduler.enter_session(node, 0x03)
            response = scheduler.run(node, lambda: client.change_session(0x03))
            _print_response(f"{node} session", response)
            response = scheduler.run(node, lambda: client.read_data_by_identifier(0xF19A))
            _print_response(f"{node} ECU name", response)
            response = scheduler.run(
                node,
                lambda: client.write_data_by_identifier(0xF191, bytes([0x12, 0x34, 0x56, 0x78])),
            )
            _print_response(f"{node} write DTC status", response)
            response = scheduler.run(
                node, lambda: client.routine_control(0x01, 0x0203)
            )
            _print_response(f"{node} self test", response)
        try:
            clients["ecu1"][0].read_data_by_identifier(0xFFFF)
        except NegativeResponse as exc:
            print(f"negative response check: {exc}")
        print("virtual dual-node demo passed")
        return 0
    finally:
        for _, cleanup in clients.values():
            cleanup()


def cmd_stress(args: argparse.Namespace) -> int:
    config = _network(args)
    load_bus = MemoryBus(config.channel, bitrate=config.baudrate)
    clients: dict[str, tuple[UdsClient, Callable[[], None]]] = {}
    try:
        for ecu in config.ecus:
            clients[ecu.name] = _open_client(args, ecu.name)
        result = run_stress(
            load_bus,
            {name: client for name, (client, _) in clients.items()},
            did=args.did,
            requests=args.frames,
            target_load=args.load,
            baudrate=config.baudrate,
        )
        report = render_markdown(result)
        print(report)
        if args.report:
            Path(args.report).write_text(report, encoding="utf-8")
            print(f"report written to {args.report}")
        return 0
    finally:
        for _, cleanup in clients.values():
            cleanup()
        load_bus.shutdown()


def cmd_fault(args: argparse.Namespace) -> int:
    client, cleanup = _open_client(args, args.node)
    injector = FaultInjector({args.node: client})
    try:
        if args.type == "dtc":
            response = injector.inject_dtc(args.node, args.dtc, args.status)
            _print_response("DTC injected", response)
            _print_response("DTC status", client.read_data_by_identifier(0xF191))
        elif args.type == "comm-loss":
            response = injector.simulate_comm_loss(args.node, args.seconds)
            _print_response("comm loss armed", response)
            try:
                client.tester_present()
            except TimeoutError:
                print(f"{args.node} is offline as expected")
            time.sleep(args.seconds + 0.3)
            _print_response("recovery check", client.tester_present())
        elif args.type == "bus-off":
            response = injector.trigger_bus_off(args.node, args.seconds)
            _print_response("bus-off injected", response)
            time.sleep(args.seconds + 0.3)
            _print_response("bus-off recovery check", client.tester_present())
        elif args.type == "nrc":
            response = injector.inject_nrc(args.node, args.service, args.nrc)
            _print_response("NRC injection armed", response)
            try:
                client.read_data_by_identifier(0xF19A)
            except NegativeResponse as exc:
                print(f"negative response observed: {exc}")
        else:
            _print_response("faults cleared", injector.clear_faults(args.node))
        return 0
    finally:
        cleanup()


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    handlers: dict[str, Callable[[argparse.Namespace], int]] = {
        "session": cmd_session,
        "read-did": cmd_read_did,
        "write-did": cmd_write_did,
        "routine": cmd_routine,
        "raw": cmd_raw,
        "dbc-generate": cmd_dbc_generate,
        "structs": cmd_structs,
        "demo": cmd_demo,
        "stress": cmd_stress,
        "fault": cmd_fault,
    }
    handler = handlers.get(args.command)
    if handler is None:
        parser.print_help()
        return 1
    try:
        return handler(args)
    except (TimeoutError, UdsError, ValueError, KeyError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
