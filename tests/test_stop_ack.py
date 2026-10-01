"""编译两端实际 C 协议代码，验证认证、回执匹配和旧帧兼容性（无需硬件）。"""
import ctypes as c
import hashlib
import hmac
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CC = os.environ.get("HOST_CC") or shutil.which("gcc") or r"C:\Program Files (x86)\Dev-Cpp\MinGW64\bin\gcc.exe"
U8 = c.c_uint8
Key = U8 * 16
Address = U8 * 6


class Frame(c.Structure):
    _fields_ = [("type", U8), ("source", Address), ("destination", Address),
                ("boot_id", c.c_uint32), ("sequence", c.c_uint32),
                ("state", U8), ("key", Key)]


class StopAckTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="stop_ack_")
        cls.libs = []
        for project in ("STOP", "Assused"):
            src = ROOT / project / "src"
            dll = Path(cls.temp.name) / (project + ".dll")
            subprocess.run([CC, "-std=c99", "-Wall", "-Wextra", "-Werror", "-shared",
                            "-I", str(src), str(src / "safety_radio_frame.c"),
                            str(src / "sha256_sw.c"), "-o", str(dll)], check=True)
            lib = c.CDLL(str(dll))
            lib.safety_radio_encode.argtypes = [c.POINTER(Frame), c.POINTER(U8), c.POINTER(U8), c.c_size_t]
            lib.safety_radio_encode.restype = c.c_size_t
            lib.safety_radio_decode.argtypes = [c.POINTER(U8), c.c_size_t, c.POINTER(U8), c.POINTER(Frame)]
            lib.safety_radio_decode.restype = c.c_bool
            lib.safety_radio_stop_ack_matches.argtypes = [c.POINTER(Frame), c.POINTER(U8), c.POINTER(U8), c.c_uint32, c.c_uint32]
            lib.safety_radio_stop_ack_matches.restype = c.c_bool
            cls.libs.append(lib)
        cls.key = Key(*range(16))

    @classmethod
    def tearDownClass(cls):
        # Windows 已加载的 DLL 暂时不能删除，先释放句柄。
        if os.name == "nt":
            import _ctypes
            for lib in cls.libs:
                _ctypes.FreeLibrary(lib._handle)
        cls.libs.clear()
        cls.temp.cleanup()

    def frame(self, kind=5, state=1):
        return Frame(kind, Address(1, 2, 3, 4, 5, 6), Address(6, 5, 4, 3, 2, 1),
                     0x12345678, 42, state, Key())

    def encode(self, lib, frame, key=None):
        buf = (U8 * 49)()
        n = lib.safety_radio_encode(c.byref(frame), key, buf, len(buf))
        return bytes(buf[:n])

    def decode(self, lib, data, key=None):
        frame = Frame()
        ok = lib.safety_radio_decode((U8 * len(data)).from_buffer_copy(data), len(data), key, c.byref(frame))
        return ok, frame

    def test_roundtrip_and_independent_hmac(self):
        for lib in self.libs:
            for kind, state in ((3, 0), (3, 1), (4, 0), (5, 0), (5, 1)):
                with self.subTest(kind=kind, state=state, lib=lib._name):
                    data = self.encode(lib, self.frame(kind, state), self.key)
                    self.assertEqual(len(data), 33)
                    self.assertEqual(data[-8:], hmac.new(bytes(self.key), data[:-8], hashlib.sha256).digest()[:8])
                    ok, frame = self.decode(lib, data, self.key)
                    self.assertTrue(ok)
                    self.assertEqual((frame.type, frame.state, frame.sequence), (kind, state, 42))

    def test_tampering_wrong_or_missing_key(self):
        for lib in self.libs:
            data = self.encode(lib, self.frame(), self.key)
            self.assertFalse(self.decode(lib, data)[0])
            self.assertFalse(self.decode(lib, data, Key(*([99] * 16)))[0])
            for i in range(len(data)):
                bad = bytearray(data)
                bad[i] ^= 1
                self.assertFalse(self.decode(lib, bad, self.key)[0], f"byte {i}")
            self.assertEqual(self.encode(lib, self.frame()), b"")

    def test_invalid_length_and_semantics(self):
        for lib in self.libs:
            data = self.encode(lib, self.frame(), self.key)
            for n in range(33):
                self.assertFalse(self.decode(lib, data[:n], self.key)[0])
            self.assertFalse(self.decode(lib, data + b"\0", self.key)[0])
            for kind, state in ((4, 1), (5, 2), (6, 0)):
                self.assertEqual(self.encode(lib, self.frame(kind, state), self.key), b"")
                bad = bytearray(data)
                bad[3], bad[24] = kind, state
                bad[-8:] = hmac.new(bytes(self.key), bad[:-8], hashlib.sha256).digest()[:8]
                self.assertFalse(self.decode(lib, bad, self.key)[0])

    def test_stale_and_foreign_ack_not_matched(self):
        for lib in self.libs:
            frame = self.frame()
            args = (frame.source, frame.destination, frame.boot_id, frame.sequence)
            self.assertTrue(lib.safety_radio_stop_ack_matches(c.byref(frame), *args))
            for attribute in ("source", "destination", "boot_id", "sequence", "type", "state"):
                bad = self.frame()
                if attribute in ("source", "destination"):
                    getattr(bad, attribute)[0] ^= 1
                else:
                    setattr(bad, attribute, getattr(bad, attribute) + 1)
                self.assertFalse(lib.safety_radio_stop_ack_matches(c.byref(bad), *args), attribute)
            frame.state = 0  # 未确认电压的回执仍属于本请求，但不是 verified 状态。
            self.assertTrue(lib.safety_radio_stop_ack_matches(c.byref(frame), *args))
            self.assertEqual(frame.state, 0)

    def test_old_pair_frames_and_cross_device_compatibility(self):
        for kind in (1, 2):
            for lib in self.libs:
                data = self.encode(lib, self.frame(kind, 0))
                self.assertEqual(len(data), 49 if kind == 2 else 33)
                self.assertTrue(self.decode(lib, data)[0])
        data = self.encode(self.libs[0], self.frame(4, 0), self.key)
        self.assertTrue(self.decode(self.libs[1], data, self.key)[0])
        ack = self.encode(self.libs[1], self.frame(), self.key)
        self.assertTrue(self.decode(self.libs[0], ack, self.key)[0])


if __name__ == "__main__":
    unittest.main(verbosity=2)
