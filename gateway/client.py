import paho.mqtt.client as mqtt

import json
from time import sleep
from typing import Final

from gateway.receiver import Receiver


class Client(mqtt.Client):
    THINGSBOARD_HOST: Final[str] = 'demo.thingsboard.io'
    THINGSBOARD_PORT: Final[int] = 1883
    DELAY_SECONDS: Final[int] = 5
    DEFAULT_QOS: Final[int] = 1

    KEY_TEMP: Final[str] = 'temperature'
    KEY_LIGHT: Final[str] = 'light_level'
    KEY_WATER: Final[str] = 'water_level'

    def __init__(self) -> None:
        super().__init__()
        self.sensor_data: dict[str, float] = {
            Client.KEY_TEMP: 0.0,
            Client.KEY_LIGHT: 0,
            Client.KEY_WATER: 0,
        }
        with open('gateway/token') as f:
            access_token = f.read().strip('\n')
        self.username_pw_set(access_token)
        self.connect(Client.THINGSBOARD_HOST, Client.THINGSBOARD_PORT, 60)
        self.receiver: Receiver = Receiver()

    def get_data(self) -> None:
        if not (recv_data_raw := self.receiver.receive()):
            raise ValueError
        recv_data: str = recv_data_raw.decode('utf-8').rstrip('\0')
        values: list[float] = [float(d) for d in recv_data.split(';')]
        if len(values) < 3:
            raise ValueError
        self.sensor_data[Client.KEY_TEMP] = values[0]
        self.sensor_data[Client.KEY_LIGHT] = values[1]
        self.sensor_data[Client.KEY_WATER] = values[2]

    def publish_sensor_data(self) -> None:
        self.loop_start()
        try:
            while True:
                try:
                    self.get_data()
                except (ValueError, UnicodeDecodeError):
                    print('No data or invalid data received from Arduino')
                else:
                    self.publish(
                        'v1/devices/me/telemetry', 
                        json.dumps(self.sensor_data), 
                        Client.DEFAULT_QOS
                    )
                    print('Published:')
                    for key, val in self.sensor_data.items():
                        print(f'\t->{key} = {val}')
                finally:
                    sleep(Client.DELAY_SECONDS)
        except KeyboardInterrupt:
            print('Sensor data publish stopped')
            self.receiver.stop()
            self.disconnect()
            return
        

def main():
    client: Client = Client()
    client.publish_sensor_data()


if __name__ == '__main__':
    main()
