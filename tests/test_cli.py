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
