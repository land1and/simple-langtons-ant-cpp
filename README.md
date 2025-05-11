## fast and simple langtons ant written in c++

this program simulates all patterns within a specified range and saves them as bmp images

msvc, gcc and clang have been tested, with the fastest being msvc: `cl main.cpp /O2 /GL /MT /arch:AVX2 /std:c++20 /link /LTCG`

testing done on windows 10, amd processor
