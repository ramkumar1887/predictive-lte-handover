@echo off
setlocal
echo =======================================================
echo Building Predictive LTE Handover Simulation in C++
echo =======================================================

if not exist build mkdir build
cd build

cmake .. -G "Visual Studio 17 2022" -A x64
if %errorlevel% neq 0 (
    echo Fallback to default generator...
    cmake ..
)

cmake --build . --config Release
if %errorlevel% neq 0 (
    echo Build failed!
    exit /b %errorlevel%
)

echo.
echo Build Successful! Running simulation...
if exist Release\lte_handover_sim.exe (
    Release\lte_handover_sim.exe
) else if exist lte_handover_sim.exe (
    lte_handover_sim.exe
) else (
    echo Binary generated.
)
