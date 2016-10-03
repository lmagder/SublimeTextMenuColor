import sublime
import sublime_plugin
import ctypes
import inspect
import os

DLLInstance = None
Settings = None

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
        queryBoolSettingProto = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_wchar_p)
        queryBinaryResourceProto = ctypes.CFUNCTYPE(None, ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_size_t))

        setCallbacksProto = ctypes.CFUNCTYPE(None, printCallbackProto, queryBoolSettingProto, queryBinaryResourceProto)
        self.setCallbacks = setCallbacksProto(("SetCallbacks", self.DLLObject))

        self.printCallback = printCallbackProto(print)
        def getSetting(settingName):
            global Settings
            return Settings.get(settingName, False) == True
        self.settingCallback = queryBoolSettingProto(getSetting)

        def getResource(resName, outBufferPtr, outBufSize):
            print("Loading {0}".format(resName))
            resData = None
            try:
                resData = sublime.load_binary_resource(resName)
            except Exception:
                pass

            if resData is None:
                print("{0} is None".format(resName))
                outBufferPtr[0] = ctypes.c_void_p()
                outBufSize[0] = ctypes.c_size_t(0)
            else:
                print("{0} is {1} bytes".format(resName, len(resData)))
                ctypes.windll.kernel32.GetProcessHeap.restype = ctypes.c_void_p
                ctypes.windll.kernel32.HeapAlloc.restype = ctypes.c_void_p
                tempBuffer = ctypes.windll.kernel32.HeapAlloc(ctypes.c_void_p(ctypes.windll.kernel32.GetProcessHeap()), ctypes.c_ulong(0), ctypes.c_ulong(len(resData)))
                interopBuffer = (ctypes.c_byte * len(resData))(*resData)
                ctypes.windll.kernel32.RtlCopyMemory.restype = None
                ctypes.windll.kernel32.RtlCopyMemory(ctypes.c_void_p(tempBuffer), interopBuffer, ctypes.c_size_t(len(resData)))
                outBufferPtr[0] = ctypes.c_void_p(tempBuffer)
                outBufSize[0] = ctypes.c_size_t(len(resData))
        self.resoureCallback = queryBinaryResourceProto(getResource)

        self.setCallbacks(self.printCallback, self.settingCallback, self.resoureCallback)

        findTopLevelWindowsProto = ctypes.CFUNCTYPE(ctypes.c_bool)
        self.findTopLevelWindows = findTopLevelWindowsProto(("FindTopLevelWindows", self.DLLObject),())

        loadIntoMainProcessProto = ctypes.CFUNCTYPE(ctypes.c_bool)
        self.loadIntoMainProcess = loadIntoMainProcessProto(("LoadIntoMainProcess", self.DLLObject),())

        unloadFromMainProcessProto = ctypes.CFUNCTYPE(ctypes.c_bool)
        self.unloadFromMainProcess = unloadFromMainProcessProto(("UnloadFromMainProcess", self.DLLObject),())

        updateThemeProto = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_wchar_p)
        self.updateTheme = updateThemeProto(("UpdateTheme", self.DLLObject))

    def __del__(self):
        print("Unloading")
        del self.setCallbacks
        del self.findTopLevelWindows
        del self.updateTheme
        del self.loadIntoMainProcess
        del self.unloadFromMainProcess
        del self.DLLObject
        ctypes.windll.kernel32.FreeLibrary.argtypes = [ctypes.c_void_p]
        ctypes.windll.kernel32.FreeLibrary(self.DLLHandle)


def refreshTheme():
    global DLLInstance
    global Settings
    themeName = Settings.get('theme', 'Default.sublime-theme').lower()
    if themeName == refreshTheme.prevTheme:
        return
    refreshTheme.prevTheme = themeName
    for themeResource in sublime.find_resources('*.sublime-theme'):
        filename = os.path.basename(themeResource)
        if filename.lower() == themeName:
            defaultThemeData = sublime.load_resource('Packages/Theme - Default/Default.sublime-theme')
            themeData = sublime.load_resource(themeResource)
            if False:
                closeArrayIdx = defaultThemeData.rfind(']')
                defaultThemeData = defaultThemeData[:closeArrayIdx]

                startArrayIdx = themeData.find('[')+1
                themeData = themeData[startArrayIdx:]

                DLLInstance.updateTheme(defaultThemeData + "\n" + themeData)
            else:
                DLLInstance.updateTheme(themeData)

            break

refreshTheme.prevTheme = None

class DebugCommand(sublime_plugin.ApplicationCommand):
    def run(self):
        global DLLInstance
        DLLInstance.findTopLevelWindows()
        refreshTheme()

class DetectNewWindowCommand(sublime_plugin.WindowCommand):
    def run(self):
        pass
    def __init__(self, *args, **kwargs):
        global DLLInstance
        super(DetectNewWindowCommand, self).__init__(*args, **kwargs)
        print("new window")
        if DLLInstance is not None:
            DLLInstance.findTopLevelWindows()

def plugin_loaded():
    global DLLInstance
    global Settings
    Settings = sublime.load_settings('Preferences.sublime-settings')
    DLLInstance = DLLInterface()
    if not DLLInstance.loadIntoMainProcess():
        raise Exception("loadIntoMainProcess failed")

    refreshTheme()
    Settings.add_on_change("MenuColor", refreshTheme)
    DLLInstance.findTopLevelWindows()

def plugin_unloaded():
    global DLLInstance
    global Settings
    Settings.clear_on_change("MenuColor")
    Settings = None
    if DLLInstance is not None:
        if not DLLInstance.unloadFromMainProcess():
            raise Exception("unloadFromMainProcess failed")

    print("Unloaded")
    del DLLInstance

