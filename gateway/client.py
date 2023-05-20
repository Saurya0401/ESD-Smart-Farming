import paho.mqtt.client as mqtt

import json
from time import sleep
from random import randrange, uniform
from typing import Final

from src.receiver import Receiver


class Gateway(mqtt.Client):
    THINGSBOARD_HOST: str = 'demo.thingsboard.io'
    THINGSBOARD_PORT: int = 1883
    ACCESS_TOKEN: str = 'td4W5nqvPdYzIYpiYOrH'
    DELAY_SECONDS: int = 5
    DEFAULT_QOS: int = 1

    KEY_TEMP: Final[str] = 'temperature'
    KEY_LIGHT: Final[str] = 'light_level'
    KEY_WATER: Final[str] = 'water_level'

    def __init__(self) -> None:
        super().__init__()
        self.sensor_data: dict[str, float] = {
            Gateway.KEY_TEMP: 0.0,
            Gateway.KEY_LIGHT: 0,
            Gateway.KEY_WATER: 0,
        }
        self.username_pw_set(Gateway.ACCESS_TOKEN)
        self.connect(Gateway.THINGSBOARD_HOST, Gateway.THINGSBOARD_PORT, 60)
        self.receiver = Receiver()

    def get_data(self) -> None:
        if not self.receiver.receive():
            raise ValueError
        # TODO: parse received data
        self.sensor_data[Gateway.KEY_TEMP] = round(uniform(20.0, 30.0), 4)
        self.sensor_data[Gateway.KEY_LIGHT] = randrange(1000, 4001)
        self.sensor_data[Gateway.KEY_WATER] = randrange(0, 11)

    def publish_sensor_data(self) -> None:
        self.loop_start()
        try:
            while True:
                try:
                    self.get_data()
                except ValueError:
                    print('No data received from Arduino')
                    continue
                self.publish(
                    'v1/devices/me/telemetry', 
                    json.dumps(self.sensor_data), 
                    Gateway.DEFAULT_QOS
                )
                print('Published:')
                for key, val in self.sensor_data.items():
                    print(f'\t->{key} = {val}')
                sleep(Gateway.DELAY_SECONDS)
        except KeyboardInterrupt:
            print('Sensor data publish stopped')
            self.receiver.radio.powerDown()
            return
        

def main():
    client: Gateway = Gateway()
    client.publish_sensor_data()


if __name__ == '__main__':
    main()
