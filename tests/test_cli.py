import json

from can_test_fakes import QueueBus
from diagnoser.cli import _network, _open_clients, build_parser, main


def test_cli_demo_smoke() -> None:
    assert main(["demo"]) == 0


def test_cli_bus_off_test_smoke() -> None:
    assert (
        main(
            [
                "--timeout",
                "0.1",
                "bus-off-test",
                "--cycles",
                "1",
                "--seconds",
                "0.01",
            ]
        )
        == 0
    )


def test_cli_stability_smoke(capsys) -> None:
    assert (
        main(
            [
                "stability",
                "--hours",
                "0.0002",
                "--interval-ms",
                "10",
                "--sample-seconds",
                "0.1",
                "--load",
                "0",
            ]
        )
        == 0
    )

    output = capsys.readouterr().out
    assert "# Dual-Node UDS Stability Report" in output
    assert "Software completion: PASS" in output


def test_cli_uds_week5_smoke() -> None:
    assert main(["uds-week5", "--node", "ecu1"]) == 0


def test_cli_dual_node_smoke(capsys) -> None:
    assert main(["dual-node-smoke", "--requests-per-node", "2"]) == 0

    output = capsys.readouterr().out
    assert "Dual-Node Shared-Bus Smoke Test" in output
    assert "PASS" in output


def test_cli_session_conflict(capsys) -> None:
    assert (
        main(
            [
                "session-conflict",
                "--hold-seconds",
                "0.2",
                "--wait-timeout",
                "0.05",
            ]
        )
        == 0
    )

    output = capsys.readouterr().out
    assert "Programming Session Conflict Test" in output
    assert "PASS" in output


def test_cli_security_smoke() -> None:
    assert main(["security", "--node", "ecu1"]) == 0


def test_cli_secure_write_smoke() -> None:
    assert (
        main(
            [
                "--timeout",
                "1.0",
                "write-did",
                "--node",
                "ecu1",
                "--session",
                "0x03",
                "--unlock",
                "0x0100",
                "DEADBEEF",
            ]
        )
        == 0
    )


def test_cli_json_positive_exchange(capsys) -> None:
    assert main(["--json", "read-did", "--node", "ecu1", "0xF190"]) == 0
    lines = capsys.readouterr().out.strip().splitlines()
    payload = json.loads(lines[-1])

    assert payload["title"] == "read DID 0xF190"
    assert payload["response"].startswith("62 F1 90")
    assert payload["elapsed_ms"] >= 0
    assert payload["negative"] is False


def test_cli_json_negative_exchange(capsys) -> None:
    assert main(["--json", "read-did", "--node", "ecu1", "0xFFFF"]) == 1
    lines = capsys.readouterr().out.strip().splitlines()
    payload = json.loads(lines[-1])

    assert payload["negative"] is True
    assert payload["nrc"] == "0x31"
    assert payload["nrc_name"] == "requestOutOfRange"


def test_cli_retries_option_smoke() -> None:
    assert main(["--retries", "0", "session", "--node", "ecu2", "0x03"]) == 0


def test_isotp_timing_options_reach_network_config() -> None:
    args = build_parser().parse_args(
        [
            "--stmin-ms",
            "1",
            "--block-size",
            "4",
            "read-did",
            "0xF190",
        ]
    )

    config = _network(args)

    assert config.stmin_ms == 1
    assert config.block_size == 4


def test_cli_reports_unavailable_can_adapter(capsys) -> None:
    code = main(
        [
            "--interface",
            "slcan",
            "--channel",
            "COM_TEST",
            "trace",
            "--seconds",
            "0.1",
        ]
    )

    captured = capsys.readouterr()
    assert code == 1
    assert "error:" in captured.err
    assert "Traceback" not in captured.err


def test_open_clients_reuses_one_physical_bus() -> None:
    raw_bus = QueueBus()
    factory_calls = []
    args = build_parser().parse_args(
        [
            "--interface",
            "slcan",
            "--channel",
            "COM_TEST",
            "stress",
        ]
    )

    def factory(config):
        factory_calls.append(config)
        return raw_bus

    clients, load_bus, cleanup = _open_clients(args, bus_factory=factory)
    try:
        raw_bus.inject(
            0x7E8,
            bytes([0x02, 0x7E, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC]),
        )
        raw_bus.inject(
            0x7E9,
            bytes([0x02, 0x7E, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC]),
        )

        assert clients["ecu1"].tester_present() == bytes([0x00])
        assert clients["ecu2"].tester_present() == bytes([0x00])
        assert len(factory_calls) == 1
        assert load_bus is not None
    finally:
        cleanup()

    assert raw_bus.shutdown_count == 1
