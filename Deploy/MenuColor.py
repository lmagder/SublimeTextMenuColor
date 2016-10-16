import sublime
import sublime_plugin
import ctypes
import inspect
import os
import struct

DLLInstance = None
Settings = None
PluginSettings = None

def allocNativeHeapForArray(byteArray):
    if byteArray is None:
        return ctypes.c_void_p(), ctypes.c_size_t(0)
    else:
        #print("{0} is {1} bytes".format(resName, len(resData)))
        ctypes.windll.kernel32.GetProcessHeap.restype = ctypes.c_void_p
        ctypes.windll.kernel32.HeapAlloc.restype = ctypes.c_void_p
        tempBufferSz = len(byteArray)
        tempBuffer = ctypes.windll.kernel32.HeapAlloc(ctypes.c_void_p(ctypes.windll.kernel32.GetProcessHeap()), ctypes.c_ulong(0), ctypes.c_ulong(tempBufferSz))
        interopBuffer = (ctypes.c_byte * tempBufferSz)(*byteArray)
        ctypes.windll.kernel32.RtlCopyMemory.restype = None
        ctypes.windll.kernel32.RtlCopyMemory(ctypes.c_void_p(tempBuffer), interopBuffer, ctypes.c_size_t(tempBufferSz))
        return ctypes.c_void_p(tempBuffer), ctypes.c_size_t(tempBufferSz)

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
        queryBoolSettingProto = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_wchar_p, ctypes.c_bool)
        queryStringSettingProto = ctypes.CFUNCTYPE(None, ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_size_t))
        queryNumberSettingProto = ctypes.CFUNCTYPE(None, ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_size_t))
        queryBinaryResourceProto = ctypes.CFUNCTYPE(None, ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_size_t))

        setCallbacksProto = ctypes.CFUNCTYPE(None, printCallbackProto, queryBoolSettingProto, queryStringSettingProto, queryNumberSettingProto, queryBinaryResourceProto)
        self.setCallbacks = setCallbacksProto(("SetCallbacks", self.DLLObject))

        self.printCallback = printCallbackProto(print)
        def getBoolSetting(settingName, default):
            global Settings
            global PluginSettings
            if Settings is None or PluginSettings is None:
                return False
            return PluginSettings.get(settingName, Settings.get(settingName, default)) == True
        self.boolSettingCallback = queryBoolSettingProto(getBoolSetting)

        def getStringSetting(settingName, outBufferPtr, outBufSize):
            global Settings
            global PluginSettings
            if Settings is None or PluginSettings is None:
                return
            settingStr = PluginSettings.get(settingName, Settings.get(settingName, ""))
            if isinstance(settingStr, list):
                buffer = bytearray()
                for item in settingStr:
                    itemStr = str(item)
                    buffer.extend(itemStr.encode("utf-16-le"))
                    buffer.extend('\0'.encode("utf-16-le"))
                outBufferPtr[0], outBufSize[0] = allocNativeHeapForArray(buffer)
            else:
                settingStr = str(settingStr)
                outBufferPtr[0], outBufSize[0] = allocNativeHeapForArray(settingStr.encode("utf-16-le") + '\0'.encode("utf-16-le"))
            
        self.stringSettingCallback = queryStringSettingProto(getStringSetting)

        def getNumberSetting(settingName, outArrayPtr, outArraySize):
            global Settings
            global PluginSettings
            if Settings is None or PluginSettings is None:
                return
            settingStr = PluginSettings.get(settingName, Settings.get(settingName, 0.0))
            buffer = bytearray()
            outArraySize[0] = 0
            if isinstance(settingStr, list):
                for item in settingStr:
                    buffer.extend(struct.pack("d", item))
                outArraySize[0] = len(settingStr)
            else:
                buffer.extend(struct.pack("d", settingStr))
                outArraySize[0] = 1
            outBufferPtr, outBufSize = allocNativeHeapForArray(buffer)
            outArrayPtr[0] = outBufferPtr
            
        self.numberSettingCallback = queryNumberSettingProto(getNumberSetting)

        def getResource(resName, outBufferPtr, outBufSize):
            #print("Loading {0}".format(resName))
            resData = None
            try:
                resData = sublime.load_binary_resource(resName)
            except Exception:
                pass
            outBufferPtr[0], outBufSize[0] = allocNativeHeapForArray(resData)

        self.resoureCallback = queryBinaryResourceProto(getResource)

        self.setCallbacks(self.printCallback, self.boolSettingCallback, self.stringSettingCallback, self.numberSettingCallback, self.resoureCallback)

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
    for themeResource in sublime.find_resources('*.sublime-theme'):
        filename = os.path.basename(themeResource)
        if filename.lower() == themeName:
            themeData = sublime.load_resource(themeResource)
            print("Sending {0}".format(themeResource))
            DLLInstance.updateTheme(themeData)
            break

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
    global PluginSettings
    Settings = sublime.load_settings('Preferences.sublime-settings')
    PluginSettings = sublime.load_settings('MenuColor.sublime-settings')
    DLLInstance = DLLInterface()
    if not DLLInstance.loadIntoMainProcess():
        raise Exception("loadIntoMainProcess failed")

    refreshTheme()
    Settings.add_on_change("MenuColor", refreshTheme)
    PluginSettings.add_on_change("MenuColor", refreshTheme)
    DLLInstance.findTopLevelWindows()

def plugin_unloaded():
    global DLLInstance
    global Settings
    global PluginSettings
    Settings.clear_on_change("MenuColor")
    PluginSettings.clear_on_change("MenuColor")
    Settings = None
    PluginSettings = None
    if DLLInstance is not None:
        if not DLLInstance.unloadFromMainProcess():
            raise Exception("unloadFromMainProcess failed")

    print("Unloaded")
    del DLLInstance

