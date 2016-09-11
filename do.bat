@echo off
GOTO start

This script performs build actions for this project. Each
action is assigned a name, and this script accepts a series of
action as arguments. This allows the developer to selectively
execute parts of the build process, skipping potentially lengthy
parts of it.

Consolidating the build process into a single .bat file, rather than
splitting it into multiple files, has several advantages:

 * Variables common to several build processes do not need to be duplicated
 * It is easy to chain together any combinatation of build steps
 * It reduces clutter in the project directory
 * It consolidates all the build steps in one place
 * There is less overhead compared to calling separate .bat files

A disadvantage of this approach is that the shell cannot tab-complete
action names. Most action names are short, so this is not too
problematic. Also, a developer is free to create their own scripts
that call this one with common combinations of actions.
 
The first few lines of this script iterate over the actions. The
remainder of this file defines build actions, each of which begins with
a label named the same as the build action. All action labels begin with
an underscore, which is automatically prepended so the user does not have
to type it when calling this script. This prevents the user from jumping to
labels that are not build actions (accidentally or maliciously).

Example calls:

 * do help
 * do compile run
 * do clean compile run
 * do all

:start

REM if no arguments are provided, print help and exit
set firstAction=%1
IF [%firstAction%]==[] GOTO _help

REM declare variables shared by multiple build steps
set buildDir=build
set projectName=caveman

REM iterate over all the build actions
:nextAction
set action=%1
shift
IF [%action%]==[] GOTO:eof
CALL :_%action% || EXIT /B
GOTO nextAction

REM ***************
REM *             *
REM *   ACTIONS   *
REM *             *
REM ***************

REM removes all build artifacts
:_clean
	IF EXIST %buildDir% (
		rmdir /Q /S %buildDir% || EXIT /B 1
	)
	EXIT /B 0

REM compiles the source code
:_compile
	set debugOptions=/MTd /Ob0 /Od /Zi
	set releaseOptions=/MT /Ox
	set ignoredWarnings=
	set libraries=Gdi32.lib User32.lib

	mkdir %buildDir% 2> nul
	pushd %buildDir% > nul
	cl.exe /Fd%projectName% /Fe%projectName% /GR- /nologo /W4 /WX %ignoredWarnings% %debugOptions% ..\win32.cpp /link /INCREMENTAL:NO %libraries%
	set clErr=%errorlevel%
	popd
EXIT /B %clErr%

REM prints the usage of this command
:_help
	echo.Executes a series of build actions.
	echo.
	echo.do [action]...
	echo.
	echo.  action    the name of a build action
	echo.
	echo.Build actions are executed in the same order as they are provided. Available
	echo.actions include:
	echo.
	echo.  clean: deletes all build artifacts
	echo.  compile: compiles the source code into an executable
	echo.  help: prints the command usage
	echo.  run: launches the project's executable
	echo.
	echo.Example calls:
	echo.
	echo.  do compile
	echo.  do compile run
	echo.  do clean compile run
EXIT /B

REM runs the project executable
:_run
	%buildDir%\%projectName%.exe
EXIT /B

