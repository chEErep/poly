import ctypes as ct
import contextlib
import io
import unittest
from monitor import Event, parse_args, decode_bytes, format_event, build_program


class MonitorTests(unittest.TestCase):
    def test_invalid_arguments(self):
        for args in (['--pid','-1'],['--summary-every','nan'],['--summary-every','-2'],['--only',''],['--only','bad'],['--comm','x'*16]):
            with self.subTest(args=args), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit): parse_args(args)

    def test_control_characters_are_escaped(self):
        value=decode_bytes(b'path\x1b[31m\n\0ignored')
        self.assertNotIn('\x1b',value)
        self.assertNotIn('\n',value)
        self.assertNotIn('ignored',value)

    def test_pid_tid_and_syscall_details(self):
        event=Event()
        event.type=3; event.pid=201; event.tgid=200; event.fd=4; event.count=123
        event.comm=b'worker'
        line=format_event(event)
        self.assertIn('tid=201',line)
        self.assertIn('pid=200',line)
        self.assertIn('fd=4 count=123',line)
        self.assertEqual(ct.sizeof(Event),320)

    def test_program_generation_does_not_inject_comm_as_code(self):
        program=build_program(parse_args(['--only','execve','--comm','";x','--pid','123']),999)
        self.assertNotIn('";x',program)
        self.assertNotIn('FILTER_CODE',program)
        self.assertIn('tgid == 999U',program)
        self.assertIn('tgid != 123U',program)


if __name__=='__main__': unittest.main()
