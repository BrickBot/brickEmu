"""BrickEmu server tests"""

from unittest import mock
from emu_server import (EmuServer, find_free_port, SENSOR_2, MOTOR_A_REV, MOTOR_A_FWD, SENSOR_2_ACTIVE, MOTOR_A, DIR_FWD,
                        DIR_REV)
import socket
import pytest
import time

emu_obj = EmuServer("localhost", find_free_port())


class TestBrickEmuServer:

    @mock.patch("emu_server.socket.socket.send", mock.Mock())
    def test_set_sensor_value(self):
        emu_obj.conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        emu_obj.set_sensor_value(5, 1023)
        emu_obj.conn.send.assert_called_with(b"A53ff\r\n")

    @mock.patch("emu_server.socket.socket.send", mock.Mock())
    def test_init_sensors(self):
        expected_calls = (mock.call(b"A2000\r\n"),
                          mock.call(b"A1000\r\n"),
                          mock.call(b"A0000\r\n"),
                          mock.call(b"A3000\r\n"))
        emu_obj.conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        emu_obj._init_sensors(0, 0, 0, 0)
        emu_obj.conn.send.assert_has_calls(expected_calls)

    @mock.patch("emu_server.socket.socket.send", mock.Mock())
    def test_set_firmware(self):
        expected_calls = (mock.call(b"PR\r\n"),
                          mock.call(b"BO1\r\n"),
                          mock.call(b"BO0\r\n"),
                          mock.call(b"F/some/path\n\n"),
                          mock.call(b"OO\r\n"))
        emu_obj.conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        emu_obj.set_firmware_path("/some/path")
        emu_obj._set_firmware()
        emu_obj.conn.send.assert_has_calls(expected_calls)


def emu_available():
    return True if pytest.emu else False


@pytest.mark.skipif(not emu_available(), reason="EMU not available")
class TestOnEmuDUT:

    def test_emu_server_sensor_motor(self):

        pytest.emu_obj.set_sensor_value(SENSOR_2, 170)
        time.sleep(1)
        assert pytest.emu_obj.lcd_state[SENSOR_2_ACTIVE] is True
        assert pytest.emu_obj.lcd_state[MOTOR_A_FWD] is True
        assert pytest.emu_obj.lcd_state[MOTOR_A_REV] is False
        assert pytest.emu_obj.motor_state[MOTOR_A]["dir"] == DIR_REV
        assert pytest.emu_obj.motor_state[MOTOR_A]["speed"] >= 255

        pytest.emu_obj.set_sensor_value(SENSOR_2, 1023)
        time.sleep(1)
        assert pytest.emu_obj.lcd_state[SENSOR_2_ACTIVE] is False
        assert pytest.emu_obj.lcd_state[MOTOR_A_FWD] is False
        assert pytest.emu_obj.lcd_state[MOTOR_A_REV] is True
        assert pytest.emu_obj.motor_state[MOTOR_A]["dir"] == DIR_FWD
        # linetrack.c has a turn speed of 2/3 * max_speed
        assert pytest.emu_obj.motor_state[MOTOR_A]["speed"] >= ((255 / 3) * 2) * 0.9
