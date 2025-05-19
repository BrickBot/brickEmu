import pytest
import subprocess
import time
import threading
from emu_server import EmuServer, find_free_port, IR_TCP_PORT


class EmuThread(threading.Thread):
    def __init__(self, emu_obj):
        threading.Thread.__init__(self)
        self.emu_obj = emu_obj

    def run(self):
        self.emu_obj.run_server()


def pytest_configure():
    pytest.emu_obj = None
    pytest.emu = None
    pytest.ir_server = None
    rom = "./rom.coff"
    firmware = "../brickos_bibo/kernel/bibo.coff"
    ir_server_bin = "./ir-server"
    emu_bin = "./emu"
    ir_client_bin = "./../brickos_bibo/util/dll"
    program = "./../brickos_bibo/demo/c/linetrack.lx"
    slot = 1
    resp = None
    pytest.emu_obj = EmuServer("localhost", find_free_port())
    pytest.emu_obj.set_rom_path(rom)
    pytest.emu_obj.set_firmware_path(firmware)

    try:
        pytest.ir_server = subprocess.Popen([ir_server_bin], stdout=subprocess.PIPE)
        time.sleep(0.2)
        emu_serv = threading.Thread(target=pytest.emu_obj.run_server)
        emu_serv.daemon = True
        emu_serv.start()
        pytest.emu = subprocess.Popen(
            [emu_bin, "-guiserverport", f"{pytest.emu_obj.port}", "-rom", f"{pytest.emu_obj.rom_path}"], stdout=subprocess.PIPE
        )

        while not pytest.emu_obj.emulator_status_up():
            time.sleep(0.2)
            continue
        resp = subprocess.check_output(
            [
                ir_client_bin,
                f"--tty=tcp:localhost:{IR_TCP_PORT}",
                "--verbose",
                # "--irmode=1", TODO: far mode misconfigured?? Loading seems more stable when running with far mode though
                f"--program={slot}",
                "--execute",
                program,
            ]
        )

    except subprocess.CalledProcessError as e:
        print(f"Error from subprocess: \n{e}")
        if resp:
            print(f"response from program transfer: {resp}")
        if pytest.emu:
            pytest.emu.kill()
            pytest.emu.stdout.close()
            pytest.emu.wait()
            pytest.emu = None
        if pytest.ir_server:
            pytest.ir_server.kill()
            pytest.ir_server.stdout.close()
            pytest.ir_server.wait()
            pytest.ir_server = None
        pytest.emu_obj.exit()
        pytest.emu_obj = None


@pytest.fixture(scope="session", autouse=True)
def setup_emu(request):
    if pytest.emu:
        request.addfinalizer(pytest.emu.kill)
        request.addfinalizer(pytest.ir_server.kill)
