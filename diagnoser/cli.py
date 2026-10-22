"""Command-line UDS diagnostic tool V1.0."""

from __future__ import annotations

import argparse
import json
import sys
import time
from collections.abc import Callable
from pathlib import Path

from can import CanError

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
from diagnoser.tools.bus_off import render_bus_off_report, run_bus_off_test
from diagnoser.tools.console import exchange_to_dict, format_exchange
from diagnoser.tools.fault_injection import FaultInjector
from diagnoser.tools.report import render_markdown
from diagnoser.tools.scheduler import DualNodeScheduler, SchedulerNode
from diagnoser.tools.stress import run_stress
from diagnoser.tools.trace import DEFAULT_TRACE_ID, render_trace, run_trace
from diagnoser.tools.uds_smoke import render_uds_smoke, run_uds_smoke
from diagnoser.tools.uds_week5 import render_uds_week5, run_uds_week5
from diagnoser.transport import create_bus
from diagnoser.transport.isotp import IsoTpTransport
from diagnoser.uds.client import UdsClient
from diagnoser.uds.dids import DID_DTC_STATUS, DID_ECU_NAME
from diagnoser.uds.errors import NegativeResponse, UdsError
from diagnoser.uds.security import DEFAULT_SECURITY_SALT, seed_to_key

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
    parser.add_argument("--p2", type=float, help="P2server timeout in ms (default 50)")
    parser.add_argument(
        "--p2-star", dest="p2_star", type=float, help="P2*server timeout in ms (default 5000)"
    )
    parser.add_argument("--retries", type=int, help="response timeout retries (default 1)")
    parser.add_argument(
        "--json",
        action="store_true",
        help="print each exchange as one JSON object",
    )

    sub = parser.add_subparsers(dest="command", required=True)

    p_session = sub.add_parser("session", help="0x10 diagnostic session control")
    p_session.add_argument("--node", default="ecu1")
    p_session.add_argument("session", type=lambda s: int(s, 0), help="session id, e.g. 0x03")

    p_read = sub.add_parser("read-did", help="0x22 read data by identifier")
    p_read.add_argument("--node", default="ecu1")
    p_read.add_argument("did", type=lambda s: int(s, 0), help="DID, e.g. 0xF190")

    p_write = sub.add_parser("write-did", help="0x2E write data by identifier")
    p_write.add_argument("--node", default="ecu1")
    p_write.add_argument(
        "--session", type=lambda s: int(s, 0), help="optional session switch first"
    )
    p_write.add_argument(
        "--unlock", action="store_true", help="run 0x27 seed/key before writing"
    )
    p_write.add_argument("did", type=lambda s: int(s, 0), help="DID, e.g. 0xF191")
    p_write.add_argument("data", help="hex bytes, e.g. 010203")

    p_routine = sub.add_parser("routine", help="0x31 routine control")
    p_routine.add_argument("--node", default="ecu1")
    p_routine.add_argument("--subfunction", type=lambda s: int(s, 0), default=0x01)
    p_routine.add_argument(
        "--session", type=lambda s: int(s, 0), help="optional session switch first"
    )
    p_routine.add_argument(
        "--unlock", action="store_true", help="run 0x27 seed/key before the routine"
    )
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

    sub.add_parser("demo", help="run the software virtual dual-node demo")

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

    p_bus_off = sub.add_parser(
        "bus-off-test",
        help="inject repeated Bus-Off events into a virtual ECU and verify recovery",
    )
    p_bus_off.add_argument("--node", default="ecu1")
    p_bus_off.add_argument("--cycles", type=int, default=3)
    p_bus_off.add_argument("--seconds", type=float, default=0.2)
    p_bus_off.add_argument("--recovery-timeout", type=float, default=3.0)
    p_bus_off.add_argument("--report", help="optional markdown report output path")

    p_uds_smoke = sub.add_parser(
        "uds-smoke",
        help="run the Week 4 single-node 0x10/0x22 smoke workflow",
    )
    p_uds_smoke.add_argument("--node", default="ecu1")
    p_uds_smoke.add_argument("--report", help="optional markdown report output path")

    p_security = sub.add_parser("security", help="run the 0x27 seed/key exchange")
    p_security.add_argument("--node", default="ecu1")
    p_security.add_argument("--level", type=lambda s: int(s, 0), default=0x01)

    p_uds_week5 = sub.add_parser(
        "uds-week5",
        help="run the Week 5 0x27/0x2E/0x31 workflow",
    )
    p_uds_week5.add_argument("--node", default="ecu1")
    p_uds_week5.add_argument("--report", help="optional markdown report output path")

    p_trace = sub.add_parser(
        "trace",
        help="trace raw CAN frames and report per-ID inter-frame gaps",
    )
    p_trace.add_argument(
        "--id",
        type=lambda s: int(s, 0),
        default=DEFAULT_TRACE_ID,
        help="arbitration ID featured in the gap summary (default 0x123)",
    )
    p_trace.add_argument("--seconds", type=float, default=5.0, help="capture window")
    p_trace.add_argument("--max-frames", type=int, help="stop after this many frames")
    p_trace.add_argument("--quiet", action="store_true", help="print only the summary")
    p_trace.add_argument("--report", help="optional markdown report output path")

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
    if args.p2 is not None:
        config.p2_ms = args.p2
    if args.p2_star is not None:
        config.p2_star_ms = args.p2_star
    if args.retries is not None:
        config.retries = args.retries
    return config


def _hex_bytes(value: str) -> bytes:
    value = value.replace(" ", "").replace("0x", "")
    if len(value) % 2:
        raise ValueError("hex data must contain an even number of digits")
    return bytes.fromhex(value)


def _make_client(config: NetworkConfig, transport: IsoTpTransport) -> UdsClient:
    return UdsClient(
        transport,
        timeout=config.timeout,
        p2_ms=config.p2_ms,
        p2_star_ms=config.p2_star_ms,
        retries=config.retries,
    )


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
        client = _make_client(config, transport)

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
    return _make_client(config, transport), bus.shutdown


def _emit(args: argparse.Namespace, title: str, client: UdsClient) -> None:
    info = client.last_exchange
    if info is None:
        print(f"{title}: (no exchange recorded)")
        return
    if args.json:
        print(json.dumps(exchange_to_dict(info, title), ensure_ascii=False))
    else:
        print(f"{title}: {format_exchange(info)}")


def cmd_session(args: argparse.Namespace) -> int:
    client, cleanup = _open_client(args, args.node)
    try:
        client.change_session(args.session)
        _emit(args, "session control", client)
        return 0
    finally:
        cleanup()


def cmd_read_did(args: argparse.Namespace) -> int:
    client, cleanup = _open_client(args, args.node)
    try:
        client.read_data_by_identifier(args.did)
        _emit(args, f"read DID 0x{args.did:04X}", client)
        return 0
    finally:
        cleanup()


def cmd_write_did(args: argparse.Namespace) -> int:
    client, cleanup = _open_client(args, args.node)
    try:
        if args.session is not None:
            client.change_session(args.session)
            _emit(args, "session control", client)
        if args.unlock:
            client.unlock()
            _emit(args, "security unlock", client)
        client.write_data_by_identifier(args.did, _hex_bytes(args.data))
        _emit(args, f"write DID 0x{args.did:04X}", client)
        return 0
    finally:
        cleanup()


def cmd_routine(args: argparse.Namespace) -> int:
    client, cleanup = _open_client(args, args.node)
    try:
        if args.session is not None:
            client.change_session(args.session)
            _emit(args, "session control", client)
        if args.unlock:
            client.unlock()
            _emit(args, "security unlock", client)
        client.routine_control(args.subfunction, args.routine_id, _hex_bytes(args.data))
        _emit(args, f"routine 0x{args.routine_id:04X}", client)
        return 0
    finally:
        cleanup()


def cmd_raw(args: argparse.Namespace) -> int:
    client, cleanup = _open_client(args, args.node)
    try:
        client.send_raw(_hex_bytes(args.data))
        _emit(args, "raw response", client)
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
            scheduler.run(node, lambda: client.change_session(0x03))
            _emit(args, f"{node} session", client)
            scheduler.run(node, lambda: client.read_data_by_identifier(DID_ECU_NAME))
            _emit(args, f"{node} ECU name", client)
            scheduler.run(node, client.unlock)
            _emit(args, f"{node} security unlock", client)
            scheduler.run(
                node,
                lambda: client.write_data_by_identifier(
                    DID_DTC_STATUS,
                    bytes([0x12, 0x34, 0x56, 0x78]),
                ),
            )
            _emit(args, f"{node} write DTC status", client)
            scheduler.run(node, lambda: client.routine_control(0x01, 0x0203))
            _emit(args, f"{node} self test", client)
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
            injector.inject_dtc(args.node, args.dtc, args.status)
            _emit(args, "DTC injected", client)
            client.read_data_by_identifier(DID_DTC_STATUS)
            _emit(args, "DTC status", client)
        elif args.type == "comm-loss":
            injector.simulate_comm_loss(args.node, args.seconds)
            _emit(args, "comm loss armed", client)
            try:
                client.tester_present()
            except TimeoutError:
                print(f"{args.node} is offline as expected")
            time.sleep(args.seconds + 0.3)
            client.tester_present()
            _emit(args, "recovery check", client)
        elif args.type == "bus-off":
            injector.trigger_bus_off(args.node, args.seconds)
            _emit(args, "bus-off injected", client)
            time.sleep(args.seconds + 0.3)
            client.tester_present()
            _emit(args, "bus-off recovery check", client)
        elif args.type == "nrc":
            injector.inject_nrc(args.node, args.service, args.nrc)
            _emit(args, "NRC injection armed", client)
            try:
                client.read_data_by_identifier(DID_ECU_NAME)
            except NegativeResponse as exc:
                print(f"negative response observed: {exc}")
        else:
            injector.clear_faults(args.node)
            _emit(args, "faults cleared", client)
        return 0
    finally:
        cleanup()


def cmd_bus_off_test(args: argparse.Namespace) -> int:
    client, cleanup = _open_client(args, args.node)
    injector = FaultInjector({args.node: client})
    try:
        result = run_bus_off_test(
            client,
            injector,
            args.node,
            cycles=args.cycles,
            seconds=args.seconds,
            recovery_timeout_s=args.recovery_timeout,
        )
        report = render_bus_off_report(result)
        print(report)
        if args.report:
            output = Path(args.report)
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(report, encoding="utf-8")
            print(f"report written to {output}")
        return 0 if result.passed else 1
    finally:
        cleanup()


def cmd_uds_smoke(args: argparse.Namespace) -> int:
    client, cleanup = _open_client(args, args.node)
    try:
        result = run_uds_smoke(client, args.node)
        report = render_uds_smoke(result)
        print(report)
        if args.report:
            output = Path(args.report)
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(report, encoding="utf-8")
            print(f"report written to {output}")
        return 0 if result.passed else 1
    finally:
        cleanup()


def cmd_security(args: argparse.Namespace) -> int:
    client, cleanup = _open_client(args, args.node)
    try:
        client.change_session(0x03)
        _emit(args, "session control", client)
        seed = client.request_seed(args.level)
        _emit(args, "request seed", client)
        if seed == bytes(4):
            print("ECU already unlocked")
            return 0
        key = seed_to_key(int.from_bytes(seed, "big"), DEFAULT_SECURITY_SALT)
        client.send_key(key.to_bytes(4, "big"), args.level)
        _emit(args, "send key", client)
        return 0
    finally:
        cleanup()


def cmd_uds_week5(args: argparse.Namespace) -> int:
    client, cleanup = _open_client(args, args.node)
    try:
        result = run_uds_week5(client, args.node)
        report = render_uds_week5(result)
        print(report)
        if args.report:
            output = Path(args.report)
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(report, encoding="utf-8")
            print(f"report written to {output}")
        return 0 if result.passed else 1
    finally:
        cleanup()


def cmd_trace(args: argparse.Namespace) -> int:
    config = _network(args)
    if config.interface == "memory":
        # The memory bus routes by arbitration ID, so the tracer listens on the
        # focus ID and only sees frames sent to it.
        bus = MemoryBus(config.channel, node_id=args.id, bitrate=config.baudrate)
    else:
        bus = create_bus(config)

    try:
        print(
            f"tracing {config.interface}:{config.channel} for {args.seconds:.1f} s "
            f"(focus ID 0x{args.id:X})"
        )
        result = run_trace(bus, seconds=args.seconds, max_frames=args.max_frames)
    finally:
        bus.shutdown()

    if args.json:
        for frame in result.frames:
            print(json.dumps(frame.as_dict(), ensure_ascii=False))
    elif not args.quiet:
        for frame in result.frames:
            print(frame.format_line())

    if not args.json:
        report = render_trace(result, focus_id=args.id)
        print(report)
        if args.report:
            output = Path(args.report)
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(report, encoding="utf-8")
            print(f"report written to {output}")
    return 0


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
        "bus-off-test": cmd_bus_off_test,
        "uds-smoke": cmd_uds_smoke,
        "security": cmd_security,
        "uds-week5": cmd_uds_week5,
        "trace": cmd_trace,
    }
    handler = handlers.get(args.command)
    if handler is None:
        parser.print_help()
        return 1
    try:
        return handler(args)
    except NegativeResponse as exc:
        info = getattr(exc, "exchange", None)
        if args.json and info is not None:
            print(json.dumps(exchange_to_dict(info, "negative response"), ensure_ascii=False))
        elif info is not None:
            print(f"negative response: {format_exchange(info)}", file=sys.stderr)
        else:
            print(f"error: {exc}", file=sys.stderr)
        return 1
    except (TimeoutError, UdsError, ValueError, KeyError, OSError, CanError) as exc:
        if args.json:
            print(
                json.dumps(
                    {"error": str(exc), "type": type(exc).__name__},
                    ensure_ascii=False,
                )
            )
        else:
            print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
