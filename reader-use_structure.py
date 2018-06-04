# reader.py
from ctypes import *
import time

class ReaderSession(Structure):
    _fields_=[
      ('file_name', c_char * 1024),
      ('status', c_int)
    ]

# dll.test.restype=POINTER(testdll)
# t=dll.test(byref(t))
# t.contents.file_name

def main():
  reader = cdll.LoadLibrary(b'./libreader.so')
  reader.session.restype = POINTER(ReaderSession)
  reader_sess = reader.session(b'./cifar-100-python/train')
  print(reader_sess.contents.file_name)
  print(reader_sess.contents.status)
  reader_thread = reader.start(reader_sess)
  if reader_thread == 0 :
    print("Error: failed to create thread of reader!")
    return
  time.sleep(10)
  reader.stop(reader_sess, reader_thread)
  
  
main()