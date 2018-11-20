@echo off
wmic useraccount where name="%username%" get sid | findstr /V "SID"