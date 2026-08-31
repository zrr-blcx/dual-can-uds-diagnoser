from diagnoser.cli import main


def test_cli_demo_smoke() -> None:
    assert main(["demo"]) == 0
