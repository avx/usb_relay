import serial
import time

class usb_relay:
    """---
    Serial port exchange protocol with USB relay:

    Request: 4 bytes length.
    Reply: 4 bytes.
    Message format: start_symbol, port_number, command, check_sum
    Start symbol = 0xa0
    Port number starts since 1
    Commands: 0 = close (quiet),
              1 = open (quiet),
              2 = close (with previous port status reply),
              3 = open (same as above),
              4 = switch (same as above),
              5 = request port status (reply current port status)
    Checksum: 0xa0 + port_number + command.
    ---"""
    keep_open = False

    def __init__(self, serial_port, keep_open = False):
        self.ser = serial.Serial(None, 9600, timeout=1)
        self.ser.port = serial_port
        self.keep_open = keep_open
        if keep_open:
            self._open_serial()

    def __del__(self):
        self._close_serial()
        del self.ser

    def _open_serial(self):
        if not self.ser.is_open:
#            print("open serial...")
            self.ser.open()

    def _close_serial(self):
        if self.ser.is_open:
#            print("close serial...")
            self.ser.close()

    def _relay_req(self, port, cmd, resp=True):
        req = [ 0xa0, port, cmd, 0xa0 + port + cmd ]
        self._open_serial()
        self.ser.write(req)
        time.sleep(0.02)
        if resp:
            res = self.ser.read(4)
        if not self.keep_open:
            self._close_serial()
        if resp:
            if len(res) == 4 and res[0] == 0xa0:
                return res[2]
            else:
                return -1

    def close_q(self, port):
        """quiet close relay port"""
        return self._relay_req(port, 0, False)

    def open_q(self, port):
        """quiet open relay port"""
        return self._relay_req(port, 1, False)

    def close(self, port):
        """close relay port"""
        return self._relay_req(port, 2)

    def open(self, port):
        """open relay port"""
        return self._relay_req(port, 3)

    def switch(self, port):
        """switch relay port"""
        return self._relay_req(port, 4)

    def status(self, port):
        """current status of relay port"""
        return self._relay_req(port, 5)
