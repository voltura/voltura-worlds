"""Run GUI checks on a private Windows desktop without switching the user's desktop."""

import ctypes as c
from ctypes import wintypes as w
import pathlib
import shutil
import subprocess
import sys
import uuid


class StartupInfo(c.Structure):
    _fields_ = (
        [("cb", w.DWORD), ("reserved", w.LPWSTR), ("desktop", w.LPWSTR), ("title", w.LPWSTR)]
        + [(name, w.DWORD) for name in ("x", "y", "width", "height", "cols", "rows", "fill", "flags")]
        + [("show", w.WORD), ("reserved_size", w.WORD), ("reserved_data", c.c_void_p)]
        + [(name, w.HANDLE) for name in ("stdin", "stdout", "stderr")]
    )


class ProcessInfo(c.Structure):
    _fields_ = [("process", w.HANDLE), ("thread", w.HANDLE), ("pid", w.DWORD), ("tid", w.DWORD)]


class JobLimits(c.Structure):
    _fields_ = [
        ("process_time", c.c_int64),
        ("job_time", c.c_int64),
        ("flags", w.DWORD),
        ("min_working_set", c.c_size_t),
        ("max_working_set", c.c_size_t),
        ("active_processes", w.DWORD),
        ("affinity", c.c_size_t),
        ("priority", w.DWORD),
        ("scheduling", w.DWORD),
        ("io", c.c_uint64 * 6),
        ("process_memory", c.c_size_t),
        ("job_memory", c.c_size_t),
        ("peak_process_memory", c.c_size_t),
        ("peak_job_memory", c.c_size_t),
    ]


def api(library, name, result, *arguments):
    function = getattr(library, name)
    function.restype = result
    function.argtypes = list(arguments)
    return function


def check(value):
    if not value:
        raise c.WinError(c.get_last_error())
    return value


def desktop_name():
    user = c.WinDLL("user32", use_last_error=True)
    kernel = c.WinDLL("kernel32", use_last_error=True)
    thread_id = api(kernel, "GetCurrentThreadId", w.DWORD)()
    desktop = check(api(user, "GetThreadDesktop", w.HANDLE, w.DWORD)(thread_id))
    name = c.create_unicode_buffer(256)
    check(
        api(
            user,
            "GetUserObjectInformationW",
            w.BOOL,
            w.HANDLE,
            c.c_int,
            c.c_void_p,
            w.DWORD,
            c.POINTER(w.DWORD),
        )(desktop, 2, name, c.sizeof(name), None)
    )
    return name.value


def run(command):
    """Own the desktop and process tree; fail closed if isolation cannot be established."""
    import msvcrt

    user = c.WinDLL("user32", use_last_error=True)
    kernel = c.WinDLL("kernel32", use_last_error=True)
    close = api(kernel, "CloseHandle", w.BOOL, w.HANDLE)
    close_desktop = api(user, "CloseDesktop", w.BOOL, w.HANDLE)
    desktop = job = None
    process = ProcessInfo()
    handles = []
    try:
        name = "VolturaTests-" + uuid.uuid4().hex
        desktop = check(
            api(
                user,
                "CreateDesktopW",
                w.HANDLE,
                w.LPCWSTR,
                w.LPCWSTR,
                c.c_void_p,
                w.DWORD,
                w.DWORD,
                c.c_void_p,
            )(name, None, None, 0, 0x01FF, None)
        )
        job = check(api(kernel, "CreateJobObjectW", w.HANDLE, c.c_void_p, w.LPCWSTR)(None, None))
        limits = JobLimits()
        limits.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        check(
            api(kernel, "SetInformationJobObject", w.BOOL, w.HANDLE, c.c_int, c.c_void_p, w.DWORD)(
                job, 9, c.byref(limits), c.sizeof(limits)
            )
        )
        startup = StartupInfo()
        startup.cb = c.sizeof(startup)
        startup.desktop = name
        startup.flags = 0x100  # STARTF_USESTDHANDLES
        current = api(kernel, "GetCurrentProcess", w.HANDLE)()
        duplicate = api(
            kernel,
            "DuplicateHandle",
            w.BOOL,
            w.HANDLE,
            w.HANDLE,
            w.HANDLE,
            c.POINTER(w.HANDLE),
            w.DWORD,
            w.BOOL,
            w.DWORD,
        )
        for field, stream in (("stdin", sys.stdin), ("stdout", sys.stdout), ("stderr", sys.stderr)):
            handle = w.HANDLE()
            check(
                duplicate(
                    current, msvcrt.get_osfhandle(stream.fileno()), current, c.byref(handle), 0, True, 2
                )
            )
            handles.append(handle)
            setattr(startup, field, handle)
        executable = shutil.which(command[0])
        if not executable:
            raise FileNotFoundError(command[0])
        arguments = c.create_unicode_buffer(subprocess.list2cmdline([executable, *command[1:]]))
        create = api(
            kernel,
            "CreateProcessW",
            w.BOOL,
            w.LPCWSTR,
            w.LPWSTR,
            c.c_void_p,
            c.c_void_p,
            w.BOOL,
            w.DWORD,
            c.c_void_p,
            w.LPCWSTR,
            c.POINTER(StartupInfo),
            c.POINTER(ProcessInfo),
        )
        # Suspend until job ownership is established so descendants cannot escape cleanup.
        check(
            create(
                executable,
                arguments,
                None,
                None,
                True,
                0x08000004,
                None,
                None,
                c.byref(startup),
                c.byref(process),
            )
        )
        check(api(kernel, "AssignProcessToJobObject", w.BOOL, w.HANDLE, w.HANDLE)(job, process.process))
        if api(kernel, "ResumeThread", w.DWORD, w.HANDLE)(process.thread) == 0xFFFFFFFF:
            raise c.WinError(c.get_last_error())
        print(f"GUI checks on private desktop: {name}", flush=True)
        wait = api(kernel, "WaitForSingleObject", w.DWORD, w.HANDLE, w.DWORD)
        while True:
            status = wait(process.process, 100)
            if status == 0:
                break
            if status != 258:
                raise c.WinError(c.get_last_error())
        code = w.DWORD()
        check(
            api(kernel, "GetExitCodeProcess", w.BOOL, w.HANDLE, c.POINTER(w.DWORD))(
                process.process, c.byref(code)
            )
        )
        return code.value
    finally:
        if process.process:
            api(kernel, "TerminateProcess", w.BOOL, w.HANDLE, w.UINT)(process.process, 1)
        if job:
            close(job)  # Also terminates surviving descendants on success, failure, or Ctrl+C.
        for handle in (process.thread, process.process, *handles):
            if handle:
                close(handle)
        if desktop:
            close_desktop(desktop)


def isolate():
    """Called before a Python GUI harness creates windows or changes process state."""
    if "--interactive" in sys.argv:
        sys.argv.remove("--interactive")
        return
    if not desktop_name().startswith("VolturaTests-"):
        raise SystemExit(run([sys.executable, str(pathlib.Path(sys.argv[0]).resolve()), *sys.argv[1:]]))


if __name__ == "__main__":
    command = sys.argv[1:]
    if command[:1] == ["--"]:
        command.pop(0)
    if not command:
        raise SystemExit("Usage: python scripts/test_desktop.py -- executable [arguments ...]")
    raise SystemExit(run(command))
