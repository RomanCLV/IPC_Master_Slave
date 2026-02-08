import struct
import time
import os
import ctypes
from ctypes import wintypes
from dataclasses import dataclass
from datetime import datetime
from threading import Thread
import traceback

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

FILE_MAP_ALL_ACCESS = 0x001F

SHM_NAME = "ipc_masterslave_shm"
SHM_SIZE = 548

# Offsets
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

# Flags
class IPCFlags:
    IDLE = 0x0
    MASTER_READY = 0x1
    SLAVE_STARTED = 0x2
    SLAVE_FINISHED = 0x4

# Error codes
class ErrorCode:
    SUCCESS = 0
    START_GREATER_THAN_END = 1
    OVERFLOW_ERROR = 2
    FILE_WRITE_ERROR = 3
    UNKNOWN_ERROR = 99

# États
class ConnectionState:
    SHM_NOT_FOUND = "ShMemNotFound"
    SHM_FOUND = "ShMemFound"

class SlaveState:
    IDLE = "Idle"
    PROCESSING = "Processing"
    WAITING_FOR_MASTER = "WaitingForMaster"

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

def write_uint32(ptr, offset: int, value: int):
    ctypes.memmove(ptr + offset, struct.pack("I", value), 4)

def write_int32(ptr, offset: int, value: int):
    ctypes.memmove(ptr + offset, struct.pack("i", value), 4)

def write_c_string(ptr, offset: int, max_len: int, text: str):
    encoded = text.encode('utf-8')[:max_len-1]
    buffer = encoded + b'\x00' * (max_len - len(encoded))
    ctypes.memmove(ptr + offset, buffer, max_len)

def shared_memory_exists(name: str, size: int):
    handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, False, name)
    if handle:
        ptr = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, size)
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

def compute_sum(start: int, end: int):
    """Calcule la somme de start à end (inclus)"""
    if start > end:
        return (ErrorCode.START_GREATER_THAN_END, 0)
    
    try:
        # Formule de Gauss: n*(n+1)/2 mais pour une plage [start, end]
        # Somme de start à end = somme(0 à end) - somme(0 à start-1)
        n_end = end
        n_start = start - 1
        
        sum_end = (n_end * (n_end + 1)) // 2
        sum_start = (n_start * (n_start + 1)) // 2
        
        result = sum_end - sum_start
        
        # Vérifier l'overflow int32
        if result > 2147483647 or result < -2147483648:
            return (ErrorCode.OVERFLOW_ERROR, 0)
        
        return (ErrorCode.SUCCESS, result)
    
    except Exception as e:
        print(f"Error computing sum: {e}")
        return (ErrorCode.UNKNOWN_ERROR, 0)

def compute_sum_slow(start: int, end: int):
    """Calcule la somme de start à end (inclus) avec une boucle"""
    if start > end:
        return (ErrorCode.START_GREATER_THAN_END, 0)
    
    try:
        result = 0
        current = start
        
        while current <= end:
            result += current
            current += 1
            
            # Vérifier l'overflow int32 pendant le calcul
            if result > 2147483647 or result < -2147483648:
                return (ErrorCode.OVERFLOW_ERROR, 0)
        
        return (ErrorCode.SUCCESS, result)
    
    except Exception as e:
        print(f"Error computing sum: {e}")
        return (ErrorCode.UNKNOWN_ERROR, 0)

def create_result_file(folder: str, result: int, elapsed_ms: int) -> tuple:
    """Crée un fichier horodaté avec le résultat"""
    try:
        # Créer le dossier si nécessaire
        if not os.path.exists(folder):
            os.makedirs(folder)
        
        # Nom du fichier horodaté
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]
        filename = f"result_{timestamp}.txt"
        filepath = os.path.join(folder, filename)
        
        # Écrire le résultat
        with open(filepath, 'w') as f:
            f.write(f"Result: {result}\n")
            f.write(f"Duration: {elapsed_ms}\n")
        
        return (ErrorCode.SUCCESS, filename)
    
    except Exception as e:
        print(f"Error creating result file: {e}")
        traceback.print_exc()
        return (ErrorCode.FILE_WRITE_ERROR, "")

def worker_loop():
    """Boucle principale du worker thread"""
    print(f"Worker thread started - PID: {os.getpid()}")
    
    connection_state = ConnectionState.SHM_NOT_FOUND
    slave_state = SlaveState.IDLE
    
    handle = None
    ptr = None
    
    shared_data = SharedData()
    
    while True:
        try:
            # Tentative de connexion à la mémoire partagée
            if connection_state == ConnectionState.SHM_NOT_FOUND:
                handle, ptr = shared_memory_exists(SHM_NAME, SHM_SIZE)
                if handle and ptr:
                    connection_state = ConnectionState.SHM_FOUND
                    slave_state = SlaveState.IDLE
                    print("> Connected to shared memory")
                else:
                    time.sleep(0.5)
                    continue
            
            # Vérifier que la mémoire existe toujours
            if not ptr:
                connection_state = ConnectionState.SHM_NOT_FOUND
                slave_state = SlaveState.IDLE
                print("> Lost connection to shared memory")
                continue
            
            # Lire les données
            raw = read_shared_memory(ptr, SHM_SIZE)
            if not raw or not extract_data_shared_memory(raw, shared_data):
                print("! Invalid shared memory data")
                time.sleep(0.1)
                continue
            
            # Machine à états
            if slave_state == SlaveState.IDLE:
                if shared_data.flags == IPCFlags.MASTER_READY:
                    print(f"> Starting computation: sum({shared_data.start} to {shared_data.end})")
                    
                    # Indexer la réponse sur le compteur de requete
                    write_uint32(ptr, OFFSET_RES_COUNTER, shared_data.req_counter)
                    print("write res_counter:", shared_data.req_counter)

                    # Signaler qu'on commence
                    write_uint32(ptr, OFFSET_FLAGS, IPCFlags.SLAVE_STARTED)

                    slave_state = SlaveState.PROCESSING
                    
                    # Enregistrer le timestamp de départ
                    start_time = time.perf_counter()
                    
                    # Faire le calcul
                    error_code, result = compute_sum_slow(shared_data.start, shared_data.end)
                    
                    # Enregistrer le timestamp de fin
                    end_time = time.perf_counter()
                    elapsed_ms = int((end_time - start_time) * 1000)
                    
                    print(f"  Computation done in {elapsed_ms} ms - Result: {result} - Code: {error_code}")
                    
                    # Créer le fichier de résultat si succès
                    filename = ""
                    if error_code == ErrorCode.SUCCESS:
                        file_error, filename = create_result_file(
                            shared_data.folder, result, elapsed_ms
                        )
                        if file_error != ErrorCode.SUCCESS:
                            error_code = file_error
                            print(f"  ! Error creating file: {file_error}")
                    
                    # Écrire les outputs
                    write_int32(ptr, OFFSET_CODE, error_code)
                    write_int32(ptr, OFFSET_SUM, result if error_code == ErrorCode.SUCCESS else 0)
                    write_c_string(ptr, OFFSET_RESULT_FILE, 256, filename)
                    
                    # Signaler la fin
                    write_uint32(ptr, OFFSET_FLAGS, IPCFlags.SLAVE_FINISHED)
                    slave_state = SlaveState.WAITING_FOR_MASTER
                    
                    print(f"> Computation complete - waiting for master ACK")
            
            elif slave_state == SlaveState.WAITING_FOR_MASTER:
                if shared_data.flags == IPCFlags.IDLE:
                    slave_state = SlaveState.IDLE
                    print("> Back to IDLE state")
            
            time.sleep(0.01)  # 10ms polling
        
        except Exception as e:
            print(f"! Worker error: {e}")
            traceback.print_exc()
            time.sleep(1)
        
        finally:
            # Cleanup temporaire (sera refait au prochain tour)
            if ptr and connection_state == ConnectionState.SHM_NOT_FOUND:
                kernel32.UnmapViewOfFile(ptr)
                ptr = None
            if handle and connection_state == ConnectionState.SHM_NOT_FOUND:
                kernel32.CloseHandle(handle)
                handle = None

def main():
    print("=" * 50)
    print("SLAVE PROCESS STARTED")
    print(f"PID: {os.getpid()}")
    print("=" * 50)
    
    # Démarrer le thread de travail
    worker_thread = Thread(target=worker_loop, daemon=True)
    worker_thread.start()
    
    # Garder le processus actif
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nSlave process interrupted")

if __name__ == "__main__":
    main()
