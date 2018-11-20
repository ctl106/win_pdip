@echo off
net.exe session 1>NUL 2>NUL || (Echo 0 & goto end)
Echo 1
:end