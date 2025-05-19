import socket
import sys
import time
from queue import Queue
from contextlib import closing

IR_TCP_PORT = 50637

SENSOR_1 = 2
SENSOR_2 = 1
SENSOR_3 = 0
BATTERY = 3

MOTOR_A = 0
MOTOR_B = 1
MOTOR_C = 2

DIR_BRAKE = 3
DIR_FWD = 2
DIR_REV = 1
DIR_OFF = 0

MOTOR_A_X = 58
MOTOR_A_FWD = 30
MOTOR_A_REV = 26
MOTOR_B_X = 6
MOTOR_B_FWD = 10
MOTOR_B_REV = 34
MOTOR_C_X = 14
MOTOR_C_FWD = 38
MOTOR_C_REV = 22

SENSOR_1_ACTIVE = 49
SENSOR_1_X = 48
SENSOR_2_ACTIVE = 40
SENSOR_2_X = 44
SENSOR_3_ACTIVE = 20
SENSOR_3_X = 36

# CMDS:
# A for sensors
# M for motors
# B for buttons
#    O on/off
#    P program
#    R run
#    V view
# F for firmware
# R for
# P for peripherals
#    R for reset
#    D for debug
# O for bibo os, if loaded, otherwise brickos
#   O check os
# L for LCD
#
# LCD elements:
# lcd elements are stored in an array /container
# format for lcd command is: LX,YY
# lcd element id = (X*8) + i, i in 0 to 7
# lit based on if YY & (1 << i) == (1 << i)
#
# So, L1,02 would give:
#    idx: (1 * 8) + i, i in (0 to 7) =>
#    8
#    9
#    10
#    11
#    12
#    12
#    14
#
#    val:
#    2 & (1 << i) == 1, i in(0 to 7) =>
#    so idx 9 is lit
#
#  L2,a0 => 16, 17, 18, 19, 20, 21, 22
#        => a0 = 160 = 10100000; i=5, i=7
#
# walking figure:
# 0,1,2,3
#
# digit 0:
# 12, 37, 21, 13, 39, 23, 15
#
# digit 1:
# 4, 45, 33, 5, 47, 35, 7
#
# digit 2:
# 8, 53, 41, 9, 55, 43, 11, 46
#
# digit 3:
# 24, 69, 65, 25, 71, 67, 27, 54
#
# digit 4:
# 28, 61, 57, 29, 63, 59, 31, 70
#
# digit 5 (minus):
# 62


def format_byte_string(msg):
    return bytes(msg.encode("ascii", "ignore"))


def find_free_port():
    with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        s.bind(("", 0))
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        return s.getsockname()[1]


class EmuServer:
    def __init__(self, ip, port):
        self.ip = ip
        self.port = port
        self.in_queue = Queue()
        self.out_queue = Queue()
        self.init_done = False
        self.run_forever = True
        self.rom_path = None
        self.firmware_path = None
        self.conn = None
        self.lcd_state = {key: False for key in range(0, 100)}
        self.motor_state = {
            MOTOR_A: {"dir": DIR_OFF, "speed": 0},
            MOTOR_B: {"dir": DIR_OFF, "speed": 0},
            MOTOR_C: {"dir": DIR_OFF, "speed": 0}
        }

    def set_rom_path(self, rom_path):
        self.rom_path = rom_path

    def set_firmware_path(self, firmware_path):
        self.firmware_path = firmware_path

    def send(self, data):
        self.in_queue.put(data)

    def recv(self):
        msgs = []
        for _ in range(self.out_queue.qsize()):
            msgs.append(self.out_queue.get())

        return msgs

    def _socket_recv(self):
        data = []
        while True:
            # It might be slow to read 1 byte a time
            # but we only expect packages to be between 1 and 10 bytes,
            # with each message being terminated by \n.
            # And message won't arrive that often
            recv_data = self.conn.recv(1).decode("ascii")
            if recv_data == "\n":
                break
            else:
                data.append(recv_data)
        return "".join(data)

    def _recv(self, queue):
        msg = None
        for _ in range(queue.qsize()):
            msg = self.queue.get()

        return msg

    def flush(self):
        with self.out_queue.mutex:
            self.out_queue.queue.clear()

    def send_cmd(self, msg, end_of_line="\r\n"):
        self.conn.send(format_byte_string(f"{msg}{end_of_line}"))

    def exit(self):
        self.run_forever = False

    def emulator_status_up(self):
        return self.init_done

    def set_sensor_value(self, sensor, value):
        if value >= 0 and value < 1024:
            self.send_cmd(f"A{sensor}{value:03x}")

    def _init_sensors(self, sensor_1, sensor_2, sensor_3, battery):
        cmd_sequence = [
            (SENSOR_1, sensor_1),
            (SENSOR_2, sensor_2),
            (SENSOR_3, sensor_3),
            (BATTERY, battery),
        ]

        for cmd in cmd_sequence:
            self.set_sensor_value(cmd[0], cmd[1])
            time.sleep(0.2)

    def _set_firmware(self):
        cmd_sequence = [
            "PR",  # Reset peripherals
            "BO1",  # Button On pressed
            "BO0",  # Button On released
            (f"F{self.firmware_path}", "\n\n"),  # Load firmware
            "OO",  # perform OS check
        ]

        for cmd in cmd_sequence:
            if isinstance(cmd, str):
                self.send_cmd(cmd)
            else:
                self.send_cmd(cmd[0], end_of_line=cmd[1])
            time.sleep(0.2)

    def _wait_for_emulator_startup(self):
        started = False
        while not started:
            self.send_cmd("OO")  # perform OS check
            time.sleep(0.2)
            data = self._socket_recv()

            if "L0,00" in data:
                started = True

    def _run(self):

        lcd_cmd = "L"
        motor_cmd = "M"

        while self.run_forever:
            time.sleep(0.001)
            # Receive data and update state
            in_data = self._socket_recv()
            if in_data:
                if lcd_cmd in in_data:
                    start_value = 8 * int(in_data[1])
                    value = int(in_data[3:5], 16)
                    for i in range(0, 8):
                        self.lcd_state[start_value + i] = (value & (1 << i)) > 0
                if motor_cmd in in_data:
                    motor = int(in_data[1])
                    direction = int(in_data[3])
                    speed = int(in_data[5:])
                    self.motor_state[motor]["dir"] = direction
                    self.motor_state[motor]["speed"] = speed

                self.out_queue.put(in_data)

            # Send data
            data = self._recv(self.in_queue)
            if data:
                self.conn.send(data)

    def run_server(self):
        # Create a TCP/IP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # Bind the socket to the port
        server_address = (self.ip, self.port)
        sock.bind(server_address)
        # Listen for incoming connections
        sock.listen(1)

        while self.run_forever:
            # Wait for a connection
            connection, client_address = sock.accept()
            self.conn = connection

            try:
                self._wait_for_emulator_startup()
                time.sleep(0.02)
                self._init_sensors(1023, 1023, 1023, 320)
                time.sleep(0.02)
                if self.firmware_path:
                    self._set_firmware()
                time.sleep(0.02)
                self.init_done = True

                self._run()

            finally:
                self.conn.close()


if __name__ == "__main__":

    ip = "localhost"
    port = int(sys.argv[1])
    rom = sys.argv[2]
    firmware = sys.argv[3]

    emu_obj = EmuServer(ip, port)
    emu_obj.set_rom_path(rom)
    emu_obj.set_firmware_path(firmware)

    emu_obj.run_server()
