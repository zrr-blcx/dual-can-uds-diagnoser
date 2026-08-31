"""UDS client exceptions."""

from diagnoser.uds.nrc import nrc_name


class UdsError(Exception):
    pass


class NegativeResponse(UdsError):
    def __init__(self, service: int, nrc: int) -> None:
        self.service = service
        self.nrc = nrc
        super().__init__(
            f"NRC 0x{nrc:02X} ({nrc_name(nrc)}) for service 0x{service:02X}"
        )


class UnexpectedResponse(UdsError):
    pass
