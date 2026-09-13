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
