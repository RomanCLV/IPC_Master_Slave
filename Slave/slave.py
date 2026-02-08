import threading
import time

class Worker(threading.Thread):
    def __init__(self):
        super().__init__()
        self._running = True

    def run(self):
        print("Worker thread started")
        while self._running:
            # boucle vide mais non bloquante CPU
            time.sleep(1)

        print("Worker thread stopped")

    def stop(self):
        self._running = False


def main():
    print("Slave process started")
    print(f"PID: {os.getpid()}")

    worker = Worker()
    worker.start()

    try:
        # boucle principale du process
        while True:
            time.sleep(1)

    except KeyboardInterrupt:
        print("Stopping...")

    worker.stop()
    worker.join()
    print("Slave process exited")


if __name__ == "__main__":
    import os
    main()
