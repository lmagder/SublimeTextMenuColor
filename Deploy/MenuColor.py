import sublime
import sublime_plugin
import ctypes
import inspect
import os

class DLLInterface(object):
    def __init__(self):
        curFile = inspect.getsourcefile(DLLInterface)
        curDir = os.path.dirname(curFile)
        if ctypes.sizeof(ctypes.c_voidp) == 4:
            dllName = "SublimeTextMenuColor32.dll"
        else:
            dllName = "SublimeTextMenuColor64.dll"

        dllPath = os.path.join(curDir, dllName)
        print("Loading DLL {0}".format(dllPath))
        
        ctypes.windll.kernel32.LoadLibraryW.restype = ctypes.c_void_p
        self.DLLHandle = ctypes.windll.kernel32.LoadLibraryW(dllPath)
        self.DLLObject = ctypes.CDLL(dllName, handle=self.DLLHandle)
        print("Loaded {0}".format(self.DLLHandle))
        print("Loaded {0} {1}".format(self.DLLObject._handle, self.DLLObject._name))

        printCallbackProto = ctypes.CFUNCTYPE(None, ctypes.c_wchar_p)
        setPrintCallbackProto = ctypes.CFUNCTYPE(None, printCallbackProto)
        self.setPrintCallback = setPrintCallbackProto(("SetPrintCallback", self.DLLObject))
        self.printCallback = printCallbackProto(print)
        self.setPrintCallback(self.printCallback)

        findTopLevelWindowsProto = ctypes.CFUNCTYPE(ctypes.c_int32)
        self.findTopLevelWindows = findTopLevelWindowsProto(("FindTopLevelWindows", self.DLLObject),())

        loadIntoMainProcessProto = ctypes.CFUNCTYPE(ctypes.c_bool)
        self.loadIntoMainProcess = loadIntoMainProcessProto(("LoadIntoMainProcess", self.DLLObject),())

        unloadFromMainProcessProto = ctypes.CFUNCTYPE(ctypes.c_bool)
        self.unloadFromMainProcess = unloadFromMainProcessProto(("UnloadFromMainProcess", self.DLLObject),())

    def __del__(self):
        print("Unloading")
        del self.setPrintCallback
        del self.findTopLevelWindows
        del self.loadIntoMainProcess
        del self.unloadFromMainProcess
        del self.DLLObject
        ctypes.windll.kernel32.FreeLibrary.argtypes = [ctypes.c_void_p]
        ctypes.windll.kernel32.FreeLibrary(self.DLLHandle)


DLLInstance = None

class DebugCommand(sublime_plugin.ApplicationCommand):
    def run(self):
        print("Called {0}".format(DLLInstance.findTopLevelWindows()))

class DetectNewWindowCommand(sublime_plugin.WindowCommand):
    def run(self):
        pass
    def __init__(self, *args, **kwargs):
        super(DetectNewWindowCommand, self).__init__(*args, **kwargs)
        print("new window")
        if DLLInstance:
            DLLInstance.findTopLevelWindows()

def plugin_loaded():
    global DLLInstance
    DLLInstance = DLLInterface()
    if not DLLInstance.loadIntoMainProcess():
        raise Exception("loadIntoMainProcess failed")
    DLLInstance.findTopLevelWindows()

def plugin_unloaded():
    global DLLInstance
    if not DLLInstance.unloadFromMainProcess():
        raise Exception("unloadFromMainProcess failed")

    print("Unloaded")
    del DLLInstance

