#!/usr/bin/env python3
"""Native CMake/Ninja argv transport, not Zig compilation or Player acceptance."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class SDKCommandTests(unittest.TestCase):
    def run_project(self, body, values):
        temporary = tempfile.TemporaryDirectory(prefix='Nexora SDK 陰陽 commands ')
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        build = root / 'build'
        values.update(tool=sys.executable, output=str(root / 'arguments.json'))
        (root / 'data.json').write_text(json.dumps(values), encoding='utf-8')
        header = '''cmake_minimum_required(VERSION 3.24)
project(SDKCommandTransport LANGUAGES NONE)
include("${SDK_MODULE}")
file(READ "${CMAKE_SOURCE_DIR}/data.json" data)
foreach(key tool output code first second manifest)
  string(JSON ${key} ERROR_VARIABLE missing GET "${data}" ${key})
endforeach()
set(runner "${CMAKE_BINARY_DIR}/commands.cmake")
file(WRITE "${runner}" "# Private SDK command test.\\n")
'''
        (root / 'CMakeLists.txt').write_text(header + body, encoding='utf-8')
        configured = subprocess.run(
            [TOOLS.cmake, '-S', str(root), '-B', str(build), '-G', 'Ninja',
             '-DCMAKE_MAKE_PROGRAM=' + TOOLS.ninja, '-DSDK_MODULE=' + TOOLS.module],
            capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=30)
        self.assertEqual(configured.returncode, 0, configured.stdout + configured.stderr)
        command = [TOOLS.cmake, '--build', str(build), '--target', 'run']
        options = {}
        if sys.platform == 'win32':
            # Exercise an OEM code page in a hidden child-owned console. The user's console
            # is untouched. ASCII wrapper data uses wide environment paths; any generated
            # UTF-8 Ninja .bat still exposes the original Unicode working-directory bug.
            wrapper = root / 'invoke.cmd'
            wrapper.write_bytes(b'@echo off\r\nchcp 437 >nul\r\n'
                                b'if errorlevel 1 exit /b 88\r\n'
                                b'echo SDK_TEST_PRIVATE_CODEPAGE=437\r\n'
                                b'"%SDK_TEST_CMAKE%" --build "%SDK_TEST_BUILD%" --target run\r\n'
                                b'exit /b %errorlevel%\r\n')
            environment = dict(os.environ, SDK_TEST_CMAKE=TOOLS.cmake, SDK_TEST_BUILD=str(build))
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = subprocess.SW_HIDE
            command = f'"{os.environ["COMSPEC"]}" /d /s /c ""{wrapper}""'
            options = dict(env=environment, creationflags=subprocess.CREATE_NEW_CONSOLE,
                           startupinfo=startup)
        result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8',
                                errors='replace', timeout=30, **options)
        if sys.platform == 'win32':
            self.assertIn('SDK_TEST_PRIVATE_CODEPAGE=437', result.stdout)
        return root, build, result

    def test_long_unicode_literal_arguments_survive_ninja(self):
        first = 'a' * 10000
        second = '\n中文 ${DO_NOT_EXPAND}; "quote" \\backslash ]=] ]==] & | < > ^'
        code = ('import json,sys; '
                'open(sys.argv[1],"w",encoding="utf-8").write('
                'json.dumps(sys.argv[2:],ensure_ascii=False))')
        root, build, result = self.run_project('''
_nexora_game_append_command("${runner}" "${tool}" -c "${code}" "${output}" "${first}" "${second}")
add_custom_target(run COMMAND "${CMAKE_COMMAND}" -P "${runner}" VERBATIM)
''', dict(code=code, first=first, second=second))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(json.loads((root / 'arguments.json').read_text(encoding='utf-8')),
                         [first, second])
        self.assertFalse(list(build.rglob('*.bat')), 'Long argv escaped into a code-page batch file')

    def test_failed_command_stops_before_success_marker(self):
        _, _, result = self.run_project('''
_nexora_game_append_command("${runner}" "${tool}" -c "import sys; sys.exit(7)")
_nexora_game_append_command("${runner}" "${CMAKE_COMMAND}" -E echo NEXORA_SCRIPT_TESTS_PASSED)
add_custom_target(run COMMAND "${CMAKE_COMMAND}" -P "${runner}" VERBATIM)
''', {})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Zig SDK command failed: 7', result.stdout + result.stderr)
        self.assertNotIn('NEXORA_SCRIPT_TESTS_PASSED', result.stdout + result.stderr)

    def test_public_sdk_preserves_three_step_order_and_module_options(self):
        # This interpreter stub only records native argv. Real Zig/ABI execution is covered
        # independently by editor.game_script_build and editor.game_script_authored_tests.
        body = '''
file(WRITE "${CMAKE_SOURCE_DIR}/game.zig" "// Command transport only.\\n")
file(WRITE "${CMAKE_SOURCE_DIR}/project.json" "{\\"schema\\":1,\\"module_name\\":\\"TransportFixture\\",\\"script\\":\\"game.zig\\"}")
set(stub [=[import json,sys
with open("calls.jsonl","a",encoding="utf-8") as out:
    out.write(json.dumps(sys.argv)+"\\n")
]=])
file(WRITE "${CMAKE_BINARY_DIR}/build-lib" "${stub}")
file(WRITE "${CMAKE_BINARY_DIR}/test" "${stub}")
nexora_add_game_module(run PROJECT "${CMAKE_SOURCE_DIR}/project.json"
  OUTPUT "${CMAKE_BINARY_DIR}/output" ZIG "${tool}")
'''
        _, build, result = self.run_project(body, {})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        calls = [json.loads(line) for line in
                 (build / 'calls.jsonl').read_text(encoding='utf-8').splitlines()]
        self.assertEqual([Path(args[0]).name for args in calls], ['build-lib', 'test', 'test'])
        self.assertEqual(calls[0].count('-OReleaseSafe'), 3)
        self.assertEqual(calls[1].count('-ODebug'), 3)
        self.assertEqual(calls[2].count('-ODebug'), 2)
        if sys.platform == 'win32':
            self.assertEqual([args.count('x86_64-windows-msvc') for args in calls], [3, 3, 2])
        self.assertEqual(result.stdout.count('NEXORA_SCRIPT_TESTS_PASSED'), 1)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--cmake', required=True)
    parser.add_argument('--ninja', required=True)
    parser.add_argument('--module', required=True)
    TOOLS = parser.parse_args()
    unittest.main(argv=[sys.argv[0]], verbosity=2)
