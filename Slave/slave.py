import struct
import time
import os
import ctypes
from ctypes import wintypes
from dataclasses import dataclass

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
kernel32.UnmapViewOfFile.argtypes = [wintypes.LPCVOID]
kernel32.UnmapViewOfFile.restype = wintypes.BOOL

kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
kernel32.CloseHandle.restype = wintypes.BOOL

OpenFileMapping = kernel32.OpenFileMappingW
OpenFileMapping.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]
OpenFileMapping.restype = wintypes.HANDLE

MapViewOfFile = kernel32.MapViewOfFile
MapViewOfFile.restype = wintypes.LPVOID
MapViewOfFile.argtypes = [
    wintypes.HANDLE,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.DWORD,
    ctypes.c_size_t
]

FILE_MAP_READ = 0x0004

SHM_NAME = "ipc_masterslave_shm"
SHM_SIZE = 548

# Offsets (alignés sur le struct C++)
OFFSET_MAGIC = 0
OFFSET_VERSION = 4
OFFSET_FOLDER = 8
OFFSET_START = 264
OFFSET_END = 268
OFFSET_REQ_COUNTER = 272
OFFSET_RES_COUNTER = 276
OFFSET_RESULT_FILE = 280
OFFSET_CODE = 536
OFFSET_SUM = 540
OFFSET_FLAGS = 544

EXPECTED_MAGIC = 0xDEADBEEF

@dataclass
class SharedData:
    version: int = 0
    start: int = 0
    end: int = 0
    req_counter: int = 0
    res_counter: int = 0
    code_result: int = 0
    sum_result: int = 0
    flags: int = 0
    folder: str = ""
    result_file: str = ""

def read_c_string(buffer: bytes) -> str:
    return buffer.split(b'\x00', 1)[0].decode(errors="ignore")

def read_uint32(raw: bytes, offset: int) -> int:
    return struct.unpack_from("I", raw, offset)[0]

def read_int32(raw: bytes, offset: int) -> int:
    return struct.unpack_from("i", raw, offset)[0]

def shared_memory_exists(name: str, size: int):
    handle = OpenFileMapping(FILE_MAP_READ, False, name)
    if handle:
        ptr = MapViewOfFile(handle, FILE_MAP_READ, 0, 0, size)
        if ptr:
            return (handle, ptr)
        kernel32.CloseHandle(handle)
    return (None, None)

def read_shared_memory(ptr, size: int):
    if ptr:
        buffer = (ctypes.c_char * size).from_address(ptr)
        return bytes(buffer)
    else:
        return None

def extract_data_shared_memory(raw_data: bytes, out_data: SharedData):

    magic = read_uint32(raw_data, OFFSET_MAGIC)

    if magic != EXPECTED_MAGIC:
        return False

    out_data.version = read_uint32(raw_data, OFFSET_VERSION)
    out_data.start = read_int32(raw_data, OFFSET_START)
    out_data.end = read_int32(raw_data, OFFSET_END)

    out_data.req_counter = read_uint32(raw_data, OFFSET_REQ_COUNTER)
    out_data.res_counter = read_uint32(raw_data, OFFSET_RES_COUNTER)

    out_data.code_result = read_int32(raw_data, OFFSET_CODE)
    out_data.sum_result = read_int32(raw_data, OFFSET_SUM)

    out_data.flags = read_uint32(raw_data, OFFSET_FLAGS)

    out_data.folder = read_c_string(raw_data[OFFSET_FOLDER:OFFSET_FOLDER + 256])
    out_data.result_file = read_c_string(raw_data[OFFSET_RESULT_FILE:OFFSET_RESULT_FILE + 256])

    return True

def print_shared_data(shared_data: SharedData):
    print("====== SHARED MEMORY ======")
    print(f"PID: {os.getpid()}")
    print(f"Version          : {shared_data.version}")
    print(f"Folder           : {shared_data.folder}")
    print(f"Start / End      : {shared_data.start} -> {shared_data.end}")
    print(f"RequestCounter   : {shared_data.req_counter}")
    print(f"ResponseCounter  : {shared_data.res_counter}")
    print(f"CodeResult       : {shared_data.code_result}")
    print(f"SumResult        : {shared_data.sum_result}")
    print(f"Flags            : 0x{shared_data.flags:08X}")
    print(f"ResultFile       : {shared_data.result_file}")
    print("===========================")


def main():
    print("Slave process started")
    print(f"PID: {os.getpid()}")

    shared_data = SharedData()
    handle = None
    ptr = None

    run = True

    last_state = None


    while run:
        try:
            handle, ptr = shared_memory_exists(SHM_NAME, SHM_SIZE)
            if handle and ptr:
                state = "found"
                print("Shared memory found")

                raw = read_shared_memory(ptr, SHM_SIZE)

                if raw and extract_data_shared_memory(raw, shared_data):
                    print_shared_data(shared_data)
                else:
                    print("Magic is invalid")

            elif handle:
                state = "half_found"
                if state != last_state:
                    print("Shared memory found but can't get ptr")
            else:
                state = "not_found"
                if state != last_state:
                    print("Shared memory not found")
            
            last_state = state

            time.sleep(1)
        
        except Exception as e:
            print("Error:", e)
            run = False

        finally:
            if ptr:
                kernel32.UnmapViewOfFile(ptr)
                ptr = None
            if handle:
                kernel32.CloseHandle(handle)
                handle = None
            

if __name__ == "__main__":
    main()
