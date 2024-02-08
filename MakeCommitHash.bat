@echo off
FOR /F "tokens=*" %%g IN ('git rev-parse HEAD') DO (SET UEVR_COMMIT_HASH=%%g)

FOR /F "tokens=2 delims==" %%a IN ('wmic OS get localdatetime /value') DO (
    SET datetime=%%a
)

SET year=%datetime:~0,4%
SET month=%datetime:~4,2%
SET day=%datetime:~6,2%
SET hour=%datetime:~8,2%
SET minute=%datetime:~10,2%

echo #pragma once > src/CommitHash.hpp
echo #define UEVR_COMMIT_HASH "%UEVR_COMMIT_HASH%" >> src/CommitHash.hpp
echo #define UEVR_BUILD_DATE "%day%.%month%.%year%" >> src/CommitHash.hpp
echo #define UEVR_BUILD_TIME "%hour%:%minute%" >> src/CommitHash.hpp
