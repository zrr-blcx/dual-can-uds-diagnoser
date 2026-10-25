import json

from diagnoser.cli import main


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


def test_cli_uds_week5_smoke() -> None:
    assert main(["uds-week5", "--node", "ecu1"]) == 0


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
