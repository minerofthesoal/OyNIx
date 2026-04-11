' OyNIx Browser v2.3 - Windows Silent Launcher
' Launches OyNIx without any console window showing
' Use this for desktop shortcuts and Start Menu entries

Set WshShell = CreateObject("WScript.Shell")
Set FSO = CreateObject("Scripting.FileSystemObject")

' Get the directory this script lives in
ScriptDir = FSO.GetParentFolderName(WScript.ScriptFullName)

' Try pythonw first (no console), then python
PythonW = ""

' Check bundled Python
If FSO.FileExists(ScriptDir & "\python\pythonw.exe") Then
    PythonW = """" & ScriptDir & "\python\pythonw.exe" & """"
ElseIf FSO.FileExists(ScriptDir & "\python\python.exe") Then
    PythonW = """" & ScriptDir & "\python\python.exe" & """"
Else
    ' Try system Python
    On Error Resume Next
    PythonW = "pythonw"
    WshShell.Run PythonW & " --version", 0, True
    If Err.Number <> 0 Then
        PythonW = "python"
        Err.Clear
    End If
    On Error Goto 0
End If

' Set PYTHONPATH
Env = WshShell.Environment("Process")
Env("PYTHONPATH") = ScriptDir & ";" & Env("PYTHONPATH")

' Launch the browser (0 = hidden window, False = don't wait)
WshShell.Run PythonW & " -m oynix", 0, False
