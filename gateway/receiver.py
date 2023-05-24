from RF24 import RF24, RF24_PA_LOW

from time import monotonic, sleep
from typing import Optional, Final


class Receiver:
    CSN_PIN: Final[int] = 0  # connected to GPIO8
    CE_PIN: Final[int] = 22  # connected to GPIO22
    P_SIZE: Final[int] = 32  # default payload size
    READ_ADDR: Final[bytes] = 0xF1F1F0F0E0

    def __init__(self) -> None:
        self.radio = RF24(Receiver.CE_PIN, Receiver.CSN_PIN)
        if not self.radio.begin():
            raise RuntimeError('NRF24 radio hardware not responding')
        self.radio.setPALevel(RF24_PA_LOW)
        self.radio.openReadingPipe(1, Receiver.READ_ADDR)
        self.radio.payloadSize = Receiver.P_SIZE
        self.radio.printPrettyDetails()
        self.radio.startListening()

    def receive(self, timeout: int = 5) -> Optional[bytes]:
        self.radio.startListening()
        start_timer: float = monotonic()
        while (monotonic() - start_timer) < timeout:
            if self.radio.available():
                recv_data: bytes = self.radio.read(self.radio.payloadSize)
                print(f'received: {recv_data}, len = {len(recv_data)}')
                return recv_data
        print('Receiver timed out with no data received')
        return None
    
    def stop(self) -> None:
        self.radio.stopListening()
        self.radio.powerDown()
    

if __name__ == '__main__':
    pass
