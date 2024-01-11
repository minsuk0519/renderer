@echo off

SetLocal EnableDelayedExpansion

for /r %%i in (source\*) do (
	set LAST=%%i
	set LAST=!LAST:~-2!
	set VERT=vs
	set PIX=ps
	set COMP=cs
	if !LAST! equ !VERT! (
		echo %%i
		dxc.exe -T vs_6_5 -E VSMain %%i -Fo %%~ni%%~xi.cso
	)
	if !LAST! equ !PIX! (
		echo %%i
		dxc.exe -T ps_6_5 -E PSMain %%i -Fo %%~ni%%~xi.cso
	)
		if !LAST! equ !COMP! (
		echo %%i
		dxc.exe -T cs_6_5 -E CSMain %%i -Fo %%~ni%%~xi.cso
	)
)

pause